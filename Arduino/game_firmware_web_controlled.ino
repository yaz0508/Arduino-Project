// ESP32 Laser Game - Web Controlled Edition (No Physical Buttons)
// - The ESP32 polls the backend server to check if game is RUNNING
// - Web dashboard controls start/stop via backend
// - Both vests sync automatically

#include <WiFi.h>
#include <HTTPClient.h>

// ======================== CONFIGURATION ========================

// ---- WiFi configuration ----
const char* WIFI_SSID     = "ADAPT LNG";
const char* WIFI_PASSWORD = "sigepo123";

// ---- Backend configuration ----
const char* BACKEND_BASE_URL = "https://lazer-game-backend.onrender.com";
const char* API_GAME_STATUS  = "/game/status";   // Poll this
const char* API_GAME_SCORE   = "/game/score";    // Send score updates
const char* API_GAME_CONTROL = "/game/control";  // Admin only

// ---- Player configuration ----
const char* PLAYER1_NAME = "Player 1";
const char* PLAYER2_NAME = "Player 2";

// ---- Pin assignments ----
// Sensors (ADC Pins)
const int PIN_P1_S1 = 32;  // Player 1 Sensor 1
const int PIN_P1_S2 = 33;  // Player 1 Sensor 2
const int PIN_P2_S1 = 34;  // Player 2 Sensor 1
const int PIN_P2_S2 = 35;  // Player 2 Sensor 2

// Buzzers
const int PIN_P1_BUZZER = 25; // Player 1 Buzzer
const int PIN_P2_BUZZER = 4;  // Player 2 Buzzer

// ---- Game tuning ----
const int SENSOR_THRESHOLD = 2000;
const int WIN_SCORE = 100;
const unsigned long SAMPLE_INTERVAL_MS = 50;
const unsigned long HIT_MIN_DURATION_MS = 200;
const unsigned long COOL_DOWN_MS = 100;
const unsigned long SCORE_TICK_MS = 100;
const unsigned long POLL_INTERVAL_MS = 2000;  // Check server every 2 seconds

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
  PlayerBuzzer* myBuzzer;
};

// ======================== GLOBALS ========================

GameState gameState = STATE_IDLE;
SensorState sensors[4];
PlayerBuzzer buzzer1;
PlayerBuzzer buzzer2;
PlayerState player1;
PlayerState player2;

String currentGameId = "";
unsigned long lastSampleTime = 0;
unsigned long lastPollTime = 0;

// ======================== BUZZER FUNCTIONS ========================

void setBuzzerState(PlayerBuzzer& b, bool on) {
  digitalWrite(b.pin, on ? HIGH : LOW);
  b.active = on;
  if (!on) b.offAt = 0;
}

void buzzerPulse(PlayerBuzzer& b, unsigned long durationMs) {
  unsigned long now = millis();
  setBuzzerState(b, true);
  b.offAt = now + durationMs;
}

void updateBuzzers() {
  unsigned long now = millis();
  if (buzzer1.active && now >= buzzer1.offAt) setBuzzerState(buzzer1, false);
  if (buzzer2.active && now >= buzzer2.offAt) setBuzzerState(buzzer2, false);
}

// ======================== POLLING & SYNC ========================

void pollServerStatus() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠ WiFi not connected");
    return;
  }

  HTTPClient http;
  String url = String(BACKEND_BASE_URL) + API_GAME_STATUS;
  http.begin(url);
  http.setTimeout(3000);

  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    Serial.print("Server Status: ");
    Serial.println(payload);

    // Check if game is RUNNING
    bool isRunning = (payload.indexOf("\"status\":\"RUNNING\"") >= 0 || payload.indexOf("\"status\": \"RUNNING\"") >= 0);
    bool isIdle = (payload.indexOf("\"status\":\"IDLE\"") >= 0 || payload.indexOf("\"status\": \"IDLE\"") >= 0);

    // Extract gameId if present
    if (isRunning) {
      int idx = payload.indexOf("\"gameId\"");
      if (idx > 0) {
        int colon = payload.indexOf(':', idx);
        int q1 = payload.indexOf('"', colon);
        int q2 = payload.indexOf('"', q1 + 1);
        if (q1 >= 0 && q2 > q1) {
          currentGameId = payload.substring(q1 + 1, q2);
        }
      }
    }

    // ---- STATE TRANSITIONS ----

    // IDLE -> RUNNING: Server started game
    if (isRunning && gameState == STATE_IDLE) {
      Serial.println("\n🎮 >>> SERVER STARTED GAME! <<<");
      startGame();
    }

    // RUNNING -> IDLE: Server stopped game
    if (isIdle && gameState == STATE_RUNNING) {
      Serial.println("\n⏹ >>> SERVER STOPPED GAME <<<");
      stopGame();
    }
  } else {
    Serial.print("Poll Error: ");
    Serial.println(httpCode);
  }

  http.end();
}

void sendScoreUpdate() {
  if (WiFi.status() != WL_CONNECTED || currentGameId.length() == 0) return;

  HTTPClient http;
  http.begin(String(BACKEND_BASE_URL) + API_GAME_SCORE);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(3000);

  String body = "{\"gameId\":\"" + currentGameId + "\",\"player1Score\":" + String(player1.score) + ",\"player2Score\":" + String(player2.score) + "}";
  
  Serial.print("Sending Score: ");
  Serial.println(body);
  
  int httpCode = http.POST(body);
  if (httpCode == 200) {
    Serial.println("✓ Score sent");
  }
  http.end();
}

