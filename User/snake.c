#include "snake.h"

static uint8_t Snake_Occupies(const SnakeGame *game, uint8_t cellId)
{
    uint8_t bodySegmentIndex;

    for (bodySegmentIndex = 0; bodySegmentIndex < game->snakeLength; bodySegmentIndex++)
    {
        if (game->bodyCellIds[bodySegmentIndex] == cellId)
        {
            return 1;
        }
    }
    return 0;
}

static void Snake_PlaceFood(SnakeGame *game)
{
    uint8_t candidateCellId;  // 当前检查的格子编号，可能有蛇，也可能为空
    uint8_t emptyCellsToSkip; // 选中食物位置前，还需要跳过几个空格

    if (game->snakeLength == SNAKE_CAPACITY)
    {
        game->foodCellId = SNAKE_NO_FOOD;
        game->state = SNAKE_WON;
        return;
    }
    /* Choose among empty cells directly, without random retry loops. */
    game->randomState = game->randomState * 1664525UL + 1013904223UL;
    emptyCellsToSkip = (uint8_t)((game->randomState >> 16) %
                                (SNAKE_CAPACITY - game->snakeLength));
    for (candidateCellId = 0; candidateCellId < SNAKE_CAPACITY; candidateCellId++)
    {
        if (!Snake_Occupies(game, candidateCellId))
        {
            if (emptyCellsToSkip == 0)
            {
                game->foodCellId = candidateCellId;
                return;
            }
            emptyCellsToSkip--;
        }
    }
}

void Snake_Init(SnakeGame *game, uint32_t seed)
{
    game->bodyCellIds[0] = 4 * SNAKE_SIDE + 3;
    game->bodyCellIds[1] = 4 * SNAKE_SIDE + 2;
    game->bodyCellIds[2] = 4 * SNAKE_SIDE + 1;
    game->snakeLength = 3;
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
    int nextHeadX;
    int nextHeadY;
    uint8_t nextHeadCellId; // 蛇头下一步将到达的格子编号
    uint8_t eatsFood;       // 下一步是否吃到食物
    uint8_t segmentsToCheck; // 本次需要检查碰撞的蛇身节数
    uint8_t bodySegmentIndex;

    if (game->state != SNAKE_PLAYING)
    {
        return;
    }

    game->direction = game->nextDirection;
    game->turnQueued = 0;
    nextHeadX = game->bodyCellIds[0] % SNAKE_SIDE;
    nextHeadY = game->bodyCellIds[0] / SNAKE_SIDE;
    switch (game->direction)
    {
        case SNAKE_UP:    nextHeadY--; break;
        case SNAKE_RIGHT: nextHeadX++; break;
        case SNAKE_DOWN:  nextHeadY++; break;
        case SNAKE_LEFT:  nextHeadX--; break;
    }

    /* Opposite edges connect; collision checks use the wrapped destination. */
    nextHeadX = (nextHeadX + SNAKE_SIDE) % SNAKE_SIDE;
    nextHeadY = (nextHeadY + SNAKE_SIDE) % SNAKE_SIDE;

    nextHeadCellId = (uint8_t)(nextHeadY * SNAKE_SIDE + nextHeadX);
    eatsFood = (nextHeadCellId == game->foodCellId);
    /* The old tail leaves this tick unless food is eaten. */
    segmentsToCheck = eatsFood ? game->snakeLength : (uint8_t)(game->snakeLength - 1);
    for (bodySegmentIndex = 0; bodySegmentIndex < segmentsToCheck; bodySegmentIndex++)
    {
        if (game->bodyCellIds[bodySegmentIndex] == nextHeadCellId)
        {
            game->state = SNAKE_LOST;
            return;
        }
    }

    if (eatsFood)
    {
        game->snakeLength++;
    }
    for (bodySegmentIndex = (uint8_t)(game->snakeLength - 1);
         bodySegmentIndex > 0; bodySegmentIndex--)
    {
        game->bodyCellIds[bodySegmentIndex] = game->bodyCellIds[bodySegmentIndex - 1];
    }
    game->bodyCellIds[0] = nextHeadCellId;

    if (eatsFood)
    {
        Snake_PlaceFood(game);
    }
}
