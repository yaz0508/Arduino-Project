// ESP32-WROOM-32 DevKit V1 (38-pin)
// Firmware for 2-player laser shooter game
// - State machine: IDLE, RUNNING, FINISHED
// - 4 analog sensors with timing-based hit validation
// - Time-based scoring using millis()
// - Buzzer feedback (short + long buzz)
// - WiFi + REST: /game/start, /game/score, /game/end

#include <WiFi.h>
#include <HTTPClient.h>

// ======================== CONFIGURATION ========================

// ---- WiFi configuration ----
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// ---- Backend configuration ----
// Your Render backend URL (use https:// for Render)
const char* BACKEND_BASE_URL = "https://lazer-game-backend.onrender.com";
const char* API_GAME_START   = "/game/start";
const char* API_GAME_SCORE   = "/game/score";
const char* API_GAME_END     = "/game/end";

// ---- Player configuration ----
// These should match what you configure / display in the frontend.
const char* PLAYER1_NAME = "Player 1";
const char* PLAYER2_NAME = "Player 2";

// ---- Pin assignments ----
// Sensors (analog inputs, input-only pins)
const int PIN_P1_S1 = 36;  // GPIO36 (VP)
const int PIN_P1_S2 = 39;  // GPIO39 (VN)
const int PIN_P2_S1 = 34;  // GPIO34
const int PIN_P2_S2 = 35;  // GPIO35

// Laser emitters (active HIGH control)
const int PIN_P1_LASER = 26;  // GPIO26
const int PIN_P2_LASER = 27;  // GPIO27

// Passive buzzer (simple on/off; could be extended to PWM tone)
const int PIN_BUZZER = 25;    // GPIO25

// Buttons (normally open to GND, use internal pull-up)
const int PIN_BTN_START = 18; // GPIO18
const int PIN_BTN_RESET = 19; // GPIO19

// ---- Game tuning ----

// ADC threshold for valid laser hit (0 - 4095).
// You MUST calibrate this value for your sensors and environment.
const int SENSOR_THRESHOLD = 2000;

// Time constants (ms)
const unsigned long SAMPLE_INTERVAL_MS   = 50;   // sensor sampling
const unsigned long HIT_MIN_DURATION_MS  = 200;  // continuous above threshold before scoring
const unsigned long COOL_DOWN_MS         = 100;  // after hit loss before new hit
const unsigned long SCORE_TICK_MS        = 100;  // +1 point every 100 ms
const unsigned long SHORT_BEEP_MS        = 50;   // beep when scoring starts
const unsigned long LONG_BUZZ_MS         = 3000; // buzz when game ends

// Target score to win
const int WIN_SCORE = 100;

// ======================== TYPES & STATE ========================

enum GameState {
  STATE_IDLE,
  STATE_RUNNING,
  STATE_FINISHED
};

struct SensorState {
  int pin;
  int value;                     // latest ADC reading
  bool aboveThreshold;           // currently above threshold
  unsigned long aboveStartTime;  // when it first went above threshold
  bool validHit;                 // has been above threshold for >= HIT_MIN_DURATION_MS
  unsigned long lastBelowTime;   // last time it went below threshold
};

struct PlayerState {
  // Indexes into sensor array for this player (0..3)
  int sensorIndexA;
  int sensorIndexB;

  // Current score
  int score;

  // Hit / scoring state
  int activeSensorIndex;         // which sensor is currently scoring (-1 = none)
  bool scoringActive;            // true if currently scoring
  unsigned long cooldownUntil;   // time until which we cannot start a new hit
  unsigned long lastScoreTick;   // last time we added score (while scoringActive == true)
};

struct BuzzerState {
  bool active;                   // currently on
  unsigned long offAt;           // time when buzzer should be turned off
};

struct PendingRequest {
  String path;
  String body;
  bool inUse;
  uint8_t attempts;
  unsigned long nextAttempt;     // when to attempt next send
};

// ======================== GLOBALS ========================

GameState gameState = STATE_IDLE;

// 4 sensors total
SensorState sensors[4];

// Player 1 uses sensors 0 and 1
// Player 2 uses sensors 2 and 3
PlayerState player1;
PlayerState player2;

// Buzzer state
BuzzerState buzzer;

// Game timing
unsigned long lastSampleTime = 0;

// Simple request queue
const int MAX_REQUESTS = 5;
PendingRequest requestQueue[MAX_REQUESTS];

// Current game ID issued by backend (/game/start response)
String currentGameId = "";

// Button state tracking (for edge detection)
bool lastStartBtnState = HIGH;
bool lastResetBtnState = HIGH;

