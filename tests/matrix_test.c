#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Exercise the actual firmware's static driver through simulated GPIO. */
#define main FirmwareMain
#include "../User/main.c"
#undef main

GPIO_TypeDef testGPIOA;
GPIO_TypeDef testGPIOB;
uint32_t SystemCoreClock = 72000000U;
static uint8_t shiftRegister;
static uint8_t latchedColumns;
static unsigned shiftedBits;
static unsigned latchCount;
static unsigned buzzerOnUs;
static unsigned buzzerOffUs;

static void TrackEdges(uint16_t previousA)
{
    uint16_t rising = (uint16_t)(testGPIOA.ODR & ~previousA);

    if (rising & (MATRIX_CLOCK_PIN | MATRIX_LATCH_PIN))
    {
        /* All row cathodes must be HIGH during shifting and latching. */
        assert((testGPIOB.ODR & MATRIX_ROW_PINS) == MATRIX_ROW_PINS);
    }
    if (rising & MATRIX_CLOCK_PIN)
    {
        shiftRegister = (uint8_t)((shiftRegister << 1) |
                                 ((testGPIOA.ODR & MATRIX_DATA_PIN) != 0));
        shiftedBits++;
    }
    if (rising & MATRIX_LATCH_PIN)
    {
        assert(shiftedBits == 8);
        latchedColumns = shiftRegister;
        shiftedBits = 0;
        latchCount++;
    }
}

void GPIO_SetBits(GPIO_TypeDef *port, uint16_t pins)
{
    uint16_t previousA = testGPIOA.ODR;
    port->ODR |= pins;
    TrackEdges(previousA);
}

void GPIO_ResetBits(GPIO_TypeDef *port, uint16_t pins)
{
    uint16_t previousA = testGPIOA.ODR;
    port->ODR &= (uint16_t)~pins;
    TrackEdges(previousA);
}

void GPIO_WriteBit(GPIO_TypeDef *port, uint16_t pin, BitAction value)
{
    if (value == Bit_SET) GPIO_SetBits(port, pin);
    else GPIO_ResetBits(port, pin);
}

void GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *config)
{
    if (port == GPIOA && config->GPIO_Pin == BUZZER_PIN)
    {
        assert(config->GPIO_Mode == GPIO_Mode_Out_OD);
        assert(port->ODR & BUZZER_PIN);
    }
    if (port == GPIOB && config->GPIO_Mode == GPIO_Mode_Out_PP)
    {
        assert(config->GPIO_Pin == MATRIX_ROW_PINS);
        assert((port->ODR & MATRIX_ROW_PINS) == MATRIX_ROW_PINS);
    }
}
uint16_t GPIO_ReadInputData(GPIO_TypeDef *port) { return port->IDR; }
void GPIO_PinRemapConfig(uint32_t remap, FunctionalState state)
{
    assert(remap == GPIO_Remap_SWJ_Disable && state == ENABLE);
}
void RCC_APB2PeriphClockCmd(uint32_t clocks, FunctionalState state)
{
    assert(clocks == (RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                      RCC_APB2Periph_AFIO) && state == ENABLE);
}
void SystemCoreClockUpdate(void) {}
void SysTick_Init(u8 clockMHz) { assert(clockMHz == 72); }
void delay_us(uint32_t us)
{
    assert(us > 0); /* The real polling SysTick implementation hangs at 0. */
    assert(us == 1 || us == BUZZER_ON_US || us == 1000U - BUZZER_ON_US);
    if (testGPIOA.ODR & BUZZER_PIN) buzzerOffUs += us;
    else buzzerOnUs += us;
}
void delay_ms(uint16_t ms)
{
    assert(ms == 1);
    assert(testGPIOA.ODR & BUZZER_PIN);
    buzzerOffUs += 1000;
}

static void TestScan(void)
{
    unsigned column;
    unsigned pattern;
    unsigned x;
    unsigned y;

    testGPIOB.ODR = 0x005A;
    Hardware_Init();
    assert(latchedColumns == 0);
    assert(testGPIOB.ODR == 0xFF5A);
    for (column = 0; column < 8; column++)
    {
        for (pattern = 0; pattern < 256; pattern++)
        {
            memset(frame, 0, sizeof(frame));
            frame[column] = (uint8_t)pattern;
            Matrix_ScanColumn((uint8_t)column);
            assert(latchedColumns == (1U << column));
            assert((testGPIOB.ODR & 0xFF) == 0x5A);
            for (x = 0; x < 8; x++)
            {
                for (y = 0; y < 8; y++)
                {
                    /* Board wiring: COL anode HIGH and ROW cathode LOW. */
                    unsigned lit = ((latchedColumns >> x) & 1U) &&
                                   !(testGPIOB.ODR & (1U << (8 + y)));
                    assert(lit == (x == column && ((pattern >> y) & 1U)));
                }
            }
        }
    }
    assert(latchCount == 1 + 8 * 256);
}

