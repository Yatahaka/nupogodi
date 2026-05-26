// Игра "Поймай яблоко"
// для нативной сборки нужен SDL2

#include <SDL2/SDL.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include <cstdlib>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <fstream>

// ---- размер окна ----
#define SCR_W 800
#define SCR_H 600

// ---- корзина ----
#define BKT_W   100
#define BKT_H    22
#define BKT_SPD   7
#define BKT_Y   (SCR_H - 55)

// ---- падающие предметы ----
#define ITEM_SZ   28
#define LIVES_MAX  3
#define SPAWN_MS 1289

// ---- типы предметов ----
enum ItemKind { APPLE, BOMB };

struct Item {
    float px, py;   // позиция
    float vy;       // скорость падения
    ItemKind kind;
    bool  active;
};

// (знаю что глобальные -- плохо, но с main loop иначе не получается)
static SDL_Window*   gWin  = nullptr;
static SDL_Renderer* gRen  = nullptr;

static int   gBktX   = 0;
static int   gScore  = 0;
static int   gLives  = LIVES_MAX;
static int   gBest   = 0;
static bool  gOver   = false;
static float gFallSpd = 2.8f;  // текущая базовая скорость

static std::vector<Item> gItems;
static Uint32 gLastSpawn = 0;
static Uint32 gFlashEnd  = 0;   // до этого времени рисуем красную вспышку
static bool   gNewBest   = false;

// для мигания "новый рекорд"
static Uint32 gBlinkTick = 0;
static bool   gBlinkVis  = true;

// ---- сохранение рекорда ----
static void saveBest() {
#ifndef __EMSCRIPTEN__
    FILE* f = fopen("save.dat", "wb");
    if (f) { fwrite(&gBest, sizeof(gBest), 1, f); fclose(f); }
#endif
}

static void loadBest() {
#ifndef __EMSCRIPTEN__
    FILE* f = fopen("save.dat", "rb");
    if (!f) return;
    fread(&gBest, sizeof(gBest), 1, f);
    fclose(f);
    if (gBest < 0) gBest = 0; // на всякий случай
#endif
}

// ---- вспомогательные рисовалки ----
static void setColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255) {
    SDL_SetRenderDrawColor(gRen, r, g, b, a);
}

static void fillBox(int x, int y, int w, int h) {
    SDL_Rect rc = { x, y, w, h };
    SDL_RenderFillRect(gRen, &rc);
}

static void dot(int x, int y) {
    SDL_RenderDrawPoint(gRen, x, y);
}

static void line(int x0, int y0, int x1, int y1) {
    SDL_RenderDrawLine(gRen, x0, y0, x1, y1);
}

// закрашенный круг через сканирование строк
static void solidCircle(int cx, int cy, int r,
                        Uint8 red, Uint8 grn, Uint8 blu)
{
    setColor(red, grn, blu);
    for (int dy = -r; dy <= r; ++dy) {
        int dx = (int)sqrtf((float)(r*r - dy*dy));
        SDL_Rect row = { cx - dx, cy + dy, 2*dx + 1, 1 };
        SDL_RenderFillRect(gRen, &row);
    }
}

// ---- пиксельный шрифт (цифры 0-9), каждый символ 5x7 пикселей ----
// закодировано построчно, бит 4 = левый пиксель, бит 0 = правый
static const Uint8 kFont5x7[10][7] = {
    { 0x0E,0x11,0x13,0x15,0x19,0x11,0x0E }, // 0
    { 0x04,0x0C,0x04,0x04,0x04,0x04,0x0E }, // 1
    { 0x0E,0x11,0x01,0x02,0x04,0x08,0x1F }, // 2
    { 0x1F,0x02,0x04,0x02,0x01,0x11,0x0E }, // 3
    { 0x02,0x06,0x0A,0x12,0x1F,0x02,0x02 }, // 4
    { 0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E }, // 5
    { 0x06,0x08,0x10,0x1E,0x11,0x11,0x0E }, // 6
    { 0x1F,0x01,0x02,0x04,0x04,0x04,0x04 }, // 7
    { 0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E }, // 8
    { 0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C }, // 9
};

// рисует одну цифру, sz -- размер одного "пикселя" в экранных точках
static void drawDigit(int d, int x, int y, int sz,
                      Uint8 r, Uint8 g, Uint8 b)
{
    if (d < 0 || d > 9) return;
    setColor(r, g, b);
    for (int row = 0; row < 7; ++row)
        for (int col = 0; col < 5; ++col)
            if (kFont5x7[d][row] & (1 << (4 - col)))
                fillBox(x + col*sz, y + row*sz, sz, sz);
}

// рисует число num начиная с (x,y), возвращает x после последней цифры
static int drawNum(int num, int x, int y, int sz,
                   Uint8 r, Uint8 g, Uint8 b)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", num);
    for (int i = 0; buf[i]; ++i) {
        drawDigit(buf[i] - '0', x, y, sz, r, g, b);
        x += (5 + 1) * sz;
    }
    return x;
}

static int numPixelWidth(int num, int sz) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", num);
    return (int)strlen(buf) * 6 * sz;
}