// ======================== SENSOR & SCORING ========================

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

void resetPlayer(PlayerState& p, int idxA, int idxB, PlayerBuzzer* b) {
  p.sensorIndexA = idxA;
  p.sensorIndexB = idxB;
  p.score = 0;
  p.activeSensorIndex = -1;
  p.scoringActive = false;
  p.cooldownUntil = 0;
  p.myBuzzer = b;
}

void sampleSensors() {
  unsigned long now = millis();
  for (int i = 0; i < 4; i++) {
    int raw = analogRead(sensors[i].pin);
    sensors[i].value = raw;
    bool isBelow = (raw < SENSOR_THRESHOLD);  // Active Low: laser blocks light = low reading

    if (isBelow) {
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

void updatePlayerScoring(PlayerState& p, const char* playerName) {
  unsigned long now = millis();

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

  if (!p.scoringActive) {
    if (now < p.cooldownUntil) return;

    p.scoringActive = true;
    p.activeSensorIndex = bestSensor;
    p.lastScoreTick = now;
    buzzerPulse(*p.myBuzzer, 100);
    Serial.println("🎯 HIT DETECTED - Buzzer activated!");
  }

  if (p.scoringActive) {
    unsigned long elapsed = now - p.lastScoreTick;
    if (elapsed >= SCORE_TICK_MS) {
      int ticks = elapsed / SCORE_TICK_MS;
      p.score += ticks;
      if (p.score > WIN_SCORE) p.score = WIN_SCORE;
      p.lastScoreTick += ticks * SCORE_TICK_MS;

      // Check win condition
      if (p.score >= WIN_SCORE) {
        Serial.print(">>> ");
        Serial.print(playerName);
        Serial.println(" WINS! <<<");
        buzzerPulse(*p.myBuzzer, 2000);  // Victory buzz
        gameState = STATE_FINISHED;
        
        // Save game to database
        if (WiFi.status() == WL_CONNECTED && currentGameId.length() > 0) {
          HTTPClient http;
          http.begin(String(BACKEND_BASE_URL) + "/game/end");
          http.addHeader("Content-Type", "application/json");
          http.setTimeout(3000);
          
          String body = "{\"gameId\":\"" + currentGameId + "\",\"winner\":\"" + String(playerName) + 
                        "\",\"player1Score\":" + String(player1.score) + ",\"player2Score\":" + String(player2.score) + "}";
          
          int httpCode = http.POST(body);
          if (httpCode == 200) {
            Serial.println("✓ Game saved to database");
          } else {
            Serial.print("✗ Failed to save game: ");
            Serial.println(httpCode);
          }
          http.end();
        }
      }
    }
  }
}

// ======================== GAME CONTROL ========================

void startGame() {
  Serial.println("Game Running - Sensors Active");
  Serial.print("Game ID: ");
  Serial.println(currentGameId);

  resetPlayer(player1, 0, 1, &buzzer1);
  resetPlayer(player2, 2, 3, &buzzer2);
  initSensors();

  gameState = STATE_RUNNING;

  // Start beep
  buzzerPulse(buzzer1, 200);
  buzzerPulse(buzzer2, 200);
}

void stopGame() {
  Serial.println("Game Stopped");
  gameState = STATE_IDLE;
  currentGameId = "";

  // Stop beep
  buzzerPulse(buzzer1, 500);
  buzzerPulse(buzzer2, 500);
}

// ======================== SETUP & LOOP ========================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n=== LASER GAME SYSTEM STARTING (Web-Controlled) ===");

  // Setup Buzzers
  buzzer1.pin = PIN_P1_BUZZER;
  buzzer2.pin = PIN_P2_BUZZER;
  pinMode(buzzer1.pin, OUTPUT);
  pinMode(buzzer2.pin, OUTPUT);
  setBuzzerState(buzzer1, false);
  setBuzzerState(buzzer2, false);

  // Setup Sensors
  initSensors();
  resetPlayer(player1, 0, 1, &buzzer1);
  resetPlayer(player2, 2, 3, &buzzer2);

  // Connect WiFi
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("✓ WiFi Connected! IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("✗ WiFi Failed");
  }

  Serial.println("Backend URL: " + String(BACKEND_BASE_URL));
  Serial.println("✓ System Ready - Waiting for Web Command...");

  // Startup beeps
  for (int i = 0; i < 3; i++) {
    buzzerPulse(buzzer1, 100);
    delay(150);
  }
}

void loop() {
  unsigned long now = millis();

  // 1. POLL SERVER (Every 2 seconds)
  if (now - lastPollTime > POLL_INTERVAL_MS) {
    lastPollTime = now;
    pollServerStatus();
  }

  // 2. GAME LOGIC (Only if game is RUNNING)
  if (gameState == STATE_RUNNING) {
    if (now - lastSampleTime >= SAMPLE_INTERVAL_MS) {
      lastSampleTime += SAMPLE_INTERVAL_MS;
      sampleSensors();
      updatePlayerScoring(player1, PLAYER1_NAME);
      updatePlayerScoring(player2, PLAYER2_NAME);

      // Send score updates periodically
      static int scoreUpdateCounter = 0;
      if (++scoreUpdateCounter >= 20) {  // Every ~1 second
        scoreUpdateCounter = 0;
        Serial.print("📊 Score - P1: ");
        Serial.print(player1.score);
        Serial.print(" | P2: ");
        Serial.println(player2.score);
        sendScoreUpdate();
      }
    }
  }

  updateBuzzers();
}
