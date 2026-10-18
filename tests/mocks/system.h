#ifndef MATRIX_TEST_SYSTEM_H
#define MATRIX_TEST_SYSTEM_H

#include <stdint.h>

typedef uint8_t u8;
typedef struct { uint16_t ODR; uint16_t IDR; } GPIO_TypeDef;
typedef struct
{
    uint16_t GPIO_Pin;
    unsigned GPIO_Mode;
    unsigned GPIO_Speed;
} GPIO_InitTypeDef;
typedef enum { Bit_RESET, Bit_SET } BitAction;
typedef enum { DISABLE, ENABLE } FunctionalState;

extern GPIO_TypeDef testGPIOA;
extern GPIO_TypeDef testGPIOB;
extern uint32_t SystemCoreClock;
#define GPIOA (&testGPIOA)
#define GPIOB (&testGPIOB)
#define GPIO_Pin_1 ((uint16_t)0x0002)
#define GPIO_Pin_2 ((uint16_t)0x0004)
#define GPIO_Pin_6 ((uint16_t)0x0040)
#define GPIO_Pin_7 ((uint16_t)0x0080)
#define GPIO_Pin_12 ((uint16_t)0x1000)
#define GPIO_Pin_13 ((uint16_t)0x2000)
#define GPIO_Pin_14 ((uint16_t)0x4000)
#define GPIO_Mode_Out_PP 0x10U
#define GPIO_Mode_Out_OD 0x14U
#define GPIO_Mode_IPU 0x48U
#define GPIO_Speed_2MHz 2U
#define RCC_APB2Periph_AFIO 1U
#define RCC_APB2Periph_GPIOA 4U
#define RCC_APB2Periph_GPIOB 8U
#define GPIO_Remap_SWJ_Disable 0x00300400U

void GPIO_SetBits(GPIO_TypeDef *port, uint16_t pins);
void GPIO_ResetBits(GPIO_TypeDef *port, uint16_t pins);
void GPIO_WriteBit(GPIO_TypeDef *port, uint16_t pin, BitAction value);
void GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *config);
uint16_t GPIO_ReadInputData(GPIO_TypeDef *port);
void GPIO_PinRemapConfig(uint32_t remap, FunctionalState state);
void RCC_APB2PeriphClockCmd(uint32_t clocks, FunctionalState state);
void SystemCoreClockUpdate(void);

#endif
