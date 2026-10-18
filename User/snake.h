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
    /* Head first; a cell is encoded as y * 8 + x. */
    uint8_t cells[SNAKE_CAPACITY];
    uint8_t length;
    uint8_t food;
    uint8_t turnQueued;
    SnakeDirection direction;
    SnakeDirection nextDirection;
    SnakeState state;
    uint32_t randomState;
} SnakeGame;

void Snake_Init(SnakeGame *game, uint32_t seed);
void Snake_TogglePlay(SnakeGame *game);
void Snake_Turn(SnakeGame *game, uint8_t clockwise);
void Snake_Step(SnakeGame *game);

#endif
