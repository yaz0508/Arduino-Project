// ESP32 Laser Game Firmware - Dual Buzzer Edition
// - Player 1: Sensors SP/SN (36/39) | Laser Pin 26 | Buzzer Pin 25
// - Player 2: Sensors 34/35         | Laser Pin 27 | Buzzer Pin 33
// - Button Start: Pin 18
// - Button Reset: Pin 19

#include <WiFi.h>
#include <HTTPClient.h>

// ======================== CONFIGURATION ========================

// ---- WiFi configuration ----
const char* WIFI_SSID     = "ADAPT LNG";     // <--- CHANGE THIS
const char* WIFI_PASSWORD = "sigepo123"; // <--- CHANGE THIS

// ---- Backend configuration ----
const char* BACKEND_BASE_URL = "https://lazer-game-backend.onrender.com";
const char* API_GAME_START   = "/game/start";
const char* API_GAME_SCORE   = "/game/score";
const char* API_GAME_END     = "/game/end";

// ---- Player configuration ----
const char* PLAYER1_NAME = "Player 1";
const char* PLAYER2_NAME = "Player 2";

// IMPORTANT: Arduino will create its own game and get the gameId
// The frontend will need to use the SAME gameId that Arduino generates
String currentGameId = "";  // Empty initially - will be set when game starts

// ---- Pin assignments ----
// Sensors (ADC Pins - use 32-35 or 4/12-15 to avoid boot conflicts)
// NOTE: Avoid pins 36, 39 as they can cause boot issues
const int PIN_P1_S1 = 32;  // GPIO 32 (ADC4_CH4)
const int PIN_P1_S2 = 33;  // GPIO 33 (ADC5_CH5)
const int PIN_P2_S1 = 34;  // GPIO 34 (ADC6_CH6)
const int PIN_P2_S2 = 35;  // GPIO 35 (ADC7_CH7)

// Lasers
// const int PIN_P1_LASER = 26;  // NOT NEEDED - External laser only
// const int PIN_P2_LASER = 27;  // NOT NEEDED - External laser only  

// Buzzers (One for each player)
const int PIN_P1_BUZZER = 25; // Player 1 Buzzer
const int PIN_P2_BUZZER = 4;  // Player 2 Buzzer (GPIO 4 - Safe pin)

// Buttons
const int PIN_BTN_START = 0; 
const int PIN_BTN_RESET = 19; 

// ---- Game tuning ----
// ADC threshold for valid laser hit (0 - 4095).
const int SENSOR_THRESHOLD = 2000; // <--- Calibrate this if needed!

// Time constants (ms)
const unsigned long SAMPLE_INTERVAL_MS   = 50;   
const unsigned long HIT_MIN_DURATION_MS  = 200;  
const unsigned long COOL_DOWN_MS         = 100;  
const unsigned long SCORE_TICK_MS        = 100;  
const unsigned long SHORT_BEEP_MS        = 100;  // Hit beep duration
const unsigned long LONG_BUZZ_MS         = 3000; // Win beep duration

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
  int value;
  bool aboveThreshold;
  unsigned long aboveStartTime;
  bool validHit;
};

// Independent Buzzer State
struct PlayerBuzzer {
  int pin;
  bool active;
  unsigned long offAt;
};

struct PlayerState {
  int sensorIndexA;
  int sensorIndexB;
  int score;
  int activeSensorIndex;
  bool scoringActive;
  unsigned long cooldownUntil;
  unsigned long lastScoreTick;
  PlayerBuzzer* myBuzzer; // Pointer to this player's buzzer
};

struct PendingRequest {
  String path;
  String body;
  bool inUse;
  uint8_t attempts;
  unsigned long nextAttempt;
};

// ======================== GLOBALS ========================

GameState gameState = STATE_IDLE;
SensorState sensors[4];

// Buzzers
PlayerBuzzer buzzer1;
PlayerBuzzer buzzer2;

// Players
PlayerState player1;
PlayerState player2;

unsigned long lastSampleTime = 0;
const int MAX_REQUESTS = 5;
PendingRequest requestQueue[MAX_REQUESTS];

