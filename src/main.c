
#include <graphx.h>
#include <keypadc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define W 320
#define H 240
#define MAX_PLAYERS 8
#define MAX_HAND 15
#define TILE_COUNT 91
#define TRAIN_MAX 64

typedef struct { uint8_t a,b; } Domino;
typedef struct {
    Domino hand[MAX_HAND];
    uint8_t count;
    bool cpu, open;
    uint8_t trainEnd;
    uint16_t score;
} Player;

static Player p[MAX_PLAYERS];
static Domino pile[TILE_COUNT];
static uint8_t pileCount;
static uint8_t players = 2, current = 0;
static uint8_t handSel = 0, handScroll = 0;
static uint8_t target = 0; /* 0 own, 1 Mexican, 2.. player trains */
static bool setup = true, gameOver = false;
static uint8_t mexicanEnd = 12;
static bool mexicanOpen = true;
static bool doublePending = false;
static uint8_t doubleOwner = 0;
static uint8_t roundNo = 1;

static const uint16_t COLORS[MAX_PLAYERS] = {
    gfx_RGBTo1555(40,170,255), gfx_RGBTo1555(255,80,80),
    gfx_RGBTo1555(70,210,110), gfx_RGBTo1555(255,190,40),
    gfx_RGBTo1555(190,100,255), gfx_RGBTo1555(255,110,190),
    gfx_RGBTo1555(60,220,210), gfx_RGBTo1555(240,240,240)
};

static void text(int x,int y,const char *s,uint16_t c){
    gfx_SetTextFGColor(c); gfx_SetTextXY(x,y); gfx_PrintString(s);
}
static void rect(int x,int y,int w,int h,uint16_t c){
    gfx_SetColor(c); gfx_FillRectangle(x,y,w,h);
}
static void frame(int x,int y,int w,int h,uint16_t c){
    gfx_SetColor(c); gfx_Rectangle(x,y,w,h);
}
static void centerText(int y,const char *s,uint16_t c){
    int x=(W-(int)strlen(s)*6)/2; if(x<0)x=0; text(x,y,s,c);
}
static void u8str(uint8_t n,char *s){ sprintf(s,"%u",n); }

static void drawTile(int x,int y,int w,int h,Domino d,bool selected){
    uint16_t bg = gfx_RGBTo1555(245,245,245);
    uint16_t fg = gfx_RGBTo1555(20,20,20);
    if(selected){ rect(x-2,y-2,w+4,h+4,gfx_RGBTo1555(255,210,30)); }
    rect(x,y,w,h,bg); frame(x,y,w,h,fg);
    gfx_SetColor(fg);
    gfx_Line(x+2,y+h/2,x+w-3,y+h/2);
    char a[4],b[4]; u8str(d.a,a); u8str(d.b,b);
    text(x+w/2-(int)strlen(a)*3,y+5,a,fg);
    text(x+w/2-(int)strlen(b)*3,y+h/2+5,b,fg);
}

static void drawTrain(int cx,int cy,uint8_t end,uint16_t c,bool open,uint8_t count){
    gfx_SetColor(c);
    gfx_FillCircle(cx,cy,7);
    frame(cx-8,cy-8,16,16,gfx_RGBTo1555(10,10,10));
    char s[4]; u8str(end,s);
    text(cx-3,cy-4,s,gfx_RGBTo1555(10,10,10));
    if(open){
        frame(cx-11,cy-11,22,22,gfx_RGBTo1555(255,210,30));
    }
    char n[8]; sprintf(n,"%u",count);
    text(cx-3,cy+12,n,gfx_RGBTo1555(230,230,230));
}

static void buildPile(void){
    uint8_t k=0;
    for(uint8_t a=0;a<=12;a++)
        for(uint8_t b=a;b<=12;b++){
            pile[k].a=a; pile[k].b=b; k++;
        }
    pileCount=TILE_COUNT;
    for(int i=TILE_COUNT-1;i>0;i--){
        int j=rand()%(i+1); Domino t=pile[i]; pile[i]=pile[j]; pile[j]=t;
    }
}
static bool isMatch(Domino d,uint8_t end){
    return d.a==end || d.b==end;
}
static Domino orient(Domino d,uint8_t end){
    if(d.a==end){ Domino z={d.a,d.b}; return z; }
    Domino z={d.b,d.a}; return z;
}
static bool isDouble(Domino d){ return d.a==d.b; }

