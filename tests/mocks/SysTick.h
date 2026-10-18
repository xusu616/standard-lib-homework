#ifndef MATRIX_TEST_SYSTICK_H
#define MATRIX_TEST_SYSTICK_H

#include "system.h"

void SysTick_Init(u8 clockMHz);
void delay_us(uint32_t us);
void delay_ms(uint16_t ms);

#endif
