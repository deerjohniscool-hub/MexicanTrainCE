#include <graphx.h>
#include <keypadc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define SCREEN_W 320
#define SCREEN_H 240

#define MAX_PLAYERS 8
#define MIN_PLAYERS 2

#define TILE_COUNT 91
#define MAX_HAND 15

typedef struct
{
    uint8_t a;
    uint8_t b;
} Domino;

typedef struct
{
    Domino hand[MAX_HAND];
    uint8_t count;

    bool cpu;
    bool open;

    uint8_t trainEnd;
    uint16_t score;
} Player;

static Player players[MAX_PLAYERS];
static Domino drawPile[TILE_COUNT];

static uint8_t playerCount = 2;
static uint8_t currentPlayer = 0;

static uint8_t pileCount = 0;

static uint8_t selectedTile = 0;
static uint8_t handScroll = 0;

static uint8_t selectedTarget = 0;
/*
    0 = current player's train
    1 = Mexican Train
    2+ = another player's train
*/

static bool setupMode = true;
static bool roundOver = false;

static uint8_t mexicanEnd = 12;
static bool mexicanOpen = true;

static bool doublePending = false;
static uint8_t doublePlayer = 0;

static uint8_t roundNumber = 1;


/* ========================================================= */
/* COLORS                                                     */
/* ========================================================= */

#define COLOR_BG        0
#define COLOR_PANEL     1
#define COLOR_PANEL2    2
#define COLOR_WHITE     3
#define COLOR_TEXT      4
#define COLOR_YELLOW    5
#define COLOR_BLACK     6
#define COLOR_BORDER    7

#define COLOR_P1        8
#define COLOR_P2        9
#define COLOR_P3        10
#define COLOR_P4        11
#define COLOR_P5        12
#define COLOR_P6        13
#define COLOR_P7        14
#define COLOR_P8        15

static const uint16_t gamePalette[] =
{
    gfx_RGBTo1555(14, 18, 27),
    gfx_RGBTo1555(27, 34, 48),
    gfx_RGBTo1555(35, 42, 58),
    gfx_RGBTo1555(245, 245, 245),
    gfx_RGBTo1555(205, 210, 220),
    gfx_RGBTo1555(255, 210, 30),
    gfx_RGBTo1555(10, 10, 10),
    gfx_RGBTo1555(75, 85, 105),

    gfx_RGBTo1555(40, 170, 255),
    gfx_RGBTo1555(255, 80, 80),
    gfx_RGBTo1555(70, 210, 110),
    gfx_RGBTo1555(255, 190, 40),
    gfx_RGBTo1555(190, 100, 255),
    gfx_RGBTo1555(255, 110, 190),
    gfx_RGBTo1555(60, 220, 210),
    gfx_RGBTo1555(240, 240, 240)
};

static const uint8_t playerColors[MAX_PLAYERS] =
{
    COLOR_P1,
    COLOR_P2,
    COLOR_P3,
    COLOR_P4,
    COLOR_P5,
    COLOR_P6,
    COLOR_P7,
    COLOR_P8
};


/* ========================================================= */
/* PALETTE                                                     */
/* ========================================================= */

static void setupPalette(void)
{
    gfx_SetPalette(
        gamePalette,
        sizeof(gamePalette),
        0
    );
}


/* ========================================================= */
/* DELAY                                                       */
/* ========================================================= */

static void wait_ms(unsigned int ms)
{
    volatile unsigned long i;

    for (
        i = 0;
        i < (unsigned long)ms * 3500UL;
        i++
    )
    {
        __asm__("");
    }
}


/* ========================================================= */
/* DRAWING HELPERS                                             */
/* ========================================================= */

static void fillScreenColor(uint8_t color)
{
    gfx_FillScreen(color);
}


static void fillRect(
    int x,
    int y,
    int w,
    int h,
    uint8_t color
)
{
    gfx_SetColor(color);
    gfx_FillRectangle(
        x,
        y,
        w,
        h
    );
}


static void drawFrame(
    int x,
    int y,
    int w,
    int h,
    uint8_t color
)
{
    gfx_SetColor(color);

    gfx_Rectangle(
        x,
        y,
        w,
        h
    );
}


