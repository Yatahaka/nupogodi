#include <SDL2/SDL.h>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>

// ── Constants ────────────────────────────────────────────────────────────────
static constexpr int WINDOW_W  = 800;
static constexpr int WINDOW_H  = 600;
static constexpr int FPS       = 60;
static constexpr int FRAME_MS  = 1000 / FPS;

static constexpr int BASKET_W  = 100;
static constexpr int BASKET_H  = 24;
static constexpr int BASKET_Y  = WINDOW_H - 60;
static constexpr int BASKET_SPEED = 6;

static constexpr int ITEM_SIZE = 28;
static constexpr float BASE_SPEED        = 3.0f;
static constexpr float SPEED_INC         = 0.15f;  // extra speed per 5 pts
static constexpr int   SPAWN_INTERVAL_MS = 1200;

static const char* SAVE_FILE = "highscore.dat";

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

// ── High-score persistence ────────────────────────────────────────────────────
static int loadHighScore()
{
    std::ifstream f(SAVE_FILE, std::ios::binary);
    if (!f.is_open()) return 0;
    int hs = 0;
    f.read(reinterpret_cast<char*>(&hs), sizeof(hs));
    return (f && hs >= 0) ? hs : 0;
}

static void saveHighScore(int hs)
{
    std::ofstream f(SAVE_FILE, std::ios::binary | std::ios::trunc);
    if (f.is_open())
        f.write(reinterpret_cast<const char*>(&hs), sizeof(hs));
}

// ── Tiny bitmap font (3×5) — digits 0-9 + colon ──────────────────────────────
static const Uint8 FONT[11][5] = {
    {0b111,0b101,0b101,0b101,0b111}, // 0
    {0b010,0b110,0b010,0b010,0b111}, // 1
    {0b111,0b001,0b111,0b100,0b111}, // 2
    {0b111,0b001,0b111,0b001,0b111}, // 3
    {0b101,0b101,0b111,0b001,0b001}, // 4
    {0b111,0b100,0b111,0b001,0b111}, // 5
    {0b111,0b100,0b111,0b101,0b111}, // 6
    {0b111,0b001,0b001,0b001,0b001}, // 7
    {0b111,0b101,0b111,0b101,0b111}, // 8
    {0b111,0b101,0b111,0b001,0b111}, // 9
    {0b000,0b010,0b000,0b010,0b000}, // :
};

static void drawChar(SDL_Renderer* r, int x, int y, char ch, int scale, Col c)
{
    int idx = (ch == ':') ? 10 : (ch - '0');
    if (idx < 0 || idx > 10) return;
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
    for (int row = 0; row < 5; ++row)
        for (int col = 0; col < 3; ++col)
            if (FONT[idx][row] & (1 << (2 - col))) {
                SDL_Rect px{x + col * scale, y + row * scale, scale, scale};
                SDL_RenderFillRect(r, &px);
            }
}

static void drawText(SDL_Renderer* r, int x, int y,
                     const std::string& s, int scale, Col c)
{
    int cx = x;
    for (char ch : s) {
        drawChar(r, cx, y, ch, scale, c);
        cx += (3 + 1) * scale;
    }
}

static int textWidth(const std::string& s, int scale)
{
    return static_cast<int>(s.size()) * (3 + 1) * scale;
}

static void drawTextCentered(SDL_Renderer* r, int y,
                              const std::string& s, int scale, Col c)
{
    drawText(r, (WINDOW_W - textWidth(s, scale)) / 2, y, s, scale, c);
}

// ── Drawing helpers ───────────────────────────────────────────────────────────
static void setCol(SDL_Renderer* r, Col c)
{
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
}

static void fillRect(SDL_Renderer* r, int x, int y, int w, int h, Col c)
{
    setCol(r, c);
    SDL_Rect rect{x, y, w, h};
    SDL_RenderFillRect(r, &rect);
}

static void drawCircle(SDL_Renderer* r, int cx, int cy, int radius, Col c)
{
    setCol(r, c);
    for (int dy = -radius; dy <= radius; ++dy)
        for (int dx = -radius; dx <= radius; ++dx)
            if (dx*dx + dy*dy <= radius*radius)
                SDL_RenderDrawPoint(r, cx + dx, cy + dy);
}

