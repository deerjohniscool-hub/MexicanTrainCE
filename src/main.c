#include <graphx.h>
#include <keypadc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PLAYERS 8
#define MAX_HAND 15
#define DOMINOES 91
#define SCREEN_W 320
#define SCREEN_H 240

typedef struct {
    uint8_t a, b;
} Domino;

typedef struct {
    Domino hand[MAX_HAND];
    uint8_t count;
    bool cpu;
    bool open;
    int8_t train_end;
    uint16_t score;
} Player;

typedef enum {
    SCREEN_SETUP,
    SCREEN_GAME,
    SCREEN_SCORES
} Screen;

static Player players[MAX_PLAYERS];
static Domino pile[DOMINOES];
static uint8_t pile_count;
static uint8_t player_count = 4;
static uint8_t setup_cursor = 0;
static uint8_t setup_field = 0;
static uint8_t turn = 0;
static uint8_t selected = 0;
static uint8_t target = 0;
static uint8_t round_no = 1;
static uint8_t winner = 255;
static uint8_t mexican_end = 12;
static bool mexican_open = false;
static bool forced_double = false;
static uint8_t forced_player = 0;
static uint8_t forced_train = 0;
static bool drawn_this_turn = false;
static Screen screen = SCREEN_SETUP;
static uint32_t rng_state = 0xA53C9E71u;
static uint8_t anim_kind = 0;
static uint8_t anim_frame = 0;
static uint8_t anim_max = 0;
static uint8_t message_timer = 0;
static char message[40] = "";

static const uint16_t COLORS[MAX_PLAYERS] = {
    gfx_RGBTo1555(40,120,255),
    gfx_RGBTo1555(235,65,65),
    gfx_RGBTo1555(50,205,105),
    gfx_RGBTo1555(245,155,45),
    gfx_RGBTo1555(165,85,230),
    gfx_RGBTo1555(35,200,205),
    gfx_RGBTo1555(245,215,55),
    gfx_RGBTo1555(235,90,175)
};

