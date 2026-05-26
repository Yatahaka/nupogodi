#include <SDL2/SDL.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>
#include <fstream>

const int WIN_W = 800;
const int WIN_H = 600;
const int TARGET_FPS = 60;

const int BASK_W = 100;
const int BASK_H = 20;
const int BASK_Y = WIN_H - 60;
const int BASK_SPD = 7;
const int MAX_LIVES = 3;

const int OBJ_SIZE = 28;
const int SPAWN_DELAY = 1322; // пробовал 1000 и 1500, это показалось нормальным
const float BASE_SPD = 3.0f;

struct FallingObj {
    float x, y, speed;
    bool apple;
    bool alive;
};

enum class Screen { Play, GameOver };

struct GameState {
    int baskX;
    int score;
    int lives;
    int highScore;
    bool newRecord;
    Screen screen;

    std::vector<FallingObj> objs;
    Uint32 lastSpawn;
    float curSpeed;

    bool flashing;
    Uint32 flashTime;

    bool blink;
    Uint32 blinkTime;
};

static SDL_Window*   gWindow   = nullptr;
static SDL_Renderer* gRenderer = nullptr;
static GameState     gs;
static bool          gRunning  = true;
// static int debugFrames = 0; // считал кадры чтобы проверить fps

// загружает рекорд из файла, возвращает 0 если файла нет
static int loadBest() {
#ifndef __EMSCRIPTEN__
    std::ifstream f("best.dat", std::ios::binary);
    if (!f) return 0;
    int v = 0;
    f.read((char*)&v, sizeof(v));
    return (f && v > 0) ? v : 0;
#else
    return 0;
#endif
}

static void saveBest(int v) {
#ifndef __EMSCRIPTEN__
    std::ofstream f("best.dat", std::ios::binary | std::ios::trunc);
    if (f) f.write((char*)&v, sizeof(v));
#endif
}

static void resetGame() {
    gs.baskX     = WIN_W / 2 - BASK_W / 2;
    gs.score     = 0;
    gs.lives     = MAX_LIVES;
    gs.newRecord = false;
    gs.screen    = Screen::Play;
    gs.objs.clear();
    gs.lastSpawn = SDL_GetTicks();
    gs.curSpeed  = BASE_SPD;
    gs.flashing  = false;
    gs.blink     = true;
    gs.blinkTime = SDL_GetTicks();
}

// маленький пиксельный шрифт для цифр (битмап 3x5)
static const unsigned char DIGITS[10][5] = {
    {0b111, 0b101, 0b101, 0b101, 0b111},
    {0b010, 0b110, 0b010, 0b010, 0b111},
    {0b111, 0b001, 0b111, 0b100, 0b111},
    {0b111, 0b001, 0b111, 0b001, 0b111},
    {0b101, 0b101, 0b111, 0b001, 0b001},
    {0b111, 0b100, 0b111, 0b001, 0b111},
    {0b111, 0b100, 0b111, 0b101, 0b111},
    {0b111, 0b001, 0b001, 0b001, 0b001},
    {0b111, 0b101, 0b111, 0b101, 0b111},
    {0b111, 0b101, 0b111, 0b001, 0b111},
};

static void drawDigit(int digit, int x, int y, int sz,
                      Uint8 r, Uint8 g, Uint8 b)
{
    if (digit < 0 || digit > 9) return;
    SDL_SetRenderDrawColor(gRenderer, r, g, b, 255);
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 3; col++) {
            if (DIGITS[digit][row] & (1 << (2 - col))) {
                SDL_Rect px = { x + col * sz, y + row * sz, sz, sz };
                SDL_RenderFillRect(gRenderer, &px);
            }
        }
    }
}

static void drawNumber(int num, int x, int y, int sz,
                       Uint8 r, Uint8 g, Uint8 b)
{
    std::string s = std::to_string(num);
    for (char c : s) {
        drawDigit(c - '0', x, y, sz, r, g, b);
        x += (3 + 1) * sz;
    }
}

static int numWidth(int num, int sz) {
    return (int)std::to_string(num).size() * 4 * sz;
}

static void fillRect(int x, int y, int w, int h,
                     Uint8 r, Uint8 g, Uint8 b)
{
    SDL_SetRenderDrawColor(gRenderer, r, g, b, 255);
    SDL_Rect rc = { x, y, w, h };
    SDL_RenderFillRect(gRenderer, &rc);
}

static void drawCircleFilled(int cx, int cy, int rad,
                              Uint8 r, Uint8 g, Uint8 b)
{
    SDL_SetRenderDrawColor(gRenderer, r, g, b, 255);
    for (int dy = -rad; dy <= rad; dy++) {
        for (int dx = -rad; dx <= rad; dx++) {
            if (dx * dx + dy * dy <= rad * rad)
                SDL_RenderDrawPoint(gRenderer, cx + dx, cy + dy);
        }
    }
}

