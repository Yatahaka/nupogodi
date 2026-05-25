#include <SDL2/SDL.h>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>
#include <algorithm>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#include <fstream>
#endif

// ── Constants ────────────────────────────────────────────────────────────────
static constexpr int WINDOW_W  = 800;
static constexpr int WINDOW_H  = 600;
static constexpr int FRAME_MS  = 1000 / 60;

static constexpr int BASKET_W     = 100;
static constexpr int BASKET_H     = 24;
static constexpr int BASKET_Y     = WINDOW_H - 60;
static constexpr int BASKET_SPEED = 6;
static constexpr int MAX_LIVES    = 3;

static constexpr int   ITEM_SIZE         = 28;
static constexpr float BASE_SPEED        = 3.0f;
static constexpr float SPEED_INC         = 0.15f;
static constexpr int   SPAWN_INTERVAL_MS = 1200;

#ifndef __EMSCRIPTEN__
static const char* SAVE_FILE = "highscore.dat";
#endif

// ── Colours ──────────────────────────────────────────────────────────────────
struct Col { Uint8 r, g, b; };
static constexpr Col C_BG      {  30,  30,  30 };
static constexpr Col C_GROUND  {  80,  55,  30 };
static constexpr Col C_BASKET  { 180, 110,  40 };
static constexpr Col C_APPLE   { 220,  50,  50 };
static constexpr Col C_STEM    {  80, 150,  40 };
static constexpr Col C_BOMB    {  30,  30,  30 };
static constexpr Col C_FUSE    { 200, 160,   0 };
static constexpr Col C_TEXT_HI { 255, 220,  80 };
static constexpr Col C_TEXT    { 220, 220, 220 };
static constexpr Col C_RED     { 220,  60,  60 };
static constexpr Col C_GREEN   {  80, 220,  80 };
static constexpr Col C_GOLD    { 255, 200,   0 };
static constexpr Col C_DIM     { 130, 130, 130 };
static constexpr Col C_HEART   { 220,  50,  50 };
static constexpr Col C_HEART_D {  74,  32,  32 };

// ── High-score I/O ────────────────────────────────────────────────────────────
static int loadHighScore()
{
#ifdef __EMSCRIPTEN__
    return EM_ASM_INT({
        return parseInt(localStorage.getItem('appleCatcherBest') || '0', 10);
    });
#else
    std::ifstream f(SAVE_FILE, std::ios::binary);
    if (!f.is_open()) return 0;
    int hs = 0;
    f.read(reinterpret_cast<char*>(&hs), sizeof(hs));
    return (f && hs >= 0) ? hs : 0;
#endif
}

static void saveHighScore(int hs)
{
#ifdef __EMSCRIPTEN__
    EM_ASM({ localStorage.setItem('appleCatcherBest', $0); }, hs);
#else
    std::ofstream f(SAVE_FILE, std::ios::binary | std::ios::trunc);
    if (f.is_open())
        f.write(reinterpret_cast<const char*>(&hs), sizeof(hs));
#endif
}

// ── Pixel font (3×5): digits 0–9 + colon ─────────────────────────────────────
static const Uint8 FONT[11][5] = {
    {0b111,0b101,0b101,0b101,0b111},
    {0b010,0b110,0b010,0b010,0b111},
    {0b111,0b001,0b111,0b100,0b111},
    {0b111,0b001,0b111,0b001,0b111},
    {0b101,0b101,0b111,0b001,0b001},
    {0b111,0b100,0b111,0b001,0b111},
    {0b111,0b100,0b111,0b101,0b111},
    {0b111,0b001,0b001,0b001,0b001},
    {0b111,0b101,0b111,0b101,0b111},
    {0b111,0b101,0b111,0b001,0b111},
    {0b000,0b010,0b000,0b010,0b000},
};