// ======================== UTILITY FUNCTIONS ========================

// Turn buzzer on or off (passive buzzer, simple on/off)
void setBuzzer(bool on) {
  digitalWrite(PIN_BUZZER, on ? HIGH : LOW);
  buzzer.active = on;
  if (!on) {
    buzzer.offAt = 0;
  }
}

// Schedule a buzzer pulse for a specific duration (non-blocking)
void buzzerPulse(unsigned long durationMs) {
  unsigned long now = millis();
  setBuzzer(true);
  buzzer.offAt = now + durationMs;
}

// Update buzzer each loop, turning it off when time expires
void updateBuzzer() {
  if (buzzer.active) {
    unsigned long now = millis();
    if (now >= buzzer.offAt) {
      setBuzzer(false);
    }
  }
}

// Enqueue an HTTP POST request; non-blocking.
// Requests will be sent later by processRequestQueue().
void enqueuePost(const String& path, const String& body) {
  for (int i = 0; i < MAX_REQUESTS; i++) {
    if (!requestQueue[i].inUse) {
      requestQueue[i].path = path;
      requestQueue[i].body = body;
      requestQueue[i].inUse = true;
      requestQueue[i].attempts = 0;
      requestQueue[i].nextAttempt = 0;  // ready to send ASAP
      return;
    }
  }
  // Queue full; could log or handle overflow here.
}

// Process one pending HTTP request (if any) with simple retry logic.
void processRequestQueue() {
  if (WiFi.status() != WL_CONNECTED) {
    // Do not attempt any requests if WiFi is down.
    return;
  }

  unsigned long now = millis();

  for (int i = 0; i < MAX_REQUESTS; i++) {
    if (!requestQueue[i].inUse) continue;
    if (now < requestQueue[i].nextAttempt) continue;

    HTTPClient http;
    String url = String(BACKEND_BASE_URL) + requestQueue[i].path;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(requestQueue[i].body);
    if (httpCode > 0 && httpCode < 400) {
      // Success; consume this request
      String payload = http.getString();

      // If this is /game/start and no gameId set yet, try to extract it
      if (requestQueue[i].path == API_GAME_START && currentGameId.length() == 0) {
        // Very simple extraction: expect {"gameId":"..."} in response
        int idx = payload.indexOf("\"gameId\"");
        if (idx >= 0) {
          int colon = payload.indexOf(':', idx);
          int quote1 = payload.indexOf('"', colon);
          int quote2 = payload.indexOf('"', quote1 + 1);
          if (quote1 >= 0 && quote2 > quote1) {
            currentGameId = payload.substring(quote1 + 1, quote2);
          }
        }
      }

      requestQueue[i].inUse = false;
    } else {
      // Failed; schedule retry if attempts left
      requestQueue[i].attempts++;
      if (requestQueue[i].attempts >= 5) {
        // Drop after too many failures
        requestQueue[i].inUse = false;
      } else {
        // Exponential backoff: 2s, 4s, 8s, ...
        unsigned long delayMs = 2000UL << (requestQueue[i].attempts - 1);
        requestQueue[i].nextAttempt = now + delayMs;
      }
    }

    http.end();

    // Process only one request per loop to keep things responsive
    return;
  }
}

// ======================== SENSOR & SCORING LOGIC ========================

// Initialize sensor metadata
void initSensors() {
  sensors[0].pin = PIN_P1_S1;
  sensors[1].pin = PIN_P1_S2;
  sensors[2].pin = PIN_P2_S1;
  sensors[3].pin = PIN_P2_S2;

  for (int i = 0; i < 4; i++) {
    sensors[i].value = 0;
    sensors[i].aboveThreshold = false;
    sensors[i].aboveStartTime = 0;
    sensors[i].validHit = false;
    sensors[i].lastBelowTime = 0;
  }
}

// Reset a player's state and link its sensors
void resetPlayer(PlayerState& p, int idxA, int idxB) {
  p.sensorIndexA = idxA;
  p.sensorIndexB = idxB;
  p.score = 0;
  p.activeSensorIndex = -1;
  p.scoringActive = false;
  p.cooldownUntil = 0;
  p.lastScoreTick = 0;
}