static void drawText(
    int x,
    int y,
    const char *str,
    uint8_t color
)
{
    gfx_SetTextFGColor(color);
    gfx_SetTextXY(x, y);
    gfx_PrintString(str);
}


static void drawCenteredText(
    int y,
    const char *str,
    uint8_t color
)
{
    int width;

    width = (int)strlen(str) * 6;

    if (width > SCREEN_W)
        width = SCREEN_W;

    drawText(
        (SCREEN_W - width) / 2,
        y,
        str,
        color
    );
}


static void numberString(
    uint8_t number,
    char *buffer
)
{
    sprintf(
        buffer,
        "%u",
        number
    );
}


/* ========================================================= */
/* DOMINO DRAWING                                              */
/* ========================================================= */

static void drawDomino(
    int x,
    int y,
    int w,
    int h,
    Domino d,
    bool selected
)
{
    char top[4];
    char bottom[4];

    if (selected)
    {
        fillRect(
            x - 2,
            y - 2,
            w + 4,
            h + 4,
            COLOR_YELLOW
        );
    }

    fillRect(
        x,
        y,
        w,
        h,
        COLOR_WHITE
    );

    drawFrame(
        x,
        y,
        w,
        h,
        COLOR_BLACK
    );

    gfx_SetColor(COLOR_BLACK);

    gfx_Line(
        x + 2,
        y + h / 2,
        x + w - 3,
        y + h / 2
    );

    numberString(
        d.a,
        top
    );

    numberString(
        d.b,
        bottom
    );

    drawText(
        x + (w - (int)strlen(top) * 6) / 2,
        y + 4,
        top,
        COLOR_BLACK
    );

    drawText(
        x + (w - (int)strlen(bottom) * 6) / 2,
        y + h / 2 + 4,
        bottom,
        COLOR_BLACK
    );
}


/* ========================================================= */
/* TRAIN MARKER                                                */
/* ========================================================= */

static void drawTrainMarker(
    int x,
    int y,
    uint8_t end,
    uint8_t color,
    bool open,
    uint8_t tileCount
)
{
    char number[4];
    char count[5];

    gfx_SetColor(color);

    gfx_FillCircle(
        x,
        y,
        7
    );

    drawFrame(
        x - 8,
        y - 8,
        16,
        16,
        COLOR_BLACK
    );

    numberString(
        end,
        number
    );

    drawText(
        x - ((int)strlen(number) * 3),
        y - 4,
        number,
        COLOR_BLACK
    );

    if (open)
    {
        drawFrame(
            x - 11,
            y - 11,
            22,
            22,
            COLOR_YELLOW
        );
    }

    sprintf(
        count,
        "%u",
        tileCount
    );

    drawText(
        x - ((int)strlen(count) * 3),
        y + 11,
        count,
        COLOR_TEXT
    );
}


/* ========================================================= */
/* BUILD DOUBLE-12 SET                                         */
/* ========================================================= */

static void buildPile(void)
{
    uint8_t index;
    uint8_t a;
    uint8_t b;

    int i;
    int j;

    index = 0;

    for (
        a = 0;
        a <= 12;
        a++
    )
    {
        for (
            b = a;
            b <= 12;
            b++
        )
        {
            drawPile[index].a = a;
            drawPile[index].b = b;

            index++;
        }
    }

    pileCount = TILE_COUNT;

    /* Shuffle */

    for (
        i = TILE_COUNT - 1;
        i > 0;
        i--
    )
    {
        j = rand() % (i + 1);

        {
            Domino temp;

            temp = drawPile[i];
            drawPile[i] = drawPile[j];
            drawPile[j] = temp;
        }
    }
}


/* ========================================================= */
/* DOMINO HELPERS                                              */
/* ========================================================= */

static bool dominoMatches(
    Domino d,
    uint8_t end
)
{
    return (
        d.a == end ||
        d.b == end
    );
}


static Domino orientDomino(
    Domino d,
    uint8_t end
)
{
    Domino result;

    if (d.a == end)
    {
        result.a = d.a;
        result.b = d.b;
    }
    else
    {
        result.a = d.b;
        result.b = d.a;
    }

    return result;
}