bool lastStartBtnState = false;
bool lastResetBtnState = false;

// ======================== BUZZER FUNCTIONS ========================

// Turn a specific buzzer on/off
void setBuzzerState(PlayerBuzzer& b, bool on) {
  digitalWrite(b.pin, on ? HIGH : LOW);
  b.active = on;
  if (!on) b.offAt = 0;
}

// Trigger a pulse for a specific buzzer
void buzzerPulse(PlayerBuzzer& b, unsigned long durationMs) {
  unsigned long now = millis();
  setBuzzerState(b, true);
  b.offAt = now + durationMs;
}

// Check both buzzers and turn them off if time is up
void updateBuzzers() {
  unsigned long now = millis();
  if (buzzer1.active && now >= buzzer1.offAt) setBuzzerState(buzzer1, false);
  if (buzzer2.active && now >= buzzer2.offAt) setBuzzerState(buzzer2, false);
}

// ======================== HTTP & UTILS ========================

void enqueuePost(const String& path, const String& body) {
  for (int i = 0; i < MAX_REQUESTS; i++) {
    if (!requestQueue[i].inUse) {
      requestQueue[i].path = path;
      requestQueue[i].body = body;
      requestQueue[i].inUse = true;
      requestQueue[i].attempts = 0;
      requestQueue[i].nextAttempt = 0;
      return;
    }
  }
}

void processRequestQueue() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠ WiFi not connected. Skipping request.");
    return;
  }
  
  unsigned long now = millis();

  for (int i = 0; i < MAX_REQUESTS; i++) {
    if (!requestQueue[i].inUse) continue;
    if (now < requestQueue[i].nextAttempt) continue;

    Serial.println("\n--- Sending HTTP Request ---");
    Serial.print("Attempt: ");
    Serial.println(requestQueue[i].attempts + 1);
    
    HTTPClient http;
    String url = String(BACKEND_BASE_URL) + requestQueue[i].path;
    Serial.print("URL: ");
    Serial.println(url);
    Serial.print("Body: ");
    Serial.println(requestQueue[i].body);

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(5000); // 5 second timeout

    int httpCode = http.POST(requestQueue[i].body);
    Serial.print("Response Code: ");
    Serial.println(httpCode);
    
    if (httpCode > 0 && httpCode < 400) {
      String payload = http.getString();
      Serial.print("Response: ");
      Serial.println(payload);
      
      // Extract gameId from /game/start response
      if (requestQueue[i].path == API_GAME_START && currentGameId.length() == 0) {
        int idx = payload.indexOf("\"gameId\"");
        if (idx >= 0) {
          int colon = payload.indexOf(':', idx);
          int q1 = payload.indexOf('"', colon);
          int q2 = payload.indexOf('"', q1 + 1);
          if (q1 >= 0 && q2 > q1) {
            currentGameId = payload.substring(q1 + 1, q2);
            Serial.print("✓ Game ID received: ");
            Serial.println(currentGameId);
            Serial.println("=== GAME READY FOR SCORING ===");
          }
        }
      }
      requestQueue[i].inUse = false;
      Serial.println("✓ Request successful");
    } else {
      Serial.print("✗ Request failed. ");
      String error = http.errorToString(httpCode);
      Serial.println(error);
      
      requestQueue[i].attempts++;
      if (requestQueue[i].attempts >= 5) {
        Serial.println("✗ Max retries reached. Giving up.");
        requestQueue[i].inUse = false;
      } else {
        requestQueue[i].nextAttempt = now + (2000UL << (requestQueue[i].attempts - 1));
        Serial.print("Retry in ");
        Serial.print(2000UL << (requestQueue[i].attempts - 1));
        Serial.println("ms");
      }
    }
    http.end();
    return;
  }
}

// ======================== SENSOR & SCORING LOGIC ========================

void initSensors() {
  sensors[0].pin = PIN_P1_S1;
  sensors[1].pin = PIN_P1_S2;
  sensors[2].pin = PIN_P2_S1;
  sensors[3].pin = PIN_P2_S2;

  for (int i = 0; i < 4; i++) {
    sensors[i].value = 0;
    sensors[i].aboveThreshold = false;
    sensors[i].validHit = false;
  }
}

