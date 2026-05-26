#ifndef COMPILER_H
#define COMPILER_H

#define UNUSED(x) ((void)(x))
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

#define COUNTER_INC16_SAT(counter_ptr)             \
    do {                                           \
        if (*(counter_ptr) < 0xFFFFu) {            \
            *(counter_ptr) = *(counter_ptr) + 1u;  \
        }                                          \
    } while (0)

#define STORE_U16_LE(buf, index, value)                      \
    do {                                                     \
        (buf)[(index)] = (unsigned char)((value) & 0x00FFu);      \
        (buf)[(unsigned char)((index) + 1u)] = (unsigned char)((value) >> 8); \
    } while (0)

#endif
