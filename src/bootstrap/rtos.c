#include <stdint.h>

#include "common.h"
#include "ipc.h"
#include "../drivers/io.h"
#include "../drivers/zbus.h"
#include "../lib/task.h"
#include "rtos.h"
#include "target_autostart.h"

#pragma codeseg CODE

#define TASK_STACK_SIZE 320u
#define KERNEL_STACK_SIZE 320u
#define STACK_PATTERN 0xA5u

#if (MAX_TASKS < 1u)
#error "MAX_TASKS must be at least 1 for boot task"
#endif

volatile task_control_block_t g_tasks[MAX_TASKS];
volatile task_control_block_t *g_current_tcb;
volatile uint8_t g_current_task;
volatile uint16_t g_kernel_sp;
volatile uint16_t g_tick_count;
volatile uint8_t g_boot_marker;
volatile uint8_t g_boot_slot_value;

static uint8_t g_task_stacks[MAX_TASKS][TASK_STACK_SIZE];
static uint8_t g_kernel_stack[KERNEL_STACK_SIZE];

/* CPU busy-rate accounting: incremented once per scheduler tick (50 Hz),
 * inside scheduler_select_next below. Resolution is one whole tick (20 ms)
 * -- a task credited for a tick may have blocked partway through it, so
 * this measures "which task/state the scheduler picked each tick", not
 * cycle-accurate occupancy. Kept private here (not in task_control_block_t)
 * and read back only through rtos_cpu_stats()'s DI/EI-guarded copy, since
 * the ISR updates these outside of any caller's control. */
static uint16_t g_task_busy_ticks[MAX_TASKS];
static uint16_t g_idle_ticks;

static void task_reset_runtime_fields(uint8_t slot)
{
    uint8_t i;

    g_tasks[slot].state = TASK_UNUSED;
    g_tasks[slot].name_len = 0u;
    for (i = 0u; i < TASK_NAME_MAX; ++i) {
        g_tasks[slot].name[i] = 0u;
    }
    g_tasks[slot].weight = 0u;
    g_tasks[slot].budget = 0u;
    g_tasks[slot].wait_status = 0u;
    g_tasks[slot].stop_flag = 0u;
    g_tasks[slot].wait_obj = 0u;
}

static uint8_t task_stop_flag_get(uint8_t slot)
{
    if (slot >= MAX_TASKS) {
        return 0u;
    }

    return (g_tasks[slot].stop_flag == (uint8_t)TASK_STOP_REQUESTED) ? 1u : 0u;
}

static void task_stop_flag_set(uint8_t slot)
{
    if (slot >= MAX_TASKS) {
        return;
    }

    g_tasks[slot].stop_flag = (uint8_t)TASK_STOP_REQUESTED;
}

static void rtos_panic_halt(void)
{
    while (1) {
        CPU_HALT();
    }
}

void rtos_start(void) RTOS_NAKED
{
    __asm__("jp _rtos_start_impl");
}

static uint8_t *stack_push_u16(uint8_t *sp, uint16_t value)
{
    *--sp = (uint8_t)(value >> 8);
    *--sp = (uint8_t)(value & 0x00ffu);
    return sp;
}

static uint8_t *build_initial_frame(uint8_t *stack_top, void (*entry)(void))
{
    uint8_t *sp = stack_top;

    /* Frame shape matches the order restored by rtos.s. */
    sp = stack_push_u16(sp, (uint16_t)entry); /* PC for ret/reti */
    sp = stack_push_u16(sp, 0u); /* AF  */
    sp = stack_push_u16(sp, 0u); /* BC  */
    sp = stack_push_u16(sp, 0u); /* DE  */
    sp = stack_push_u16(sp, 0u); /* HL  */
    sp = stack_push_u16(sp, 0u); /* IX  */
    sp = stack_push_u16(sp, 0u); /* IY  */
    sp = stack_push_u16(sp, 0u); /* AF' */
    sp = stack_push_u16(sp, 0u); /* BC' */
    sp = stack_push_u16(sp, 0u); /* DE' */
    sp = stack_push_u16(sp, 0u); /* HL' */

    return sp;
}

static void fill_stack(uint8_t *stack, uint16_t size)
{
    uint16_t i;

    for (i = 0u; i < size; ++i) {
        stack[i] = STACK_PATTERN;
    }
}

static uint8_t *task_stack_base(uint8_t slot)
{
    if (slot >= MAX_TASKS) {
        return (uint8_t *)0;
    }

    return &g_task_stacks[slot][0];
}

