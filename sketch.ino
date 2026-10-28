#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <EEPROM.h>

U8G2_SH1107_128X128_1_HW_I2C u8g2(U8G2_R0);

const uint8_t PIN_CLK = 2;
const uint8_t PIN_DT = 3;
const uint8_t PIN_SW = 4;
const uint8_t PIN_BUZZER = 8;

volatile int encoderDelta = 0;
volatile uint8_t lastCLK = HIGH;

bool btnDown = false;
unsigned long btnDownAt = 0;
const unsigned long LONG_PRESS_MS = 500;

bool selectEvent = false;
bool backEvent = false;

const char* MENU_ITEMS[] = {
  "Calculator",
  "Clock",
  "Stopwatch",
  "Notes",
  "Flappy Bird"
};

const uint8_t MENU_COUNT = 5;
const uint8_t VISIBLE_ROWS = 4;

int cursor = 0;
int topRow = 0;

enum AppId {
  APP_NONE,
  APP_CALC,
  APP_CLOCK,
  APP_STOPWATCH,
  APP_NOTES,
  APP_GAME
};

AppId currentApp = APP_NONE;

void runMenu();
void runCalculator();
void runClock();
void runStopwatch();
void runNotes();
void runFlappyBird();
void notesEnter();
void gameEnter();
void calcEnter();
void clockEnter();
void stopwatchEnter();

// App initialization flags: switching apps does not reset their state.
bool calcInitialized = false;
bool clockInitialized = false;
bool stopwatchInitialized = false;
bool notesInitialized = false;
bool gameInitialized = false;

void encoderISR() {
  uint8_t clkState = digitalRead(PIN_CLK);

  if (clkState != lastCLK) {
    if (digitalRead(PIN_DT) != clkState)
      encoderDelta++;
    else
      encoderDelta--;
  }

  lastCLK = clkState;
}

int readEncoderStep() {
  noInterrupts();
  int d = encoderDelta;
  encoderDelta = 0;
  interrupts();

  if (d > 0) return 1;
  if (d < 0) return -1;
  return 0;
}

void pollButton() {
  bool pressed = digitalRead(PIN_SW) == LOW;

  if (pressed && !btnDown) {
    btnDown = true;
    btnDownAt = millis();
  }
  else if (!pressed && btnDown) {
    btnDown = false;

    unsigned long held = millis() - btnDownAt;

    if (held >= LONG_PRESS_MS)
      backEvent = true;
    else
      selectEvent = true;

    tone(PIN_BUZZER, 1800, 30);
  }
}

void drawHeader(const char* title) {
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 9, title);
  u8g2.drawLine(0, 11, 127, 11);
}

void drawSelectedRow(int y, const char* text) {
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, y - 9, 128, 12);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(4, y, text);
  u8g2.setDrawColor(1);
}

void drawNormalRow(int y, const char* text) {
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(4, y, text);
}

void setup() {
  pinMode(PIN_CLK, INPUT_PULLUP);
  pinMode(PIN_DT, INPUT_PULLUP);
  pinMode(PIN_SW, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);

  lastCLK = digitalRead(PIN_CLK);

  attachInterrupt(
    digitalPinToInterrupt(PIN_CLK),
    encoderISR,
    CHANGE
  );

  u8g2.begin();
  u8g2.setContrast(255);
}

void loop() {
  pollButton();

  if (currentApp == APP_NONE)
    runMenu();
  else if (currentApp == APP_CALC)
    runCalculator();
  else if (currentApp == APP_CLOCK)
    runClock();
  else if (currentApp == APP_STOPWATCH)
    runStopwatch();
  else if (currentApp == APP_NOTES)
    runNotes();
  else if (currentApp == APP_GAME)
    runFlappyBird();
}

