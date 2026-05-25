# Apple Catcher 🍎

C++ / SDL2 аркада, скомпилированная в WebAssembly через Emscripten.

## Собрать локально (нативный SDL2)

```bash
cmake -S . -B build
cmake --build build
./build/apple-catcher
```

## Собрать для браузера (Emscripten)

```bash
emcmake cmake -S . -B build
cd build && emmake make
# открыть build/apple-catcher.html
```

## Управление

| Клавиша | Действие |
|---|---|
| ← → | Двигать корзину |
| R | Рестарт после Game Over |
