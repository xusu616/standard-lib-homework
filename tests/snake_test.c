#include "snake.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void AssertValid(const SnakeGame *game)
{
    unsigned i;
    unsigned j;

    assert(game->snakeLength >= 3 && game->snakeLength <= SNAKE_CAPACITY);
    for (i = 0; i < game->snakeLength; i++)
    {
        assert(game->bodyCellIds[i] < SNAKE_CAPACITY);
        for (j = i + 1; j < game->snakeLength; j++)
        {
            assert(game->bodyCellIds[i] != game->bodyCellIds[j]);
        }
        if (game->state != SNAKE_WON)
        {
            assert(game->foodCellId != game->bodyCellIds[i]);
        }
    }
    if (game->state == SNAKE_WON)
    {
        assert(game->snakeLength == SNAKE_CAPACITY);
        assert(game->foodCellId == SNAKE_NO_FOOD);
    }
    else
    {
        assert(game->foodCellId < SNAKE_CAPACITY);
    }
}

static void TestGrowthAndControls(void)
{
    SnakeGame game;
    SnakeGame snapshot;

    Snake_Init(&game, 123);
    AssertValid(&game);
    assert(game.state == SNAKE_READY);
    snapshot = game;
    Snake_Step(&game);
    assert(memcmp(&snapshot, &game, sizeof(game)) == 0);

    Snake_TogglePlay(&game);
    game.foodCellId = 36;
    Snake_Step(&game);
    assert(game.snakeLength == 4 && game.bodyCellIds[0] == 36);
    assert(game.bodyCellIds[1] == 35 && game.bodyCellIds[3] == 33);
    AssertValid(&game);

    Snake_Turn(&game, 0);
    Snake_Turn(&game, 0);
    assert(game.nextDirection == SNAKE_UP);
    Snake_TogglePlay(&game);
    snapshot = game;
    Snake_Step(&game);
    Snake_Turn(&game, 1);
    assert(memcmp(&snapshot, &game, sizeof(game)) == 0);
    Snake_TogglePlay(&game);
    game.foodCellId = 63;
    Snake_Step(&game);
    assert(game.bodyCellIds[0] == 28 && game.direction == SNAKE_UP);
    Snake_Turn(&game, 1);
    Snake_Step(&game);
    assert(game.bodyCellIds[0] == 29 && game.direction == SNAKE_RIGHT);
    AssertValid(&game);
}

static void TestCollisions(void)
{
    static const uint8_t tailLoop[] = {9, 8, 0, 1};
    static const uint8_t bodyLoop[] = {9, 8, 0, 1, 2, 10};
    SnakeGame game;

    Snake_Init(&game, 1);
    memcpy(game.bodyCellIds, tailLoop, sizeof(tailLoop));
    game.snakeLength = 4;
    game.direction = game.nextDirection = SNAKE_UP;
    game.foodCellId = 63;
    game.state = SNAKE_PLAYING;
    Snake_Step(&game);
    assert(game.state == SNAKE_PLAYING && game.bodyCellIds[0] == 1);
    AssertValid(&game);

    memcpy(game.bodyCellIds, bodyLoop, sizeof(bodyLoop));
    game.snakeLength = 6;
    Snake_Step(&game);
    assert(game.state == SNAKE_LOST);
    Snake_TogglePlay(&game);
    assert(game.state == SNAKE_PLAYING && game.snakeLength == 3);
    AssertValid(&game);
}

