// ESP32 Laser Game - Dual Core Edition (PLAYER 2)
// Core 0: Network/WiFi (Background)
// Core 1: Sensors/Game Logic (Responsive)

#include <WiFi.h>
#include <HTTPClient.h>

// ======================== CONFIGURATION ========================

// ---- WiFi configuration ----
const char* WIFI_SSID     = "ADAPT LNG";
const char* WIFI_PASSWORD = "sigepo123";

// ---- Backend configuration ----
const char* BACKEND_BASE_URL = "https://lazer-game-backend.onrender.com";
const char* API_GAME_STATUS  = "/game/status";
const char* API_GAME_SCORE   = "/game/score";

// ---- Player configuration ----
const char* PLAYER_NAME = "Player 2";
const int PLAYER_NUMBER = 2;

// ---- Pin assignments (PLAYER 2 ONLY) ----
const int PIN_SENSOR_1 = 34;  // Sensor 1
const int PIN_SENSOR_2 = 35;  // Sensor 2
const int PIN_BUZZER = 4;     // Player 2 Buzzer

// ---- Game tuning ----
const int SENSOR_THRESHOLD = 2000;
const int WIN_SCORE = 100;
const int POINTS_PER_HIT = 10;
const unsigned long HIT_COOLDOWN_MS = 500;  // Min time between hits
const unsigned long NETWORK_POLL_MS = 2000; // Poll server every 2 seconds

// ======================== SHARED VARIABLES (volatile for thread safety) ========================

volatile bool gameRunning = false;     // Shared game state
volatile int localScore = 0;           // Real-time score on device
volatile bool hasWon = false;          // Victory flag
String sharedGameId = "";              // Game ID (String, not volatile)
volatile unsigned long lastHitTime = 0;

TaskHandle_t NetworkTask;              // Core 0 task handle

// ======================== BUZZER CONTROL ========================

void buzzerPulse(unsigned long durationMs) {
  digitalWrite(PIN_BUZZER, HIGH);
  delay(durationMs);
  digitalWrite(PIN_BUZZER, LOW);
}

// ======================== CORE 0: NETWORK TASK (Background) ========================

