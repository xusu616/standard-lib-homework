#include "snake.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void AssertValid(const SnakeGame *game)
{
    unsigned i;
    unsigned j;

    assert(game->length >= 3 && game->length <= SNAKE_CAPACITY);
    for (i = 0; i < game->length; i++)
    {
        assert(game->cells[i] < SNAKE_CAPACITY);
        for (j = i + 1; j < game->length; j++)
        {
            assert(game->cells[i] != game->cells[j]);
        }
        if (game->state != SNAKE_WON)
        {
            assert(game->food != game->cells[i]);
        }
    }
    if (game->state == SNAKE_WON)
    {
        assert(game->length == SNAKE_CAPACITY);
        assert(game->food == SNAKE_NO_FOOD);
    }
    else
    {
        assert(game->food < SNAKE_CAPACITY);
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
    game.food = 36;
    Snake_Step(&game);
    assert(game.length == 4 && game.cells[0] == 36);
    assert(game.cells[1] == 35 && game.cells[3] == 33);
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
    game.food = 63;
    Snake_Step(&game);
    assert(game.cells[0] == 28 && game.direction == SNAKE_UP);
    Snake_Turn(&game, 1);
    Snake_Step(&game);
    assert(game.cells[0] == 29 && game.direction == SNAKE_RIGHT);
    AssertValid(&game);
}

static void TestCollisions(void)
{
    static const uint8_t tailLoop[] = {9, 8, 0, 1};
    static const uint8_t bodyLoop[] = {9, 8, 0, 1, 2, 10};
    SnakeGame game;

    Snake_Init(&game, 1);
    memcpy(game.cells, tailLoop, sizeof(tailLoop));
    game.length = 4;
    game.direction = game.nextDirection = SNAKE_UP;
    game.food = 63;
    game.state = SNAKE_PLAYING;
    Snake_Step(&game);
    assert(game.state == SNAKE_PLAYING && game.cells[0] == 1);
    AssertValid(&game);

    memcpy(game.cells, bodyLoop, sizeof(bodyLoop));
    game.length = 6;
    Snake_Step(&game);
    assert(game.state == SNAKE_LOST);
    Snake_TogglePlay(&game);
    assert(game.state == SNAKE_PLAYING && game.length == 3);
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
                    game.cells[0] = (uint8_t)position;
                    game.cells[1] = (uint8_t)(position + 8);
                    game.cells[2] = (uint8_t)(position + 16);
                    target = (uint8_t)(56 + position);
                    break;
                case SNAKE_RIGHT:
                    game.cells[0] = (uint8_t)(position * 8 + 7);
                    game.cells[1] = (uint8_t)(position * 8 + 6);
                    game.cells[2] = (uint8_t)(position * 8 + 5);
                    target = (uint8_t)(position * 8);
                    break;
                case SNAKE_DOWN:
                    game.cells[0] = (uint8_t)(56 + position);
                    game.cells[1] = (uint8_t)(48 + position);
                    game.cells[2] = (uint8_t)(40 + position);
                    target = (uint8_t)position;
                    break;
                case SNAKE_LEFT:
                    game.cells[0] = (uint8_t)(position * 8);
                    game.cells[1] = (uint8_t)(position * 8 + 1);
                    game.cells[2] = (uint8_t)(position * 8 + 2);
                    target = (uint8_t)(position * 8 + 7);
                    break;
            }
            game.food = (uint8_t)((target + 1) % 64);
            game.direction = game.nextDirection = (SnakeDirection)direction;
            game.state = SNAKE_PLAYING;
            Snake_Step(&game);
            assert(game.state == SNAKE_PLAYING && game.cells[0] == target);
            assert(game.length == 3);
            AssertValid(&game);
        }
    }

    Snake_Init(&game, 1);
    game.cells[0] = 7;
    game.cells[1] = 6;
    game.cells[2] = 5;
    game.food = 0;
    game.state = SNAKE_PLAYING;
    Snake_Step(&game);
    assert(game.length == 4 && game.cells[0] == 0);
    AssertValid(&game);

    /* Wrapping into a body segment still kills; the departing tail is legal. */
    Snake_Init(&game, 1);
    memcpy(game.cells, wrappedBody, sizeof(wrappedBody));
    game.length = 7;
    game.food = 63;
    game.state = SNAKE_PLAYING;
    Snake_Step(&game);
    assert(game.state == SNAKE_LOST);
    game.length = 6;
    game.state = SNAKE_PLAYING;
    Snake_Step(&game);
    assert(game.state == SNAKE_PLAYING && game.cells[0] == 0);
    AssertValid(&game);

    /* Repeated straight movement must continue through several complete wraps. */
    Snake_Init(&game, 1);
    game.food = 0;
    game.state = SNAKE_PLAYING;
    for (position = 0; position < 40; position++)
    {
        Snake_Step(&game);
        assert(game.state == SNAKE_PLAYING);
        assert(game.cells[0] == 32 + (3 + position + 1) % 8);
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
        game.cells[i] = path[61 - i];
    }
    game.length = 62;
    game.food = path[62];
    game.direction = game.nextDirection = SNAKE_LEFT;
    game.state = SNAKE_PLAYING;
    AssertValid(&game);
    Snake_Step(&game);
    assert(game.length == 63 && game.food == path[63]);
    AssertValid(&game);
    Snake_Step(&game);
    assert(game.state == SNAKE_WON);
    AssertValid(&game);
    Snake_Step(&game);
    AssertValid(&game);
    Snake_TogglePlay(&game);
    assert(game.state == SNAKE_PLAYING && game.length == 3);
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
            unsigned x = game.cells[0] % 8;
            unsigned y = game.cells[0] / 8;
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
