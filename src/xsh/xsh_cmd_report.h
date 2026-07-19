#ifndef XSH_CMD_REPORT_H
#define XSH_CMD_REPORT_H

#include <stdint.h>

#include "xsh.h"

extern const uint8_t g_xsh_cmd_name_tasks[];
extern const uint8_t g_xsh_cmd_usage_tasks[];
extern const uint8_t g_xsh_cmd_name_heap[];
extern const uint8_t g_xsh_cmd_usage_heap[];
extern const uint8_t g_xsh_cmd_name_stack[];
extern const uint8_t g_xsh_cmd_usage_stack[];
extern const uint8_t g_xsh_cmd_name_stats[];
extern const uint8_t g_xsh_cmd_name_cpu[];
extern const uint8_t g_xsh_cmd_usage_cpu[];

uint8_t cmd_tasks(xsh_t *sh, uint8_t argc, uint8_t *argv[]);
uint8_t cmd_heap(xsh_t *sh, uint8_t argc, uint8_t *argv[]);
uint8_t cmd_stack(xsh_t *sh, uint8_t argc, uint8_t *argv[]);
uint8_t cmd_stats(xsh_t *sh, uint8_t argc, uint8_t *argv[]);
uint8_t cmd_cpu(xsh_t *sh, uint8_t argc, uint8_t *argv[]);

#endif