static void drawApple(SDL_Renderer* r, int x, int y)
{
    int cx = x + ITEM_SIZE / 2;
    int cy = y + ITEM_SIZE / 2 + 2;
    drawCircle(r, cx, cy, ITEM_SIZE / 2 - 2, C_APPLE);
    fillRect(r, cx - 1, y + 1, 3, 6, C_STEM);
    setCol(r, C_STEM);
    SDL_RenderDrawLine(r, cx, y + 4, cx + 6, y + 1);
}

static void drawBomb(SDL_Renderer* r, int x, int y)
{
    int cx = x + ITEM_SIZE / 2;
    int cy = y + ITEM_SIZE / 2 + 2;
    drawCircle(r, cx, cy, ITEM_SIZE / 2 - 2, C_BOMB);
    drawCircle(r, cx - 4, cy - 4, 3, {80, 80, 80});
    setCol(r, C_FUSE);
    SDL_RenderDrawLine(r, cx + 6, y + 4, cx + 10, y - 2);
    SDL_RenderDrawLine(r, cx + 10, y - 2, cx + 8, y - 6);
    drawCircle(r, cx + 8, y - 7, 2, {255, 200, 0});
}

static void drawBasket(SDL_Renderer* r, int x)
{
    fillRect(r, x, BASKET_Y, BASKET_W, BASKET_H, C_BASKET);
    fillRect(r, x, BASKET_Y, BASKET_W, 3, {220, 150, 60});
    setCol(r, {140, 85, 25});
    for (int i = 1; i < 5; ++i)
        SDL_RenderDrawLine(r, x, BASKET_Y + i * 5, x + BASKET_W, BASKET_Y + i * 5);
    for (int i = 1; i < 10; ++i)
        SDL_RenderDrawLine(r, x + i * 10, BASKET_Y, x + i * 10, BASKET_Y + BASKET_H);
}

// ── Item ─────────────────────────────────────────────────────────────────────
struct Item {
    float x, y, speed;
    bool  isApple, active;
};

// ── Game ──────────────────────────────────────────────────────────────────────
enum class State { PLAYING, GAME_OVER };

struct Game {
    int   basketX    = WINDOW_W / 2 - BASKET_W / 2;
    int   score      = 0;
    int   highScore  = 0;
    bool  newRecord  = false;
    State state      = State::PLAYING;

    std::vector<Item> items;
    Uint32 lastSpawn = 0;
    float  itemSpeed = BASE_SPEED;

    // Blink state for "NEW RECORD" label
    Uint32 blinkTimer = 0;
    bool   blinkOn    = true;

    void reset() {
        basketX   = WINDOW_W / 2 - BASKET_W / 2;
        score     = 0;
        newRecord = false;
        state     = State::PLAYING;
        items.clear();
        lastSpawn = SDL_GetTicks();
        itemSpeed = BASE_SPEED;
        blinkTimer = 0;
        blinkOn    = true;
    }

    void spawnItem(Uint32 now) {
        if (now - lastSpawn < static_cast<Uint32>(SPAWN_INTERVAL_MS)) return;
        lastSpawn = now;

        Item it;
        it.x      = static_cast<float>(std::rand() % (WINDOW_W - ITEM_SIZE));
        it.y      = -static_cast<float>(ITEM_SIZE);
        it.speed  = itemSpeed + static_cast<float>(std::rand() % 100) / 60.0f;
        it.isApple = (std::rand() % 4) != 0;
        it.active = true;
        items.push_back(it);
    }