static void drawChar(SDL_Renderer* r, int x, int y, char ch, int scale, Col c)
{
    int idx = (ch == ':') ? 10 : (ch - '0');
    if (idx < 0 || idx > 10) return;
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
    for (int row = 0; row < 5; ++row)
        for (int col = 0; col < 3; ++col)
            if (FONT[idx][row] & (1 << (2 - col))) {
                SDL_Rect px{x + col*scale, y + row*scale, scale, scale};
                SDL_RenderFillRect(r, &px);
            }
}

static void drawText(SDL_Renderer* r, int x, int y,
                     const std::string& s, int scale, Col c)
{
    int cx = x;
    for (char ch : s) { drawChar(r, cx, y, ch, scale, c); cx += (3+1)*scale; }
}

static int textWidth(const std::string& s, int scale)
{
    return static_cast<int>(s.size()) * (3+1) * scale;
}

static void drawTextCentered(SDL_Renderer* r, int y,
                              const std::string& s, int scale, Col c)
{
    drawText(r, (WINDOW_W - textWidth(s, scale)) / 2, y, s, scale, c);
}

// ── Drawing helpers ───────────────────────────────────────────────────────────
static void fillRect(SDL_Renderer* r, int x, int y, int w, int h, Col c)
{
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
    SDL_Rect rect{x, y, w, h};
    SDL_RenderFillRect(r, &rect);
}

static void drawCircle(SDL_Renderer* r, int cx, int cy, int radius, Col c)
{
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
    for (int dy = -radius; dy <= radius; ++dy)
        for (int dx = -radius; dx <= radius; ++dx)
            if (dx*dx + dy*dy <= radius*radius)
                SDL_RenderDrawPoint(r, cx+dx, cy+dy);
}

static void drawApple(SDL_Renderer* r, int x, int y)
{
    int cx = x + ITEM_SIZE/2, cy = y + ITEM_SIZE/2 + 2;
    drawCircle(r, cx, cy, ITEM_SIZE/2 - 2, C_APPLE);
    fillRect(r, cx-1, y+1, 3, 6, C_STEM);
    SDL_SetRenderDrawColor(r, C_STEM.r, C_STEM.g, C_STEM.b, 255);
    SDL_RenderDrawLine(r, cx, y+4, cx+6, y+1);
}

static void drawBomb(SDL_Renderer* r, int x, int y)
{
    int cx = x + ITEM_SIZE/2, cy = y + ITEM_SIZE/2 + 2;
    drawCircle(r, cx, cy, ITEM_SIZE/2 - 2, C_BOMB);
    drawCircle(r, cx-4, cy-4, 3, {80,80,80});
    SDL_SetRenderDrawColor(r, C_FUSE.r, C_FUSE.g, C_FUSE.b, 255);
    SDL_RenderDrawLine(r, cx+6, y+4, cx+10, y-2);
    SDL_RenderDrawLine(r, cx+10, y-2, cx+8, y-6);
    drawCircle(r, cx+8, y-7, 2, {255,200,0});
}

static void drawBasket(SDL_Renderer* r, int x)
{
    fillRect(r, x, BASKET_Y, BASKET_W, BASKET_H, C_BASKET);
    fillRect(r, x, BASKET_Y, BASKET_W, 3, {220,150,60});
    SDL_SetRenderDrawColor(r, 140, 85, 25, 255);
    for (int i = 1; i < 5; ++i)
        SDL_RenderDrawLine(r, x, BASKET_Y+i*5, x+BASKET_W, BASKET_Y+i*5);
    for (int i = 1; i < 10; ++i)
        SDL_RenderDrawLine(r, x+i*10, BASKET_Y, x+i*10, BASKET_Y+BASKET_H);
}

static const Uint8 HEART_ROWS[6] = {
    0b0110110,
    0b1111111,
    0b1111111,
    0b0111110,
    0b0011100,
    0b0001000,
};

static void drawHeart(SDL_Renderer* r, int x, int y, int scale, bool filled)
{
    Col c = filled ? C_HEART : C_HEART_D;
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
    for (int row = 0; row < 6; ++row)
        for (int col = 0; col < 7; ++col)
            if (HEART_ROWS[row] & (1 << (6 - col))) {
                SDL_Rect px{x + col*scale, y + row*scale, scale, scale};
                SDL_RenderFillRect(r, &px);
            }
}

