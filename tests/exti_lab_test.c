#include <assert.h>
#include <stdio.h>
#include <string.h>

#define main FirmwareMain
#include "../User/main.c"
#undef main

GPIO_TypeDef testGPIOA;
GPIO_TypeDef testGPIOB;
uint32_t SystemCoreClock = 72000000U;
static uint32_t pendingLines;
static uint32_t enabledLines;
static uint32_t interruptMask;
static uint16_t routedLines;
static uint8_t enabledIrqs[64];

void GPIO_SetBits(GPIO_TypeDef *port, uint16_t pins) { port->ODR |= pins; }
void GPIO_ResetBits(GPIO_TypeDef *port, uint16_t pins) { port->ODR &= (uint16_t)~pins; }
void GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *config)
{
    assert(config->GPIO_Speed == GPIO_Speed_2MHz);
    if (config->GPIO_Mode == GPIO_Mode_IPU)
    {
        assert(port == GPIOA && config->GPIO_Pin == BUTTON_PINS);
        port->IDR |= config->GPIO_Pin;
    }
    else if (port == GPIOB)
    {
        assert(config->GPIO_Mode == GPIO_Mode_Out_PP && config->GPIO_Pin == LED_PINS);
        assert((port->ODR & LED_PINS) == LED_PINS);
    }
    else
    {
        assert(config->GPIO_Mode == GPIO_Mode_Out_OD && config->GPIO_Pin == BUZZER_PIN);
        assert(port->ODR & BUZZER_PIN);
    }
}
BitAction GPIO_ReadInputDataBit(GPIO_TypeDef *port, uint16_t pin)
{
    return (port->IDR & pin) ? Bit_SET : Bit_RESET;
}
void GPIO_PinRemapConfig(uint32_t remap, FunctionalState state)
{
    assert(remap == GPIO_Remap_SWJ_Disable && state == ENABLE);
}
void GPIO_EXTILineConfig(uint8_t portSource, uint8_t pinSource)
{
    assert(portSource == GPIO_PortSourceGPIOA);
    routedLines |= (uint16_t)(1U << pinSource);
}
void RCC_APB2PeriphClockCmd(uint32_t clocks, FunctionalState state)
{
    assert(clocks == (RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                      RCC_APB2Periph_AFIO) && state == ENABLE);
}
void EXTI_Init(EXTI_InitTypeDef *config)
{
    assert(config->EXTI_Mode == EXTI_Mode_Interrupt);
    assert(config->EXTI_Trigger == EXTI_Trigger_Rising_Falling);
    assert(config->EXTI_LineCmd == ENABLE);
    enabledLines = config->EXTI_Line;
}
ITStatus EXTI_GetITStatus(uint32_t line)
{
    return (pendingLines & enabledLines & line) ? SET : RESET;
}
void EXTI_ClearITPendingBit(uint32_t lines) { pendingLines &= ~lines; }
void NVIC_PriorityGroupConfig(uint32_t group) { assert(group == NVIC_PriorityGroup_2); }
void NVIC_Init(NVIC_InitTypeDef *config)
{
    assert(config->NVIC_IRQChannelPreemptionPriority == 1);
    assert(config->NVIC_IRQChannelSubPriority == 0);
    assert(config->NVIC_IRQChannelCmd == ENABLE);
    enabledIrqs[config->NVIC_IRQChannel] = 1;
}
uint32_t __get_PRIMASK(void) { return interruptMask; }
void __disable_irq(void) { interruptMask = 1; }
void __set_PRIMASK(uint32_t mask) { interruptMask = mask; }
void __WFI(void) {}
void SystemCoreClockUpdate(void) {}
uint32_t SysTick_Config(uint32_t ticks)
{
    assert(ticks == 72000);
    return 0;
}

static void ResetLab(void)
{
    memset(&testGPIOA, 0, sizeof(testGPIOA));
    memset(&testGPIOB, 0, sizeof(testGPIOB));
    testGPIOB.ODR = 0xA500;
    memset(enabledIrqs, 0, sizeof(enabledIrqs));
    memset((void *)buttonLastEdgeMs, 0, sizeof(buttonLastEdgeMs));
    memset((void *)buttonEdgePending, 0, sizeof(buttonEdgePending));
    memset(buttonStableDown, 0, sizeof(buttonStableDown));
    systemMilliseconds = 0;
    ledDirection = 0;
    currentLedIndex = 0;
    lastLedStepMs = 0;
    pendingLines = enabledLines = interruptMask = routedLines = 0;
    Hardware_Init();
    exti_init();
    SysTick_Config(SystemCoreClock / 1000U);
    assert(enabledLines == BUTTON_EXTI_LINES && routedLines == BUTTON_PINS);
    assert(enabledIrqs[EXTI2_IRQn] && enabledIrqs[EXTI9_5_IRQn] &&
           enabledIrqs[EXTI15_10_IRQn]);
    assert(testGPIOB.ODR == 0xA5FF);
    assert(testGPIOA.ODR & BUZZER_PIN);
}