    void update(const Uint8* keys) {
        Uint32 now = SDL_GetTicks();

        // Blink ticker (500 ms period)
        if (now - blinkTimer >= 500) {
            blinkTimer = now;
            blinkOn = !blinkOn;
        }

        if (state == State::GAME_OVER) return;

        if (keys[SDL_SCANCODE_LEFT]  && basketX > 0)
            basketX -= BASKET_SPEED;
        if (keys[SDL_SCANCODE_RIGHT] && basketX < WINDOW_W - BASKET_W)
            basketX += BASKET_SPEED;

        spawnItem(now);

        SDL_Rect basket{basketX, BASKET_Y, BASKET_W, BASKET_H};

        for (auto& it : items) {
            if (!it.active) continue;
            it.y += it.speed;

            SDL_Rect box{static_cast<int>(it.x), static_cast<int>(it.y),
                         ITEM_SIZE, ITEM_SIZE};

            if (SDL_HasIntersection(&box, &basket)) {
                it.active = false;
                if (it.isApple) {
                    ++score;
                    itemSpeed = BASE_SPEED + (score / 5) * SPEED_INC;
                } else {
                    // Game over — check/save high score
                    if (score > highScore) {
                        highScore = score;
                        newRecord = true;
                        saveHighScore(highScore);
                    }
                    state = State::GAME_OVER;
                    return;
                }
            }

            if (it.y > WINDOW_H) it.active = false;
        }

        items.erase(std::remove_if(items.begin(), items.end(),
                    [](const Item& i){ return !i.active; }), items.end());
    }

    void draw(SDL_Renderer* r) const {
        fillRect(r, 0, 0, WINDOW_W, WINDOW_H, C_BG);
        fillRect(r, 0, WINDOW_H - 30, WINDOW_W, 30, C_GROUND);

        for (const auto& it : items) {
            if (!it.active) continue;
            int ix = static_cast<int>(it.x);
            int iy = static_cast<int>(it.y);
            if (it.isApple) drawApple(r, ix, iy);
            else            drawBomb (r, ix, iy);
        }

        drawBasket(r, basketX);

        // ── HUD ──────────────────────────────────────────────────────────────
        // Current score (left)
        drawText(r, 12, 12, std::to_string(score), 4, C_TEXT_HI);

        // High score (centre)
        std::string hsLabel = "BEST:" + std::to_string(highScore);
        drawTextCentered(r, 12, hsLabel, 2,
                         (score > 0 && score >= highScore) ? C_GOLD : C_DIM);

        // Legend (right)
        std::string legend = "BOMBS:END";
        drawText(r, WINDOW_W - textWidth(legend, 2) - 10, 12,
                 legend, 2, C_RED);

        // ── Game Over overlay ─────────────────────────────────────────────────
        if (state == State::GAME_OVER) {
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r, 0, 0, 0, 165);
            SDL_Rect full{0, 0, WINDOW_W, WINDOW_H};
            SDL_RenderFillRect(r, &full);
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

            drawTextCentered(r, WINDOW_H / 2 - 70, "GAME", 8, C_RED);
            drawTextCentered(r, WINDOW_H / 2,      "OVER", 8, C_RED);

            // Final score
            std::string sc = "SCORE:" + std::to_string(score);
            drawTextCentered(r, WINDOW_H / 2 + 85, sc, 4, C_TEXT_HI);

            // Best score
            std::string hs = "BEST:" + std::to_string(highScore);
            drawTextCentered(r, WINDOW_H / 2 + 115, hs, 3, C_GOLD);

            // NEW RECORD blinking label
            if (newRecord && blinkOn)
                drawTextCentered(r, WINDOW_H / 2 + 145, "NEW:RECORD", 3, C_GREEN);

            drawTextCentered(r, WINDOW_H / 2 + 175, "PRESS:R", 3, C_TEXT);
        }
    }
};

// ── Entry point ───────────────────────────────────────────────────────────────
int main(int /*argc*/, char** /*argv*/)
{
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL_Init error: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Apple Catcher",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_W, WINDOW_H, 0);
    if (!window) {
        SDL_Log("SDL_CreateWindow error: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer error: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    Game game;
    game.highScore = loadHighScore();
    game.reset();

    bool running = true;
    SDL_Event ev;

    while (running) {
        Uint32 frameStart = SDL_GetTicks();

        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = false;
            } else if (ev.type == SDL_KEYDOWN) {
                if (ev.key.keysym.sym == SDLK_ESCAPE) running = false;
                if (ev.key.keysym.sym == SDLK_r && game.state == State::GAME_OVER) {
                    int savedHs = game.highScore;
                    game.reset();
                    game.highScore = savedHs;
                }
            }
        }

        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        game.update(keys);
        game.draw(renderer);
        SDL_RenderPresent(renderer);

        Uint32 elapsed = SDL_GetTicks() - frameStart;
        if (elapsed < static_cast<Uint32>(FRAME_MS))
            SDL_Delay(FRAME_MS - elapsed);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