void networkLoop(void * parameter) {
  unsigned long lastPollTime = 0;
  int lastSentScore = -1;

  for(;;) {  // Infinite loop for Core 0
    unsigned long now = millis();
    
    // 1. POLL SERVER STATUS (Every 2 seconds)
    if (WiFi.status() == WL_CONNECTED && (now - lastPollTime > NETWORK_POLL_MS)) {
      lastPollTime = now;
      
      HTTPClient http;
      http.begin(String(BACKEND_BASE_URL) + API_GAME_STATUS);
      http.setTimeout(3000);
      int code = http.GET();
      
      if (code == 200) {
        String payload = http.getString();
        
        // Check game status
        bool isRunning = (payload.indexOf("\"status\":\"RUNNING\"") >= 0 || 
                         payload.indexOf("\"status\": \"RUNNING\"") >= 0);
        
        // Update shared game state
        gameRunning = isRunning;
        
        if (isRunning) {
          // Extract gameId
          int idIndex = payload.indexOf("\"gameId\"");
          if (idIndex > 0) {
            int colon = payload.indexOf(':', idIndex);
            int q1 = payload.indexOf('"', colon);
            int q2 = payload.indexOf('"', q1 + 1);
            if (q1 >= 0 && q2 > q1) {
              sharedGameId = payload.substring(q1 + 1, q2);
            }
          }
          
          Serial.println("[Core 0] ✓ Game is RUNNING");
        } else {
          Serial.println("[Core 0] ⏹ Game is IDLE");
          // Reset score if game stops
          if (!hasWon) {
            localScore = 0;
          }
        }
      } else {
        Serial.print("[Core 0] Poll error: ");
        Serial.println(code);
      }
      http.end();
    }
    
    // 2. SEND SCORE UPDATE (Only if changed and gameId exists)
    if (localScore != lastSentScore && sharedGameId != "" && gameRunning) {
      HTTPClient httpScore;
      httpScore.begin(String(BACKEND_BASE_URL) + API_GAME_SCORE);
      httpScore.addHeader("Content-Type", "application/json");
      httpScore.setTimeout(3000);
      
      String body = "{\"gameId\":\"" + sharedGameId + "\",\"player2Score\":" + String(localScore) + "}";
      int scoreCode = httpScore.POST(body);
      
      if (scoreCode == 200) {
        lastSentScore = localScore;
        Serial.print("[Core 0] Sent score: ");
        Serial.println(localScore);
      }
      httpScore.end();
    }
    
    // 3. SEND WIN NOTIFICATION
    if (hasWon && sharedGameId != "") {
      HTTPClient httpEnd;
      httpEnd.begin(String(BACKEND_BASE_URL) + "/game/end");
      httpEnd.addHeader("Content-Type", "application/json");
      httpEnd.setTimeout(3000);
      
      String body = "{\"gameId\":\"" + sharedGameId + "\",\"winner\":\"" + String(PLAYER_NAME) + 
                    "\",\"player2Score\":" + String(localScore) + "}";
      
      int endCode = httpEnd.POST(body);
      if (endCode == 200) {
        Serial.println("[Core 0] ✓ Victory sent to server");
      }
      httpEnd.end();
      
      hasWon = false;  // Prevent re-sending
    }
    
    // Yield to other tasks
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// ======================== CORE 1: SETUP (Main) ========================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=== LASER GAME - DUAL CORE EDITION (PLAYER 2) ===");
  Serial.println("Core 0: Network/WiFi (Background)");
  Serial.println("Core 1: Sensors/Game (Responsive)");
  
  // Setup Buzzer
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  
  // Setup Sensors
  pinMode(PIN_SENSOR_1, INPUT);
  pinMode(PIN_SENSOR_2, INPUT);
  
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
  
  // Launch Network Task on Core 0
  xTaskCreatePinnedToCore(
    networkLoop,        // Function to run
    "NetworkTask",      // Name
    10000,              // Stack size (bytes)
    NULL,               // Parameter
    1,                  // Priority
    &NetworkTask,       // Handle
    0                   // Core ID (0 = Background)
  );
  
  Serial.println("✓ Network task started on Core 0");
  Serial.println("✓ Sensor loop running on Core 1");
  Serial.println("✓ System Ready - Waiting for Web Command...");
  
  // Startup beeps
  for (int i = 0; i < 3; i++) {
    buzzerPulse(100);
    delay(150);
  }
}

// ======================== CORE 1: LOOP (Main - Sensors & Game) ========================

void loop() {
  // This runs on CORE 1 and is NEVER blocked by WiFi!
  
  if (gameRunning && !hasWon) {
    // Read both sensors
    int val1 = analogRead(PIN_SENSOR_1);
    int val2 = analogRead(PIN_SENSOR_2);
    
    bool hit1 = (val1 < SENSOR_THRESHOLD);
    bool hit2 = (val2 < SENSOR_THRESHOLD);
    
    // Log sensor readings periodically
    static unsigned long lastLogTime = 0;
    unsigned long now = millis();
    if (now - lastLogTime > 500) {
      lastLogTime = now;
      Serial.print("[Core 1] Sensor 1: ");
      Serial.print(val1);
      Serial.print(" (");
      Serial.print(hit1 ? "HIT" : "safe");
      Serial.print(") | Sensor 2: ");
      Serial.print(val2);
      Serial.print(" (");
      Serial.print(hit2 ? "HIT" : "safe");
      Serial.println(")");
    }
    
    // Process hits with cooldown
    if ((hit1 || hit2) && (now - lastHitTime > HIT_COOLDOWN_MS)) {
      lastHitTime = now;
      
      // Award points
      localScore += POINTS_PER_HIT;
      if (localScore > WIN_SCORE) localScore = WIN_SCORE;
      
      // Feedback
      Serial.print("[Core 1] 🎯 HIT! Score: ");
      Serial.println(localScore);
      
      buzzerPulse(100);
      
      // Check win condition
      if (localScore >= WIN_SCORE) {
        Serial.println("[Core 1] >>> PLAYER 2 WINS! <<<");
        hasWon = true;
        gameRunning = false;
        
        // Victory buzz (2 seconds)
        buzzerPulse(2000);
      }
    }
  }
  
  // Very short delay to prevent CPU overload
  delay(10);
}