static bool isDouble(Domino d)
{
    return d.a == d.b;
}


/* ========================================================= */
/* DEAL ROUND                                                  */
/* ========================================================= */

static void dealRound(void)
{
    uint8_t i;
    uint8_t j;
    uint8_t handSize;

    buildPile();

    if (playerCount <= 4)
        handSize = 15;
    else if (playerCount <= 6)
        handSize = 12;
    else
        handSize = 10;

    for (
        i = 0;
        i < MAX_PLAYERS;
        i++
    )
    {
        players[i].count = 0;
        players[i].open = false;
        players[i].trainEnd = 12;

        for (
            j = 0;
            j < MAX_HAND;
            j++
        )
        {
            players[i].hand[j].a = 0;
            players[i].hand[j].b = 0;
        }
    }

    for (
        j = 0;
        j < handSize;
        j++
    )
    {
        for (
            i = 0;
            i < playerCount;
            i++
        )
        {
            if (pileCount > 0)
            {
                pileCount--;

                players[i].hand[
                    players[i].count
                ] = drawPile[pileCount];

                players[i].count++;
            }
        }
    }

    currentPlayer = 0;

    selectedTile = 0;
    handScroll = 0;
    selectedTarget = 0;

    mexicanEnd = 12;
    mexicanOpen = true;

    doublePending = false;
    doublePlayer = 0;

    roundOver = false;
}


/* ========================================================= */
/* TARGET CHECKING                                             */
/* ========================================================= */

static bool canPlayOnTarget(
    Domino d,
    uint8_t target,
    uint8_t who
)
{
    uint8_t otherPlayer;

    /* Own train */

    if (target == 0)
    {
        return dominoMatches(
            d,
            players[who].trainEnd
        );
    }

    /* Mexican train */

    if (target == 1)
    {
        if (!mexicanOpen)
            return false;

        return dominoMatches(
            d,
            mexicanEnd
        );
    }

    /* Another player's train */

    otherPlayer = target - 2;

    if (otherPlayer >= playerCount)
        return false;

    if (otherPlayer == who)
        return false;

    if (!players[otherPlayer].open)
        return false;

    return dominoMatches(
        d,
        players[otherPlayer].trainEnd
    );
}


/* ========================================================= */
/* PLAYER MOVE CHECK                                           */
/* ========================================================= */

static bool playerHasMove(
    uint8_t who
)
{
    uint8_t i;
    uint8_t t;

    for (
        i = 0;
        i < players[who].count;
        i++
    )
    {
        for (
            t = 0;
            t < playerCount + 2;
            t++
        )
        {
            if (
                canPlayOnTarget(
                    players[who].hand[i],
                    t,
                    who
                )
            )
            {
                return true;
            }
        }
    }

    return false;
}


/* ========================================================= */
/* REMOVE DOMINO                                               */
/* ========================================================= */

static void removeFromHand(
    uint8_t who,
    uint8_t index
)
{
    uint8_t i;

    for (
        i = index;
        i + 1 < players[who].count;
        i++
    )
    {
        players[who].hand[i] =
            players[who].hand[i + 1];
    }

    if (players[who].count > 0)
        players[who].count--;

    if (
        players[who].count == 0 ||
        selectedTile >= players[who].count
    )
    {
        selectedTile =
            players[who].count > 0
                ? players[who].count - 1
                : 0;
    }
}


/* ========================================================= */
/* PLAY DOMINO                                                 */
/* ========================================================= */

static void playDomino(
    uint8_t who,
    uint8_t index,
    uint8_t target
)
{
    Domino d;
    uint8_t endValue;

    d = players[who].hand[index];

    if (target == 0)
    {
        endValue =
            players[who].trainEnd;
    }
    else if (target == 1)
    {
        endValue =
            mexicanEnd;
    }
    else
    {
        endValue =
            players[target - 2].trainEnd;
    }

    d = orientDomino(
        d,
        endValue
    );

    if (target == 0)
    {
        players[who].trainEnd = d.b;
        players[who].open = false;
    }
    else if (target == 1)
    {
        mexicanEnd = d.b;
        mexicanOpen = true;
    }
    else
    {
        players[target - 2].trainEnd = d.b;
        players[target - 2].open = false;
    }

    removeFromHand(
        who,
        index
    );

    if (isDouble(d))
    {
        doublePending = true;
        doublePlayer = who;
    }
    else if (
        doublePending &&
        who == doublePlayer
    )
    {
        doublePending = false;
    }

    if (players[who].count == 0)
    {
        roundOver = true;
        return;
    }

    currentPlayer =
        (currentPlayer + 1) % playerCount;

    selectedTile = 0;
    handScroll = 0;
    selectedTarget = 0;

    wait_ms(70);
}


