import { useEffect, useRef } from "react";

const W = 800;
const H = 600;
const BASKET_W = 100;
const BASKET_H = 24;
const BASKET_Y = H - 60;
const BASKET_SPEED = 6;
const ITEM_SIZE = 28;
const BASE_SPEED = 3.0;
const SPEED_INC = 0.15;
const SPAWN_MS = 1200;
const MAX_LIVES = 3;
const HS_KEY = "appleCatcherBest";
const FLASH_MS = 300; // red flash duration when life lost

type State = "playing" | "gameover";

interface Item {
  x: number;
  y: number;
  speed: number;
  isApple: boolean;
  active: boolean;
}

// ── pixel font (digits 0-9 + colon) ─────────────────────────────────────────
const FONT: number[][] = [
  [0b111, 0b101, 0b101, 0b101, 0b111],
  [0b010, 0b110, 0b010, 0b010, 0b111],
  [0b111, 0b001, 0b111, 0b100, 0b111],
  [0b111, 0b001, 0b111, 0b001, 0b111],
  [0b101, 0b101, 0b111, 0b001, 0b001],
  [0b111, 0b100, 0b111, 0b001, 0b111],
  [0b111, 0b100, 0b111, 0b101, 0b111],
  [0b111, 0b001, 0b001, 0b001, 0b001],
  [0b111, 0b101, 0b111, 0b101, 0b111],
  [0b111, 0b101, 0b111, 0b001, 0b111],
  [0b000, 0b010, 0b000, 0b010, 0b000],
];

function charIdx(ch: string): number {
  if (ch === ":") return 10;
  const n = parseInt(ch, 10);
  return isNaN(n) ? -1 : n;
}

function drawPixelChar(
  ctx: CanvasRenderingContext2D,
  x: number, y: number, ch: string, scale: number
) {
  const idx = charIdx(ch);
  if (idx < 0) return;
  for (let r = 0; r < 5; r++)
    for (let c = 0; c < 3; c++)
      if (FONT[idx][r] & (1 << (2 - c)))
        ctx.fillRect(x + c * scale, y + r * scale, scale, scale);
}

function drawPixelText(
  ctx: CanvasRenderingContext2D,
  x: number, y: number, text: string, scale: number, color: string
) {
  ctx.fillStyle = color;
  let cx = x;
  for (const ch of text) {
    drawPixelChar(ctx, cx, y, ch, scale);
    cx += (3 + 1) * scale;
  }
}

function pxW(text: string, scale: number) {
  return text.length * (3 + 1) * scale;
}

function drawPixelCentered(
  ctx: CanvasRenderingContext2D,
  y: number, text: string, scale: number, color: string
) {
  drawPixelText(ctx, (W - pxW(text, scale)) / 2, y, text, scale, color);
}

// ── pixel heart (9 × 8 bitmap) ───────────────────────────────────────────────
//  .##.##.
//  #######
//  #######
//  .#####.
//  ..###..
//  ...#...
const HEART_ROWS = [
  0b0110110,
  0b1111111,
  0b1111111,
  0b0111110,
  0b0011100,
  0b0001000,
];
const HEART_W = 7;

function drawHeart(
  ctx: CanvasRenderingContext2D,
  x: number, y: number, scale: number, filled: boolean
) {
  ctx.fillStyle = filled ? "#e03232" : "#4a2020";
  for (let r = 0; r < HEART_ROWS.length; r++)
    for (let c = 0; c < HEART_W; c++)
      if (HEART_ROWS[r] & (1 << (HEART_W - 1 - c)))
        ctx.fillRect(x + c * scale, y + r * scale, scale, scale);
}

// ── drawing helpers ──────────────────────────────────────────────────────────
function drawCircle(
  ctx: CanvasRenderingContext2D,
  cx: number, cy: number, r: number, color: string
) {
  ctx.fillStyle = color;
  ctx.beginPath();
  ctx.arc(cx, cy, r, 0, Math.PI * 2);
  ctx.fill();
}

function drawApple(ctx: CanvasRenderingContext2D, x: number, y: number) {
  const cx = x + ITEM_SIZE / 2;
  const cy = y + ITEM_SIZE / 2 + 2;
  drawCircle(ctx, cx, cy, ITEM_SIZE / 2 - 2, "#dc3232");
  ctx.fillStyle = "#50963c";
  ctx.fillRect(cx - 1, y + 1, 3, 6);
  ctx.strokeStyle = "#50963c";
  ctx.lineWidth = 1.5;
  ctx.beginPath();
  ctx.moveTo(cx, y + 4);
  ctx.lineTo(cx + 6, y + 1);
  ctx.stroke();
}

