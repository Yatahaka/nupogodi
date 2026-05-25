# Apple Catcher 🍎

C++ / SDL2 аркада, скомпилированная в WebAssembly через Emscripten.

## Деплой на GitHub Pages

1. Залить файлы в репозиторий (`main` ветка)
2. **Settings → Pages → Source → GitHub Actions**
3. Запушить в `main` — Actions сам соберёт и задеплоит
4. Игра появится на `https://ВАШ_ЛОГИН.github.io/ИМЯ_РЕПО/`

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
