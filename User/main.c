#include "system.h"
#include "SysTick.h"
#include "snake.h"

#define MATRIX_ROW_PINS ((uint16_t)0xFF00)
#define MATRIX_DATA_PIN GPIO_Pin_12
#define MATRIX_LATCH_PIN GPIO_Pin_13
#define MATRIX_CLOCK_PIN GPIO_Pin_14
#define KEY_LEFT_PIN GPIO_Pin_2
#define KEY_RIGHT_PIN GPIO_Pin_6
#define KEY_PLAY_PIN GPIO_Pin_7
#define KEY_DEBOUNCE_MS 20
#define MOVE_INTERVAL_MS 300
#define BUZZER_PIN GPIO_Pin_1
#define BUZZER_DURATION_MS 1000
#define BUZZER_ON_US 1000U

#if BUZZER_ON_US > 1000U
#error BUZZER_ON_US_must_be_between_0_and_1000
#endif

typedef struct
{
    uint8_t stableDown;
    uint8_t changeTicks;
} KeyFilter;

static SnakeGame game;
static uint8_t frame[8];
static uint32_t buzzerTicksRemaining;
//上一次是写uint8_t导致到的数字被截断了
static SnakeState buzzerPreviousState = SNAKE_READY;

static void Buzzer_UpdateState(SnakeState state)
{
    /* Entering LOST triggers the configured duration once, without retriggering. */
    if (state != buzzerPreviousState)
    {
        buzzerPreviousState = state;
        buzzerTicksRemaining = (state == SNAKE_LOST) ? BUZZER_DURATION_MS : 0;
        GPIO_SetBits(GPIOA, BUZZER_PIN);
    }
}

static void Buzzer_WaitOneMs(void)
{
    if (buzzerTicksRemaining != 0)
    {
        /* Gate at about 1 kHz; BUZZER_ON_US sets the on time per millisecond.
           0% and 100% omit the empty phase: this SysTick driver cannot delay 0.
           The total wait remains 1 ms so matrix refresh keeps running. */
        if (BUZZER_ON_US > 0U)
        {
            GPIO_ResetBits(GPIOA, BUZZER_PIN);
            delay_us(BUZZER_ON_US);
        }
        GPIO_SetBits(GPIOA, BUZZER_PIN);
        if (BUZZER_ON_US < 1000U)
        {
            delay_us(1000U - BUZZER_ON_US);
        }
        buzzerTicksRemaining--;
    }
    else
    {
        delay_ms(1);
    }
}

static void Matrix_WriteColumns(uint8_t value)
{
    uint8_t bit;

    GPIO_ResetBits(GPIOA, MATRIX_LATCH_PIN);
    /* MSB first: the final bit reaches QA, which is physical column 1. */
    for (bit = 0; bit < 8; bit++)
    {
        GPIO_ResetBits(GPIOA, MATRIX_CLOCK_PIN);
        GPIO_WriteBit(GPIOA, MATRIX_DATA_PIN,
                      (value & 0x80U) ? Bit_SET : Bit_RESET);
        delay_us(1);
        GPIO_SetBits(GPIOA, MATRIX_CLOCK_PIN);
        delay_us(1);
        value <<= 1;
    }
    GPIO_ResetBits(GPIOA, MATRIX_CLOCK_PIN);
    GPIO_SetBits(GPIOA, MATRIX_LATCH_PIN);
    delay_us(1);
    GPIO_ResetBits(GPIOA, MATRIX_LATCH_PIN);
}

static void Hardware_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                          RCC_APB2Periph_AFIO, ENABLE);

    /* PA13/PA14 drive the matrix, so both JTAG and SWD must be released.
       USART1 ISP programming on PA9/PA10 is still available. */
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_Disable, ENABLE);

    /* Common-cathode rows are disabled HIGH before enabling the outputs. */
    GPIO_SetBits(GPIOB, MATRIX_ROW_PINS);
    gpio.GPIO_Pin = MATRIX_ROW_PINS;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &gpio);

    GPIO_ResetBits(GPIOA, MATRIX_DATA_PIN | MATRIX_LATCH_PIN | MATRIX_CLOCK_PIN);
    gpio.GPIO_Pin = MATRIX_DATA_PIN | MATRIX_LATCH_PIN | MATRIX_CLOCK_PIN;
    GPIO_Init(GPIOA, &gpio);

    /* P11/PA1: active LOW. Released open-drain output is the silent state. */
    GPIO_SetBits(GPIOA, BUZZER_PIN);
    gpio.GPIO_Pin = BUZZER_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_Init(GPIOA, &gpio);
    Matrix_WriteColumns(0x00);

    /* SW2 shares PA13 with the latch: do not use or press it during play. */
    gpio.GPIO_Pin = KEY_LEFT_PIN | KEY_RIGHT_PIN | KEY_PLAY_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &gpio);
}