function drawBomb(ctx: CanvasRenderingContext2D, x: number, y: number) {
  const cx = x + ITEM_SIZE / 2;
  const cy = y + ITEM_SIZE / 2 + 2;
  drawCircle(ctx, cx, cy, ITEM_SIZE / 2 - 2, "#1e1e1e");
  drawCircle(ctx, cx - 4, cy - 4, 3, "#505050");
  ctx.strokeStyle = "#c8a000";
  ctx.lineWidth = 2;
  ctx.beginPath();
  ctx.moveTo(cx + 6, y + 4);
  ctx.lineTo(cx + 10, y - 2);
  ctx.lineTo(cx + 8, y - 6);
  ctx.stroke();
  drawCircle(ctx, cx + 8, y - 7, 2, "#ffc800");
}

function drawBasket(ctx: CanvasRenderingContext2D, x: number) {
  ctx.fillStyle = "#b46e28";
  ctx.fillRect(x, BASKET_Y, BASKET_W, BASKET_H);
  ctx.fillStyle = "#dc9632";
  ctx.fillRect(x, BASKET_Y, BASKET_W, 3);
  ctx.strokeStyle = "#8c5519";
  ctx.lineWidth = 1;
  for (let i = 1; i < 5; i++) {
    ctx.beginPath();
    ctx.moveTo(x, BASKET_Y + i * 5);
    ctx.lineTo(x + BASKET_W, BASKET_Y + i * 5);
    ctx.stroke();
  }
  for (let i = 1; i < 10; i++) {
    ctx.beginPath();
    ctx.moveTo(x + i * 10, BASKET_Y);
    ctx.lineTo(x + i * 10, BASKET_Y + BASKET_H);
    ctx.stroke();
  }
}

// ── renderer ─────────────────────────────────────────────────────────────────
interface RenderState {
  basketX: number;
  score: number;
  highScore: number;
  lives: number;
  items: Item[];
  gameState: State;
  newRecord: boolean;
  blinkOn: boolean;
  isBeating: boolean;
  flashAlpha: number; // 0..1 red damage flash
}

function renderFrame(ctx: CanvasRenderingContext2D, s: RenderState) {
  ctx.fillStyle = "#1e1e1e";
  ctx.fillRect(0, 0, W, H);
  ctx.fillStyle = "#50371e";
  ctx.fillRect(0, H - 30, W, 30);

  for (const it of s.items) {
    if (!it.active) continue;
    if (it.isApple) drawApple(ctx, Math.round(it.x), Math.round(it.y));
    else drawBomb(ctx, Math.round(it.x), Math.round(it.y));
  }

  drawBasket(ctx, s.basketX);

  // ── HUD ──────────────────────────────────────────────────────────────────
  // score (top-left)
  drawPixelText(ctx, 12, 12, String(s.score), 4, "#ffdc50");

  // hearts (below score, top-left)
  const heartScale = 2;
  const heartSpacing = (HEART_W + 2) * heartScale;
  for (let i = 0; i < MAX_LIVES; i++) {
    drawHeart(ctx, 12 + i * heartSpacing, 38, heartScale, i < s.lives);
  }

  // best (top-centre)
  const hsLabel = "BEST:" + s.highScore;
  const hsColor = s.isBeating ? "#ffc800" : "#828282";
  drawPixelCentered(ctx, 12, hsLabel, 2, hsColor);

  // bombs (top-right)
  const legend = "BOMBS:END";
  drawPixelText(ctx, W - pxW(legend, 2) - 10, 12, legend, 2, "#dc3c3c");

  // ── damage flash ─────────────────────────────────────────────────────────
  if (s.flashAlpha > 0) {
    ctx.fillStyle = `rgba(220,40,40,${s.flashAlpha * 0.45})`;
    ctx.fillRect(0, 0, W, H);
  }

  // ── game over overlay ─────────────────────────────────────────────────────
  if (s.gameState === "gameover") {
    ctx.fillStyle = "rgba(0,0,0,0.65)";
    ctx.fillRect(0, 0, W, H);

    drawPixelCentered(ctx, H / 2 - 70, "GAME", 8, "#dc3c3c");
    drawPixelCentered(ctx, H / 2, "OVER", 8, "#dc3c3c");

    drawPixelCentered(ctx, H / 2 + 85, "SCORE:" + s.score, 4, "#ffdc50");
    drawPixelCentered(ctx, H / 2 + 115, "BEST:" + s.highScore, 3, "#ffc800");

    if (s.newRecord && s.blinkOn)
      drawPixelCentered(ctx, H / 2 + 145, "NEW:RECORD", 3, "#50dc50");

    drawPixelCentered(ctx, H / 2 + 178, "PRESS:R", 3, "#dcdcdc");
  }
}

