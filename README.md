# Apple Catcher

Аркада: лови яблоки, уворачивайся от бомб, не упусти ни одного!

## Версии

### C++ / SDL2  →  `apple-catcher/`

Требования: CMake 3.16+, SDL2, GCC / Clang

```bash
cd apple-catcher
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/apple-catcher
```

Управление: ← → двигать корзину, R — рестарт, Escape — выход.

### Web / React + Vite  →  `apple-catcher-web/`

Требования: Node.js 18+, pnpm (или npm)

```bash
cd apple-catcher-web
pnpm install      # или: npm install
pnpm dev          # или: npm run dev
```

Открыть http://localhost:5173 в браузере.

Управление: ← → двигать корзину, R — рестарт.

## Правила
- 🍎 Поймал яблоко → +1 очко
- 💣 Поймал бомбу  → немедленный конец
- Пропустил яблоко → −1 жизнь (3 жизни)
- Скорость растёт каждые 5 очков
- Рекорд сохраняется между сессиями
