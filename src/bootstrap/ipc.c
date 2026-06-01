#include <stdint.h>

#include "common.h"
#include "ipc.h"
#include "rtos.h"

#pragma codeseg CODE

#define IPC_TASK_WAIT_OK 1u

static uint16_t ipc_task_bit(uint8_t task_id)
{
    return (uint16_t)(1u << task_id);
}

static uint8_t ipc_find_waiter(uint16_t mask)
{
    uint8_t task_id;

    for (task_id = 0u; task_id < MAX_TASKS; ++task_id) {
        if ((mask & ipc_task_bit(task_id)) != 0u) {
            return task_id;
        }
    }

    return (uint8_t)MAX_TASKS;
}

static uint8_t ipc_sem_wait_internal(ipc_semaphore_t *sem, uint8_t wait_state)
{
    uint8_t slot;

    if ((sem == (ipc_semaphore_t *)0) || (wait_state == TASK_UNUSED) || (wait_state == TASK_READY)) {
        return 0u;
    }

    CPU_DI();

    if (sem->count > 0) {
        sem->count--;
        CPU_EI();
        return 1u;
    }

    slot = g_current_task;
    if (slot >= MAX_TASKS) {
        CPU_EI();
        return 0u;
    }
    if (g_tasks[slot].state != TASK_READY) {
        CPU_EI();
        return 0u;
    }

    sem->wait_mask = (uint16_t)(sem->wait_mask | ipc_task_bit(slot));
    g_tasks[slot].wait_obj = (uint16_t)sem;
    g_tasks[slot].wait_status = 0u;
    g_tasks[slot].state = wait_state;
    CPU_EI();

    while (g_tasks[slot].state != TASK_READY) {
        CPU_EI();
        CPU_HALT();
    }

    return (g_tasks[slot].wait_status == IPC_TASK_WAIT_OK) ? 1u : 0u;
}

static uint8_t ipc_queue_is_valid(const ipc_queue_t *queue)
{
    if ((queue == (const ipc_queue_t *)0) || (queue->storage == (uint8_t *)0)
        || (queue->item_size == 0u) || (queue->capacity == 0u)) {
        return 0u;
    }

    return 1u;
}

static uint16_t ipc_queue_item_offset(const ipc_queue_t *queue, uint8_t index)
{
    return (uint16_t)((uint16_t)index * (uint16_t)queue->item_size);
}

static void ipc_queue_copy_to_storage(ipc_queue_t *queue, uint8_t index, const uint8_t *item)
{
    uint16_t offset = ipc_queue_item_offset(queue, index);
    uint8_t i;

    for (i = 0u; i < queue->item_size; ++i) {
        queue->storage[(uint16_t)(offset + i)] = item[i];
    }
}

static void ipc_queue_copy_from_storage(const ipc_queue_t *queue, uint8_t index, uint8_t *out_item)
{
    uint16_t offset = ipc_queue_item_offset(queue, index);
    uint8_t i;

    for (i = 0u; i < queue->item_size; ++i) {
        out_item[i] = queue->storage[(uint16_t)(offset + i)];
    }
}

static uint8_t ipc_queue_push_locked(ipc_queue_t *queue, const uint8_t *item)
{
    if (queue->used >= queue->capacity) {
        return 0u;
    }

    ipc_queue_copy_to_storage(queue, queue->head, item);
    queue->head++;
    if (queue->head >= queue->capacity) {
        queue->head = 0u;
    }
    queue->used++;
    return 1u;
}

static uint8_t ipc_queue_pop_locked(ipc_queue_t *queue, uint8_t *out_item)
{
    if (queue->used == 0u) {
        return 0u;
    }

    ipc_queue_copy_from_storage(queue, queue->tail, out_item);
    queue->tail++;
    if (queue->tail >= queue->capacity) {
        queue->tail = 0u;
    }
    queue->used--;
    return 1u;
}

static void ipc_queue_init_raw(ipc_queue_t *queue, uint8_t *storage, uint8_t item_size, uint8_t capacity)
{
    queue->storage = storage;
    queue->item_size = item_size;
    queue->capacity = capacity;
    queue->head = 0u;
    queue->tail = 0u;
    queue->used = 0u;
    queue->sem_items.count = 0;
    queue->sem_items.wait_mask = 0u;
    queue->sem_slots.count = (int8_t)capacity;
    queue->sem_slots.wait_mask = 0u;
}

void ipc_sem_init(ipc_semaphore_t *sem, int8_t initial_count)
{
    if (sem == (ipc_semaphore_t *)0) {
        return;
    }

    CPU_DI();
    sem->count = initial_count;
    sem->wait_mask = 0u;
    CPU_EI();
}

uint8_t ipc_sem_try_wait(ipc_semaphore_t *sem)
{
    if (sem == (ipc_semaphore_t *)0) {
        return 0u;
    }

    CPU_DI();
    if (sem->count > 0) {
        sem->count--;
        CPU_EI();
        return 1u;
    }
    CPU_EI();
    return 0u;
}

