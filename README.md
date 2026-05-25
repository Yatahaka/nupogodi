# Apple Catcher 🍎

Аркада на C++ / SDL2, скомпилированная в WebAssembly через Emscripten.

## Запустить онлайн

После деплоя на GitHub Pages игра доступна по адресу:
`https://ВАШ_ЛОГИН.github.io/ИМЯ_РЕПОЗИТОРИЯ/`

## Как задеплоить на GitHub Pages

1. Загрузить все файлы в репозиторий на GitHub
2. Перейти в **Settings → Pages → Source → GitHub Actions**
3. Запушить в ветку `main` — GitHub Actions сам соберёт проект через Emscripten
4. Через ~2 минуты игра будет доступна по ссылке выше

## Собрать локально (нативный SDL2)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/apple-catcher
```

## Собрать для браузера (Emscripten)

```bash
# Требуется установленный emsdk: https://emscripten.org/docs/getting_started/downloads.html
emcmake cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
emmake cmake --build build
# Открыть build/apple-catcher.html в браузере
```

## Управление

| Клавиша | Действие |
|---|---|
| ← → | Двигать корзину |
| R | Рестарт после Game Over |
| Escape | Выход (только в нативной версии) |

## Правила

- 🍎 Поймал яблоко → **+1 очко**
- 💣 Поймал бомбу → **мгновенный конец**
- Пропустил яблоко → **−1 жизнь** (3 жизни)
- Скорость растёт каждые 5 очков
- Рекорд сохраняется в localStorage (веб) или в файле (нативный)
