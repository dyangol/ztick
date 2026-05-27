#ifndef CPU_H
#define CPU_H

#define CPU_DI() __asm__("di")
#define CPU_EI() __asm__("ei")
#define CPU_NOP() __asm__("nop")
#define CPU_HALT() __asm__("halt")

#endif