// Link player to sensors AND specific buzzer
void resetPlayer(PlayerState& p, int idxA, int idxB, PlayerBuzzer* b) {
  p.sensorIndexA = idxA;
  p.sensorIndexB = idxB;
  p.score = 0;
  p.activeSensorIndex = -1;
  p.scoringActive = false;
  p.cooldownUntil = 0;
  p.myBuzzer = b; // Assign buzzer
}

void sampleSensors() {
  unsigned long now = millis();
  for (int i = 0; i < 4; i++) {
    int raw = analogRead(sensors[i].pin);
    sensors[i].value = raw;
    bool isAbove = (raw >= SENSOR_THRESHOLD);

    if (isAbove) {
      if (!sensors[i].aboveThreshold) {
        sensors[i].aboveThreshold = true;
        sensors[i].aboveStartTime = now;
      } else {
        if (!sensors[i].validHit && (now - sensors[i].aboveStartTime >= HIT_MIN_DURATION_MS)) {
          sensors[i].validHit = true;
        }
      }
    } else {
      sensors[i].aboveThreshold = false;
      sensors[i].validHit = false;
    }
  }
}

void updatePlayerScoring(PlayerState& p) {
  unsigned long now = millis();
  
  // Find valid hits
  bool hitA = sensors[p.sensorIndexA].validHit;
  bool hitB = sensors[p.sensorIndexB].validHit;
  int bestSensor = -1;

  if (hitA && hitB) bestSensor = (sensors[p.sensorIndexA].value > sensors[p.sensorIndexB].value) ? p.sensorIndexA : p.sensorIndexB;
  else if (hitA) bestSensor = p.sensorIndexA;
  else if (hitB) bestSensor = p.sensorIndexB;

  if (bestSensor == -1) {
    if (p.scoringActive) {
      p.scoringActive = false;
      p.cooldownUntil = now + COOL_DOWN_MS;
    }
    return;
  }

  // Valid hit detected
  if (!p.scoringActive) {
    if (now < p.cooldownUntil) return;
    
    // START SCORING & BUZZ
    p.scoringActive = true;
    p.activeSensorIndex = bestSensor;
    p.lastScoreTick = now;
    buzzerPulse(*p.myBuzzer, SHORT_BEEP_MS); // Beep only this player's buzzer
    Serial.println("HIT DETECTED - Buzzer activated!");
  }

  // Accumulate Score
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

// ======================== GAME CONTROL ========================

void startNewGame() {
  Serial.println("\n>>> GAME START INITIATED <<<");
  
  // Reset players & Assign Buzzers
  resetPlayer(player1, 0, 1, &buzzer1);
  resetPlayer(player2, 2, 3, &buzzer2);

  initSensors();
  gameState = STATE_RUNNING;
  
  Serial.println("Game Running - Sensors Active");
  Serial.println("Creating game and waiting for gameId...");

  currentGameId = "";
  String body = "{\"player1Name\":\"" + String(PLAYER1_NAME) + "\",\"player2Name\":\"" + String(PLAYER2_NAME) + "\"}";
  enqueuePost(API_GAME_START, body);
}

void endGame(const String& winner) {
  // Lasers are external - no control needed
  
  // VICTORY BUZZ: Only buzz the winner's buzzer!
  if (winner == PLAYER1_NAME) {
    buzzerPulse(buzzer1, LONG_BUZZ_MS);
  } else if (winner == PLAYER2_NAME) {
    buzzerPulse(buzzer2, LONG_BUZZ_MS);
  } else {
    // Tie? Buzz both briefly
    buzzerPulse(buzzer1, 1000);
    buzzerPulse(buzzer2, 1000);
  }

  String body = "{\"gameId\":\"" + currentGameId + "\",\"player1Score\":" + String(player1.score) + ",\"player2Score\":" + String(player2.score) + ",\"winner\":\"" + winner + "\"}";
  enqueuePost(API_GAME_END, body);
  gameState = STATE_FINISHED;
}

// ======================== SETUP & LOOP ========================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=== LASER GAME SYSTEM STARTING ===");
  Serial.println("Initializing pins...");

  // Setup Outputs (Only Buzzers - Lasers are external)
  // No laser pin setup needed
  
  // Setup Dual Buzzers
  buzzer1.pin = PIN_P1_BUZZER;
  buzzer2.pin = PIN_P2_BUZZER;
  pinMode(buzzer1.pin, OUTPUT);
  pinMode(buzzer2.pin, OUTPUT);
  setBuzzerState(buzzer1, false);
  setBuzzerState(buzzer2, false);

  // Setup Inputs
  pinMode(PIN_BTN_START, INPUT_PULLUP);
  pinMode(PIN_BTN_RESET, INPUT_PULLUP);

  initSensors();
  resetPlayer(player1, 0, 1, &buzzer1);
  resetPlayer(player2, 2, 3, &buzzer2);

  // Connect WiFi
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("✓ WiFi CONNECTED! IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("✗ WiFi FAILED TO CONNECT");
  }
  
  Serial.println("Backend URL: " + String(BACKEND_BASE_URL));
  Serial.println("System Ready. Press START button.");
}