static void TestWrap(void)
{
    static const uint8_t wrappedBody[] = {7, 6, 14, 15, 8, 0, 1};
    SnakeGame game;
    unsigned direction;
    unsigned position;
    uint8_t target = 0;

    /* Every exit cell on all four edges must preserve the other coordinate. */
    for (direction = SNAKE_UP; direction <= SNAKE_LEFT; direction++)
    {
        for (position = 0; position < 8; position++)
        {
            Snake_Init(&game, position);
            switch (direction)
            {
                case SNAKE_UP:
                    game.bodyCellIds[0] = (uint8_t)position;
                    game.bodyCellIds[1] = (uint8_t)(position + 8);
                    game.bodyCellIds[2] = (uint8_t)(position + 16);
                    target = (uint8_t)(56 + position);
                    break;
                case SNAKE_RIGHT:
                    game.bodyCellIds[0] = (uint8_t)(position * 8 + 7);
                    game.bodyCellIds[1] = (uint8_t)(position * 8 + 6);
                    game.bodyCellIds[2] = (uint8_t)(position * 8 + 5);
                    target = (uint8_t)(position * 8);
                    break;
                case SNAKE_DOWN:
                    game.bodyCellIds[0] = (uint8_t)(56 + position);
                    game.bodyCellIds[1] = (uint8_t)(48 + position);
                    game.bodyCellIds[2] = (uint8_t)(40 + position);
                    target = (uint8_t)position;
                    break;
                case SNAKE_LEFT:
                    game.bodyCellIds[0] = (uint8_t)(position * 8);
                    game.bodyCellIds[1] = (uint8_t)(position * 8 + 1);
                    game.bodyCellIds[2] = (uint8_t)(position * 8 + 2);
                    target = (uint8_t)(position * 8 + 7);
                    break;
            }
            game.foodCellId = (uint8_t)((target + 1) % 64);
            game.direction = game.nextDirection = (SnakeDirection)direction;
            game.state = SNAKE_PLAYING;
            Snake_Step(&game);
            assert(game.state == SNAKE_PLAYING && game.bodyCellIds[0] == target);
            assert(game.snakeLength == 3);
            AssertValid(&game);
        }
    }

    Snake_Init(&game, 1);
    game.bodyCellIds[0] = 7;
    game.bodyCellIds[1] = 6;
    game.bodyCellIds[2] = 5;
    game.foodCellId = 0;
    game.state = SNAKE_PLAYING;
    Snake_Step(&game);
    assert(game.snakeLength == 4 && game.bodyCellIds[0] == 0);
    AssertValid(&game);

    /* Wrapping into a body segment still kills; the departing tail is legal. */
    Snake_Init(&game, 1);
    memcpy(game.bodyCellIds, wrappedBody, sizeof(wrappedBody));
    game.snakeLength = 7;
    game.foodCellId = 63;
    game.state = SNAKE_PLAYING;
    Snake_Step(&game);
    assert(game.state == SNAKE_LOST);
    game.snakeLength = 6;
    game.state = SNAKE_PLAYING;
    Snake_Step(&game);
    assert(game.state == SNAKE_PLAYING && game.bodyCellIds[0] == 0);
    AssertValid(&game);

    /* Repeated straight movement must continue through several complete wraps. */
    Snake_Init(&game, 1);
    game.foodCellId = 0;
    game.state = SNAKE_PLAYING;
    for (position = 0; position < 40; position++)
    {
        Snake_Step(&game);
        assert(game.state == SNAKE_PLAYING);
        assert(game.bodyCellIds[0] == 32 + (3 + position + 1) % 8);
        AssertValid(&game);
    }
}

static void TestWin(void)
{
    SnakeGame game;
    uint8_t path[64];
    unsigned x;
    unsigned y;
    unsigned i;

    Snake_Init(&game, 1);
    for (y = 0; y < 8; y++)
    {
        for (x = 0; x < 8; x++)
        {
            path[y * 8 + x] = (uint8_t)(y * 8 + (y % 2 ? 7 - x : x));
        }
    }
    for (i = 0; i < 62; i++)
    {
        game.bodyCellIds[i] = path[61 - i];
    }
    game.snakeLength = 62;
    game.foodCellId = path[62];
    game.direction = game.nextDirection = SNAKE_LEFT;
    game.state = SNAKE_PLAYING;
    AssertValid(&game);
    Snake_Step(&game);
    assert(game.snakeLength == 63 && game.foodCellId == path[63]);
    AssertValid(&game);
    Snake_Step(&game);
    assert(game.state == SNAKE_WON);
    AssertValid(&game);
    Snake_Step(&game);
    AssertValid(&game);
    Snake_TogglePlay(&game);
    assert(game.state == SNAKE_PLAYING && game.snakeLength == 3);
    AssertValid(&game);
}

static void TestManyGames(void)
{
    SnakeGame game;
    unsigned seed;
    unsigned tick;

    for (seed = 0; seed < 256; seed++)
    {
        Snake_Init(&game, seed);
        Snake_TogglePlay(&game);
        /* A clockwise perimeter loop repeatedly exercises growth and food placement. */
        for (tick = 0; tick < 1000 && game.state == SNAKE_PLAYING; tick++)
        {
            unsigned x = game.bodyCellIds[0] % 8;
            unsigned y = game.bodyCellIds[0] / 8;
            if ((game.direction == SNAKE_RIGHT && x == 7) ||
                (game.direction == SNAKE_DOWN && y == 7) ||
                (game.direction == SNAKE_LEFT && x == 0) ||
                (game.direction == SNAKE_UP && y == 0))
            {
                Snake_Turn(&game, 1);
            }
            Snake_Step(&game);
            AssertValid(&game);
        }
    }
}

int main(void)
{
    TestGrowthAndControls();
    TestCollisions();
    TestWrap();
    TestWin();
    TestManyGames();
    puts("Snake tests passed: four-edge wrap, wrapped food/body/tail, growth, controls, pause, restart, win.");
    return 0;
}