uint8_t ipc_sem_wait(ipc_semaphore_t *sem)
{
    return ipc_sem_wait_internal(sem, TASK_WAIT_SEM);
}

void ipc_sem_signal(ipc_semaphore_t *sem)
{
    uint8_t waiter;

    if (sem == (ipc_semaphore_t *)0) {
        return;
    }

    CPU_DI();

    waiter = ipc_find_waiter(sem->wait_mask);
    if (waiter < MAX_TASKS) {
        sem->wait_mask = (uint16_t)(sem->wait_mask & (uint16_t)(~ipc_task_bit(waiter)));
        if (g_tasks[waiter].state != TASK_UNUSED) {
            g_tasks[waiter].wait_obj = 0u;
            g_tasks[waiter].wait_status = IPC_TASK_WAIT_OK;
            g_tasks[waiter].state = TASK_READY;
        }
        CPU_EI();
        return;
    }

    if (sem->count < 127) {
        sem->count++;
    }
    CPU_EI();
}

void ipc_queue_init(ipc_queue_t *queue, uint8_t *storage, uint8_t item_size, uint8_t capacity)
{
    if ((queue == (ipc_queue_t *)0) || (storage == (uint8_t *)0) || (item_size == 0u) || (capacity == 0u)) {
        return;
    }

    CPU_DI();
    ipc_queue_init_raw(queue, storage, item_size, capacity);
    CPU_EI();
}

void ipc_queue_init_isr(ipc_queue_t *queue, uint8_t *storage, uint8_t item_size, uint8_t capacity)
{
    if ((queue == (ipc_queue_t *)0) || (storage == (uint8_t *)0) || (item_size == 0u) || (capacity == 0u)) {
        return;
    }

    ipc_queue_init_raw(queue, storage, item_size, capacity);
}

uint8_t ipc_queue_send(ipc_queue_t *queue, const uint8_t *item)
{
    if ((item == (const uint8_t *)0) || (ipc_queue_is_valid(queue) == 0u)) {
        return 0u;
    }

    if (ipc_sem_wait_internal(&queue->sem_slots, TASK_WAIT_Q_SEND) == 0u) {
        return 0u;
    }

    CPU_DI();
    if (ipc_queue_push_locked(queue, item) == 0u) {
        CPU_EI();
        ipc_sem_signal(&queue->sem_slots);
        return 0u;
    }
    CPU_EI();

    ipc_sem_signal(&queue->sem_items);
    return 1u;
}

uint8_t ipc_queue_recv(ipc_queue_t *queue, uint8_t *out_item)
{
    if ((out_item == (uint8_t *)0) || (ipc_queue_is_valid(queue) == 0u)) {
        return 0u;
    }

    if (ipc_sem_wait_internal(&queue->sem_items, TASK_WAIT_Q_RECV) == 0u) {
        return 0u;
    }

    CPU_DI();
    if (ipc_queue_pop_locked(queue, out_item) == 0u) {
        CPU_EI();
        ipc_sem_signal(&queue->sem_items);
        return 0u;
    }
    CPU_EI();

    ipc_sem_signal(&queue->sem_slots);
    return 1u;
}

uint8_t ipc_queue_try_send(ipc_queue_t *queue, const uint8_t *item)
{
    if ((item == (const uint8_t *)0) || (ipc_queue_is_valid(queue) == 0u)) {
        return 0u;
    }

    if (ipc_sem_try_wait(&queue->sem_slots) == 0u) {
        return 0u;
    }

    CPU_DI();
    if (ipc_queue_push_locked(queue, item) == 0u) {
        CPU_EI();
        ipc_sem_signal(&queue->sem_slots);
        return 0u;
    }
    CPU_EI();

    ipc_sem_signal(&queue->sem_items);
    return 1u;
}

uint8_t ipc_queue_try_recv(ipc_queue_t *queue, uint8_t *out_item)
{
    if ((out_item == (uint8_t *)0) || (ipc_queue_is_valid(queue) == 0u)) {
        return 0u;
    }

    if (ipc_sem_try_wait(&queue->sem_items) == 0u) {
        return 0u;
    }

    CPU_DI();
    if (ipc_queue_pop_locked(queue, out_item) == 0u) {
        CPU_EI();
        ipc_sem_signal(&queue->sem_items);
        return 0u;
    }
    CPU_EI();

    ipc_sem_signal(&queue->sem_slots);
    return 1u;
}

uint8_t ipc_queue_used(const ipc_queue_t *queue)
{
    if (queue == (const ipc_queue_t *)0) {
        return 0u;
    }

    CPU_DI();
    {
        uint8_t used = queue->used;
        CPU_EI();
        return used;
    }
}

void ipc_task_on_exit(uint8_t task_id, uint16_t wait_obj)
{
    ipc_semaphore_t *sem;

    if ((task_id >= MAX_TASKS) || (wait_obj == 0u)) {
        return;
    }

    sem = (ipc_semaphore_t *)wait_obj;
    sem->wait_mask = (uint16_t)(sem->wait_mask & (uint16_t)(~ipc_task_bit(task_id)));
}
