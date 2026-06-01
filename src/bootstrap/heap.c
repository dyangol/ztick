#include <stdint.h>

#include "common.h"
#include "rtos.h"

#pragma codeseg CODE

typedef struct heap_block {
    struct heap_block *next;
    uint16_t size;
    uint8_t used;
} heap_block_t;

static uint8_t g_heap_areas[MAX_TASKS][TASK_HEAP_SIZE];

static uint16_t heap_align_even(uint16_t size)
{
    return (uint16_t)((size + 1u) & 0xFFFEu);
}

static uint16_t heap_align_even_down(uint16_t size)
{
    return (uint16_t)(size & 0xFFFEu);
}

static heap_block_t *heap_task_first_block(uint8_t slot)
{
    return (heap_block_t *)(g_tasks[slot].heap_first);
}

static uint8_t *heap_task_base(uint8_t slot)
{
    return (uint8_t *)(g_tasks[slot].heap_base);
}

static uint16_t heap_task_size(uint8_t slot)
{
    return g_tasks[slot].heap_size;
}

static uint8_t heap_collect_stats(uint8_t slot, heap_stats_t *out)
{
    heap_block_t *block;

    if ((slot >= MAX_TASKS) || (out == (heap_stats_t *)0)) {
        return 0u;
    }

    out->free_bytes = 0u;
    out->free_blocks = 0u;
    out->used_blocks = 0u;

    block = heap_task_first_block(slot);
    while (block != (heap_block_t *)0) {
        if (block->used == 0u) {
            out->free_blocks++;
            out->free_bytes = (uint16_t)(out->free_bytes + block->size);
        } else {
            out->used_blocks++;
        }

        block = block->next;
    }

    return 1u;
}

void rtos_heap_reset_task(uint8_t slot)
{
    heap_block_t *first;
    uint16_t usable;

    if (slot >= MAX_TASKS) {
        return;
    }

    g_tasks[slot].heap_base = (uint16_t)(&g_heap_areas[slot][0]);
    g_tasks[slot].heap_size = TASK_HEAP_SIZE;
    g_tasks[slot].heap_first = g_tasks[slot].heap_base;

    first = heap_task_first_block(slot);
    usable = (uint16_t)(TASK_HEAP_SIZE - (uint16_t)sizeof(heap_block_t));
    first->next = (heap_block_t *)0;
    first->size = heap_align_even_down(usable);
    first->used = 0u;
}

void *rtos_malloc(uint16_t size)
{
    uint8_t slot;
    heap_block_t *block;
    void *result = (void *)0;

    if (size == 0u) {
        return (void *)0;
    }

    size = heap_align_even(size);

    CPU_DI();

    slot = g_current_task;
    if (slot >= MAX_TASKS) {
        CPU_EI();
        return (void *)0;
    }

    block = heap_task_first_block(slot);
    while (block != (heap_block_t *)0) {
        if ((block->used == 0u) && (block->size >= size)) {
            uint16_t remainder = (uint16_t)(block->size - size);

            if (remainder > (uint16_t)(sizeof(heap_block_t) + 2u)) {
                uint8_t *split_addr = (uint8_t *)(block + 1) + size;
                heap_block_t *next = (heap_block_t *)split_addr;

                next->next = block->next;
                next->size = (uint16_t)(remainder - (uint16_t)sizeof(heap_block_t));
                next->used = 0u;

                block->next = next;
                block->size = size;
            }

            block->used = 1u;
            result = (void *)(block + 1);
            break;
        }

        block = block->next;
    }

    CPU_EI();
    return result;
}

uint8_t rtos_free(void *ptr)
{
    uint8_t slot;
    uint8_t *base;
    uint8_t *end;
    heap_block_t *block;
    heap_block_t *prev = (heap_block_t *)0;

    if (ptr == (void *)0) {
        return 0u;
    }

    CPU_DI();

    slot = g_current_task;
    if (slot >= MAX_TASKS) {
        CPU_EI();
        return 0u;
    }

    base = heap_task_base(slot);
    end = base + heap_task_size(slot);

    if (((uint8_t *)ptr < base) || ((uint8_t *)ptr >= end)) {
        CPU_EI();
        return 0u;
    }

    block = heap_task_first_block(slot);
    while (block != (heap_block_t *)0) {
        if ((void *)(block + 1) == ptr) {
            heap_block_t *next;
            uint8_t *block_end;

            if (block->used == 0u) {
                CPU_EI();
                return 0u;
            }

            block->used = 0u;

            next = block->next;
            while (next != (heap_block_t *)0) {
                block_end = (uint8_t *)(block + 1) + block->size;
                if ((next->used != 0u) || ((uint8_t *)next != block_end)) {
                    break;
                }

                block->size = (uint16_t)(block->size + (uint16_t)sizeof(heap_block_t) + next->size);
                block->next = next->next;
                next = block->next;
            }

            if (prev != (heap_block_t *)0) {
                uint8_t *prev_end = (uint8_t *)(prev + 1) + prev->size;
                if ((prev->used == 0u) && (prev_end == (uint8_t *)block)) {
                    prev->size = (uint16_t)(prev->size + (uint16_t)sizeof(heap_block_t) + block->size);
                    prev->next = block->next;
                }
            }

            CPU_EI();
            return 1u;
        }

        prev = block;
        block = block->next;
    }

    CPU_EI();
    return 0u;
}

uint8_t rtos_heap_stats(uint8_t slot, heap_stats_t *out)
{
    uint8_t ok;

    CPU_DI();
    ok = heap_collect_stats(slot, out);
    CPU_EI();
    return ok;
}

uint8_t rtos_heap_stats_isr(uint8_t slot, heap_stats_t *out)
{
    return heap_collect_stats(slot, out);
}