static void deal(void){
    buildPile();
    for(uint8_t i=0;i<players;i++){
        p[i].count=0; p[i].open=false; p[i].trainEnd=12;
        for(uint8_t j=0;j<MAX_HAND;j++) p[i].hand[j]=(Domino){0,0};
    }
    uint8_t handSize = players<=4 ? 15 : (players<=6 ? 12 : 10);
    for(uint8_t r=0;r<handSize;r++)
        for(uint8_t i=0;i<players;i++)
            if(pileCount){ p[i].hand[p[i].count++]=pile[--pileCount]; }
    current=0; handSel=0; handScroll=0; target=0;
    mexicanEnd=12; mexicanOpen=true; doublePending=false;
}

static int playableOnTarget(Domino d,uint8_t t,uint8_t who){
    if(t==0) return isMatch(d,p[who].trainEnd);
    if(t==1) return mexicanOpen && isMatch(d,mexicanEnd);
    uint8_t other=t-2;
    if(other>=players || other==who) return 0;
    return p[other].open && isMatch(d,p[other].trainEnd);
}

static bool canPlayAny(uint8_t who){
    for(uint8_t i=0;i<p[who].count;i++)
        for(uint8_t t=0;t<players+2;t++)
            if(playableOnTarget(p[who].hand[i],t,who)) return true;
    return false;
}

static void removeHand(uint8_t who,uint8_t idx){
    for(uint8_t i=idx;i+1<p[who].count;i++) p[who].hand[i]=p[who].hand[i+1];
    if(p[who].count) p[who].count--;
    if(handSel>=p[who].count && handSel) handSel--;
}

static void playTile(uint8_t who,uint8_t idx,uint8_t t){
    Domino d=p[who].hand[idx];
    d=orient(d, t==0?p[who].trainEnd:(t==1?mexicanEnd:p[t-2].trainEnd));
    if(t==0){ p[who].trainEnd=d.b; p[who].open=false; }
    else if(t==1){ mexicanEnd=d.b; mexicanOpen=true; }
    else { p[t-2].trainEnd=d.b; p[t-2].open=false; }
    removeHand(who,idx);
    if(isDouble(d)){ doublePending=true; doubleOwner=who; }
    else if(doublePending && who==doubleOwner) doublePending=false;
    current=(current+1)%players;
    handSel=0; handScroll=0; target=0;
    gfx_SwapDraw(); delay(90);
}

static void drawOne(uint8_t who){
    if(pileCount && p[who].count<MAX_HAND) p[who].hand[p[who].count++]=pile[--pileCount];
}

static void cpuTurn(void){
    uint8_t best=255,bt=255;
    for(uint8_t i=0;i<p[current].count;i++){
        for(uint8_t t=0;t<players+2;t++){
            if(playableOnTarget(p[current].hand[i],t,current)){
                if(isDouble(p[current].hand[i])){ best=i;bt=t; }
                else if(best==255){ best=i;bt=t; }
            }
        }
    }
    if(best!=255){ playTile(current,best,bt); return; }
    drawOne(current);
    for(uint8_t i=0;i<p[current].count;i++)
        for(uint8_t t=0;t<players+2;t++)
            if(playableOnTarget(p[current].hand[i],t,current)){ playTile(current,i,t); return; }
    p[current].open=true;
    current=(current+1)%players;
}

