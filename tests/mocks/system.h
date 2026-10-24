#ifndef EXTI_LAB_TEST_SYSTEM_H
#define EXTI_LAB_TEST_SYSTEM_H

#include <stdint.h>

typedef struct { uint16_t ODR; uint16_t IDR; } GPIO_TypeDef;
typedef struct
{
    uint16_t GPIO_Pin;
    unsigned GPIO_Mode;
    unsigned GPIO_Speed;
} GPIO_InitTypeDef;
typedef struct
{
    uint32_t EXTI_Line;
    unsigned EXTI_Mode;
    unsigned EXTI_Trigger;
    unsigned EXTI_LineCmd;
} EXTI_InitTypeDef;
typedef struct
{
    uint8_t NVIC_IRQChannel;
    uint8_t NVIC_IRQChannelPreemptionPriority;
    uint8_t NVIC_IRQChannelSubPriority;
    unsigned NVIC_IRQChannelCmd;
} NVIC_InitTypeDef;
typedef enum { Bit_RESET, Bit_SET } BitAction;
typedef enum { DISABLE, ENABLE } FunctionalState;
typedef enum { RESET, SET } ITStatus;

extern GPIO_TypeDef testGPIOA;
extern GPIO_TypeDef testGPIOB;
extern uint32_t SystemCoreClock;
#define GPIOA (&testGPIOA)
#define GPIOB (&testGPIOB)
#define GPIO_Pin_0 ((uint16_t)0x0001)
#define GPIO_Pin_1 ((uint16_t)0x0002)
#define GPIO_Pin_2 ((uint16_t)0x0004)
#define GPIO_Pin_6 ((uint16_t)0x0040)
#define GPIO_Pin_7 ((uint16_t)0x0080)
#define GPIO_Pin_13 ((uint16_t)0x2000)
#define GPIO_Mode_Out_PP 0x10U
#define GPIO_Mode_Out_OD 0x14U
#define GPIO_Mode_IPU 0x48U
#define GPIO_Speed_2MHz 2U
#define RCC_APB2Periph_AFIO 1U
#define RCC_APB2Periph_GPIOA 4U
#define RCC_APB2Periph_GPIOB 8U
#define GPIO_Remap_SWJ_Disable 0x00300400U
#define GPIO_PortSourceGPIOA 0U
#define GPIO_PinSource2 2U
#define GPIO_PinSource6 6U
#define GPIO_PinSource7 7U
#define GPIO_PinSource13 13U
#define EXTI_Line2 (1U << 2)
#define EXTI_Line6 (1U << 6)
#define EXTI_Line7 (1U << 7)
#define EXTI_Line13 (1U << 13)
#define EXTI_Mode_Interrupt 0U
#define EXTI_Trigger_Rising_Falling 0x10U
#define NVIC_PriorityGroup_2 0x500U
#define EXTI2_IRQn 8U
#define EXTI9_5_IRQn 23U
#define EXTI15_10_IRQn 40U

void GPIO_SetBits(GPIO_TypeDef *port, uint16_t pins);
void GPIO_ResetBits(GPIO_TypeDef *port, uint16_t pins);
void GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *config);
BitAction GPIO_ReadInputDataBit(GPIO_TypeDef *port, uint16_t pin);
void GPIO_PinRemapConfig(uint32_t remap, FunctionalState state);
void GPIO_EXTILineConfig(uint8_t portSource, uint8_t pinSource);
void RCC_APB2PeriphClockCmd(uint32_t clocks, FunctionalState state);
void EXTI_Init(EXTI_InitTypeDef *config);
ITStatus EXTI_GetITStatus(uint32_t line);
void EXTI_ClearITPendingBit(uint32_t lines);
void NVIC_PriorityGroupConfig(uint32_t group);
void NVIC_Init(NVIC_InitTypeDef *config);
uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __set_PRIMASK(uint32_t mask);
void __WFI(void);
void SystemCoreClockUpdate(void);
uint32_t SysTick_Config(uint32_t ticks);

#endif