static void AdvanceMs(unsigned count)
{
    while (count--)
    {
        SysTick_Handler();
        Buttons_Process();
        Led_Update();
        assert(interruptMask == 0);
    }
}

static void ButtonEdge(unsigned buttonIndex, unsigned down)
{
    uint32_t line = buttonPins[buttonIndex];
    if (down) testGPIOA.IDR &= (uint16_t)~line;
    else testGPIOA.IDR |= (uint16_t)line;
    pendingLines |= line;
    if (buttonIndex == 0) EXTI2_IRQHandler();
    else if (buttonIndex == 1) EXTI15_10_IRQHandler();
    else EXTI9_5_IRQHandler();
    assert((pendingLines & line) == 0); /* IRQ must clear the hardware flag. */
    assert(buttonEdgePending[buttonIndex]);
}

static void AssertLed(unsigned index)
{
    assert((testGPIOB.ODR & LED_PINS) == (LED_PINS ^ (1U << index)));
    assert((testGPIOB.ODR & 0xFF00) == 0xA500);
}

static void TestDirections(void)
{
    unsigned step;
    ResetLab();
    ButtonEdge(0, 1);
    AdvanceMs(19);
    assert(ledDirection == 0);
    AdvanceMs(1);
    assert(ledDirection == 1);
    AssertLed(0);
    for (step = 1; step <= 8; step++)
    {
        AdvanceMs(LED_STEP_MS);
        AssertLed(step % 8);
    }
    ButtonEdge(1, 1);
    AdvanceMs(BUTTON_DEBOUNCE_MS);
    assert(ledDirection == -1);
    AssertLed(7);
    for (step = 1; step <= 8; step++)
    {
        AdvanceMs(LED_STEP_MS);
        AssertLed((7 + 8 - step) % 8);
    }
}

static void TestBuzzerAndBounce(void)
{
    ResetLab();
    ButtonEdge(0, 1);
    AdvanceMs(4);
    ButtonEdge(0, 0);
    AdvanceMs(4);
    ButtonEdge(0, 1);
    AdvanceMs(19);
    assert(ledDirection == 0);
    AdvanceMs(1);
    AssertLed(0);
    AdvanceMs(LED_STEP_MS * 2);
    AssertLed(2); /* Holding SW1 does not restart the sequence. */

    ButtonEdge(2, 1);
    AdvanceMs(19);
    assert(testGPIOA.ODR & BUZZER_PIN);
    AdvanceMs(1);
    assert(!(testGPIOA.ODR & BUZZER_PIN));
    AdvanceMs(1000);
    assert(!(testGPIOA.ODR & BUZZER_PIN));
    assert(currentLedIndex != 2); /* LEDs continue while buzzer is sounding. */
    ButtonEdge(3, 1);
    AdvanceMs(BUTTON_DEBOUNCE_MS);
    assert(testGPIOA.ODR & BUZZER_PIN);
}

static void TestSharedIrqAndWrap(void)
{
    ResetLab();
    /* A changed level alone must not trigger an action without EXTI. */
    testGPIOA.IDR &= (uint16_t)~GPIO_Pin_2;
    AdvanceMs(30);
    assert(ledDirection == 0);

    testGPIOA.IDR &= (uint16_t)~(GPIO_Pin_6 | GPIO_Pin_7);
    pendingLines = EXTI_Line6 | EXTI_Line7;
    EXTI9_5_IRQHandler();
    assert(pendingLines == 0 && buttonEdgePending[2] && buttonEdgePending[3]);
    AdvanceMs(BUTTON_DEBOUNCE_MS);
    assert(buttonStableDown[2] && buttonStableDown[3]);
    assert(testGPIOA.ODR & BUZZER_PIN); /* SW4 wins simultaneous on/off requests. */

    ResetLab();
    systemMilliseconds = UINT32_MAX - 10;
    ButtonEdge(0, 1);
    AdvanceMs(19);
    assert(ledDirection == 0);
    AdvanceMs(1);
    AssertLed(0);
    AdvanceMs(LED_STEP_MS);
    AssertLed(1);

    interruptMask = 1;
    Buttons_Process();
    assert(interruptMask == 1); /* Preserve the caller's previous IRQ mask. */
    interruptMask = 0;
}

int main(void)
{
    TestDirections();
    TestBuzzerAndBounce();
    TestSharedIrqAndWrap();
    puts("EXTI lab tests passed: AFIO/NVIC, interrupt flags, debounce, both LED directions, buzzer, tick wrap.");
    return 0;
}