static void drawApple(int x, int y) {
    int cx = x + OBJ_SIZE / 2;
    int cy = y + OBJ_SIZE / 2 + 2;
    drawCircleFilled(cx, cy, OBJ_SIZE / 2 - 2, 220, 50, 50);
    fillRect(cx - 1, y + 1, 3, 6, 80, 150, 40);
    SDL_SetRenderDrawColor(gRenderer, 80, 150, 40, 255);
    SDL_RenderDrawLine(gRenderer, cx, y + 4, cx + 6, y + 1);
}

static void drawBomb(int x, int y) {
    int cx = x + OBJ_SIZE / 2;
    int cy = y + OBJ_SIZE / 2 + 2;
    drawCircleFilled(cx, cy, OBJ_SIZE / 2 - 2, 30, 30, 30);
    drawCircleFilled(cx - 4, cy - 4, 3, 80, 80, 80);
    SDL_SetRenderDrawColor(gRenderer, 200, 160, 0, 255);
    SDL_RenderDrawLine(gRenderer, cx + 6, y + 4, cx + 10, y - 2);
    SDL_RenderDrawLine(gRenderer, cx + 10, y - 2, cx + 8, y - 6);
    drawCircleFilled(cx + 8, y - 7, 2, 255, 200, 0);
}

static void drawBasket(int x) {
    fillRect(x, BASK_Y, BASK_W, BASK_H, 180, 110, 40);
    fillRect(x, BASK_Y, BASK_W, 3, 220, 150, 60);
    SDL_SetRenderDrawColor(gRenderer, 140, 85, 25, 255);
    for (int i = 1; i < 5; i++)
        SDL_RenderDrawLine(gRenderer, x, BASK_Y + i * 4,
                           x + BASK_W, BASK_Y + i * 4);
    for (int i = 1; i < 10; i++)
        SDL_RenderDrawLine(gRenderer, x + i * 10, BASK_Y,
                           x + i * 10, BASK_Y + BASK_H);
}

// сердечки для отображения жизней (битмап 7x6)
static const unsigned char HEART[6] = {
    0b0110110,
    0b1111111,
    0b1111111,
    0b0111110,
    0b0011100,
    0b0001000,
};

static void drawHeart(int x, int y, int sz, bool filled) {
    Uint8 r = filled ? 220 : 74;
    Uint8 g = filled ?  50 : 32;
    Uint8 b = filled ?  50 : 32;
    SDL_SetRenderDrawColor(gRenderer, r, g, b, 255);
    for (int row = 0; row < 6; row++) {
        for (int col = 0; col < 7; col++) {
            if (HEART[row] & (1 << (6 - col))) {
                SDL_Rect px = { x + col * sz, y + row * sz, sz, sz };
                SDL_RenderFillRect(gRenderer, &px);
            }
        }
    }
}

static void doUpdate() {
    Uint32 now = SDL_GetTicks();

    if (now - gs.blinkTime >= 500) {
        gs.blinkTime = now;
        gs.blink = !gs.blink;
    }

    if (gs.screen == Screen::GameOver) return;

    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    if (keys[SDL_SCANCODE_LEFT]  && gs.baskX > 0)
        gs.baskX -= BASK_SPD;
    if (keys[SDL_SCANCODE_RIGHT] && gs.baskX < WIN_W - BASK_W)
        gs.baskX += BASK_SPD;

    // спавним новый объект
    if (now - gs.lastSpawn >= (Uint32)SPAWN_DELAY) {
        gs.lastSpawn = now;
        FallingObj o;
        o.x     = (float)(rand() % (WIN_W - OBJ_SIZE));
        o.y     = -(float)OBJ_SIZE;
        o.speed = gs.curSpeed + (rand() % 100) / 60.0f;
        o.apple = (rand() % 4) != 0;
        o.alive = true;
        gs.objs.push_back(o);
    }

    bool lostLife = false;
    for (auto& o : gs.objs) {
        if (!o.alive) continue;
        o.y += o.speed;

        // проверяем столкновение с корзиной
        SDL_Rect box  = { (int)o.x, (int)o.y, OBJ_SIZE, OBJ_SIZE };
        SDL_Rect bask = { gs.baskX, BASK_Y, BASK_W, BASK_H };

        if (SDL_HasIntersection(&box, &bask)) {
            o.alive = false;
            if (o.apple) {
                gs.score++;
                gs.curSpeed = BASE_SPD + (gs.score / 5) * 0.15f;
                // printf("счёт: %d скорость: %.2f\n", gs.score, gs.curSpeed);
            } else {
                // поймали бомбу - конец игры
                if (gs.score > gs.highScore) {
                    gs.highScore = gs.score;
                    gs.newRecord = true;
                    saveBest(gs.highScore);
                }
                gs.screen = Screen::GameOver;
                return;
            }
        } else if (o.y > WIN_H) {
            o.alive = false;
            if (o.apple && !lostLife) {
                lostLife = true;
                gs.lives--;
                gs.flashing  = true;
                gs.flashTime = now;
                if (gs.lives <= 0) {
                    if (gs.score > gs.highScore) {
                        gs.highScore = gs.score;
                        gs.newRecord = true;
                        saveBest(gs.highScore);
                    }
                    gs.screen = Screen::GameOver;
                    return;
                }
            }
        }
    }

    // удаляем мёртвые объекты
    auto it = gs.objs.begin();
    while (it != gs.objs.end()) {
        if (!it->alive) it = gs.objs.erase(it);
        else ++it;
    }
}

