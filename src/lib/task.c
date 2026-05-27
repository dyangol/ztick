#include <stdint.h>

#include "../common/common.h"
#include "../bootstrap/rtos.h"
#include "task.h"

#pragma codeseg CODE

extern void main_b(void);
extern void main_c(void);

static const uint8_t g_task_name_b[] = "b";
static const uint8_t g_task_name_c[] = "c";

static const task_spec_t g_task_specs[] = {
    {g_task_name_b, main_b, TASK_WEIGHT_MIN},
    {g_task_name_c, main_c, TASK_WEIGHT_MIN}
};

static uint8_t task_registry_size(void)
{
    return (uint8_t)ARRAY_LEN(g_task_specs);
}

static uint8_t task_name_equals(const uint8_t *name, const uint8_t *lit)
{
    uint8_t i = 0u;

    if (name == (const uint8_t *)0) {
        return 0u;
    }

    while (lit[i] != 0u) {
        if (name[i] != lit[i]) {
            return 0u;
        }
        i++;
    }

    return (name[i] == 0u) ? 1u : 0u;
}

const task_spec_t *task_registry_find(const uint8_t *name)
{
    uint8_t i;
    uint8_t count = task_registry_size();

    for (i = 0u; i < count; ++i) {
        if (task_name_equals(name, g_task_specs[i].name) != 0u) {
            return &g_task_specs[i];
        }
    }

    return (const task_spec_t *)0;
}

const task_spec_t *task_registry_get(uint8_t index)
{
    if (index >= task_registry_size()) {
        return (const task_spec_t *)0;
    }

    return &g_task_specs[index];
}

uint8_t task_registry_count(void)
{
    return task_registry_size();
}