/* ========================================================= */
/* DRAW TILE                                                   */
/* ========================================================= */

static bool drawTileFromPile(
    uint8_t who
)
{
    if (
        pileCount == 0 ||
        players[who].count >= MAX_HAND
    )
    {
        return false;
    }

    pileCount--;

    players[who].hand[
        players[who].count
    ] = drawPile[pileCount];

    players[who].count++;

    return true;
}


/* ========================================================= */
/* CPU TURN                                                    */
/* ========================================================= */

static void cpuTurn(void)
{
    uint8_t i;
    uint8_t t;

    /* Look for a playable double first */

    for (
        i = 0;
        i < players[currentPlayer].count;
        i++
    )
    {
        if (
            isDouble(
                players[currentPlayer].hand[i]
            )
        )
        {
            for (
                t = 0;
                t < playerCount + 2;
                t++
            )
            {
                if (
                    canPlayOnTarget(
                        players[currentPlayer].hand[i],
                        t,
                        currentPlayer
                    )
                )
                {
                    playDomino(
                        currentPlayer,
                        i,
                        t
                    );

                    return;
                }
            }
        }
    }

    /* Then any playable tile */

    for (
        i = 0;
        i < players[currentPlayer].count;
        i++
    )
    {
        for (
            t = 0;
            t < playerCount + 2;
            t++
        )
        {
            if (
                canPlayOnTarget(
                    players[currentPlayer].hand[i],
                    t,
                    currentPlayer
                )
            )
            {
                playDomino(
                    currentPlayer,
                    i,
                    t
                );

                return;
            }
        }
    }

    /* Draw */

    if (
        drawTileFromPile(currentPlayer)
    )
    {
        for (
            i = 0;
            i < players[currentPlayer].count;
            i++
        )
        {
            for (
                t = 0;
                t < playerCount + 2;
                t++
            )
            {
                if (
                    canPlayOnTarget(
                        players[currentPlayer].hand[i],
                        t,
                        currentPlayer
                    )
                )
                {
                    playDomino(
                        currentPlayer,
                        i,
                        t
                    );

                    return;
                }
            }
        }
    }

    /* No move */

    players[currentPlayer].open = true;

    currentPlayer =
        (currentPlayer + 1) % playerCount;

    selectedTile = 0;
    handScroll = 0;
    selectedTarget = 0;
}


/* ========================================================= */
/* SETUP SCREEN                                                */
/* ========================================================= */

static void drawSetupScreen(void)
{
    uint8_t i;
    int y;

    char buffer[32];

    fillScreenColor(COLOR_BG);

    drawCenteredText(
        7,
        "MEXICAN TRAIN",
        COLOR_YELLOW
    );

    drawCenteredText(
        21,
        "DOUBLE-12",
        COLOR_TEXT
    );

    drawText(
        18,
        43,
        "PLAYERS:",
        COLOR_WHITE
    );

    sprintf(
        buffer,
        "%u",
        playerCount
    );

    fillRect(
        92,
        39,
        42,
        18,
        COLOR_PANEL2
    );

    drawFrame(
        92,
        39,
        42,
        18,
        COLOR_BORDER
    );

    drawCenteredText(
        44,
        buffer,
        COLOR_WHITE
    );

    drawText(
        145,
        43,
        "TRACE +  GRAPHVAR -",
        COLOR_TEXT
    );

    for (
        i = 0;
        i < playerCount;
        i++
    )
    {
        y = 66 + i * 20;

        sprintf(
            buffer,
            "PLAYER %u",
            i + 1
        );

        drawText(
            18,
            y,
            buffer,
            playerColors[i]
        );

        fillRect(
            102,
            y - 3,
            68,
            17,
            COLOR_PANEL2
        );

        drawFrame(
            102,
            y - 3,
            68,
            17,
            playerColors[i]
        );

        if (players[i].cpu)
        {
            drawText(
                118,
                y,
                "CPU",
                COLOR_WHITE
            );
        }
        else
        {
            drawText(
                112,
                y,
                "HUMAN",
                COLOR_WHITE
            );
        }

        if (i == currentPlayer)
        {
            drawText(
                180,
                y,
                "< SELECTED",
                COLOR_YELLOW
            );
        }
    }

    drawText(
        12,
        231,
        "UP/DOWN PLAYER",
        COLOR_TEXT
    );

    drawText(
        120,
        231,
        "L/R HUMAN/CPU",
        COLOR_TEXT
    );

    drawText(
        230,
        231,
        "2ND START",
        COLOR_TEXT
    );
}