static void doDraw() {
    Uint32 now = SDL_GetTicks();

    fillRect(0, 0, WIN_W, WIN_H, 30, 30, 30);
    fillRect(0, WIN_H - 30, WIN_W, 30, 80, 55, 30);

    for (auto& o : gs.objs) {
        if (!o.alive) continue;
        if (o.apple) drawApple((int)o.x, (int)o.y);
        else         drawBomb ((int)o.x, (int)o.y);
    }

    drawBasket(gs.baskX);

    // счёт в левом верхнем углу
    drawNumber(gs.score, 12, 12, 4, 255, 220, 80);

    // жизни (сердечки)
    for (int i = 0; i < MAX_LIVES; i++)
        drawHeart(12 + i * 18, 40, 2, i < gs.lives);

    // рекорд по центру сверху
    {
        Uint8 r = 130, g2 = 130, b = 130;
        if (gs.score > 0 && gs.score >= gs.highScore)
            r = 255, g2 = 200, b = 0;
        int bx = (WIN_W - numWidth(gs.highScore, 2)) / 2;
        drawNumber(gs.highScore, bx, 12, 2, r, g2, b);
    }

    // вспышка при потере жизни
    if (gs.flashing && now - gs.flashTime < 300) {
        float t = 1.0f - (float)(now - gs.flashTime) / 300.0f;
        Uint8 alpha = (Uint8)(t * 0.45f * 255);
        SDL_SetRenderDrawBlendMode(gRenderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(gRenderer, 220, 40, 40, alpha);
        SDL_Rect full = { 0, 0, WIN_W, WIN_H };
        SDL_RenderFillRect(gRenderer, &full);
        SDL_SetRenderDrawBlendMode(gRenderer, SDL_BLENDMODE_NONE);
    }

    if (gs.screen == Screen::GameOver) {
        SDL_SetRenderDrawBlendMode(gRenderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 170);
        SDL_Rect full = { 0, 0, WIN_W, WIN_H };
        SDL_RenderFillRect(gRenderer, &full);
        SDL_SetRenderDrawBlendMode(gRenderer, SDL_BLENDMODE_NONE);

        // TODO: нарисовать надпись "GAME OVER", пока просто показываю счёт
        int gox = (WIN_W - numWidth(gs.score, 5)) / 2;
        drawNumber(gs.score, gox, WIN_H / 2 - 20, 5, 255, 220, 80);

        if (gs.newRecord && gs.blink) {
            int nx = (WIN_W - numWidth(gs.highScore, 3)) / 2;
            drawNumber(gs.highScore, nx, WIN_H / 2 + 45, 3, 80, 220, 80);
        }
    }

    SDL_RenderPresent(gRenderer);
}

static void frame() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            gRunning = false;
#ifdef __EMSCRIPTEN__
            emscripten_cancel_main_loop();
#endif
        }
        if (e.type == SDL_KEYDOWN) {
            if (e.key.keysym.sym == SDLK_ESCAPE) {
                gRunning = false;
#ifdef __EMSCRIPTEN__
                emscripten_cancel_main_loop();
#endif
            }
            if (e.key.keysym.sym == SDLK_r &&
                gs.screen == Screen::GameOver)
            {
                int saved = gs.highScore;
                resetGame();
                gs.highScore = saved;
            }
        }
    }

    doUpdate();
    doDraw();

#ifndef __EMSCRIPTEN__
    SDL_Delay(17); // примерно 60 кадров в секунду
#endif
}

int main(int argc, char* argv[]) {
    srand((unsigned)time(nullptr));

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    gWindow = SDL_CreateWindow("Apple Catcher",
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               WIN_W, WIN_H, 0);
    if (!gWindow) { SDL_Quit(); return 1; }

    gRenderer = SDL_CreateRenderer(gWindow, -1,
                                   SDL_RENDERER_ACCELERATED |
                                   SDL_RENDERER_PRESENTVSYNC);
    if (!gRenderer) { SDL_DestroyWindow(gWindow); SDL_Quit(); return 1; }

    gs.highScore = loadBest();
    resetGame();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(frame, 0, 1);
#else
    while (gRunning) frame();

    SDL_DestroyRenderer(gRenderer);
    SDL_DestroyWindow(gWindow);
    SDL_Quit();
#endif
    return 0;
}