static void Matrix_ScanColumn(uint8_t column)
{
    /* Row cathodes: HIGH = off. Column anodes: HIGH = selected.
       Blank every row before shifting/latching to prevent ghost pixels. */
    GPIO_SetBits(GPIOB, MATRIX_ROW_PINS);
    Matrix_WriteColumns((uint8_t)(1U << column));
    GPIO_ResetBits(GPIOB, (uint16_t)((uint16_t)frame[column] << 8));
}

static uint8_t Key_Pressed(KeyFilter *key, uint8_t rawDown)
{
    if (rawDown == key->stableDown)
    {
        key->changeTicks = 0;
        return 0;
    }

    key->changeTicks++;
    if (key->changeTicks == KEY_DEBOUNCE_MS)
    {
        key->stableDown = rawDown;
        key->changeTicks = 0;
        return rawDown;
    }
    return 0;
}

static void Frame_AddCell(uint8_t cellId)
{
    frame[cellId % SNAKE_SIDE] |= (uint8_t)(1U << (cellId / SNAKE_SIDE));
}

static void Frame_Update(uint32_t elapsedMs)
{
    static const uint8_t winFace[8] = {0x3C, 0x42, 0x95, 0xA1,
                                     0xA1, 0x95, 0x42, 0x3C};
    uint8_t i;

    for (i = 0; i < 8; i++)
    {
        frame[i] = 0;
    }

    if (game.state == SNAKE_LOST)
    {
        for (i = 0; i < 8; i++)
        {
            frame[i] = (uint8_t)((1U << i) | (1U << (7 - i)));
        }
        return;
    }
    if (game.state == SNAKE_WON)
    {
        for (i = 0; i < 8; i++)
        {
            frame[i] = winFace[i];
        }
        return;
    }
    if (game.state == SNAKE_PAUSED && (elapsedMs / 400U) % 2U != 0)
    {
        return;
    }

    for (i = 0; i < game.snakeLength; i++)
    {
        Frame_AddCell(game.bodyCellIds[i]);
    }
    if (game.foodCellId != SNAKE_NO_FOOD && (elapsedMs / 200U) % 2U == 0)
    {
        Frame_AddCell(game.foodCellId);
    }
}

int main(void)
{
    KeyFilter leftKey = {0, 0};
    KeyFilter rightKey = {0, 0};
    KeyFilter playKey = {0, 0};
    uint32_t elapsedMs = 0;
    uint16_t moveTicks = 0;
    uint16_t keyLevels;
    uint8_t column = 0;
    uint8_t leftPressed;
    uint8_t rightPressed;
    uint8_t playPressed;

    /* Startup SystemInit configures the 8 MHz HSE x 9 = 72 MHz clock. */
    SystemCoreClockUpdate();
    SysTick_Init((u8)(SystemCoreClock / 1000000U));
    Hardware_Init();
    Snake_Init(&game, 1);
    Frame_Update(elapsedMs);

    while (1)
    {
        /* One column per millisecond: about 120 complete frames per second.
           The game runs cooperatively; no long movement delays stop scanning. */
        Matrix_ScanColumn(column);
        Buzzer_WaitOneMs();
        GPIO_SetBits(GPIOB, MATRIX_ROW_PINS);
        elapsedMs++;

        keyLevels = GPIO_ReadInputData(GPIOA);
        leftPressed = Key_Pressed(&leftKey, (keyLevels & KEY_LEFT_PIN) == 0);
        rightPressed = Key_Pressed(&rightKey, (keyLevels & KEY_RIGHT_PIN) == 0);
        playPressed = Key_Pressed(&playKey, (keyLevels & KEY_PLAY_PIN) == 0);

        if (playPressed)
        {
            /* Vary the food sequence with the user's start/restart timing. */
            if (game.state == SNAKE_READY)
            {
                Snake_Init(&game, elapsedMs);
            }
            else if (game.state == SNAKE_LOST || game.state == SNAKE_WON)
            {
                game.randomState ^= elapsedMs;
            }
            Snake_TogglePlay(&game);
            moveTicks = 0;
        }
        else if (leftPressed && !rightPressed)
        {
            Snake_Turn(&game, 0);
        }
        else if (rightPressed && !leftPressed)
        {
            Snake_Turn(&game, 1);
        }

        if (game.state == SNAKE_PLAYING)
        {
            moveTicks++;
            if (moveTicks >= MOVE_INTERVAL_MS)
            {
                moveTicks = 0;
                Snake_Step(&game);
            }
        }

        Buzzer_UpdateState(game.state);

        column = (uint8_t)((column + 1) % 8);
        if (column == 0)
        {
            /* Publish a complete frame only between two refresh cycles. */
            Frame_Update(elapsedMs);
        }
    }
}
