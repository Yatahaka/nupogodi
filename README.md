# Apple Catcher

Arcade game in C++ using SDL3. Catch apples with the basket, avoid bombs.

Written as a university project. Compiled to WebAssembly with Emscripten for browser deployment.

## Controls

Arrow keys left/right to move the basket.
Press R to restart after game over.

## Rules

- Catching an apple gives 1 point
- Catching a bomb ends the game immediately
- Missing an apple costs 1 life (3 lives total)
- Speed increases every 5 points

## Build - native

Requires SDL3 installed on your system.

```
cmake -S . -B build
cmake --build build
./build/apple_catcher
```

## Build - browser (Emscripten)

Requires emsdk: https://emscripten.org/docs/getting_started/downloads.html

```
emcmake cmake -S . -B build
cd build
emmake make
```

Open `build/apple_catcher.html` in a browser.

## GitHub Pages deployment

Push the code to a GitHub repository, then go to Settings > Pages and set the source to GitHub Actions. The workflow will build and deploy automatically on each push to main.

Note: high score is not saved between sessions in the browser version.