static void drawSetup(void){
    gfx_FillScreen(gfx_RGBTo1555(18,22,32));
    centerText(8,"MEXICAN TRAIN",gfx_RGBTo1555(255,210,30));
    centerText(22,"DOUBLE-12",gfx_RGBTo1555(220,220,220));
    text(22,43,"PLAYERS",gfx_RGBTo1555(255,255,255));
    char s[8]; sprintf(s,"%u",players);
    rect(105,39,45,20,gfx_RGBTo1555(35,42,58)); frame(105,39,45,20,gfx_RGBTo1555(100,110,130));
    centerText(44,s,gfx_RGBTo1555(255,255,255));
    text(172,43,"LEFT/RIGHT",gfx_RGBTo1555(150,160,180));

    for(uint8_t i=0;i<players;i++){
        int y=68+i*20;
        char name[16]; sprintf(name,"PLAYER %u",i+1);
        text(22,y,name,COLORS[i]);
        rect(105,y-3,75,16,gfx_RGBTo1555(35,42,58));
        frame(105,y-3,75,16,COLORS[i]);
        text(114,y,i==current && p[i].cpu==false ? "HUMAN" : (p[i].cpu ? "CPU" : "HUMAN"),
             gfx_RGBTo1555(245,245,245));
        if(i==current) text(190,y,"< SELECT",gfx_RGBTo1555(255,210,30));
    }
    text(22,232,"UP/DOWN: PLAYER   LEFT/RIGHT: HUMAN/CPU   2ND: START",gfx_RGBTo1555(190,200,215));
}
static void setupKeys(void){
    kb_Scan();
    if(kb_IsDown(kb_Up)){ if(current) current--; delay(100); }
    if(kb_IsDown(kb_Down)){ if(current+1<players) current++; delay(100); }
    if(kb_IsDown(kb_Left) || kb_IsDown(kb_Right)){
        p[current].cpu=!p[current].cpu; delay(140);
    }
    if(kb_IsDown(kb_Alpha)){ /* add one player */ }
    if(kb_IsDown(kb_2nd)){
        setup=false; current=0; deal(); delay(200);
    }
    if(kb_IsDown(kb_Clear)){
        setup=true; current=0; delay(150);
    }
    /* player count: mode key cycles 2..8 with +/− on Zoom/Trace */
    if(kb_IsDown(kb_Trace)){ if(players<8)players++; if(current>=players)current=players-1; delay(130); }
    if(kb_IsDown(kb_GraphVar)){ if(players>2)players--; if(current>=players)current=players-1; delay(130); }
}

static void drawGame(void){
    gfx_FillScreen(gfx_RGBTo1555(14,18,27));
    /* top status */
    rect(0,0,W,30,gfx_RGBTo1555(28,34,48));
    char s[32];
    sprintf(s,"PLAYER %u'S TURN",current+1);
    text(8,8,s,COLORS[current]);
    sprintf(s,"PILE %u",pileCount);
    text(118,8,s,gfx_RGBTo1555(210,215,225));
    sprintf(s,"ROUND %u",roundNo);
    text(235,8,s,gfx_RGBTo1555(210,215,225));

    /* board area */
    frame(8,36,304,108,gfx_RGBTo1555(75,85,105));
    centerText(40,"TRAINS",gfx_RGBTo1555(180,190,205));

    /* Mexican train */
    text(18,61,"MEX",gfx_RGBTo1555(255,210,30));
    drawTrain(67,69,mexicanEnd,gfx_RGBTo1555(255,210,30),mexicanOpen,0);

    /* opponent trains, compact */
    uint8_t shown=0;
    for(uint8_t i=0;i<players;i++) if(i!=current){
        int x=120+(shown%3)*62, y=65+(shown/3)*35;
        drawTrain(x,y,p[i].trainEnd,COLORS[i],p[i].open,p[i].count);
        char n[10]; sprintf(n,"P%u",i+1); text(x-8,y-22,n,COLORS[i]);
        shown++;
    }
    /* own train */
    text(18,111,"YOU",COLORS[current]);
    drawTrain(67,119,p[current].trainEnd,COLORS[current],p[current].open,p[current].count);

    /* target + message */
    rect(8,148,304,20,gfx_RGBTo1555(24,29,41));
    if(doublePending){
        sprintf(s,"DOUBLE: PLAYER %u MUST PLAY A DOUBLE",doubleOwner+1);
    } else if(target==0) strcpy(s,"TARGET: YOUR TRAIN");
    else if(target==1) strcpy(s,"TARGET: MEXICAN TRAIN");
    else sprintf(s,"TARGET: PLAYER %u TRAIN",target-1);
    text(14,154,s,gfx_RGBTo1555(255,255,255));

    /* hand viewport */
    rect(8,171,304,50,gfx_RGBTo1555(24,29,41));
    text(12,174,"YOUR HAND",COLORS[current]);
    uint8_t visible=6;
    int x0=13;
    for(uint8_t k=0;k<visible;k++){
        uint8_t idx=handScroll+k;
        if(idx>=p[current].count) break;
        drawTile(x0+k*49,184,39,32,p[current].hand[idx],idx==handSel);
    }
    if(handScroll>0) text(2,202,"<",gfx_RGBTo1555(255,210,30));
    if(handScroll+visible<p[current].count) text(309,202,">",gfx_RGBTo1555(255,210,30));

    text(8,226,"LEFT/RIGHT TILE  UP/DOWN TARGET",gfx_RGBTo1555(175,185,200));
    text(8,235,"2ND PLAY  GRAPH DRAW  CLEAR PASS",gfx_RGBTo1555(175,185,200));
}