/* ========================================================= */
/* SETUP CONTROLS                                              */
/* ========================================================= */

static void setupControls(void)
{
    kb_Scan();

    /* Player selection */

    if (kb_IsDown(kb_Up))
    {
        if (currentPlayer > 0)
            currentPlayer--;

        wait_ms(120);
    }

    if (kb_IsDown(kb_Down))
    {
        if (
            currentPlayer + 1 <
            playerCount
        )
        {
            currentPlayer++;
        }

        wait_ms(120);
    }

    /* Toggle HUMAN / CPU */

    if (
        kb_IsDown(kb_Left) ||
        kb_IsDown(kb_Right)
    )
    {
        players[currentPlayer].cpu =
            !players[currentPlayer].cpu;

        wait_ms(160);
    }

    /* TRACE adds player */

    if (kb_IsDown(kb_Trace))
    {
        if (playerCount < MAX_PLAYERS)
        {
            playerCount++;

            players[playerCount - 1].cpu = true;
            players[playerCount - 1].open = false;
            players[playerCount - 1].trainEnd = 12;
            players[playerCount - 1].score = 0;
        }

        wait_ms(160);
    }

    /* GRAPHVAR removes player */

    if (kb_IsDown(kb_GraphVar))
    {
        if (playerCount > MIN_PLAYERS)
        {
            playerCount--;

            if (
                currentPlayer >= playerCount
            )
            {
                currentPlayer =
                    playerCount - 1;
            }
        }

        wait_ms(160);
    }

    /* START */

    if (kb_IsDown(kb_2nd))
    {
        setupMode = false;
        roundOver = false;
        currentPlayer = 0;

        dealRound();

        wait_ms(200);
    }
}


/* ========================================================= */
/* GAME SCREEN                                                 */
/* ========================================================= */

