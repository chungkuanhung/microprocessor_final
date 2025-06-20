#include "stm32f10x.h"

int main() {
    RCC->APB2ENR |= 0xFC;  // 啟用 GPIO 連接埠的 clock

    GPIOB->CRL = 0x33333333;  // PB0 to PB7 outputs
    GPIOB->CRH = 0x33333333;  // PB8 to PB15 outputs

    while (1) {
        GPIOB->ODR = 0x3F5B;
    }
}
