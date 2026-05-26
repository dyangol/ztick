#ifndef TASK_H
#define TASK_H

#include <stdint.h>

typedef void (*task_entry_t)(void);

typedef struct task_spec {
    const uint8_t *name;
    task_entry_t entry;
    uint8_t default_weight;
} task_spec_t;

const task_spec_t *task_registry_find(const uint8_t *name);
const task_spec_t *task_registry_get(uint8_t index);
uint8_t task_registry_count(void);

#endif