static void TestGameFrame(void)
{
    unsigned x;
    unsigned y;
    unsigned count = 0;

    Snake_Init(&game, 1);
    Frame_Update(0);
    for (x = 0; x < 8; x++)
    {
        for (y = 0; y < 8; y++) count += (frame[x] >> y) & 1U;
    }
    assert(count == 4); /* Three body pixels plus one food pixel. */
    Frame_Update(200);
    count = 0;
    for (x = 0; x < 8; x++)
    {
        for (y = 0; y < 8; y++) count += (frame[x] >> y) & 1U;
    }
    assert(count == 3);
}

static void TestFailureBuzzer(void)
{
    unsigned tick;
    unsigned previousOnUs;

    buzzerOnUs = buzzerOffUs = 0;
    Buzzer_UpdateState(SNAKE_READY);
    Buzzer_WaitOneMs();
    Buzzer_UpdateState(SNAKE_PLAYING);
    Buzzer_WaitOneMs();
    Buzzer_UpdateState(SNAKE_PAUSED);
    Buzzer_WaitOneMs();
    assert(buzzerOnUs == 0 && buzzerOffUs == 3000);

    Buzzer_UpdateState(SNAKE_PLAYING);
    Buzzer_UpdateState(SNAKE_LOST);
    assert(buzzerTicksRemaining == BUZZER_DURATION_MS);
    buzzerOnUs = buzzerOffUs = 0;
    for (tick = 0; tick < BUZZER_DURATION_MS; tick++)
    {
        Buzzer_WaitOneMs();
        assert(testGPIOA.ODR & BUZZER_PIN);
        Buzzer_UpdateState(SNAKE_LOST);
    }
    assert(buzzerOnUs == BUZZER_DURATION_MS * BUZZER_ON_US);
    assert(buzzerOnUs + buzzerOffUs == BUZZER_DURATION_MS * 1000U);
    assert(buzzerTicksRemaining == 0);
    previousOnUs = buzzerOnUs;
    for (tick = 0; tick < 100; tick++)
    {
        Buzzer_UpdateState(SNAKE_LOST);
        Buzzer_WaitOneMs();
    }
    assert(buzzerOnUs == previousOnUs);

    Buzzer_UpdateState(SNAKE_PLAYING);
    Buzzer_UpdateState(SNAKE_LOST);
    Buzzer_WaitOneMs();
    assert(buzzerOnUs == previousOnUs + BUZZER_ON_US);
    Buzzer_UpdateState(SNAKE_PLAYING); /* Early restart cancels remaining sound. */
    assert(buzzerTicksRemaining == 0);
    Buzzer_UpdateState(SNAKE_WON);
    Buzzer_WaitOneMs();
    assert(buzzerOnUs == previousOnUs + BUZZER_ON_US);
}

static void TestCollisionFeedback(void)
{
    static const uint8_t bodyLoop[] = {9, 8, 0, 1, 2, 10};
    unsigned column;

    Snake_Init(&game, 1);
    memcpy(game.bodyCellIds, bodyLoop, sizeof(bodyLoop));
    game.snakeLength = 6;
    game.foodCellId = 63;
    game.direction = game.nextDirection = SNAKE_UP;
    game.state = SNAKE_PLAYING;
    Buzzer_UpdateState(game.state);
    Snake_Step(&game);
    assert(game.state == SNAKE_LOST);
    Buzzer_UpdateState(game.state);
    assert(buzzerTicksRemaining == BUZZER_DURATION_MS);

    /* A failed movement must reach X rendering even while the buzzer is on. */
    Frame_Update(0);
    for (column = 0; column < 8; column++)
    {
        assert(frame[column] == ((1U << column) | (1U << (7 - column))));
        Matrix_ScanColumn((uint8_t)column);
        Buzzer_WaitOneMs();
    }
    assert(buzzerTicksRemaining == BUZZER_DURATION_MS - 8);
    Buzzer_UpdateState(SNAKE_PLAYING);
    assert(buzzerTicksRemaining == 0);
}

int main(void)
{
    TestScan();
    TestGameFrame();
    TestFailureBuzzer();
    TestCollisionFeedback();
    printf("Driver tests passed: matrix, collision-to-X, one-shot buzzer %u ms/%u us, no zero delays.\n",
           (unsigned)BUZZER_DURATION_MS, (unsigned)BUZZER_ON_US);
    return 0;
}