// ── Game ──────────────────────────────────────────────────────────────────────
struct Item { float x, y, speed; bool isApple, active; };
enum class State { PLAYING, GAME_OVER };

struct Game {
    int   basketX   = WINDOW_W/2 - BASKET_W/2;
    int   score     = 0;
    int   lives     = MAX_LIVES;
    int   highScore = 0;
    bool  newRecord = false;
    State state     = State::PLAYING;

    std::vector<Item> items;
    Uint32 lastSpawn  = 0;
    float  itemSpeed  = BASE_SPEED;
    Uint32 flashStart = 0;
    bool   flashing   = false;
    Uint32 blinkTimer = 0;
    bool   blinkOn    = true;

    void reset() {
        basketX   = WINDOW_W/2 - BASKET_W/2;
        score     = 0;
        lives     = MAX_LIVES;
        newRecord = false;
        state     = State::PLAYING;
        items.clear();
        lastSpawn  = SDL_GetTicks();
        itemSpeed  = BASE_SPEED;
        flashing   = false;
        blinkOn    = true;
    }

    void spawnItem(Uint32 now) {
        if (now - lastSpawn < static_cast<Uint32>(SPAWN_INTERVAL_MS)) return;
        lastSpawn = now;
        Item it;
        it.x       = static_cast<float>(std::rand() % (WINDOW_W - ITEM_SIZE));
        it.y       = -static_cast<float>(ITEM_SIZE);
        it.speed   = itemSpeed + static_cast<float>(std::rand() % 100) / 60.0f;
        it.isApple = (std::rand() % 4) != 0;
        it.active  = true;
        items.push_back(it);
    }

    void triggerGameOver() {
        if (score > highScore) {
            highScore = score;
            newRecord = true;
            saveHighScore(highScore);
        }
        state = State::GAME_OVER;
    }

    void update(const Uint8* keys) {
        Uint32 now = SDL_GetTicks();
        if (now - blinkTimer >= 500) { blinkTimer = now; blinkOn = !blinkOn; }
        if (state == State::GAME_OVER) return;

        if (keys[SDL_SCANCODE_LEFT]  && basketX > 0)
            basketX -= BASKET_SPEED;
        if (keys[SDL_SCANCODE_RIGHT] && basketX < WINDOW_W - BASKET_W)
            basketX += BASKET_SPEED;

        spawnItem(now);

        bool lifeLost = false;
        for (auto& it : items) {
            if (!it.active) continue;
            it.y += it.speed;
            SDL_Rect box{(int)it.x, (int)it.y, ITEM_SIZE, ITEM_SIZE};
            SDL_Rect basket{basketX, BASKET_Y, BASKET_W, BASKET_H};
            if (SDL_HasIntersection(&box, &basket)) {
                it.active = false;
                if (it.isApple) { ++score; itemSpeed = BASE_SPEED + (score/5)*SPEED_INC; }
                else { triggerGameOver(); return; }
            } else if (it.y > WINDOW_H) {
                it.active = false;
                if (it.isApple && !lifeLost) {
                    lifeLost = true;
                    if (--lives <= 0) { triggerGameOver(); return; }
                    flashing = true; flashStart = now;
                }
            }
        }
        items.erase(std::remove_if(items.begin(), items.end(),
            [](const Item& i){ return !i.active; }), items.end());
    }

