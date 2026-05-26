#ifndef IPC_DEMO_H
#define IPC_DEMO_H

#include <stdint.h>

void ipc_demo_init_once(void);
uint8_t ipc_demo_send_u16(uint16_t value);
uint8_t ipc_demo_recv_u16(uint16_t *out_value);
uint8_t ipc_demo_queue_used(void);
uint8_t ipc_demo_queue_capacity(void);

#endif
