#include <LiquidCrystal.h>

LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

const int joyX = A1;
const int joyY = A2; // not used for movement, only as extra entropy for seeding
const int joySW = 7;

const uint8_t COLS = 16;

// Wall speed starts slow and ramps up as the score increases, down to a
// floor so it never becomes unplayable. Player speed is always 1/5th of
// the current wall interval, so it stays 5x as fast as the walls.
const unsigned long WALL_INTERVAL_START_MS = 1600;
const unsigned long WALL_INTERVAL_MIN_MS = 500;
const unsigned long WALL_INTERVAL_STEP_MS = 20; // speedup per wall tick

unsigned long wallInterval;
unsigned long playerInterval;

// Single master timer driven at player speed. The wall only ever
// shifts as an action appended to every 5th player tick (subTick wraps
// 0..4), so player movement is applied unconditionally on every tick,
// including the tick where the walls also move.
unsigned long lastTick;
uint8_t subTick;

// Max columns the gap is allowed to jump from one spawned line to the
// next. Kept small early on so the path is always reachable, then
// widened as the wall speed ramps up, in step with the same progress
// used for the speed ramp.
const int GAP_JUMP_START = 3;
const int GAP_JUMP_MAX = 6;
int gapJump;

// Gap width (in columns) starts wide and forgiving, then narrows down
// to a single column as the game gets harder, on the same ramp.
const int GAP_WIDTH_START = 5;
const int GAP_WIDTH_MIN = 1;
int gapWidthCurrent; // width used for newly spawned lines

// Joystick centers vary board to board, so calibrate at boot instead of
// hardcoding thresholds (assumes the stick is untouched at power-up).
int joyCenterX;
const int JOY_DEADZONE = 150;

// wallGapStart[i] / wallGapWidth[i] describe the gap (columns with NO
// wall) for that line: columns [start, start + width - 1] are open,
// everything else in that line is a wall. Width is frozen at spawn
// time per line, even as gapWidthCurrent narrows for future spawns.
// Index 0 = top LCD row,    pixel-row 2 (topmost line, spawns here)
// Index 1 = top LCD row,    pixel-row 6
// Index 2 = bottom LCD row, pixel-row 2
// Index 3 = bottom LCD row, pixel-row 6 (last row shown before a line
//           scrolls off screen; that's when it's checked, see below)
int wallGapStart[4];
int wallGapWidth[4];

int playerCol;
bool gameOver;
unsigned long score;
unsigned long highScore; // persists across rounds, resets on power cycle

// Each custom character encodes 3 independent bits as pixels:
//   row index 1 (2nd from top) -> wall1 bar
//   row index 5 (6th from top) -> wall2 bar
//   row index 7 (bottom)       -> player dot
// 2^3 = 8 combinations, exactly filling the 8 available CGRAM slots.
void buildChar(uint8_t idx, bool player, bool w1, bool w2) {
  byte data[8];
  data[0] = 0b00000;
  data[1] = w1 ? 0b11111 : 0b00000;
  data[2] = 0b00000;
  data[3] = 0b00000;
  data[4] = 0b00000;
  data[5] = w2 ? 0b11111 : 0b00000;
  data[6] = 0b00000;
  data[7] = player ? 0b00100 : 0b00000;
  lcd.createChar(idx, data);
}

void setupChars() {
  for (uint8_t i = 0; i < 8; i++) {
    buildChar(i, i & 0x4, i & 0x2, i & 0x1);
  }
}

uint8_t charIndex(bool player, bool w1, bool w2) {
  return (player ? 4 : 0) | (w1 ? 2 : 0) | (w2 ? 1 : 0);
}

void calibrateJoystick() {
  long sum = 0;
  const uint8_t samples = 16;
  for (uint8_t i = 0; i < samples; i++) {
    sum += analogRead(joyX);
    delay(2);
  }
  joyCenterX = sum / samples;
}

void resetGame() {
  playerCol = COLS / 2;
  gapWidthCurrent = GAP_WIDTH_START;
  // Start every line clear around the player's column so there's no
  // chance of an instant collision; hazards only appear as new lines
  // spawn at the top and scroll down.
  int startGapStart = constrain(playerCol - gapWidthCurrent / 2, 0, (int)COLS - gapWidthCurrent);
  for (uint8_t i = 0; i < 4; i++) {
    wallGapStart[i] = startGapStart;
    wallGapWidth[i] = gapWidthCurrent;
  }
  gameOver = false;
  score = 0;
  wallInterval = WALL_INTERVAL_START_MS;
  playerInterval = wallInterval / 5;
  gapJump = GAP_JUMP_START;
  subTick = 0;
  lastTick = millis();
}

