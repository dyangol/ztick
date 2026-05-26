#include <stdint.h>

#include "../bootstrap/ipc.h"
#include "../common/common.h"
#include "ipc_demo.h"

#pragma codeseg CODE

#define IPC_DEMO_QUEUE_CAPACITY 8u
#define IPC_DEMO_MSG_SIZE 2u
#define IPC_DEMO_INIT_IDLE 0u
#define IPC_DEMO_INIT_BUSY 0xFFu
#define IPC_DEMO_INIT_DONE 1u

static ipc_queue_t g_ipc_demo_queue;
static uint8_t g_ipc_demo_storage[(uint8_t)(IPC_DEMO_QUEUE_CAPACITY * IPC_DEMO_MSG_SIZE)];
static volatile uint8_t g_ipc_demo_initialized;

void ipc_demo_init_once(void)
{
    while (1) {
        CPU_DI();
        if (g_ipc_demo_initialized == IPC_DEMO_INIT_DONE) {
            CPU_EI();
            return;
        }
        if ((g_ipc_demo_initialized != IPC_DEMO_INIT_IDLE) && (g_ipc_demo_initialized != IPC_DEMO_INIT_BUSY)) {
            g_ipc_demo_initialized = IPC_DEMO_INIT_IDLE;
        }
        if (g_ipc_demo_initialized == IPC_DEMO_INIT_IDLE) {
            g_ipc_demo_initialized = IPC_DEMO_INIT_BUSY;
            ipc_queue_init_isr(&g_ipc_demo_queue, g_ipc_demo_storage, (uint8_t)IPC_DEMO_MSG_SIZE, (uint8_t)IPC_DEMO_QUEUE_CAPACITY);
            g_ipc_demo_initialized = IPC_DEMO_INIT_DONE;
            CPU_EI();
            return;
        }
        CPU_EI();
        CPU_NOP();
        CPU_NOP();
    }
}

uint8_t ipc_demo_send_u16(uint16_t value)
{
    uint8_t msg[IPC_DEMO_MSG_SIZE];

    ipc_demo_init_once();
    msg[0] = (uint8_t)(value & 0x00FFu);
    msg[1] = (uint8_t)((value >> 8) & 0x00FFu);
    return ipc_queue_try_send(&g_ipc_demo_queue, msg);
}

uint8_t ipc_demo_recv_u16(uint16_t *out_value)
{
    uint8_t msg[IPC_DEMO_MSG_SIZE];

    if (out_value == (uint16_t *)0) {
        return 0u;
    }

    ipc_demo_init_once();
    if (ipc_queue_recv(&g_ipc_demo_queue, msg) == 0u) {
        return 0u;
    }

    *out_value = (uint16_t)((uint16_t)msg[0] | ((uint16_t)msg[1] << 8));
    return 1u;
}

uint8_t ipc_demo_queue_used(void)
{
    ipc_demo_init_once();
    return ipc_queue_used(&g_ipc_demo_queue);
}

uint8_t ipc_demo_queue_capacity(void)
{
    return (uint8_t)IPC_DEMO_QUEUE_CAPACITY;
}