static uint32_t rnd(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static void wait_release(void) {
    while (kb_AnyKey()) kb_Scan();
}

static bool pressed(kb_lkey_t key) {
    kb_Scan();
    return kb_IsDown(key);
}

static void set_message(const char *s) {
    strncpy(message, s, sizeof(message) - 1);
    message[sizeof(message) - 1] = 0;
    message_timer = 50;
}

static uint16_t player_color(uint8_t p) {
    return COLORS[p % MAX_PLAYERS];
}

static void text(int x, int y, uint16_t color, const char *s) {
    gfx_SetTextFGColor(color);
    gfx_SetTextXY(x, y);
    gfx_PrintString(s);
}

static void centered(int y, uint16_t color, const char *s) {
    int w = gfx_GetStringWidth(s);
    text((SCREEN_W - w) / 2, y, color, s);
}

static void draw_title(void) {
    gfx_SetColor(gfx_RGBTo1555(20, 28, 45));
    gfx_FillRectangle(0, 0, SCREEN_W, 28);
    centered(7, gfx_RGBTo1555(255,255,255), "MEXICAN TRAIN");
}

static void draw_domino(Domino d, int x, int y, int w, int h, uint16_t accent, bool selected_tile) {
    gfx_SetColor(gfx_RGBTo1555(248,248,242));
    gfx_FillRectangle(x, y, w, h);
    gfx_SetColor(selected_tile ? accent : gfx_RGBTo1555(80,80,80));
    gfx_Rectangle(x, y, w, h);
    gfx_SetColor(gfx_RGBTo1555(80,80,80));
    gfx_FillRectangle(x + 2, y + h/2 - 1, w - 4, 2);

    gfx_SetColor(gfx_RGBTo1555(25,25,30));
    gfx_FillCircle(x + w/2, y + h/4, 3);
    gfx_FillCircle(x + w/2, y + (h*3)/4, 3);

    char buf[4];
    buf[0] = '0' + d.a; buf[1] = 0;
    text(x + w/2 - 3, y + 5, gfx_RGBTo1555(25,25,30), buf);
    buf[0] = '0' + d.b; buf[1] = 0;
    text(x + w/2 - 3, y + h/2 + 4, gfx_RGBTo1555(25,25,30), buf);
}

static void draw_train(int x, int y, uint16_t color, const char *label, int end_value, bool open) {
    gfx_SetColor(color);
    gfx_FillCircle(x, y, 8);
    gfx_SetColor(gfx_RGBTo1555(245,245,245));
    gfx_FillCircle(x, y, 5);
    gfx_SetColor(color);
    gfx_FillCircle(x, y, 2);
    text(x + 12, y - 5, gfx_RGBTo1555(245,245,245), label);

    char n[3];
    n[0] = '0' + (end_value / 10);
    n[1] = '0' + (end_value % 10);
    n[2] = 0;
    if (end_value < 10) { n[0] = n[1]; n[1] = 0; }
    text(x + 12, y + 7, gfx_RGBTo1555(200,210,220), n);
    if (open) {
        gfx_SetColor(gfx_RGBTo1555(255,80,80));
        gfx_FillCircle(x + 95, y, 4);
    }
}

static void draw_setup(void) {
    gfx_SetColor(gfx_RGBTo1555(12,18,30));
    gfx_FillScreen(gfx_RGBTo1555(12,18,30));
    draw_title();

    centered(40, gfx_RGBTo1555(220,225,235), "GAME SETUP");

    text(35, 62, gfx_RGBTo1555(190,200,215), "PLAYERS");
    gfx_SetColor(gfx_RGBTo1555(35,45,65));
    gfx_FillRectangle(30, 55, 260, 28);

    text(45, 64, gfx_RGBTo1555(255,255,255), "TOTAL PLAYERS:");
    char n[3];
    n[0] = '0' + player_count;
    n[1] = 0;
    text(225, 64, gfx_RGBTo1555(255,220,70), n);

    text(45, 92, gfx_RGBTo1555(190,200,215), "PLAYER TYPES");
    for (uint8_t i = 0; i < player_count; ++i) {
        int y = 108 + i * 13;
        if (i == setup_cursor && setup_field == 1) {
            gfx_SetColor(gfx_RGBTo1555(45,55,80));
            gfx_FillRectangle(30, y - 2, 260, 13);
        }
        gfx_SetColor(player_color(i));
        gfx_FillCircle(40, y + 4, 4);

        char line[20];
        line[0] = 'P'; line[1] = '1' + i; line[2] = ' ';
        if (players[i].cpu) {
            line[3]='C'; line[4]='P'; line[5]='U'; line[6]=0;
        } else {
            line[3]='H'; line[4]='U'; line[5]='M'; line[6]='A'; line[7]='N'; line[8]=0;
        }
        text(50, y, gfx_RGBTo1555(240,240,245), line);
    }

    text(32, 222, gfx_RGBTo1555(170,180,195), "UP/DOWN  SELECT    LEFT/RIGHT  CHANGE");
    text(32, 232, gfx_RGBTo1555(255,220,70), "ENTER = START");
}

static void reset_players(void) {
    for (uint8_t i = 0; i < MAX_PLAYERS; ++i) {
        players[i].count = 0;
        players[i].cpu = (i != 0);
        players[i].open = false;
        players[i].train_end = 12;
        players[i].score = 0;
    }
}

static void build_deck(void) {
    pile_count = 0;
    for (uint8_t a = 0; a <= 12; ++a) {
        for (uint8_t b = a; b <= 12; ++b) {
            if (a == 12 && b == 12) continue;
            pile[pile_count++] = (Domino){a,b};
        }
    }
    for (int i = pile_count - 1; i > 0; --i) {
        int j = (int)(rnd() % (uint32_t)(i + 1));
        Domino t = pile[i]; pile[i] = pile[j]; pile[j] = t;
    }
}

static Domino draw_pile(void) {
    if (!pile_count) return (Domino){255,255};
    return pile[--pile_count];
}

static void start_round(void) {
    build_deck();
    mexican_end = 12;
    mexican_open = false;
    forced_double = false;
    winner = 255;
    turn = 0;
    selected = 0;
    target = 0;
    drawn_this_turn = false;

    for (uint8_t i = 0; i < player_count; ++i) {
        players[i].count = 0;
        players[i].open = false;
        players[i].train_end = 12;
    }

    uint8_t hand_size = player_count <= 4 ? 15 : (player_count <= 6 ? 12 : 10);
    for (uint8_t i = 0; i < player_count; ++i) {
        for (uint8_t k = 0; k < hand_size; ++k)
            players[i].hand[players[i].count++] = draw_pile();
    }

    set_message("ROUND START");
    anim_kind = 1; anim_frame = 0; anim_max = 16;
    screen = SCREEN_GAME;
}

static bool matches(Domino d, int end) {
    return d.a == end || d.b == end;
}

static int other_side(Domino d, int end) {
    if (d.a == end) return d.b;
    if (d.b == end) return d.a;
    return -1;
}

/* target: 0 = own train, 1 = Mexican, 2..9 = player train 0..7 */
static bool target_allowed(uint8_t p, uint8_t t) {
    if (t == 0) return true;
    if (t == 1) return true;
    uint8_t other = t - 2;
    if (other >= player_count || other == p) return false;
    return players[other].open;
}

static int target_end(uint8_t p, uint8_t t) {
    if (t == 0) return players[p].train_end;
    if (t == 1) return mexican_end;
    return players[t - 2].train_end;
}

static void remove_hand(uint8_t p, uint8_t idx) {
    for (uint8_t i = idx; i + 1 < players[p].count; ++i)
        players[p].hand[i] = players[p].hand[i + 1];
    if (players[p].count) players[p].count--;
    if (selected >= players[p].count && players[p].count) selected = players[p].count - 1;
}

static bool play_tile(uint8_t p, uint8_t idx, uint8_t t) {
    if (idx >= players[p].count || !target_allowed(p,t)) return false;
    Domino d = players[p].hand[idx];
    int end = target_end(p,t);
    if (!matches(d,end)) return false;

    int new_end = other_side(d,end);
    if (new_end < 0) return false;

    if (t == 0) {
        players[p].train_end = new_end;
        players[p].open = false;
    } else if (t == 1) {
        mexican_end = new_end;
        mexican_open = false;
    } else {
        players[t-2].train_end = new_end;
        players[t-2].open = false;
    }

    bool is_double = (d.a == d.b);
    remove_hand(p,idx);

    if (is_double) {
        forced_double = true;
        forced_player = p;
        forced_train = t;
    } else {
        forced_double = false;
        turn = (p + 1) % player_count;
    }

    anim_kind = 2; anim_frame = 0; anim_max = 10;
    drawn_this_turn = false;

    if (players[p].count == 0) {
        winner = p;
        screen = SCREEN_SCORES;
    }
    return true;
}

static bool find_cpu_move(uint8_t p, uint8_t *out_i, uint8_t *out_t) {
    int best_score = -9999;
    bool found = false;

    for (uint8_t i = 0; i < players[p].count; ++i) {
        Domino d = players[p].hand[i];
        for (uint8_t t = 0; t < 2 + player_count; ++t) {
            if (!target_allowed(p,t)) continue;
            int end = target_end(p,t);
            if (!matches(d,end)) continue;

            int score = (d.a + d.b);
            if (d.a == d.b) score += 30;
            if (t == 0) score += 5;
            if (t == 1) score += 2;
            if (score > best_score) {
                best_score = score;
                *out_i = i;
                *out_t = t;
                found = true;
            }
        }
    }
    return found;
}

static void cpu_turn(void) {
    uint8_t p = turn;

    if (forced_double) {
        p = forced_player;
        turn = p;
    }

    uint8_t i, t;
    if (find_cpu_move(p,&i,&t)) {
        play_tile(p,i,t);
        return;
    }

    if (!drawn_this_turn && pile_count) {
        players[p].hand[players[p].count++] = draw_pile();
        drawn_this_turn = true;
        if (find_cpu_move(p,&i,&t)) {
            play_tile(p,i,t);
            return;
        }
    }

    players[p].open = true;
    drawn_this_turn = false;
    forced_double = false;
    turn = (p + 1) % player_count;
    anim_kind = 3; anim_frame = 0; anim_max = 8;
}

static void draw_board(void) {
    gfx_SetColor(gfx_RGBTo1555(13,25,30));
    gfx_FillScreen(gfx_RGBTo1555(13,25,30));
    draw_title();

    gfx_SetColor(gfx_RGBTo1555(25,55,48));
    gfx_FillRectangle(0, 29, SCREEN_W, 122);

    /* Center hub */
    gfx_SetColor(gfx_RGBTo1555(55,45,30));
    gfx_FillCircle(160,88,22);
    text(143,82,gfx_RGBTo1555(255,225,110),"12");
    text(133,96,gfx_RGBTo1555(210,210,210),"START");

    /* Mexican train */
    gfx_SetColor(gfx_RGBTo1555(230,170,45));
    gfx_FillCircle(160,132,7);
    text(173,126,gfx_RGBTo1555(255,225,100),"MEXICAN");
    if (mexican_open) {
        gfx_SetColor(gfx_RGBTo1555(255,80,80));
        gfx_FillCircle(248,132,4);
    }

    /* Player trains around the board */
    for (uint8_t i = 0; i < player_count; ++i) {
        int x = 28 + (i % 4) * 88;
        int y = 45 + (i / 4) * 40;
        char lab[5] = {'P','1'+i,0};
        draw_train(x,y,player_color(i),lab,players[i].train_end,players[i].open);
    }

    /* Hand */
    gfx_SetColor(gfx_RGBTo1555(18,28,42));
    gfx_FillRectangle(0,153,SCREEN_W,87);

    text(8,158,player_color(turn),"TURN:");
    char who[5] = {'P','1'+turn,0};
    text(42,158,gfx_RGBTo1555(245,245,245),who);
    if (players[turn].cpu) text(70,158,gfx_RGBTo1555(170,180,195),"(CPU)");
    else text(70,158,gfx_RGBTo1555(170,180,195),"(YOU)");

    if (forced_double) text(125,158,gfx_RGBTo1555(255,150,70),"DOUBLE!");

    uint8_t p = turn;
    int max_show = players[p].count;
    int w = max_show > 0 ? (300 / max_show) : 24;
    if (w > 30) w = 30;
    if (w < 17) w = 17;

    int start_x = (SCREEN_W - w * max_show) / 2;
    for (uint8_t i = 0; i < max_show; ++i) {
        int x = start_x + i * w;
        int y = 176 + (i == selected ? -5 : 0);
        draw_domino(players[p].hand[i],x,y,w-2,54,player_color(p),i==selected);
    }

    char piletxt[8];
    text(8,218,gfx_RGBTo1555(190,200,215),"DRAW:");
    char pc[4];
    pc[0]='0'+(pile_count/10); pc[1]='0'+(pile_count%10); pc[2]=0;
    if (pile_count < 10) { pc[0]=pc[1]; pc[1]=0; }
    text(42,218,gfx_RGBTo1555(255,220,70),pc);

    text(75,218,gfx_RGBTo1555(170,180,195),"A/D TILE  2ND PLAY  GRAPH DRAW");
    text(75,230,gfx_RGBTo1555(170,180,195),"UP/DOWN TARGET  ENTER PLAY  CLEAR PASS");

    if (message_timer) {
        gfx_SetColor(gfx_RGBTo1555(20,25,35));
        gfx_FillRectangle(75,35,170,20);
        centered(41,gfx_RGBTo1555(255,220,80),message);
    }
}

static void draw_scores(void) {
    gfx_SetColor(gfx_RGBTo1555(10,15,25));
    gfx_FillScreen(gfx_RGBTo1555(10,15,25));
    centered(18,gfx_RGBTo1555(255,220,80),"ROUND COMPLETE");

    if (winner < player_count) {
        char s[12] = "PLAYER 1 WINS";
        s[7] = '1' + winner;
        centered(42,player_color(winner),s);
    }

    for (uint8_t i = 0; i < player_count; ++i) {
        int y = 68 + i * 18;
        gfx_SetColor(player_color(i));
        gfx_FillRectangle(30,y,10,10);
        char s[20];
        s[0]='P'; s[1]='1'+i; s[2]=0;
        text(48,y,gfx_RGBTo1555(240,240,245),s);
        text(80,y,players[i].cpu ? gfx_RGBTo1555(160,170,185) : gfx_RGBTo1555(230,230,235),
             players[i].cpu ? "CPU" : "HUMAN");
        text(145,y,gfx_RGBTo1555(200,210,220),"DOMINOES:");
        char c[4];
        c[0]='0'+(players[i].count/10);
        c[1]='0'+(players[i].count%10);
        c[2]=0;
        if (players[i].count < 10) { c[0]=c[1]; c[1]=0; }
        text(220,y,gfx_RGBTo1555(255,220,70),c);
    }

    centered(220,gfx_RGBTo1555(180,190,205),"ENTER = NEXT ROUND    CLEAR = MENU");
}

static void handle_setup(void) {
    if (pressed(kb_KeyUp)) {
        if (setup_field == 1) {
            setup_cursor = (setup_cursor == 0) ? player_count - 1 : setup_cursor - 1;
        }
        wait_release();
    } else if (pressed(kb_KeyDown)) {
        if (setup_field == 1) setup_cursor = (setup_cursor + 1) % player_count;
        wait_release();
    } else if (pressed(kb_KeyLeft)) {
        if (setup_field == 0) {
            if (player_count > 2) player_count--;
            if (setup_cursor >= player_count) setup_cursor = player_count - 1;
        } else {
            players[setup_cursor].cpu = !players[setup_cursor].cpu;
        }
        wait_release();
    } else if (pressed(kb_KeyRight)) {
        if (setup_field == 0) {
            if (player_count < 8) player_count++;
        } else {
            players[setup_cursor].cpu = !players[setup_cursor].cpu;
        }
        wait_release();
    } else if (pressed(kb_Key2nd)) {
        setup_field = setup_field ? 0 : 1;
        wait_release();
    } else if (pressed(kb_KeyEnter)) {
        start_round();
        wait_release();
    }
}

static void human_turn(void) {
    uint8_t p = turn;
    if (forced_double) p = forced_player;

    if (players[p].count == 0) return;
    if (selected >= players[p].count) selected = players[p].count - 1;

    if (pressed(kb_KeyRight)) {
        selected = (selected + 1) % players[p].count;
        wait_release();
    } else if (pressed(kb_KeyLeft)) {
        selected = (selected == 0) ? players[p].count - 1 : selected - 1;
        wait_release();
    } else if (pressed(kb_KeyDown)) {
        target = (target + 1) % (2 + player_count);
        wait_release();
    } else if (pressed(kb_KeyUp)) {
        target = (target == 0) ? (1 + player_count) : target - 1;
        wait_release();
    } else if (pressed(kb_KeyGraph)) {
        if (!drawn_this_turn && pile_count && players[p].count < MAX_HAND) {
            players[p].hand[players[p].count++] = draw_pile();
            drawn_this_turn = true;
            anim_kind = 4; anim_frame = 0; anim_max = 10;
        } else {
            set_message("CANNOT DRAW");
        }
        wait_release();
    } else if (pressed(kb_KeyEnter)) {
        if (play_tile(p,selected,target)) {
            set_message("NICE PLAY");
        } else {
            set_message("ILLEGAL MOVE");
        }
        wait_release();
    } else if (pressed(kb_KeyClear)) {
        players[p].open = true;
        drawn_this_turn = false;
        forced_double = false;
        turn = (p + 1) % player_count;
        wait_release();
    }
}

static void update_animation(void) {
    if (anim_frame < anim_max) anim_frame++;
    else anim_kind = 0;
    if (message_timer) message_timer--;
}

static void draw_animation_overlay(void) {
    if (!anim_kind) return;
    if (anim_kind == 2) {
        int x = 140 + (anim_frame < anim_max/2 ? anim_frame*2 : (anim_max-anim_frame)*2);
        gfx_SetColor(gfx_RGBTo1555(255,220,70));
        gfx_FillCircle(x,110,3);
    } else if (anim_kind == 4) {
        gfx_SetColor(gfx_RGBTo1555(70,190,255));
        gfx_FillCircle(260,205,4 + (anim_frame % 4));
    } else if (anim_kind == 3) {
        gfx_SetColor(gfx_RGBTo1555(255,100,100));
        gfx_FillCircle(160,132,4 + (anim_frame % 3));
    }
}

static void update(void) {
    update_animation();

    if (screen == SCREEN_SETUP) {
        handle_setup();
        return;
    }

    if (screen == SCREEN_SCORES) {
        if (pressed(kb_KeyEnter)) {
            round_no++;
            start_round();
            wait_release();
        } else if (pressed(kb_KeyClear)) {
            screen = SCREEN_SETUP;
            wait_release();
        }
        return;
    }

    if (players[turn].cpu) {
        cpu_turn();
    } else {
        human_turn();
    }
}

static void draw(void) {
    if (screen == SCREEN_SETUP) draw_setup();
    else if (screen == SCREEN_GAME) {
        draw_board();
        draw_animation_overlay();
    } else draw_scores();
}

int main(void) {
    reset_players();

    gfx_Begin();
    gfx_SetDrawBuffer();

    while (1) {
        update();
        draw();
        gfx_SwapDraw();
    }

    gfx_End();
    return 0;
}
