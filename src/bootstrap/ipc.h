#ifndef IPC_H
#define IPC_H

#include <stdint.h>

typedef struct ipc_semaphore {
    int8_t count;
    uint16_t wait_mask;
} ipc_semaphore_t;

typedef struct ipc_queue {
    uint8_t *storage;
    uint8_t item_size;
    uint8_t capacity;
    uint8_t head;
    uint8_t tail;
    uint8_t used;
    ipc_semaphore_t sem_items;
    ipc_semaphore_t sem_slots;
} ipc_queue_t;

void ipc_sem_init(ipc_semaphore_t *sem, int8_t initial_count);
uint8_t ipc_sem_try_wait(ipc_semaphore_t *sem);
uint8_t ipc_sem_wait(ipc_semaphore_t *sem);
void ipc_sem_signal(ipc_semaphore_t *sem);

void ipc_queue_init(ipc_queue_t *queue, uint8_t *storage, uint8_t item_size, uint8_t capacity);
void ipc_queue_init_isr(ipc_queue_t *queue, uint8_t *storage, uint8_t item_size, uint8_t capacity);
uint8_t ipc_queue_send(ipc_queue_t *queue, const uint8_t *item);
uint8_t ipc_queue_recv(ipc_queue_t *queue, uint8_t *out_item);
uint8_t ipc_queue_try_send(ipc_queue_t *queue, const uint8_t *item);
uint8_t ipc_queue_try_recv(ipc_queue_t *queue, uint8_t *out_item);
uint8_t ipc_queue_used(const ipc_queue_t *queue);

/*
 * Called by RTOS task lifecycle while IRQs are masked.
 * Removes task from pending semaphore wait mask if needed.
 */
void ipc_task_on_exit(uint8_t task_id, uint16_t wait_obj);

#endif
