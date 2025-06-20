#include "stm32f10x.h"

int main() {
    RCC->APB2ENR |= 0xFC;      // 啟用 GPIO 連接埠的 clock
    RCC->APB1ENR |= (1 << 0);  // 啟用 TIM2 clock
    GPIOA->CRL |= 0xB;         // PA0 為 50MHz output
    TIM2->CCER = 0x1;          // CC1P = 0, CC1E = 1
    TIM2->CCMR1 |= 0x60;       // 設定為 PWM 1
    TIM2->PSC = 72 - 1;        // TIM2 時脈頻率為 72MHz
    TIM2->ARR = 20000 - 1;     // 週期為 20ms
    TIM2->CCR1 = 5000;         // Duty Cycle = 5000 / 20000 = 25%
    TIM2->CR1 = 1;             // 正向計數

    while (1) { }
}
