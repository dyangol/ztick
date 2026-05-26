#include "rtos.h"
#include "common.h"
#include "../drivers/zbus.h"
#include "../drivers/vdp.h"

#pragma codeseg CODE

void main_boot(void)
{
    zbus_init();
    rtos_init();
    /*
     * Keep IRQs masked until rtos_start() switches to the first task context.
     * Enabling/disabling the stats stream here would unmask IRQs too early.
     */
    vdp_init_screen1();
    timer_init();
    rtos_start();

    while (1) {
        CPU_HALT();
    }
}