// ── component ────────────────────────────────────────────────────────────────
export default function App() {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext("2d")!;

    let basketX = W / 2 - BASKET_W / 2;
    let score = 0;
    let lives = MAX_LIVES;
    let highScore = parseInt(localStorage.getItem(HS_KEY) ?? "0", 10) || 0;
    let newRecord = false;
    let gameState: State = "playing";
    let items: Item[] = [];
    let lastSpawn = performance.now();
    let itemSpeed = BASE_SPEED;
    let blinkTimer = performance.now();
    let blinkOn = true;
    let flashStart = -Infinity; // timestamp when last life was lost
    let rafId = 0;

    const keys: Record<string, boolean> = {};

    const onKey = (e: KeyboardEvent) => {
      keys[e.code] = e.type === "keydown";
      if (e.type === "keydown" && e.code === "KeyR" && gameState === "gameover")
        restart();
    };
    window.addEventListener("keydown", onKey);
    window.addEventListener("keyup", onKey);

    function restart() {
      basketX = W / 2 - BASKET_W / 2;
      score = 0;
      lives = MAX_LIVES;
      newRecord = false;
      gameState = "playing";
      items = [];
      lastSpawn = performance.now();
      itemSpeed = BASE_SPEED;
      flashStart = -Infinity;
    }

    function spawnItem(now: number) {
      if (now - lastSpawn < SPAWN_MS) return;
      lastSpawn = now;
      items.push({
        x: Math.random() * (W - ITEM_SIZE),
        y: -ITEM_SIZE,
        speed: itemSpeed + Math.random() * 1.67,
        isApple: Math.random() < 0.75,
        active: true,
      });
    }

    function endGame(score_: number) {
      if (score_ > highScore) {
        highScore = score_;
        newRecord = true;
        localStorage.setItem(HS_KEY, String(highScore));
      }
      gameState = "gameover";
    }

    function tick(now: number) {
      // blink timer
      if (now - blinkTimer >= 500) {
        blinkTimer = now;
        blinkOn = !blinkOn;
      }

      if (gameState === "playing") {
        if (keys["ArrowLeft"] && basketX > 0) basketX -= BASKET_SPEED;
        if (keys["ArrowRight"] && basketX < W - BASKET_W) basketX += BASKET_SPEED;

        spawnItem(now);

        let lifeLostThisFrame = false;

        for (const it of items) {
          if (!it.active) continue;
          it.y += it.speed;

          const hit =
            it.x + ITEM_SIZE > basketX &&
            it.x < basketX + BASKET_W &&
            it.y + ITEM_SIZE > BASKET_Y &&
            it.y < BASKET_Y + BASKET_H;

          if (hit) {
            it.active = false;
            if (it.isApple) {
              score++;
              itemSpeed = BASE_SPEED + Math.floor(score / 5) * SPEED_INC;
            } else {
              endGame(score);
              break;
            }
          } else if (it.isApple && it.y > H) {
            // Apple missed — lose a life
            it.active = false;
            if (!lifeLostThisFrame) {
              lifeLostThisFrame = true;
              lives--;
              flashStart = now;
              if (lives <= 0) {
                endGame(score);
                break;
              }
            }
          } else if (!it.isApple && it.y > H) {
            it.active = false;
          }
        }

        items = items.filter((it) => it.active);
      }

      // flash alpha: decays over FLASH_MS
      const elapsed = now - flashStart;
      const flashAlpha = elapsed < FLASH_MS ? 1 - elapsed / FLASH_MS : 0;

      renderFrame(ctx, {
        basketX,
        score,
        highScore,
        lives,
        items,
        gameState,
        newRecord,
        blinkOn,
        isBeating: score > 0 && score >= highScore,
        flashAlpha,
      });

      rafId = requestAnimationFrame(tick);
    }

    rafId = requestAnimationFrame(tick);

    return () => {
      cancelAnimationFrame(rafId);
      window.removeEventListener("keydown", onKey);
      window.removeEventListener("keyup", onKey);
    };
  }, []);

  return (
    <div
      style={{
        background: "#111",
        minHeight: "100vh",
        display: "flex",
        flexDirection: "column",
        alignItems: "center",
        justifyContent: "center",
        gap: 12,
        fontFamily: "monospace",
      }}
    >
      <canvas
        ref={canvasRef}
        width={W}
        height={H}
        style={{ display: "block", imageRendering: "pixelated", maxWidth: "100%" }}
      />
      <p style={{ color: "#828282", fontSize: 13, margin: 0 }}>
        ← → Move &nbsp;|&nbsp; R Restart &nbsp;|&nbsp; Catch 🍎 avoid 💣 &nbsp;|&nbsp; Miss apple = −❤️
      </p>
    </div>
  );
}