// Sample all sensors every SAMPLE_INTERVAL_MS
void sampleSensors() {
  unsigned long now = millis();
  for (int i = 0; i < 4; i++) {
    int raw = analogRead(sensors[i].pin);
    sensors[i].value = raw;

    bool wasAbove = sensors[i].aboveThreshold;
    bool isAbove = (raw >= SENSOR_THRESHOLD);

    if (isAbove) {
      if (!wasAbove) {
        // Just crossed threshold
        sensors[i].aboveThreshold = true;
        sensors[i].aboveStartTime = now;
      } else {
        // Still above; check if valid hit time reached
        if (!sensors[i].validHit && (now - sensors[i].aboveStartTime >= HIT_MIN_DURATION_MS)) {
          sensors[i].validHit = true;
        }
      }
    } else {
      // Below threshold
      sensors[i].aboveThreshold = false;
      sensors[i].validHit = false;
      sensors[i].lastBelowTime = now;
    }
  }
}

// Determine which sensor (if any) is currently valid for scoring for a given player.
// Returns index in sensors[] or -1 if none valid.
int chooseValidSensorForPlayer(const PlayerState& p) {
  int idxA = p.sensorIndexA;
  int idxB = p.sensorIndexB;

  bool validA = sensors[idxA].validHit;
  bool validB = sensors[idxB].validHit;

  if (!validA && !validB) return -1;
  if (validA && !validB) return idxA;
  if (!validA && validB) return idxB;

  // Both valid -> choose stronger signal
  if (sensors[idxA].value >= sensors[idxB].value) {
    return idxA;
  } else {
    return idxB;
  }
}

// Update scoring for one player based on current sensor states
void updatePlayerScoring(PlayerState& p) {
  unsigned long now = millis();
  int chosenSensor = chooseValidSensorForPlayer(p);

  if (chosenSensor == -1) {
    // No valid hit currently
    if (p.scoringActive) {
      // Hit just ended -> start cooldown
      p.scoringActive = false;
      p.activeSensorIndex = -1;
      p.cooldownUntil = now + COOL_DOWN_MS;
    }
    return;
  }

  // We have at least one valid hit
  if (!p.scoringActive) {
    // Check cooldown
    if (now < p.cooldownUntil) {
      // Still cooling down; ignore
      return;
    }
    // Start a new scoring hit
    p.scoringActive = true;
    p.activeSensorIndex = chosenSensor;
    p.lastScoreTick = now;

    // Short beep on new valid scoring hit (once per hit start)
    buzzerPulse(SHORT_BEEP_MS);
  } else {
    // Already scoring; ensure we use the stronger of the two if both valid.
    p.activeSensorIndex = chosenSensor;
  }

  // While scoring is active, accumulate score every SCORE_TICK_MS
  if (p.scoringActive) {
    unsigned long elapsed = now - p.lastScoreTick;
    if (elapsed >= SCORE_TICK_MS) {
      int ticks = elapsed / SCORE_TICK_MS;
      p.score += ticks;
      if (p.score > WIN_SCORE) p.score = WIN_SCORE;
      p.lastScoreTick += ticks * SCORE_TICK_MS;
    }
  }
}

// ======================== GAME LOGIC ========================

// Send a /game/start REST event
void sendGameStart() {
  // Reset game ID so that /game/start response can set it.
  currentGameId = "";

  String body = "{";
  body += "\"player1Name\":\"" + String(PLAYER1_NAME) + "\",";
  body += "\"player2Name\":\"" + String(PLAYER2_NAME) + "\"";
  body += "}";

  enqueuePost(API_GAME_START, body);
}

// Send a /game/score REST event
void sendGameScore() {
  String body = "{";
  body += "\"gameId\":\"" + currentGameId + "\",";
  body += "\"player1Score\":" + String(player1.score) + ",";
  body += "\"player2Score\":" + String(player2.score);
  body += "}";

  enqueuePost(API_GAME_SCORE, body);
}

// Send a /game/end REST event
void sendGameEnd(const String& winner) {
  String body = "{";
  body += "\"gameId\":\"" + currentGameId + "\",";
  body += "\"player1Score\":" + String(player1.score) + ",";
  body += "\"player2Score\":" + String(player2.score) + ",";
  body += "\"winner\":\"" + winner + "\"";
  body += "}";

  enqueuePost(API_GAME_END, body);
}

// Determine human-readable winner name based on scores
String determineWinnerName() {
  if (player1.score >= WIN_SCORE && player1.score > player2.score) {
    return String(PLAYER1_NAME);
  } else if (player2.score >= WIN_SCORE && player2.score > player1.score) {
    return String(PLAYER2_NAME);
  } else if (player1.score >= WIN_SCORE && player2.score >= WIN_SCORE) {
    // Tie-break: higher score wins (or Player 1 by default)
    if (player1.score > player2.score) return String(PLAYER1_NAME);
    if (player2.score > player1.score) return String(PLAYER2_NAME);
    return "Tie";
  }
  return "";
}