static uint16_t stack_peak_used_from_pattern(const uint8_t *stack, uint16_t size)
{
    uint16_t i = 0u;

    while ((i < size) && (stack[i] == (uint8_t)STACK_PATTERN)) {
        i++;
    }

    return (uint16_t)(size - i);
}

static uint16_t stack_current_used_from_sp(const uint8_t *stack, uint16_t size, uint16_t sp)
{
    uint16_t base = (uint16_t)stack;
    uint16_t top = (uint16_t)(base + size);

    if ((sp < base) || (sp > top)) {
        return 0u;
    }

    return (uint16_t)(top - sp);
}

static uint8_t collect_task_stack_watermark(uint8_t slot, uint16_t *out_peak_used, uint16_t *out_current_used, uint16_t *out_stack_size)
{
    const uint8_t *stack;

    if ((slot >= MAX_TASKS) || (out_peak_used == (uint16_t *)0)
        || (out_current_used == (uint16_t *)0) || (out_stack_size == (uint16_t *)0)) {
        return 0u;
    }

    if (g_tasks[slot].state == TASK_UNUSED) {
        return 0u;
    }

    stack = task_stack_base(slot);
    if (stack == (const uint8_t *)0) {
        return 0u;
    }

    *out_stack_size = (uint16_t)TASK_STACK_SIZE;
    *out_peak_used = stack_peak_used_from_pattern(stack, (uint16_t)TASK_STACK_SIZE);
    *out_current_used = stack_current_used_from_sp(stack, (uint16_t)TASK_STACK_SIZE, g_tasks[slot].sp);
    return 1u;
}

static uint8_t task_weight_sanitize(uint8_t weight)
{
    if (weight < TASK_WEIGHT_MIN) {
        return TASK_WEIGHT_MIN;
    }
    if (weight > TASK_WEIGHT_MAX) {
        return TASK_WEIGHT_MAX;
    }

    return weight;
}

static void task_weight_set_new(uint8_t slot, uint8_t weight)
{
    weight = task_weight_sanitize(weight);
    g_tasks[slot].weight = weight;
    g_tasks[slot].budget = weight;
}

static void task_weight_set_existing(uint8_t slot, uint8_t weight)
{
    weight = task_weight_sanitize(weight);
    g_tasks[slot].weight = weight;
    if ((g_tasks[slot].budget == 0u) || (g_tasks[slot].budget > weight)) {
        g_tasks[slot].budget = weight;
    }
}

static void task_set_name(uint8_t slot, const uint8_t *name)
{
    uint8_t i = 0u;

    if (name != (const uint8_t *)0) {
        while ((i < TASK_NAME_MAX) && (name[i] != 0u)) {
            g_tasks[slot].name[i] = name[i];
            i++;
        }
    }

    g_tasks[slot].name_len = i;

    while (i < TASK_NAME_MAX) {
        g_tasks[slot].name[i] = 0u;
        i++;
    }
}

static void task_mark_unused(uint8_t slot)
{
    ipc_task_on_exit(slot, g_tasks[slot].wait_obj);
    (void)zbus_tty_detach_for_task(slot);
    task_reset_runtime_fields(slot);
    rtos_heap_reset_task(slot);
}

static void task_try_attach_kernel_tty(uint8_t slot, uint8_t requested_tty)
{
    uint8_t tty_id;
    uint8_t fallback;

    (void)zbus_tty_attach_for_task_preferred(slot, requested_tty, (uint8_t)IO_DEFAULT_PORT, 1u, &tty_id, &fallback);
}

static uint8_t init_task_slot(uint8_t slot, void (*entry)(void), uint8_t weight, const uint8_t *name)
{
    uint8_t *stack = task_stack_base(slot);

    if (stack == (uint8_t *)0) {
        return 0u;
    }

    fill_stack(stack, TASK_STACK_SIZE);

    g_tasks[slot].sp = (uint16_t)build_initial_frame(stack + TASK_STACK_SIZE, entry);
    g_tasks[slot].state = TASK_READY;
    task_set_name(slot, name);
    task_weight_set_new(slot, weight);
    rtos_heap_reset_task(slot);
    g_task_busy_ticks[slot] = 0u;
    return 1u;
}

static uint8_t find_unused_task_slot(uint8_t *out_slot)
{
    uint8_t slot;

    if (out_slot == (uint8_t *)0) {
        return 0u;
    }

    for (slot = 0u; slot < MAX_TASKS; ++slot) {
        if (g_tasks[slot].state == TASK_UNUSED) {
            *out_slot = slot;
            return 1u;
        }
    }

    return 0u;
}