void runMenu() {
  int step = readEncoderStep();

  if (step != 0) {
    cursor += step;

    if (cursor < 0)
      cursor = MENU_COUNT - 1;

    if (cursor >= MENU_COUNT)
      cursor = 0;

    if (cursor < topRow)
      topRow = cursor;

    if (cursor >= topRow + VISIBLE_ROWS)
      topRow = cursor - VISIBLE_ROWS + 1;
  }

  if (selectEvent) {
    selectEvent = false;

    if (cursor == 0) {
      currentApp = APP_CALC;
      if (!calcInitialized) {
        calcEnter();
        calcInitialized = true;
      }
    }
    else if (cursor == 1) {
      currentApp = APP_CLOCK;
      if (!clockInitialized) {
        clockEnter();
        clockInitialized = true;
      }
    }
    else if (cursor == 2) {
      currentApp = APP_STOPWATCH;
      if (!stopwatchInitialized) {
        stopwatchEnter();
        stopwatchInitialized = true;
      }
    }
    else if (cursor == 3) {
      currentApp = APP_NOTES;
      if (!notesInitialized) {
        notesEnter();
        notesInitialized = true;
      }
    }
    else if (cursor == 4) {
      currentApp = APP_GAME;
      if (!gameInitialized) {
        gameEnter();
        gameInitialized = true;
      }
    }
  }

  backEvent = false;

  u8g2.firstPage();

  do {
    drawHeader("MINI OS MAIN MENU");

    for (uint8_t row = 0; row < VISIBLE_ROWS; row++) {
      int idx = topRow + row;

      if (idx >= MENU_COUNT)
        break;

      int y = 24 + row * 16;

      char item[25];

      snprintf(
        item,
        sizeof(item),
        "%d. %s",
        idx + 1,
        MENU_ITEMS[idx]
      );

      if (idx == cursor)
        drawSelectedRow(y, item);
      else
        drawNormalRow(y, item);
    }

  } while (u8g2.nextPage());
}

void exitToMenu() {
  currentApp = APP_NONE;
  backEvent = false;
  selectEvent = false;
}

long calcA = 0;
long calcB = 0;

uint8_t calcField = 0;
uint8_t calcOp = 0;

const char OP_CHARS[] = {
  '+',
  '-',
  '*',
  '/'
};

void calcEnter() {
  // Called only the first time Calculator is opened.
  calcA = 0;
  calcB = 0;
  calcField = 0;
  calcOp = 0;
}

long calcResult() {
  switch (calcOp) {
    case 0:
      return calcA + calcB;
    case 1:
      return calcA - calcB;
    case 2:
      return calcA * calcB;
    case 3:
      return calcB != 0 ? calcA / calcB : 0;
  }

  return 0;
}

void runCalculator() {
  int step = readEncoderStep();

  if (step != 0) {
    if (calcField == 0)
      calcA += step;
    else if (calcField == 1)
      calcOp = (calcOp + step + 4) % 4;
    else
      calcB += step;
  }

  if (selectEvent) {
    selectEvent = false;

    calcField++;

    if (calcField > 2)
      calcField = 0;
  }

  if (backEvent) {
    exitToMenu();
    return;
  }

  u8g2.firstPage();

  do {
    drawHeader("CALCULATOR");

    u8g2.setFont(u8g2_font_7x14B_tr);

    char expression[30];

    snprintf(
      expression,
      sizeof(expression),
      "%ld %c %ld",
      calcA,
      OP_CHARS[calcOp],
      calcB
    );

    u8g2.drawStr(2, 32, expression);

    u8g2.setFont(u8g2_font_6x10_tr);

    char resultText[25];

    snprintf(
      resultText,
      sizeof(resultText),
      "= %ld",
      calcResult()
    );

    u8g2.drawStr(2, 49, resultText);
    u8g2.drawStr(2, 66, "SELECT: next field");
    u8g2.drawStr(2, 78, "Hold: BACK");

  } while (u8g2.nextPage());
}

unsigned long clockBaseMillis = 0;
long clockOffsetSeconds = 0;
bool clockSetMode = false;

void clockEnter() {
  // Called only the first time Clock is opened.
  clockBaseMillis = millis();
  clockOffsetSeconds = 0;
  clockSetMode = false;
}

void runClock() {
  int step = readEncoderStep();

  if (selectEvent) {
    selectEvent = false;
    clockSetMode = !clockSetMode;
  }

  if (step != 0 && clockSetMode)
    clockOffsetSeconds += step * 60L;

  if (backEvent) {
    exitToMenu();
    return;
  }

  long totalSeconds =
    clockOffsetSeconds +
    (millis() - clockBaseMillis) / 1000UL;

  int h = (totalSeconds / 3600) % 24;
  int m = (totalSeconds / 60) % 60;
  int s = totalSeconds % 60;

  char timeText[10];

  snprintf(
    timeText,
    sizeof(timeText),
    "%02d:%02d:%02d",
    h,
    m,
    s
  );

  u8g2.firstPage();

  do {
    drawHeader("CLOCK");

    u8g2.setFont(u8g2_font_ncenB18_tr);
    u8g2.drawStr(12, 38, timeText);

    u8g2.setFont(u8g2_font_6x10_tr);

    if (clockSetMode) {
      u8g2.drawStr(2, 58, "SET MODE");
      u8g2.drawStr(2, 70, "Rotate: +/- 1 min");
      u8g2.drawStr(2, 82, "SELECT: finish");
    }
    else {
      u8g2.drawStr(2, 58, "SELECT: set");
      u8g2.drawStr(2, 70, "Hold: BACK");
    }

  } while (u8g2.nextPage());
}