    void draw(SDL_Renderer* r) const {
        fillRect(r, 0, 0, WINDOW_W, WINDOW_H, C_BG);
        fillRect(r, 0, WINDOW_H-30, WINDOW_W, 30, C_GROUND);

        for (const auto& it : items) {
            if (!it.active) continue;
            if (it.isApple) drawApple(r, (int)it.x, (int)it.y);
            else            drawBomb (r, (int)it.x, (int)it.y);
        }
        drawBasket(r, basketX);

        drawText(r, 12, 12, std::to_string(score), 4, C_TEXT_HI);
        for (int i = 0; i < MAX_LIVES; ++i)
            drawHeart(r, 12 + i*18, 40, 2, i < lives);

        std::string hsl = "BEST:" + std::to_string(highScore);
        Col hsCol = (score > 0 && score >= highScore) ? C_GOLD : C_DIM;
        drawTextCentered(r, 12, hsl, 2, hsCol);

        std::string leg = "BOMBS:END";
        drawText(r, WINDOW_W - textWidth(leg, 2) - 10, 12, leg, 2, C_RED);

        if (flashing) {
            Uint32 elapsed = SDL_GetTicks() - flashStart;
            if (elapsed < 300) {
                float alpha = (1.0f - elapsed/300.0f) * 0.45f;
                SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(r, 220, 40, 40, (Uint8)(alpha*255));
                SDL_Rect full{0,0,WINDOW_W,WINDOW_H};
                SDL_RenderFillRect(r, &full);
                SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
            }
        }

        if (state == State::GAME_OVER) {
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r, 0, 0, 0, 165);
            SDL_Rect full{0,0,WINDOW_W,WINDOW_H};
            SDL_RenderFillRect(r, &full);
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
            drawTextCentered(r, WINDOW_H/2-70, "GAME", 8, C_RED);
            drawTextCentered(r, WINDOW_H/2,    "OVER", 8, C_RED);
            drawTextCentered(r, WINDOW_H/2+85,  "SCORE:"+std::to_string(score), 4, C_TEXT_HI);
            drawTextCentered(r, WINDOW_H/2+115, "BEST:"+std::to_string(highScore), 3, C_GOLD);
            if (newRecord && blinkOn)
                drawTextCentered(r, WINDOW_H/2+145, "NEW:RECORD", 3, C_GREEN);
            drawTextCentered(r, WINDOW_H/2+175, "PRESS:R", 3, C_TEXT);
        }
    }
};

// ── Global context for Emscripten callback ────────────────────────────────────
struct AppCtx {
    SDL_Window*   window;
    SDL_Renderer* renderer;
    Game          game;
    bool          running;
};

static AppCtx* g_ctx = nullptr;

static void runFrame()
{
    if (!g_ctx) return;
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) {
            g_ctx->running = false;
#ifdef __EMSCRIPTEN__
            emscripten_cancel_main_loop();
#endif
        } else if (ev.type == SDL_KEYDOWN) {
            if (ev.key.keysym.sym == SDLK_ESCAPE) {
                g_ctx->running = false;
#ifdef __EMSCRIPTEN__
                emscripten_cancel_main_loop();
#endif
            }
            if (ev.key.keysym.sym == SDLK_r &&
                g_ctx->game.state == State::GAME_OVER) {
                int savedHs = g_ctx->game.highScore;
                g_ctx->game.reset();
                g_ctx->game.highScore = savedHs;
            }
        }
    }
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    g_ctx->game.update(keys);
    g_ctx->game.draw(g_ctx->renderer);
    SDL_RenderPresent(g_ctx->renderer);

#ifndef __EMSCRIPTEN__
    static Uint32 lastFrame = 0;
    Uint32 elapsed = SDL_GetTicks() - lastFrame;
    if (elapsed < static_cast<Uint32>(FRAME_MS))
        SDL_Delay(FRAME_MS - elapsed);
    lastFrame = SDL_GetTicks();
#endif
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main(int, char**)
{
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;

    SDL_Window* window = SDL_CreateWindow(
        "Apple Catcher",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_W, WINDOW_H, 0);
    if (!window) { SDL_Quit(); return 1; }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) { SDL_DestroyWindow(window); SDL_Quit(); return 1; }

    g_ctx = new AppCtx{window, renderer, Game{}, true};
    g_ctx->game.highScore = loadHighScore();
    g_ctx->game.reset();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(runFrame, 0, 1);
#else
    while (g_ctx->running) runFrame();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    delete g_ctx;
    g_ctx = nullptr;
#endif
    return 0;
}