static void gameKeys(void){
    kb_Scan();
    if(p[current].cpu){ cpuTurn(); return; }

    if(kb_IsDown(kb_Left)){
        if(handSel>0) handSel--;
        if(handSel<handScroll) handScroll=handSel;
        delay(100);
    }
    if(kb_IsDown(kb_Right)){
        if(handSel+1<p[current].count) handSel++;
        if(handSel>=handScroll+6) handScroll=handSel-5;
        delay(100);
    }
    if(kb_IsDown(kb_Up)){
        if(target>0) target--; else target=players+1;
        delay(100);
    }
    if(kb_IsDown(kb_Down)){
        if(target<players+1) target++; else target=0;
        delay(100);
    }
    if(kb_IsDown(kb_Graph)){
        drawOne(current); delay(150);
    }
    if(kb_IsDown(kb_2nd) && p[current].count){
        if(playableOnTarget(p[current].hand[handSel],target,current)){
            playTile(current,handSel,target);
        }
        delay(130);
    }
    if(kb_IsDown(kb_Clear)){
        if(!canPlayAny(current)){ p[current].open=true; current=(current+1)%players; handSel=0; handScroll=0; }
        delay(130);
    }
}

static void drawScore(void){
    gfx_FillScreen(gfx_RGBTo1555(18,22,32));
    centerText(12,"ROUND COMPLETE",gfx_RGBTo1555(255,210,30));
    for(uint8_t i=0;i<players;i++){
        char s[32];
        sprintf(s,"PLAYER %u    TILES LEFT: %u",i+1,p[i].count);
        text(35,40+i*22,s,COLORS[i]);
    }
    centerText(220,"2ND: NEW ROUND    CLEAR: SETUP",gfx_RGBTo1555(190,200,215));
}

static void checkEnd(void){
    for(uint8_t i=0;i<players;i++) if(p[i].count==0) { gameOver=true; return; }
}

int main(void){
    gfx_Begin();
    gfx_SetDrawBuffer();
    srand(12345);
    for(uint8_t i=0;i<MAX_PLAYERS;i++){ p[i].cpu=(i!=0); p[i].open=false; p[i].score=0; }
    while(1){
        if(setup){
            drawSetup(); gfx_SwapDraw(); setupKeys();
        } else if(gameOver){
            drawScore(); gfx_SwapDraw();
            kb_Scan();
            if(kb_IsDown(kb_Clear)){ setup=true; gameOver=false; current=0; delay(180); }
            if(kb_IsDown(kb_2nd)){ roundNo++; gameOver=false; deal(); delay(180); }
        } else {
            checkEnd();
            if(gameOver) continue;
            drawGame(); gfx_SwapDraw(); gameKeys();
        }
    }
    gfx_End();
    return 0;
}