// Stopwatch app. It uses millis(), so it keeps counting while another app
// is visible. Only the display/input handling pauses when it is not active.
unsigned long stopwatchStartedAt = 0;
unsigned long stopwatchElapsedBeforePause = 0;
bool stopwatchRunning = false;

void stopwatchEnter() {
  stopwatchStartedAt = millis();
  stopwatchElapsedBeforePause = 0;
  stopwatchRunning = false;
}

unsigned long stopwatchElapsed() {
  if (stopwatchRunning)
    return stopwatchElapsedBeforePause + (millis() - stopwatchStartedAt);
  return stopwatchElapsedBeforePause;
}

void runStopwatch() {
  int step = readEncoderStep();

  // SELECT toggles run/pause. Rotate while paused to reset to zero.
  if (selectEvent) {
    selectEvent = false;

    if (stopwatchRunning) {
      stopwatchElapsedBeforePause += millis() - stopwatchStartedAt;
      stopwatchRunning = false;
    } else {
      stopwatchStartedAt = millis();
      stopwatchRunning = true;
    }
  }

  if (step != 0 && !stopwatchRunning) {
    // Rotate while paused to reset the stopwatch.
    stopwatchElapsedBeforePause = 0;
    stopwatchStartedAt = millis();
  }

  if (backEvent) {
    exitToMenu();
    return;
  }

  unsigned long elapsed = stopwatchElapsed();
  unsigned long totalSeconds = elapsed / 1000UL;
  unsigned int minutes = (totalSeconds / 60UL) % 100;
  unsigned int seconds = totalSeconds % 60UL;
  unsigned int centiseconds = (elapsed % 1000UL) / 10UL;

  char timeText[16];
  snprintf(timeText, sizeof(timeText), "%02u:%02u.%02u",
           minutes, seconds, centiseconds);

  u8g2.firstPage();
  do {
    drawHeader("STOPWATCH");

    u8g2.setFont(u8g2_font_ncenB18_tr);
    u8g2.drawStr(10, 40, timeText);

    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(2, 62, stopwatchRunning ? "RUNNING" : "PAUSED");
    u8g2.drawStr(2, 76, "SELECT: start/pause");
    u8g2.drawStr(2, 88, "Rotate when paused: reset");
    u8g2.drawStr(2, 100, "Hold: BACK");
  } while (u8g2.nextPage());
}


