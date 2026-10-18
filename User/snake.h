#ifndef SNAKE_H
#define SNAKE_H

#include <stdint.h>

#define SNAKE_SIDE 8
#define SNAKE_CAPACITY 64
#define SNAKE_NO_FOOD 64

typedef enum
{
    SNAKE_UP,
    SNAKE_RIGHT,
    SNAKE_DOWN,
    SNAKE_LEFT
} SnakeDirection;

typedef enum
{
    SNAKE_READY,
    SNAKE_PLAYING,
    SNAKE_PAUSED,
    SNAKE_LOST,
    SNAKE_WON
} SnakeState;

typedef struct
{
    /* 格子编号 = y * 8 + x，范围 0~63；数组第 0 项始终是蛇头。 */
    uint8_t bodyCellIds[SNAKE_CAPACITY]; // 每一节蛇身所在的格子编号
    uint8_t snakeLength;                // 当前蛇身节数
    uint8_t foodCellId;                 // 食物所在的格子编号
    uint8_t turnQueued;
    SnakeDirection direction;
    SnakeDirection nextDirection;
    SnakeState state;
    uint32_t randomState;
} SnakeGame;
//差不多就是这样装了两层结构体，其实还是清楚简单的

void Snake_Init(SnakeGame *game, uint32_t seed);
void Snake_TogglePlay(SnakeGame *game);
void Snake_Turn(SnakeGame *game, uint8_t clockwise);
void Snake_Step(SnakeGame *game);

#endif