bool inGap(uint8_t col, uint8_t lineIdx) {
  return (int)col >= wallGapStart[lineIdx] && (int)col < wallGapStart[lineIdx] + wallGapWidth[lineIdx];
}

void render() {
  for (uint8_t c = 0; c < COLS; c++) {
    bool topW1 = !inGap(c, 0);
    bool topW2 = !inGap(c, 1);
    bool botW1 = !inGap(c, 2);
    bool botW2 = !inGap(c, 3);
    bool isPlayer = (c == playerCol);

    lcd.setCursor(c, 0);
    lcd.write(charIndex(false, topW1, topW2));

    lcd.setCursor(c, 1);
    lcd.write(charIndex(isPlayer, botW1, botW2));
  }
}

void showGameOver() {
  if (score > highScore) {
    highScore = score;
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Game Over!");
  lcd.setCursor(0, 1);
  lcd.print("S:");
  lcd.print(score);
  lcd.print(" High:");
  lcd.print(highScore);
}

void setup() {
  pinMode(joySW, INPUT_PULLUP);
  calibrateJoystick();
  // XOR in micros() and the (unused-for-movement) Y axis so the seed
  // differs run to run even if X/Y happen to read the same at boot.
  randomSeed(analogRead(joyY) ^ micros());
  lcd.begin(16, 2);
  setupChars();
  resetGame();
  render();
}

void movePlayer() {
  int x = analogRead(joyX);
  // Wrap left/right off the edges, Pac-Man style.
  if (x < joyCenterX - JOY_DEADZONE) {
    playerCol = (playerCol == 0) ? COLS - 1 : playerCol - 1;
  } else if (x > joyCenterX + JOY_DEADZONE) {
    playerCol = (playerCol == COLS - 1) ? 0 : playerCol + 1;
  }
}

// Returns false if this shift kills the player.
bool shiftWalls() {
  // The line currently in slot 3 is the one about to scroll off the
  // bottom of the screen this tick -- that's the only moment a
  // collision is checked, not while it's still visible in that row.
  int exitingStart = wallGapStart[3];
  int exitingWidth = wallGapWidth[3];

  wallGapStart[3] = wallGapStart[2];
  wallGapWidth[3] = wallGapWidth[2];
  wallGapStart[2] = wallGapStart[1];
  wallGapWidth[2] = wallGapWidth[1];
  wallGapStart[1] = wallGapStart[0];
  wallGapWidth[1] = wallGapWidth[0];

  // New gap must be within gapJump columns of the previous spawned
  // gap (now sitting in slot 1), so the path is always walkable, and
  // must fully fit on screen given the current width.
  int minCol = max(0, wallGapStart[1] - gapJump);
  int maxCol = min((int)COLS - gapWidthCurrent, wallGapStart[1] + gapJump);
  if (minCol > maxCol) {
    minCol = maxCol = constrain(wallGapStart[1], 0, (int)COLS - gapWidthCurrent);
  }
  wallGapStart[0] = random(minCol, maxCol + 1);
  wallGapWidth[0] = gapWidthCurrent;
  score++;

  if (wallInterval > WALL_INTERVAL_MIN_MS) {
    unsigned long shrink = min(WALL_INTERVAL_STEP_MS, wallInterval - WALL_INTERVAL_MIN_MS);
    wallInterval -= shrink;
    playerInterval = wallInterval / 5;
  }

  // Widen the allowed gap jump and narrow the gap width in step with
  // how far the speed has ramped, so both reach their hardest values
  // right as the speed bottoms out.
  unsigned long speedRange = WALL_INTERVAL_START_MS - WALL_INTERVAL_MIN_MS;
  unsigned long progressed = WALL_INTERVAL_START_MS - wallInterval;
  gapJump = GAP_JUMP_START + (int)((long)(GAP_JUMP_MAX - GAP_JUMP_START) * progressed / speedRange);
  gapWidthCurrent = GAP_WIDTH_START - (int)((long)(GAP_WIDTH_START - GAP_WIDTH_MIN) * progressed / speedRange);

  return playerCol >= exitingStart && playerCol < exitingStart + exitingWidth;
}

void loop() {
  unsigned long now = millis();

  if (gameOver) {
    if (digitalRead(joySW) == LOW) {
      resetGame();
      render();
    }
    return;
  }

  if (now - lastTick < playerInterval) {
    return;
  }
  lastTick += playerInterval;

  // Player movement is unconditional on every tick, whether or not
  // this tick also happens to shift the walls.
  movePlayer();

  subTick++;
  if (subTick >= 5) {
    subTick = 0;
    if (!shiftWalls()) {
      gameOver = true;
      showGameOver();
      return;
    }
  }

  render();
}
