#include "stm32f10x.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// --- 全域變數 ---
// 超音波感測器
static volatile uint32_t in_sensor_echo_begin_us = 0;   // 入口 Echo 上升時間
static volatile uint32_t out_sensor_echo_begin_us = 0;  // 出口 Echo 上升時間
volatile uint32_t in_sensor_echo_duration_us = 0;       // 入口 Echo 持續時間
volatile uint32_t out_sensor_echo_duration_us = 0;      // 出口 Echo 持續時間
volatile bool in_sensor_measurement = false;            // 入口是否偵測到車輛
volatile bool out_sensor_measurement = false;           // 出口是否偵測到車輛

// 計時與計數
volatile uint32_t us_counter = 0;            // SysTick 中斷累加的計時器 (us)
volatile int available_parking_spaces = 20;  // 剩餘停車位

// USART
char usart_tx_buffer[128];            // 發送資料的緩衝區
volatile uint16_t usart_tx_head = 0;  // 寫入指標
volatile uint16_t usart_tx_tail = 0;  // 讀取指標

// 宣告函式
void setup(void);                      // 初始設定
void delay_us(uint32_t us);            // 延遲 us
void delay_ms(uint32_t ms);            // 延遲 ms
void usart1_send_str(char *str);       // 透過 USART1 發送字串
void update_seven_segment(int count);  // 更新七段顯示器顯示

int main(void) {
	setup();

	uint32_t last_usart_send_us = 0;
	uint32_t last_in_sensor_trig_us = 0;
	uint32_t last_out_sensor_trig_us = 0;
	bool out_sensor_trig_active = false;  // 用於錯開兩個感測器的觸發時間

	update_seven_segment(available_parking_spaces);

	while (1) {
		float distance_m;
		char buffer[30];

		// --- 週期性 Trig ---
		// 每 5 秒觸發一次入口感測器
		if (us_counter - last_in_sensor_trig_us >= 5 * 1000 * 1000) {

			last_in_sensor_trig_us = us_counter;
			GPIOC->BSRR = (1 << 13);
			delay_us(10);
			GPIOC->BRR = (1 << 13);
		}

		// 延遲 2.5 秒後才開始觸發出口感測器
		if (!out_sensor_trig_active && us_counter >= 5 * 1000 * 1000 / 2) {
			out_sensor_trig_active = true;
			last_out_sensor_trig_us = us_counter;
		}

		// 如果出口觸發已啟動，則每 5 秒觸發一次出口感測器
		if (out_sensor_trig_active
				&& (us_counter - last_out_sensor_trig_us >= 5 * 1000 * 1000)) {
			last_out_sensor_trig_us = us_counter;
			GPIOC->BSRR = (1 << 14);
			delay_us(10);
			GPIOC->BRR = (1 << 14);
		}

		// --- 感測器偵測 ---
		if (in_sensor_measurement) {
			in_sensor_measurement = false;

			distance_m = (in_sensor_echo_duration_us * 343.0 / 2.0) / 1000000.0;
			if (distance_m < 1.0 && available_parking_spaces > 0) {
				available_parking_spaces--;
				TIM1->CCR1 = 1500;  // 開啟入口閘門
				delay_ms(450);  // 閘門開啟持續時間 (ms)，大於 500 會出現錯誤
				TIM1->CCR1 = 2500;  // 關閉入口閘門
			}
		}

		if (out_sensor_measurement) {
			out_sensor_measurement = false;

			distance_m = (out_sensor_echo_duration_us * 343.0 / 2.0) / 1000000.0;

			if (distance_m < 1.0 && available_parking_spaces < 20) {
				available_parking_spaces++;
				TIM2->CCR1 = 1500;  // 開啟出口閘門
				delay_ms(450);  // 閘門開啟持續時間 (ms)，大於 500 會出現錯誤
				TIM2->CCR1 = 500;  // 關閉出口閘門
			}
		}

		if (available_parking_spaces == 0) {
			if ((us_counter / 500000) % 2) {
				GPIOB->ODR = 0x0000;
			} else {
				update_seven_segment(0);
			}
		} else {
			update_seven_segment(available_parking_spaces);
		}

		if (us_counter - last_usart_send_us >= 500000) {
			last_usart_send_us = us_counter;
			sprintf(buffer, "Parking Cars: %02d\r\n",
					20 - available_parking_spaces);
			usart1_send_str(buffer);
		}
	}
}