// Called when we transition to a new game
void startNewGame() {
  // Reset players
  resetPlayer(player1, 0, 1);
  resetPlayer(player2, 2, 3);

  // Reset sensors
  initSensors();

  // Turn lasers ON (or adjust logic for your game)
  digitalWrite(PIN_P1_LASER, HIGH);
  digitalWrite(PIN_P2_LASER, HIGH);

  // Transition state
  gameState = STATE_RUNNING;

  // Send game start event to backend
  sendGameStart();
}

// End the current game and notify backend
void endGame(const String& winnerName) {
  // Turn lasers OFF for safety
  digitalWrite(PIN_P1_LASER, LOW);
  digitalWrite(PIN_P2_LASER, LOW);

  // Long buzz
  buzzerPulse(LONG_BUZZ_MS);

  // Send game end event
  sendGameEnd(winnerName);

  gameState = STATE_FINISHED;
}

// ======================== WIFI ========================

// Connect (or reconnect) to WiFi; called at setup and periodically.
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Non-blocking-ish connect: try for ~10 seconds in short steps
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(200); // acceptable at setup time
  }
}

// ======================== SETUP & LOOP ========================

void setup() {
  Serial.begin(115200);

  // Configure output pins
  pinMode(PIN_P1_LASER, OUTPUT);
  pinMode(PIN_P2_LASER, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  // Ensure outputs off initially
  digitalWrite(PIN_P1_LASER, LOW);
  digitalWrite(PIN_P2_LASER, LOW);
  setBuzzer(false);

  // Configure buttons
  pinMode(PIN_BTN_START, INPUT_PULLUP);
  pinMode(PIN_BTN_RESET, INPUT_PULLUP);

  // Initialize sensors and players
  initSensors();
  resetPlayer(player1, 0, 1);
  resetPlayer(player2, 2, 3);

  lastSampleTime = millis();

  // Connect to WiFi
  connectWiFi();

  Serial.println("Laser game firmware ready. Press START button to begin.");
}

void loop() {
  unsigned long now = millis();

  // ---- Button handling (edge detection) ----
  bool startBtnState = digitalRead(PIN_BTN_START); // HIGH = not pressed, LOW = pressed
  bool resetBtnState = digitalRead(PIN_BTN_RESET);

  bool startPressed = (lastStartBtnState == HIGH && startBtnState == LOW);
  bool resetPressed = (lastResetBtnState == HIGH && resetBtnState == LOW);

  lastStartBtnState = startBtnState;
  lastResetBtnState = resetBtnState;

  // ---- State machine ----
  switch (gameState) {
    case STATE_IDLE: {
      // Waiting for start button to begin a new game
      if (startPressed) {
        Serial.println("Starting new game...");
        startNewGame();
      }
      break;
    }

    case STATE_RUNNING: {
      // Sampling sensors every SAMPLE_INTERVAL_MS
      if (now - lastSampleTime >= SAMPLE_INTERVAL_MS) {
        lastSampleTime += SAMPLE_INTERVAL_MS;
        sampleSensors();

        // Update players' scoring based on current sensor states
        updatePlayerScoring(player1);
        updatePlayerScoring(player2);

        // Check for win condition
        bool p1Won = (player1.score >= WIN_SCORE);
        bool p2Won = (player2.score >= WIN_SCORE);

        if (p1Won || p2Won) {
          String winner = determineWinnerName();
          Serial.print("Game finished. Winner: ");
          Serial.println(winner);
          endGame(winner);
        } else {
          // Send periodic score update while the game is running.
          // To reduce traffic, we send only every N samples.
          static int sampleCount = 0;
          sampleCount++;
          if (sampleCount >= 4) { // roughly every 200 ms (4 * 50 ms)
            sendGameScore();
            sampleCount = 0;
          }
        }
      }

      // Allow reset button to abort game and go back to IDLE
      if (resetPressed) {
        Serial.println("Game aborted by reset.");
        endGame("Aborted");
      }
      break;
    }

    case STATE_FINISHED: {
      // Game over; wait for reset button to return to IDLE
      if (resetPressed) {
        Serial.println("Resetting to IDLE.");
        gameState = STATE_IDLE;
      }
      break;
    }
  }

  // ---- Background tasks ----
  updateBuzzer();
  processRequestQueue();

  // Attempt WiFi reconnect if disconnected (non-blocking check)
  static unsigned long lastWiFiCheck = 0;
  if (now - lastWiFiCheck > 5000) {
    lastWiFiCheck = now;
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected, attempting reconnect...");
      connectWiFi();
    }
  }

  // Do not use delay() here to keep everything non-blocking
}

