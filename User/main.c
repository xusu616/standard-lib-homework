#include "system.h"

#define LED_PINS ((uint16_t)0x00FF)
#define BUZZER_PIN GPIO_Pin_1
#define BUTTON_PINS (GPIO_Pin_2 | GPIO_Pin_13 | GPIO_Pin_6 | GPIO_Pin_7)
#define BUTTON_EXTI_LINES (EXTI_Line2 | EXTI_Line13 | EXTI_Line6 | EXTI_Line7)
#define LED_STEP_MS 200U
#define BUTTON_DEBOUNCE_MS 20U
#define BUTTON_COUNT 4

static const uint16_t buttonPins[BUTTON_COUNT] =
{
    GPIO_Pin_2, GPIO_Pin_13, GPIO_Pin_6, GPIO_Pin_7
};

static volatile uint32_t systemMilliseconds;
static volatile uint32_t buttonLastEdgeMs[BUTTON_COUNT];
static volatile uint8_t buttonEdgePending[BUTTON_COUNT];
static uint8_t buttonStableDown[BUTTON_COUNT];
static int8_t ledDirection;
static uint8_t currentLedIndex;
static uint32_t lastLedStepMs;

static void Led_Show(uint8_t ledIndex)
{
    /* D4~D11 = PB0~PB7, low level lights the LED. */
    GPIO_SetBits(GPIOB, LED_PINS);
    GPIO_ResetBits(GPIOB, (uint16_t)(GPIO_Pin_0 << ledIndex));
}

static void Hardware_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                          RCC_APB2Periph_AFIO, ENABLE);

    /* SW2 uses PA13, and LEDs use PB3/PB4: release SWD and JTAG. */
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_Disable, ENABLE);

    GPIO_SetBits(GPIOB, LED_PINS);
    gpio.GPIO_Pin = LED_PINS;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &gpio);

    /* PA1/P11: the active buzzer sounds LOW; released output is silent. */
    GPIO_SetBits(GPIOA, BUZZER_PIN);
    gpio.GPIO_Pin = BUZZER_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin = BUTTON_PINS;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &gpio);
}

static void exti_init(void)
{
    EXTI_InitTypeDef exti;
    NVIC_InitTypeDef nvic;

    /* AFIO selects which GPIO port is connected to each EXTI line. */
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource2);
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource13);
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource6);
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource7);

    EXTI_ClearITPendingBit(BUTTON_EXTI_LINES);
    exti.EXTI_Line = BUTTON_EXTI_LINES;
    exti.EXTI_Mode = EXTI_Mode_Interrupt;
    /* Both edges record press/release changes for nonblocking debounce. */
    exti.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
    exti.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti);

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    nvic.NVIC_IRQChannelPreemptionPriority = 1;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;

    nvic.NVIC_IRQChannel = EXTI2_IRQn;
    NVIC_Init(&nvic);
    nvic.NVIC_IRQChannel = EXTI15_10_IRQn;
    NVIC_Init(&nvic);
    nvic.NVIC_IRQChannel = EXTI9_5_IRQn;
    NVIC_Init(&nvic);
}

void SysTick_Handler(void)
{
    systemMilliseconds++;
}

static void Button_RecordEdge(uint8_t buttonIndex, uint32_t extiLine)
{
    if (EXTI_GetITStatus(extiLine) != RESET)
    {
        EXTI_ClearITPendingBit(extiLine);
        buttonLastEdgeMs[buttonIndex] = systemMilliseconds;
        buttonEdgePending[buttonIndex] = 1;
    }
}

void EXTI2_IRQHandler(void)
{
    Button_RecordEdge(0, EXTI_Line2);       /* SW1 / PA2 */
}

void EXTI15_10_IRQHandler(void)
{
    Button_RecordEdge(1, EXTI_Line13);      /* SW2 / PA13 */
}

void EXTI9_5_IRQHandler(void)
{
    /* Shared vector: check both lines, including simultaneous events. */
    Button_RecordEdge(2, EXTI_Line6);       /* SW3 / PA6 */
    Button_RecordEdge(3, EXTI_Line7);       /* SW4 / PA7 */
}

static void App_OnButtonPressed(uint8_t buttonIndex)
{
    switch (buttonIndex)
    {
        case 0: /* SW1: D4 -> D5 -> ... -> D11 -> D4 */
            ledDirection = 1;
            currentLedIndex = 0;
            lastLedStepMs = systemMilliseconds;
            Led_Show(currentLedIndex);
            break;
        case 1: /* SW2: D11 -> D10 -> ... -> D4 -> D11 */
            ledDirection = -1;
            currentLedIndex = 7;
            lastLedStepMs = systemMilliseconds;
            Led_Show(currentLedIndex);
            break;
        case 2: /* SW3: buzzer on until SW4 is pressed. */
            GPIO_ResetBits(GPIOA, BUZZER_PIN);
            break;
        case 3: /* SW4: buzzer off. */
            GPIO_SetBits(GPIOA, BUZZER_PIN);
            break;
    }
}

static void Buttons_Process(void)
{
    uint8_t buttonIndex;
    uint8_t sampleReady;
    uint8_t isDown;
    uint32_t previousInterruptMask;

    for (buttonIndex = 0; buttonIndex < BUTTON_COUNT; buttonIndex++)
    {
        sampleReady = 0;
        isDown = 0;

        /* Read the ISR-owned timestamp and consume its event atomically.
           IRQs are disabled only for this short check, never for 20 ms. */
        previousInterruptMask = __get_PRIMASK();
        __disable_irq();
        if (buttonEdgePending[buttonIndex] &&
            (uint32_t)(systemMilliseconds - buttonLastEdgeMs[buttonIndex]) >=
                BUTTON_DEBOUNCE_MS)
        {
            isDown = (GPIO_ReadInputDataBit(GPIOA, buttonPins[buttonIndex]) == Bit_RESET);
            buttonEdgePending[buttonIndex] = 0;
            sampleReady = 1;
        }
        __set_PRIMASK(previousInterruptMask);

        if (sampleReady && isDown != buttonStableDown[buttonIndex])
        {
            buttonStableDown[buttonIndex] = isDown;
            if (isDown)
            {
                App_OnButtonPressed(buttonIndex);
            }
        }
    }
}

static void Led_Update(void)
{
    uint32_t now = systemMilliseconds;

    if (ledDirection != 0 && (uint32_t)(now - lastLedStepMs) >= LED_STEP_MS)
    {
        lastLedStepMs = now;
        currentLedIndex = (uint8_t)((currentLedIndex + ledDirection + 8) % 8);
        Led_Show(currentLedIndex);
    }
}

int main(void)
{
    SystemCoreClockUpdate();
    Hardware_Init();
    exti_init();

    /* 72 MHz / 1000 = 72000 cycles: a periodic 1 ms system tick.
       Do not call the legacy delay_ms/delay_us helpers: they also own SysTick. */
    SysTick_Config(SystemCoreClock / 1000U);

    while (1)
    {
        Buttons_Process();
        Led_Update();
        __WFI(); /* Wake on the next SysTick or button interrupt. */
    }
}