void setup(void) {
	// 啟用時脈
	RCC->APB2ENR |= (1 << 0) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 11)
			| (1 << 14);
	RCC->APB1ENR |= (1 << 0);

	// GPIO 腳位功能
	// PA0, PA1, PA2
	GPIOA->CRL = (GPIOA->CRL & ~0x00000FFF) | (0x8 << 8) | (0x8 << 4)
			| (0xB << 0);
	GPIOA->ODR &= ~((1 << 1) | (1 << 2));
	// PA8, PA9, PA10
	GPIOA->CRH = (GPIOA->CRH & ~0x00000FFF) | (0x4 << 8) | (0xB << 4)
			| (0xB << 0);
	GPIOB->CRL = 0x33333333;  // PB0 to PB7
	GPIOB->CRH = 0x33333333;  // PB8 to PB15
	GPIOC->CRH = (GPIOC->CRH & ~0x0FF00000) | 0x03300000;  // PC13 to PC14

	// SysTick 計時器 (每 10us 觸發一次中斷，並將全域變數 us_counter 增加 10)
	SysTick->LOAD = 270 - 1;  // (10 * 27MHz / 1MHz) - 1
	SysTick->VAL = 0;
	SysTick->CTRL = (1 << 0) | (1 << 1);

	// TIM1 & TIM2 PWM
	TIM1->PSC = 144 - 1;    // (144MHz / 1MHz) - 1
	TIM1->ARR = 20000 - 1;  // 週期為 20ms
	TIM1->CCR1 = 2500;      // 關閉入口閘門
	TIM1->CCMR1 = (1 << 6) | (1 << 5) | (1 << 3);
	TIM1->CCER = (1 << 0);
	TIM1->BDTR = (1 << 15);
	TIM1->CR1 = (1 << 7) | (1 << 0);

	TIM2->PSC = 72 - 1;     // TIM2 時脈頻率為 72MHz
	TIM2->ARR = 20000 - 1;  // 週期為 20ms
	TIM2->CCR1 = 500;       // 關閉出口閘門
	TIM2->CCMR1 = (1 << 6) | (1 << 5) | (1 << 3);
	TIM2->CCER = (1 << 0);
	TIM2->CR1 = (1 << 7) | (1 << 0);

	// USART1
	USART1->BRR = 7500;  // 設定鮑率為 9600 (在 72MHz APB2 時脈下，72M / 9600 = 7500)
	USART1->CR1 = (1 << 13) | (1 << 2) | (1 << 3) | (1 << 5);  // 啟用 USART、傳送器、接收器及接收中斷

	// EXTI & NVIC
	AFIO->EXTICR[0] = (AFIO->EXTICR[0] & ~0x0FF0) | (0x0 << 8) | (0x0 << 4);
	EXTI->IMR |= (1 << 1) | (1 << 2);
	EXTI->RTSR |= (1 << 1) | (1 << 2);
	EXTI->FTSR |= (1 << 1) | (1 << 2);
	NVIC->ISER[0] |= (1 << 7) | (1 << 8);  // 只啟用 EXTI1, EXTI2 中斷
	NVIC->ISER[1] |= (1U << (USART1_IRQn - 32));  // 啟用 USART1 中斷 (IRQ 37)
}

void delay_us(uint32_t us) {
	uint32_t start = us_counter;
	while ((us_counter - start) < us)
		;
}

void delay_ms(uint32_t ms) {
	for (uint32_t i = 0; i < ms; i++) {
		delay_us(1000);
	}
}

// ISRs (中斷)
// SysTick 中斷：僅用於累加 us_counter
void SysTick_Handler(void) {
	us_counter += 10; // 每 10us 一次 += 10
}

void USART1_IRQHandler(void) {
	// 檢查是否為 TXE (發送緩存區空) 中斷
	if ((USART1->SR & (1 << 7)) != 0) {
		if (usart_tx_head != usart_tx_tail) {
			// 如果緩衝區還有資料，載入下一個字元至傳送暫存器
			USART1->DR = usart_tx_buffer[usart_tx_tail];
			usart_tx_tail = (usart_tx_tail + 1) % 128;
		} else {
			// 如果緩衝區已空，關閉 TXE 中斷
			USART1->CR1 &= ~(1 << 7);
		}
	}
}

// EXTI 中斷
void EXTI1_IRQHandler(void) {  // 出口感測器 Echo (PA1)
	if ((EXTI->PR & (1 << 1)) != 0) {
		if ((GPIOA->IDR & (1 << 1)) != 0) {
			out_sensor_echo_begin_us = us_counter;
		} else {
			out_sensor_echo_duration_us = us_counter - out_sensor_echo_begin_us;
			out_sensor_measurement = true;
		}
		EXTI->PR = (1 << 1);  // 清除中斷
	}
}

void EXTI2_IRQHandler(void) {  // 入口感測器 Echo (PA2)
	if ((EXTI->PR & (1 << 2)) != 0) {
		if ((GPIOA->IDR & (1 << 2)) != 0) {
			in_sensor_echo_begin_us = us_counter;
		} else {
			in_sensor_echo_duration_us = us_counter - in_sensor_echo_begin_us;
			in_sensor_measurement = true;
		}
		EXTI->PR = (1 << 2);  // 清除中斷
	}
}

void usart1_send_str(char *str) {
	while (*str) {
		// 將字元存入傳送緩衝區
		usart_tx_buffer[usart_tx_head] = *str++;
		usart_tx_head = (usart_tx_head + 1) % 128;
	}
	// 啟用 TXE 中斷，啟動發送
	USART1->CR1 |= (1 << 7);
}

void update_seven_segment(int count) {
	uint16_t arr[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x27, 0x7F,
			0x6F };
	if (count >= 0 && count <= 99) {
		GPIOB->ODR = (arr[count / 10] << 8) | arr[count % 10];
	}
}
