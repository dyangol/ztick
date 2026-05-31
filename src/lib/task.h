#ifndef TASK_H
#define TASK_H

#include <stdint.h>

typedef void (*task_entry_t)(void);
typedef uint8_t (*task_start_args_configure_t)(uint8_t argc, uint8_t *argv[]);
typedef void (*task_start_args_reset_t)(void);

typedef struct task_spec {
    const uint8_t *name;
    task_entry_t entry;
    uint8_t default_weight;
    const uint8_t *start_args_usage;
    task_start_args_configure_t start_args_configure;
    task_start_args_reset_t start_args_reset;
} task_spec_t;

const task_spec_t *task_registry_find(const uint8_t *name);
const task_spec_t *task_registry_get(uint8_t index);
uint8_t task_registry_count(void);
uint8_t task_registry_start_configure(const task_spec_t *spec, uint8_t argc, uint8_t *argv[]);
void task_registry_start_reset(const task_spec_t *spec);

#endif