// ---------------- NOTES APP ----------------
// 64 characters are stored in EEPROM so notes survive app switching and power-off.
const uint8_t NOTE_MAX = 64;
const int NOTE_EEPROM_BASE = 0;
const int NOTE_MAGIC_ADDRESS = 100;
const uint8_t NOTE_MAGIC = 0x5A;
const char NOTE_CHARSET[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.,!?'-:";
const uint8_t NOTE_CHARSET_LEN = sizeof(NOTE_CHARSET) - 1;
char noteText[NOTE_MAX + 1];
uint8_t noteCursor = 0;

void saveNotes() {
  for (uint8_t i = 0; i < NOTE_MAX; i++) {
    EEPROM.update(NOTE_EEPROM_BASE + i, noteText[i]);
  }
  EEPROM.update(NOTE_MAGIC_ADDRESS, NOTE_MAGIC);
}

void notesEnter() {
  if (EEPROM.read(NOTE_MAGIC_ADDRESS) == NOTE_MAGIC) {
    for (uint8_t i = 0; i < NOTE_MAX; i++) {
      char c = (char)EEPROM.read(NOTE_EEPROM_BASE + i);
      noteText[i] = (c == '\0' || c == (char)0xFF) ? ' ' : c;
    }
  } else {
    for (uint8_t i = 0; i < NOTE_MAX; i++) noteText[i] = ' ';
  }
  noteText[NOTE_MAX] = '\0';
  noteCursor = 0;
}

void runNotes() {
  int step = readEncoderStep();

  if (step != 0) {
    char current = noteText[noteCursor];
    int index = 0;
    for (uint8_t i = 0; i < NOTE_CHARSET_LEN; i++) {
      if (NOTE_CHARSET[i] == current) { index = i; break; }
    }
    index = (index + step + NOTE_CHARSET_LEN) % NOTE_CHARSET_LEN;
    noteText[noteCursor] = NOTE_CHARSET[index];
  }

  if (selectEvent) {
    selectEvent = false;
    noteCursor = (noteCursor + 1) % NOTE_MAX;
  }

  if (backEvent) {
    saveNotes();
    exitToMenu();
    return;
  }

  u8g2.firstPage();
  do {
    drawHeader("NOTES - EDIT");
    u8g2.setFont(u8g2_font_6x10_tr);

    for (uint8_t line = 0; line < 4; line++) {
      char row[17];
      for (uint8_t col = 0; col < 16; col++) {
        uint8_t i = line * 16 + col;
        row[col] = noteText[i];
      }
      row[16] = '\0';
      u8g2.drawStr(0, 28 + line * 12, row);
    }

    // Highlight the character currently being edited.
    uint8_t line = noteCursor / 16;
    uint8_t col = noteCursor % 16;
    u8g2.drawFrame(col * 8, 17 + line * 12, 8, 12);

    u8g2.drawStr(0, 80, "Rotate: change char");
    u8g2.drawStr(0, 92, "Click: next position");
    u8g2.drawStr(0, 104, "Space erases a char");
    u8g2.drawStr(0, 116, "Hold: save and exit");
  } while (u8g2.nextPage());
}

// ---------------- FLAPPY BIRD GAME ----------------
// Non-blocking game loop: the game advances with millis() and can be exited
// with a long press. Short press makes the bird flap or restarts after game over.
int birdY = 58;
int birdVelocity = 0;
int pipeX = 128;
int gapY = 58;
int gameScore = 0;
bool gameOver = false;
bool gameStarted = false;
unsigned long lastGameFrame = 0;
unsigned long lastPipeMove = 0;
const int PIPE_WIDTH = 14;
const int PIPE_GAP = 38;
const int BIRD_X = 24;

void gameEnter() {
  birdY = 58;
  birdVelocity = 0;
  pipeX = 128;
  gapY = 58;
  gameScore = 0;
  gameOver = false;
  gameStarted = false;
  lastGameFrame = millis();
  lastPipeMove = millis();
}

void resetGame() {
  gameEnter();
  gameStarted = true;
}

void runFlappyBird() {
  if (selectEvent) {
    selectEvent = false;
    if (gameOver) {
      resetGame();
    } else if (!gameStarted) {
      gameStarted = true;
      birdVelocity = -4;
    } else {
      birdVelocity = -4;
      tone(PIN_BUZZER, 1300, 35);
    }
  }

  if (backEvent) {
    exitToMenu();
    return;
  }

  unsigned long now = millis();
  if (gameStarted && !gameOver && now - lastGameFrame >= 35) {
    lastGameFrame = now;

    birdVelocity += 1;
    if (birdVelocity > 4) birdVelocity = 4;
    birdY += birdVelocity;

    if (now - lastPipeMove >= 35) {
      pipeX -= 2;
      lastPipeMove = now;
    }

    if (pipeX < -PIPE_WIDTH) {
      pipeX = 128;
      gapY = random(34, 88);
      gameScore++;
      tone(PIN_BUZZER, 1900, 25);
    }

    // Collision with screen edges or pipe columns.
    if (birdY < 13 || birdY > 119) gameOver = true;

    if (pipeX < BIRD_X + 7 && pipeX + PIPE_WIDTH > BIRD_X - 5) {
      if (birdY - 4 < gapY - PIPE_GAP / 2 ||
          birdY + 4 > gapY + PIPE_GAP / 2) {
        gameOver = true;
      }
    }
  }

  u8g2.firstPage();
  do {
    drawHeader("FLAPPY BIRD");
    u8g2.setFont(u8g2_font_6x10_tr);

    char scoreText[16];
    snprintf(scoreText, sizeof(scoreText), "Score: %d", gameScore);
    u8g2.drawStr(76, 9, scoreText);

    // Pipes with a gap.
    if (gameStarted) {
      u8g2.drawBox(pipeX, 13, PIPE_WIDTH, max(0, gapY - PIPE_GAP / 2 - 13));
      int lowerY = gapY + PIPE_GAP / 2;
      if (lowerY < 126) u8g2.drawBox(pipeX, lowerY, PIPE_WIDTH, 126 - lowerY);
    }

    // Bird
    u8g2.drawDisc(BIRD_X, birdY, 4);
    u8g2.drawPixel(BIRD_X + 2, birdY - 1);

    if (!gameStarted) {
      u8g2.drawFrame(12, 38, 104, 42);
      u8g2.drawStr(22, 51, "FLAPPY BIRD");
      u8g2.drawStr(18, 64, "Click to start/flap");
      u8g2.drawStr(16, 75, "Hold to exit");
    } else if (gameOver) {
      u8g2.drawBox(18, 43, 92, 35);
      u8g2.setDrawColor(0);
      u8g2.drawStr(39, 55, "GAME OVER");
      char finalScore[18];
      snprintf(finalScore, sizeof(finalScore), "Score: %d", gameScore);
      u8g2.drawStr(39, 66, finalScore);
      u8g2.drawStr(24, 75, "Click: retry");
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(0, 126, "Click: flap | Hold: exit");
    }
  } while (u8g2.nextPage());
}