static void drawGameScreen(void)
{
    uint8_t i;
    uint8_t shown;
    uint8_t visible;
    uint8_t index;

    int x;
    int y;

    char buffer[64];

    fillScreenColor(COLOR_BG);

    /* Header */

    fillRect(
        0,
        0,
        SCREEN_W,
        30,
        COLOR_PANEL
    );

    sprintf(
        buffer,
        "PLAYER %u'S TURN",
        currentPlayer + 1
    );

    drawText(
        7,
        8,
        buffer,
        playerColors[currentPlayer]
    );

    sprintf(
        buffer,
        "PILE %u",
        pileCount
    );

    drawText(
        118,
        8,
        buffer,
        COLOR_TEXT
    );

    sprintf(
        buffer,
        "ROUND %u",
        roundNumber
    );

    drawText(
        238,
        8,
        buffer,
        COLOR_TEXT
    );

    /* Board */

    fillRect(
        8,
        36,
        304,
        108,
        COLOR_PANEL
    );

    drawFrame(
        8,
        36,
        304,
        108,
        COLOR_BORDER
    );

    drawText(
        15,
        41,
        "TRAINS",
        COLOR_TEXT
    );

    /* Mexican Train */

    drawText(
        16,
        61,
        "MEX",
        COLOR_YELLOW
    );

    drawTrainMarker(
        64,
        69,
        mexicanEnd,
        COLOR_YELLOW,
        mexicanOpen,
        0
    );

    /* Opponents */

    shown = 0;

    for (
        i = 0;
        i < playerCount;
        i++
    )
    {
        if (i == currentPlayer)
            continue;

        x =
            124 +
            (shown % 3) * 61;

        y =
            67 +
            (shown / 3) * 34;

        sprintf(
            buffer,
            "P%u",
            i + 1
        );

        drawText(
            x - 8,
            y - 22,
            buffer,
            playerColors[i]
        );

        drawTrainMarker(
            x,
            y,
            players[i].trainEnd,
            playerColors[i],
            players[i].open,
            players[i].count
        );

        shown++;
    }

    /* Current player's train */

    drawText(
        16,
        110,
        "YOU",
        playerColors[currentPlayer]
    );

    drawTrainMarker(
        64,
        119,
        players[currentPlayer].trainEnd,
        playerColors[currentPlayer],
        players[currentPlayer].open,
        players[currentPlayer].count
    );

    /* Target panel */

    fillRect(
        8,
        148,
        304,
        20,
        COLOR_PANEL
    );

    if (doublePending)
    {
        sprintf(
            buffer,
            "DOUBLE: PLAYER %u MUST PLAY",
            doublePlayer + 1
        );

        drawText(
            13,
            154,
            buffer,
            COLOR_YELLOW
        );
    }
    else if (selectedTarget == 0)
    {
        drawText(
            13,
            154,
            "TARGET: YOUR TRAIN",
            COLOR_WHITE
        );
    }
    else if (selectedTarget == 1)
    {
        drawText(
            13,
            154,
            "TARGET: MEXICAN TRAIN",
            COLOR_WHITE
        );
    }
    else
    {
        sprintf(
            buffer,
            "TARGET: PLAYER %u TRAIN",
            selectedTarget - 1
        );

        drawText(
            13,
            154,
            buffer,
            COLOR_WHITE
        );
    }

    /* Hand */

    fillRect(
        8,
        171,
        304,
        50,
        COLOR_PANEL
    );

    drawText(
        12,
        174,
        "YOUR HAND",
        playerColors[currentPlayer]
    );

    visible = 6;

    for (
        i = 0;
        i < visible;
        i++
    )
    {
        index =
            handScroll + i;

        if (
            index >=
            players[currentPlayer].count
        )
        {
            break;
        }

        x =
            13 +
            i * 49;

        drawDomino(
            x,
            184,
            39,
            32,
            players[currentPlayer].hand[index],
            index == selectedTile
        );
    }

    if (handScroll > 0)
    {
        drawText(
            2,
            202,
            "<",
            COLOR_YELLOW
        );
    }

    if (
        handScroll + visible <
        players[currentPlayer].count
    )
    {
        drawText(
            309,
            202,
            ">",
            COLOR_YELLOW
        );
    }

    /* Controls */

    drawText(
        7,
        226,
        "L/R TILE",
        COLOR_TEXT
    );

    drawText(
        69,
        226,
        "U/D TARGET",
        COLOR_TEXT
    );

    drawText(
        153,
        226,
        "2ND PLAY",
        COLOR_TEXT
    );

    drawText(
        220,
        226,
        "GRAPH DRAW",
        COLOR_TEXT
    );

    drawText(
        7,
        236,
        "CLEAR PASS",
        COLOR_TEXT
    );
}


/* ========================================================= */
/* GAME CONTROLS                                               */
/* ========================================================= */