// ---- рисование сердечка (жизни) ----
// используя вспомогательный битмап 9x8
static const Uint8 kHeart[8] = {
    0b00000000,
    0b01101100,
    0b11111110,
    0b11111110,
    0b01111100,
    0b00111000,
    0b00010000,
    0b00000000,
};

static void drawHeart(int x, int y, int sz, bool lit) {
    Uint8 r = lit ? 230 : 70,
          g = lit ?  55 : 25,
          b = lit ?  55 : 25;
    setColor(r, g, b);
    for (int row = 0; row < 8; ++row)
        for (int col = 0; col < 8; ++col)
            if (kHeart[row] & (1 << (7 - col)))
                fillBox(x + col*sz, y + row*sz, sz, sz);
}

// ---- рисование предметов ----
static void drawApple(int ax, int ay) {
    int cx = ax + ITEM_SZ/2;
    int cy = ay + ITEM_SZ/2 + 2;
    // тело яблока
    solidCircle(cx, cy, ITEM_SZ/2 - 2, 210, 45, 45);
    // блик
    solidCircle(cx - 3, cy - 3, 3, 240, 100, 100);
    // хвостик
    setColor(70, 140, 40);
    fillBox(cx - 1, ay + 1, 2, 5);
    // листик
    line(cx, ay + 4, cx + 7, ay);
    line(cx, ay + 4, cx + 4, ay + 7);
}

static void drawBomb(int bx, int by) {
    int cx = bx + ITEM_SZ/2;
    int cy = by + ITEM_SZ/2 + 2;
    // корпус бомбы
    solidCircle(cx, cy, ITEM_SZ/2 - 2, 25, 25, 25);
    // блик на корпусе
    solidCircle(cx - 3, cy - 4, 3, 75, 75, 75);
    // запал
    setColor(180, 140, 10);
    line(cx + 5, by + 5, cx + 9, by - 1);
    // искра
    solidCircle(cx + 9, by - 2, 3, 255, 210, 0);
    solidCircle(cx + 9, by - 2, 1, 255, 255, 180);
}

static void drawBasket(int bx) {
    // основное тело
    setColor(170, 100, 35);
    fillBox(bx, BKT_Y, BKT_W, BKT_H);
    // верхняя полоска (светлее)
    setColor(210, 145, 65);
    fillBox(bx, BKT_Y, BKT_W, 3);
    // нижняя тень
    setColor(120, 70, 20);
    fillBox(bx, BKT_Y + BKT_H - 3, BKT_W, 3);
    // вертикальные прутья
    setColor(130, 80, 25);
    for (int i = 1; i < 10; ++i)
        line(bx + i*10, BKT_Y, bx + i*10, BKT_Y + BKT_H);
}

// ---- логика игры ----
static void startGame() {
    gBktX    = SCR_W/2 - BKT_W/2;
    gScore   = 0;
    gLives   = LIVES_MAX;
    gOver    = false;
    gNewBest = false;
    gFallSpd = 2.8f;
    gItems.clear();
    gLastSpawn = SDL_GetTicks();
    gFlashEnd  = 0;
}

static void spawnItem(Uint32 now) {
    if (now - gLastSpawn < SPAWN_MS) return;
    gLastSpawn = now;

    Item it;
    it.px   = (float)(rand() % (SCR_W - ITEM_SZ));
    it.py   = -(float)ITEM_SZ;
    it.vy   = gFallSpd + (rand() % 80) / 50.0f;
    it.kind = (rand() % 5 == 0) ? BOMB : APPLE;
    it.active = true;
    gItems.push_back(it);
}

static void checkCollision(Item& it, Uint32 now) {
    // AABB с корзиной
    bool hx = (it.px + ITEM_SZ > gBktX) &&
               (it.px            < gBktX + BKT_W);
    bool hy = (it.py + ITEM_SZ > BKT_Y) &&
               (it.py            < BKT_Y + BKT_H);
    if (hx && hy) {
        it.active = false;
        if (it.kind == APPLE) {
            ++gScore;
            // каждые 8 очков немного ускоряем
            gFallSpd = 2.8f + (gScore / 8) * 0.2f;
        } else {
            // поймали бомбу -- game over сразу
            if (gScore > gBest) { gBest = gScore; gNewBest = true; saveBest(); }
            gOver = true;
        }
        return;
    }
    // улетело вниз
    if (it.py > SCR_H) {
        it.active = false;
        if (it.kind == APPLE) {
            --gLives;
            gFlashEnd = now + 250; // красная вспышка на 250 мс
            if (gLives <= 0) {
                if (gScore > gBest) { gBest = gScore; gNewBest = true; saveBest(); }
                gOver = true;
            }
        }
    }
}

