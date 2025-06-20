#include "stm32f10x.h"

void delay_ms(uint16_t t);
void usart1_sendByte(unsigned char c);

int main() {
    RCC->APB2ENR |= (1 << 14) | (1 << 2);  // 啟用 GPIOA 和 USART1 clock
    GPIOA->CRH |= 0x000000B0;  // PA9 為 50MHz output
    USART1->CR1 = 0x200C;  // UE, TE, RE 為 1
    USART1->BRR = 7500;

    usart1_sendByte('A');
}

void delay_ms(uint16_t t) {
    volatile unsigned long l = 0;

    for (uint16_t i = 0; i < t; i++)
        for (l = 0; l < 9000; l++)
            ;
}

void usart1_sendByte(unsigned char c) {
    USART1->DR = c;  // 字元放進 Data register
    while ((USART1->SR & (1 << 6)) == 0)  // 檢查 SR 和 TC flag
        ;
}
