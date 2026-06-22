#include <stdint.h>

#define RCC_BASE        0x40021000u
#define GPIOA_BASE      0x40010800u
#define USART2_BASE     0x40004400u

#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18u))
#define RCC_APB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x1Cu))
#define GPIOA_CRL       (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_ODR       (*(volatile uint32_t *)(GPIOA_BASE + 0x0Cu))
#define USART2_SR       (*(volatile uint32_t *)(USART2_BASE + 0x00u))
#define USART2_DR       (*(volatile uint32_t *)(USART2_BASE + 0x04u))
#define USART2_BRR      (*(volatile uint32_t *)(USART2_BASE + 0x08u))
#define USART2_CR1      (*(volatile uint32_t *)(USART2_BASE + 0x0Cu))
#define USART2_CR2      (*(volatile uint32_t *)(USART2_BASE + 0x10u))
#define USART2_CR3      (*(volatile uint32_t *)(USART2_BASE + 0x14u))

#define RCC_APB2ENR_IOPAEN (1u << 2u)
#define RCC_APB1ENR_USART2EN (1u << 17u)

#define LED_PIN         5u
#define LED_PIN_SHIFT   (LED_PIN * 4u)
#define LED_MODE_MASK   (0xFu << LED_PIN_SHIFT)
#define LED_MODE_OUT    (0x2u << LED_PIN_SHIFT)

#define USART2_TX_PIN        2u
#define USART2_TX_PIN_SHIFT  (USART2_TX_PIN * 4u)
#define USART2_TX_MODE_MASK   (0xFu << USART2_TX_PIN_SHIFT)
#define USART2_TX_MODE_AFPP   (0xBu << USART2_TX_PIN_SHIFT)

#define USART2_SR_TXE     (1u << 7u)
#define USART2_CR1_UE     (1u << 13u)
#define USART2_CR1_TE     (1u << 3u)

#define USART2_PCLK_HZ    8000000u

static void delay(volatile uint32_t cycles)
{
    while (cycles-- > 0u) {
        __asm__ volatile ("nop");
    }
}

static void led_init(void)
{
    uint32_t crl;

    RCC_APB2ENR |= RCC_APB2ENR_IOPAEN;

    crl = GPIOA_CRL;
    crl &= ~LED_MODE_MASK;
    crl |= LED_MODE_OUT;
    GPIOA_CRL = crl;
}

static void uart2_init(void)
{
    uint32_t crl;

    RCC_APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC_APB1ENR |= RCC_APB1ENR_USART2EN;

    crl = GPIOA_CRL;
    crl &= ~USART2_TX_MODE_MASK;
    crl |= USART2_TX_MODE_AFPP;
    GPIOA_CRL = crl;

    USART2_CR1 = 0u;
    USART2_CR2 = 0u;
    USART2_CR3 = 0u;
    USART2_BRR = (USART2_PCLK_HZ + (115200u / 2u)) / 115200u;
    USART2_CR1 = USART2_CR1_UE | USART2_CR1_TE;
}

static void led_off(void)
{
    GPIOA_ODR &= ~(1u << LED_PIN);
}

static void led_toggle(void)
{
    GPIOA_ODR ^= (1u << LED_PIN);
}

static void uart2_write_char(char ch)
{
    while ((USART2_SR & USART2_SR_TXE) == 0u) {
    }

    USART2_DR = (uint32_t)(unsigned char)ch;
}

static void uart2_write_string(const char *message)
{
    while (*message != '\0') {
        uart2_write_char(*message++);
    }
}

int main(void)
{
    led_init();
    uart2_init();
    led_off();

    for (;;) {
        uart2_write_string("hello world\r\n");
        led_toggle();
        delay(5000000u);
    }

    return 0;
}