// ---- отрисовка кадра ----
static void renderScene(Uint32 now) {
    // фон -- тёмно-серый
    setColor(28, 28, 28);
    SDL_RenderClear(gRen);

    // земля внизу
    setColor(75, 50, 28);
    fillBox(0, SCR_H - 28, SCR_W, 28);

    // предметы
    for (auto& it : gItems) {
        if (!it.active) continue;
        if (it.kind == APPLE) drawApple((int)it.px, (int)it.py);
        else                  drawBomb ((int)it.px, (int)it.py);
    }

    drawBasket(gBktX);

    // счёт -- левый верхний угол, крупно
    drawNum(gScore, 10, 10, 4, 255, 215, 75);

    // жизни
    for (int i = 0; i < LIVES_MAX; ++i)
        drawHeart(10 + i * 20, 44, 2, i < gLives);

    // рекорд по центру сверху, маленько
    {
        int bw = numPixelWidth(gBest, 2);
        Uint8 r = 120, g2 = 120, b = 120;
        if (gScore >= gBest && gScore > 0)
            r = 255, g2 = 195, b = 0;
        drawNum(gBest, (SCR_W - bw)/2, 10, 2, r, g2, b);
    }

    // красная вспышка при потере жизни
    if (now < gFlashEnd) {
        float t = (float)(gFlashEnd - now) / 250.0f;
        Uint8 a = (Uint8)(t * 110);
        SDL_SetRenderDrawBlendMode(gRen, SDL_BLENDMODE_BLEND);
        setColor(220, 35, 35, a);
        fillBox(0, 0, SCR_W, SCR_H);
        SDL_SetRenderDrawBlendMode(gRen, SDL_BLENDMODE_NONE);
    }

    // экран Game Over
    if (gOver) {
        SDL_SetRenderDrawBlendMode(gRen, SDL_BLENDMODE_BLEND);
        setColor(0, 0, 0, 160);
        fillBox(0, 0, SCR_W, SCR_H);
        SDL_SetRenderDrawBlendMode(gRen, SDL_BLENDMODE_NONE);

        // финальный счёт по центру
        // TODO: нарисовать красивую надпись "ИГРА ОКОНЧЕНА"
        int sw = numPixelWidth(gScore, 6);
        drawNum(gScore, (SCR_W - sw)/2, SCR_H/2 - 30, 6, 255, 215, 75);

        // рекорд -- зелёным и мигает если новый
        if (gNewBest && gBlinkVis) {
            int bw = numPixelWidth(gBest, 3);
            drawNum(gBest, (SCR_W - bw)/2, SCR_H/2 + 55, 3, 80, 225, 80);
        } else if (!gNewBest) {
            int bw = numPixelWidth(gBest, 2);
            drawNum(gBest, (SCR_W - bw)/2, SCR_H/2 + 55, 2, 120, 120, 120);
        }
    }

    SDL_RenderPresent(gRen);
}

// ---- главный цикл (один кадр) ----
static void tick() {
    Uint32 now = SDL_GetTicks();

    // мигание (период 480 мс)
    if (now - gBlinkTick >= 480) {
        gBlinkTick = now;
        gBlinkVis  = !gBlinkVis;
    }

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) {
#ifdef __EMSCRIPTEN__
            emscripten_cancel_main_loop();
#else
            SDL_Quit();
            exit(0);
#endif
        }
        if (ev.type == SDL_KEYDOWN) {
            SDL_Keycode k = ev.key.keysym.sym;
            if (k == SDLK_ESCAPE) {
#ifdef __EMSCRIPTEN__
                emscripten_cancel_main_loop();
#else
                SDL_Quit();
                exit(0);
#endif
            }
            // рестарт по R или Enter когда игра кончилась
            if (gOver && (k == SDLK_r || k == SDLK_RETURN)) {
                int saved = gBest;
                startGame();
                gBest = saved;
            }
        }
    }

    if (!gOver) {
        // управление корзиной
        const Uint8* kb = SDL_GetKeyboardState(nullptr);
        if (kb[SDL_SCANCODE_LEFT]  && gBktX > 0)
            gBktX -= BKT_SPD;
        if (kb[SDL_SCANCODE_RIGHT] && gBktX < SCR_W - BKT_W)
            gBktX += BKT_SPD;

        spawnItem(now);

        for (auto& it : gItems)
            if (it.active) {
                it.py += it.vy;
                checkCollision(it, now);
            }

        // убираем неактивные предметы
        size_t i = 0;
        while (i < gItems.size()) {
            if (!gItems[i].active)
                gItems.erase(gItems.begin() + i);
            else
                ++i;
        }
    }

    renderScene(now);

#ifndef __EMSCRIPTEN__
    SDL_Delay(16); // ~60 fps
#endif
}

int main(int argc, char* argv[]) {
    srand((unsigned)time(nullptr));
    loadBest();

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return 1;
    }

    gWin = SDL_CreateWindow("Поймай яблоко",
                            SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED,
                            SCR_W, SCR_H, 0);
    if (!gWin) { SDL_Quit(); return 1; }

    gRen = SDL_CreateRenderer(gWin, -1,
                              SDL_RENDERER_ACCELERATED |
                              SDL_RENDERER_PRESENTVSYNC);
    if (!gRen) { SDL_DestroyWindow(gWin); SDL_Quit(); return 1; }

    startGame();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(tick, 0, 1);
#else
    while (true) tick();
#endif
    return 0;
}