static void gameControls(void)
{
    kb_Scan();

    /* CPU turn */

    if (players[currentPlayer].cpu)
    {
        cpuTurn();
        return;
    }

    /* Previous tile */

    if (kb_IsDown(kb_Left))
    {
        if (selectedTile > 0)
            selectedTile--;

        if (
            selectedTile <
            handScroll
        )
        {
            handScroll =
                selectedTile;
        }

        wait_ms(110);
    }

    /* Next tile */

    if (kb_IsDown(kb_Right))
    {
        if (
            selectedTile + 1 <
            players[currentPlayer].count
        )
        {
            selectedTile++;
        }

        if (
            selectedTile >=
            handScroll + 6
        )
        {
            handScroll =
                selectedTile - 5;
        }

        wait_ms(110);
    }

    /* Previous target */

    if (kb_IsDown(kb_Up))
    {
        if (selectedTarget > 0)
        {
            selectedTarget--;
        }
        else
        {
            selectedTarget =
                playerCount + 1;
        }

        wait_ms(110);
    }

    /* Next target */

    if (kb_IsDown(kb_Down))
    {
        if (
            selectedTarget <
            playerCount + 1
        )
        {
            selectedTarget++;
        }
        else
        {
            selectedTarget = 0;
        }

        wait_ms(110);
    }

    /* Draw */

    if (kb_IsDown(kb_Graph))
    {
        drawTileFromPile(
            currentPlayer
        );

        wait_ms(150);
    }

    /* Play */

    if (
        kb_IsDown(kb_2nd) &&
        players[currentPlayer].count > 0
    )
    {
        if (
            canPlayOnTarget(
                players[currentPlayer].hand[selectedTile],
                selectedTarget,
                currentPlayer
            )
        )
        {
            playDomino(
                currentPlayer,
                selectedTile,
                selectedTarget
            );
        }

        wait_ms(130);
    }

    /* Pass */

    if (kb_IsDown(kb_Clear))
    {
        if (
            !playerHasMove(
                currentPlayer
            )
        )
        {
            players[currentPlayer].open = true;

            currentPlayer =
                (currentPlayer + 1) %
                playerCount;

            selectedTile = 0;
            handScroll = 0;
            selectedTarget = 0;
        }

        wait_ms(130);
    }
}


/* ========================================================= */
/* SCORE SCREEN                                                */
/* ========================================================= */

static void drawScoreScreen(void)
{
    uint8_t i;

    char buffer[48];

    fillScreenColor(COLOR_BG);

    drawCenteredText(
        12,
        "ROUND COMPLETE",
        COLOR_YELLOW
    );

    for (
        i = 0;
        i < playerCount;
        i++
    )
    {
        sprintf(
            buffer,
            "PLAYER %u    TILES: %u",
            i + 1,
            players[i].count
        );

        drawText(
            35,
            42 + i * 22,
            buffer,
            playerColors[i]
        );
    }

    drawCenteredText(
        220,
        "2ND NEW ROUND",
        COLOR_TEXT
    );

    drawCenteredText(
        232,
        "CLEAR SETUP",
        COLOR_TEXT
    );
}


/* ========================================================= */
/* SCORE CONTROLS                                              */
/* ========================================================= */

static void scoreControls(void)
{
    kb_Scan();

    if (kb_IsDown(kb_2nd))
    {
        roundNumber++;

        roundOver = false;

        dealRound();

        wait_ms(180);
    }

    if (kb_IsDown(kb_Clear))
    {
        setupMode = true;
        roundOver = false;
        currentPlayer = 0;

        wait_ms(180);
    }
}


/* ========================================================= */
/* MAIN                                                         */
/* ========================================================= */

int main(void)
{
    uint8_t i;

    gfx_Begin();

    setupPalette();

    gfx_SetDrawBuffer();

    srand(12345);

    for (
        i = 0;
        i < MAX_PLAYERS;
        i++
    )
    {
        players[i].cpu = true;
        players[i].open = false;
        players[i].trainEnd = 12;
        players[i].score = 0;
        players[i].count = 0;
    }

    /*
        Player 1 starts HUMAN.
        Everyone else starts CPU.
    */

    players[0].cpu = false;

    while (1)
    {
        if (setupMode)
        {
            drawSetupScreen();

            gfx_SwapDraw();

            setupControls();
        }
        else if (roundOver)
        {
            drawScoreScreen();

            gfx_SwapDraw();

            scoreControls();
        }
        else
        {
            /*
                Check for an empty hand.
            */

            for (
                i = 0;
                i < playerCount;
                i++
            )
            {
                if (
                    players[i].count == 0
                )
                {
                    roundOver = true;
                    break;
                }
            }

            if (!roundOver)
            {
                drawGameScreen();

                gfx_SwapDraw();

                gameControls();
            }
        }
    }

    gfx_End();

    return 0;
}