static uint8_t boot_task_register_spec(const task_spec_t *spec, uint8_t weight, uint8_t *out_task_id)
{
    uint8_t slot;

    if ((spec == (const task_spec_t *)0) || (spec->entry == (void (*)(void))0)) {
        return 0u;
    }

    if (find_unused_task_slot(&slot) == 0u) {
        return 0u;
    }

    if (init_task_slot(slot, spec->entry, weight, spec->name) == 0u) {
        return 0u;
    }

    task_try_attach_kernel_tty(slot, spec->requested_tty);
    if (out_task_id != (uint8_t *)0) {
        *out_task_id = slot;
    }
    return 1u;
}

static uint8_t boot_autostart_apply(uint8_t *out_first_slot)
{
#if TARGET_AUTOSTART_COUNT > 0u
    uint8_t i;
    uint8_t first_slot = (uint8_t)MAX_TASKS;
    uint8_t created_slot;

    if (out_first_slot == (uint8_t *)0) {
        return 0u;
    }
    for (i = 0u; i < (uint8_t)TARGET_AUTOSTART_COUNT; ++i) {
        const task_spec_t *spec = task_registry_find(g_target_autostart[i].name);
        if (spec == (const task_spec_t *)0) {
            if (TARGET_AUTOSTART_STRICT != 0u) {
                return 0u;
            }
            continue;
        }

        if (boot_task_register_spec(spec, g_target_autostart[i].weight, &created_slot) == 0u) {
            if (TARGET_AUTOSTART_STRICT != 0u) {
                return 0u;
            }
            continue;
        }
        if (first_slot >= MAX_TASKS) {
            first_slot = created_slot;
        }
    }
    *out_first_slot = first_slot;
#endif

#if TARGET_AUTOSTART_COUNT == 0u
    UNUSED(out_first_slot);
#endif
    return 1u;
}

uint8_t rtos_task_register_named(void (*entry)(void), uint8_t weight, const uint8_t *name, uint8_t *out_task_id)
{
    const task_spec_t spec = {
        name,
        entry,
        weight,
        (uint8_t)TASK_TTY_AUTO,
        (const uint8_t *)0,
        (task_start_args_configure_t)0,
        (task_start_args_reset_t)0
    };

    return rtos_task_register_spec(&spec, weight, out_task_id);
}

uint8_t rtos_task_register_spec(const task_spec_t *spec, uint8_t weight, uint8_t *out_task_id)
{
    uint8_t slot;

    if ((spec == (const task_spec_t *)0) || (spec->entry == (void (*)(void))0)) {
        return 0u;
    }

    CPU_DI();

    if (find_unused_task_slot(&slot) == 0u) {
        CPU_EI();
        return 0u;
    }

    if (init_task_slot(slot, spec->entry, weight, spec->name) == 0u) {
        CPU_EI();
        return 0u;
    }

    task_try_attach_kernel_tty(slot, spec->requested_tty);
    if (out_task_id != (uint8_t *)0) {
        *out_task_id = slot;
    }

    CPU_EI();
    return 1u;
}

uint8_t rtos_task_register(void (*entry)(void), uint8_t weight, uint8_t *out_task_id)
{
    return rtos_task_register_named(entry, weight, (const uint8_t *)0, out_task_id);
}

uint8_t rtos_task_unregister(uint8_t task_id)
{
    if (task_id >= MAX_TASKS) {
        return 0u;
    }

    CPU_DI();

    if (g_tasks[task_id].state == TASK_UNUSED) {
        CPU_EI();
        return 0u;
    }

    task_mark_unused(task_id);

    CPU_EI();
    return 1u;
}

uint8_t rtos_task_set_weight(uint8_t task_id, uint8_t weight)
{
    if (task_id >= MAX_TASKS) {
        return 0u;
    }

    CPU_DI();

    if (g_tasks[task_id].state == TASK_UNUSED) {
        CPU_EI();
        return 0u;
    }

    task_weight_set_existing(task_id, weight);

    CPU_EI();
    return 1u;
}

uint8_t rtos_task_request_stop(uint8_t task_id)
{
    uint16_t wait_obj;

    if (task_id >= MAX_TASKS) {
        return 0u;
    }

    CPU_DI();

    if (g_tasks[task_id].state == TASK_UNUSED) {
        CPU_EI();
        return 0u;
    }

    task_stop_flag_set(task_id);

    wait_obj = g_tasks[task_id].wait_obj;
    if ((wait_obj != 0u) && (g_tasks[task_id].state != TASK_READY)) {
        ipc_task_on_exit(task_id, wait_obj);
        g_tasks[task_id].wait_obj = 0u;
        g_tasks[task_id].wait_status = 0u;
        g_tasks[task_id].state = TASK_READY;
    }

    CPU_EI();
    return 1u;
}