void loop() {
  unsigned long now = millis();

  // Debounce buttons simply
  bool startBtn = (digitalRead(PIN_BTN_START) == LOW);
  bool resetBtn = (digitalRead(PIN_BTN_RESET) == LOW);
  
  // DEBUG: Print button states every few seconds
  static unsigned long lastButtonDebug = 0;
  if (now - lastButtonDebug > 3000) {
    lastButtonDebug = now;
    Serial.print("Button States - START: ");
    Serial.print(startBtn ? "PRESSED" : "released");
    Serial.print(" | RESET: ");
    Serial.println(resetBtn ? "PRESSED" : "released");
  }

  // WiFi Reconnect Check
  static unsigned long lastWiFiCheck = 0;
  if (now - lastWiFiCheck > 5000) {
    lastWiFiCheck = now;
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected, reconnecting...");
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  }

  // State Machine
  switch (gameState) {
    case STATE_IDLE:
      if (startBtn && !lastStartBtnState) {  // Detect transition: released -> pressed
         Serial.println("\n>>> START BUTTON PRESSED <<<");
         startNewGame();
      }
      break;

    case STATE_RUNNING:
      if (now - lastSampleTime >= SAMPLE_INTERVAL_MS) {
        lastSampleTime += SAMPLE_INTERVAL_MS;
        sampleSensors();
        updatePlayerScoring(player1);
        updatePlayerScoring(player2);

        // Win Condition
        if (player1.score >= WIN_SCORE) {
          Serial.println("\n>>> PLAYER 1 WINS <<<");
          endGame(PLAYER1_NAME);
        }
        else if (player2.score >= WIN_SCORE) {
          Serial.println("\n>>> PLAYER 2 WINS <<<");
          endGame(PLAYER2_NAME);
        }
        else {
           // Send periodic score (approx every 200ms)
           static int c=0; if(++c>=4){
             c=0;
             Serial.print("Score update - P1: ");
             Serial.print(player1.score);
             Serial.print(" | P2: ");
             Serial.println(player2.score);
             String body = "{\"gameId\":\"" + currentGameId + "\",\"player1Score\":" + String(player1.score) + ",\"player2Score\":" + String(player2.score) + "}";
             enqueuePost(API_GAME_SCORE, body);
           }
        }
      }
      if (resetBtn && !lastResetBtnState) {  // Detect transition: released -> pressed
         Serial.println("\n>>> RESET BUTTON PRESSED <<<");
         endGame("Aborted");
      }
      break;

    case STATE_FINISHED:
      if (resetBtn && !lastResetBtnState) {  // Detect transition: released -> pressed
         Serial.println("Reset to IDLE");
         gameState = STATE_IDLE;
      }
      break;
  }

  lastStartBtnState = startBtn; 
  lastResetBtnState = resetBtn;

  updateBuzzers();
  processRequestQueue();
}