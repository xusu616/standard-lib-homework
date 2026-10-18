#include "snake.h"

static uint8_t Snake_Occupies(const SnakeGame *game, uint8_t cell)
{
    uint8_t i;

    for (i = 0; i < game->length; i++)
    {
        if (game->cells[i] == cell)
        {
            return 1;
        }
    }
    return 0;
}

static void Snake_PlaceFood(SnakeGame *game)
{
    uint8_t cell;
    uint8_t freeIndex;

    if (game->length == SNAKE_CAPACITY)
    {
        game->food = SNAKE_NO_FOOD;
        game->state = SNAKE_WON;
        return;
    }

    /* Choose among empty cells directly, without random retry loops. */
    game->randomState = game->randomState * 1664525UL + 1013904223UL;
    freeIndex = (uint8_t)((game->randomState >> 16) %
                         (SNAKE_CAPACITY - game->length));
    for (cell = 0; cell < SNAKE_CAPACITY; cell++)
    {
        if (!Snake_Occupies(game, cell))
        {
            if (freeIndex == 0)
            {
                game->food = cell;
                return;
            }
            freeIndex--;
        }
    }
}

void Snake_Init(SnakeGame *game, uint32_t seed)
{
    game->cells[0] = 4 * SNAKE_SIDE + 3;
    game->cells[1] = 4 * SNAKE_SIDE + 2;
    game->cells[2] = 4 * SNAKE_SIDE + 1;
    game->length = 3;
    game->turnQueued = 0;
    game->direction = SNAKE_RIGHT;
    game->nextDirection = SNAKE_RIGHT;
    game->state = SNAKE_READY;
    game->randomState = seed;
    Snake_PlaceFood(game);
}

void Snake_TogglePlay(SnakeGame *game)
{
    switch (game->state)
    {
        case SNAKE_READY:
        case SNAKE_PAUSED:
            game->state = SNAKE_PLAYING;
            break;
        case SNAKE_PLAYING:
            game->state = SNAKE_PAUSED;
            break;
        case SNAKE_LOST:
        case SNAKE_WON:
            Snake_Init(game, game->randomState);
            game->state = SNAKE_PLAYING;
            break;
    }
}

void Snake_Turn(SnakeGame *game, uint8_t clockwise)
{
    /* Only one turn per move, so two quick presses cannot reverse the snake. */
    if (game->state != SNAKE_PLAYING || game->turnQueued)
    {
        return;
    }
    game->nextDirection = (SnakeDirection)((game->direction +
                                           (clockwise ? 1 : 3)) % 4);
    game->turnQueued = 1;
}

void Snake_Step(SnakeGame *game)
{
    int x;
    int y;
    uint8_t nextCell;
    uint8_t grows;
    uint8_t collisionLength;
    uint8_t i;

    if (game->state != SNAKE_PLAYING)
    {
        return;
    }

    game->direction = game->nextDirection;
    game->turnQueued = 0;
    x = game->cells[0] % SNAKE_SIDE;
    y = game->cells[0] / SNAKE_SIDE;
    switch (game->direction)
    {
        case SNAKE_UP:    y--; break;
        case SNAKE_RIGHT: x++; break;
        case SNAKE_DOWN:  y++; break;
        case SNAKE_LEFT:  x--; break;
    }

    /* Opposite edges connect; collision checks use the wrapped destination. */
    x = (x + SNAKE_SIDE) % SNAKE_SIDE;
    y = (y + SNAKE_SIDE) % SNAKE_SIDE;

    nextCell = (uint8_t)(y * SNAKE_SIDE + x);
    grows = (nextCell == game->food);
    /* The old tail leaves this tick unless food is eaten. */
    collisionLength = grows ? game->length : (uint8_t)(game->length - 1);
    for (i = 0; i < collisionLength; i++)
    {
        if (game->cells[i] == nextCell)
        {
            game->state = SNAKE_LOST;
            return;
        }
    }

    if (grows)
    {
        game->length++;
    }
    for (i = (uint8_t)(game->length - 1); i > 0; i--)
    {
        game->cells[i] = game->cells[i - 1];
    }
    game->cells[0] = nextCell;

    if (grows)
    {
        Snake_PlaceFood(game);
    }
}