void rtos_init(void)
{
    uint8_t slot;
    uint8_t spec_idx;
    uint8_t spec_count;
    uint8_t first_slot = (uint8_t)MAX_TASKS;

    /* Keep IRQs masked while boot-time task frames are being prepared. */
    CPU_DI();

    fill_stack(g_kernel_stack, KERNEL_STACK_SIZE);

    for (slot = 0u; slot < MAX_TASKS; ++slot) {
        task_reset_runtime_fields(slot);
    }

    spec_count = task_registry_count();
    for (spec_idx = 0u; spec_idx < spec_count; ++spec_idx) {
        const task_spec_t *spec = task_registry_get(spec_idx);
        task_registry_start_reset(spec);
    }

    if (boot_autostart_apply(&first_slot) == 0u) {
        rtos_panic_halt();
    }
    if ((first_slot >= MAX_TASKS) || (g_tasks[first_slot].state != TASK_READY)) {
        rtos_panic_halt();
    }

    g_kernel_sp = (uint16_t)(g_kernel_stack + KERNEL_STACK_SIZE);
    g_tick_count = 0u;
    g_current_task = first_slot;
    g_current_tcb = &g_tasks[first_slot];

    /* rtos_start() restores the first task context and control never returns here. */
}

void scheduler_select_next(void)
{
    volatile task_control_block_t *current;
    uint8_t probe;
    uint8_t remaining;

    g_tick_count++;

    current = &g_tasks[g_current_task];
    if (current->state == TASK_READY) {
        if (current->budget > 1u) {
            current->budget--;
            g_current_tcb = current;
            g_task_busy_ticks[g_current_task]++;
            return;
        }

        current->budget = current->weight;
    }

    probe = g_current_task;
    remaining = MAX_TASKS;

    while (remaining > 0u) {
        probe++;
        if (probe >= MAX_TASKS) {
            probe = 0u;
        }

        if (g_tasks[probe].state == TASK_READY) {
            if (g_tasks[probe].budget == 0u) {
                g_tasks[probe].budget = g_tasks[probe].weight;
            }
            g_current_task = probe;
            g_current_tcb = &g_tasks[probe];
            g_task_busy_ticks[probe]++;
            return;
        }

        remaining--;
    }

    g_current_tcb = &g_tasks[g_current_task];
    g_idle_ticks++;
}

void timer_init(void)
{
    /*
     * VDP interrupts are enabled by vdp_init_screen1().
     * This hook stays in place so the scheduler setup keeps the same shape.
     */
}

void timer_acknowledge(void)
{
    /* Reading VDP status clears the pending frame interrupt. */
    (void)io_read_port(0x99u);
}

void task_exit(void)
{
    CPU_DI();

    if (g_current_task < MAX_TASKS) {
        task_mark_unused(g_current_task);
    }

    CPU_EI();

    while (1) {
        CPU_HALT();
    }
}

uint8_t rtos_task_stop_requested(void)
{
    uint8_t stop_requested;

    CPU_DI();
    stop_requested = (g_current_task < MAX_TASKS) ? task_stop_flag_get(g_current_task) : 0u;
    CPU_EI();
    return stop_requested;
}

uint8_t rtos_stack_watermark(uint8_t slot, uint16_t *out_peak_used, uint16_t *out_current_used, uint16_t *out_stack_size)
{
    uint8_t ok;

    CPU_DI();
    ok = collect_task_stack_watermark(slot, out_peak_used, out_current_used, out_stack_size);
    CPU_EI();
    return ok;
}

uint8_t rtos_stack_watermark_isr(uint8_t slot, uint16_t *out_peak_used, uint16_t *out_current_used, uint16_t *out_stack_size)
{
    return collect_task_stack_watermark(slot, out_peak_used, out_current_used, out_stack_size);
}

uint8_t rtos_cpu_stats(uint8_t slot, uint16_t *out_busy_ticks, uint16_t *out_idle_ticks, uint16_t *out_total_ticks)
{
    if ((slot >= MAX_TASKS) || (out_busy_ticks == (uint16_t *)0)
        || (out_idle_ticks == (uint16_t *)0) || (out_total_ticks == (uint16_t *)0)) {
        return 0u;
    }

    CPU_DI();
    *out_busy_ticks = g_task_busy_ticks[slot];
    *out_idle_ticks = g_idle_ticks;
    *out_total_ticks = g_tick_count;
    CPU_EI();
    return 1u;
}
