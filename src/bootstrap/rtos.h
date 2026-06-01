#ifndef BOOTSTRAP_RTOS_H
#define BOOTSTRAP_RTOS_H

#include <stdint.h>

#define MAX_TASKS 6u
#define TASK_NAME_MAX 8u

#define TASK_UNUSED 0u
#define TASK_READY  1u
#define TASK_WAIT_SEM 2u
#define TASK_WAIT_Q_SEND 3u
#define TASK_WAIT_Q_RECV 4u
#define TASK_STOP_REQUESTED 1u
#define TASK_HEAP_SIZE 384u
#define TASK_WEIGHT_MIN 1u
#define TASK_WEIGHT_MAX 3u

#ifdef __SDCC
#define RTOS_NAKED __naked
#else
#define RTOS_NAKED
#endif

typedef struct task_control_block {
    uint16_t sp;
    uint8_t state;
    uint8_t name_len;
    uint8_t name[TASK_NAME_MAX];
    uint8_t weight;
    uint8_t budget;
    uint8_t wait_status;
    uint8_t stop_flag;
    uint16_t wait_obj;
    uint16_t heap_base;
    uint16_t heap_size;
    uint16_t heap_first;
} task_control_block_t;

typedef struct heap_stats {
    uint16_t free_bytes;
    uint8_t free_blocks;
    uint8_t used_blocks;
} heap_stats_t;

extern volatile task_control_block_t g_tasks[MAX_TASKS];
extern volatile task_control_block_t *g_current_tcb;
extern volatile uint8_t g_current_task;
extern volatile uint16_t g_kernel_sp;
extern volatile uint16_t g_tick_count;
extern volatile uint8_t g_boot_marker;
extern volatile uint8_t g_boot_slot_value;

void rtos_init(void);
void rtos_start(void) RTOS_NAKED;
void scheduler_select_next(void);
void timer_init(void);
void timer_acknowledge(void);
uint8_t rtos_task_register(void (*entry)(void), uint8_t weight, uint8_t *out_task_id);
uint8_t rtos_task_register_named(void (*entry)(void), uint8_t weight, const uint8_t *name, uint8_t *out_task_id);
uint8_t rtos_task_unregister(uint8_t task_id);
uint8_t rtos_task_request_stop(uint8_t task_id);
/* Weight is clamped to TASK_WEIGHT_MIN..TASK_WEIGHT_MAX. */
uint8_t rtos_task_set_weight(uint8_t task_id, uint8_t weight);
uint8_t rtos_task_stop_requested(void);
void task_exit(void);
void *rtos_malloc(uint16_t size);
uint8_t rtos_free(void *ptr);
void rtos_heap_reset_task(uint8_t slot);
uint8_t rtos_heap_stats(uint8_t slot, heap_stats_t *out);
uint8_t rtos_heap_stats_isr(uint8_t slot, heap_stats_t *out);
uint8_t rtos_stack_watermark(uint8_t slot, uint16_t *out_peak_used, uint16_t *out_current_used, uint16_t *out_stack_size);
uint8_t rtos_stack_watermark_isr(uint8_t slot, uint16_t *out_peak_used, uint16_t *out_current_used, uint16_t *out_stack_size);

#endif
