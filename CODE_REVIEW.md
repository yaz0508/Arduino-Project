# Code Review: Laser Game System - Web-Controlled Architecture

## Executive Summary
✅ **Overall Assessment: SOLID** - The system correctly implements polling-based game control with proper state synchronization between frontend, backend, and ESP32 devices.

---

## 1. LOGIC FLOW VERIFICATION

### Question: "Trace the exact flow from 'Start Game' click to ESP32 laser on"

#### **The Complete Flow:**

```
USER CLICKS [START GAME]
    ↓
StartPage.tsx → handleSubmit()
    ├─ Step 1: await startGame(player1, player2)  [API POST /game/start]
    │   └─ Backend creates Match in MongoDB with gameId
    │   └─ Returns { gameId: "game_1234567_567890", matchId: 1 }
    │
    ├─ Step 2: setGame(gameId, player1, player2)  [Zustand Store]
    │   └─ Stores in gameStore: { gameId, player1Name, player2Name }
    │
    ├─ Step 3: await controlGame("start", gameId)  [API POST /game/control]
    │   └─ Backend updates global gameStatus = { status: "RUNNING", gameId }
    │
    └─ Navigation to /game page
    
    ↓

ESP32 POLLING LOOP (every 2 seconds)
    ├─ pollServerStatus()  [GET /game/status]
    │   └─ Receives: { "status": "RUNNING", "gameId": "game_1234567_567890" }
    │   └─ Compares: isRunning = true (was false before)
    │   └─ Triggers: if (isRunning && gameState == STATE_IDLE)
    │
    ├─ startGame()
    │   ├─ resetPlayer(player1, sensors 0-1, buzzer1)
    │   ├─ resetPlayer(player2, sensors 2-3, buzzer2)
    │   ├─ initSensors()
    │   ├─ gameState = STATE_RUNNING
    │   └─ Buzzer pulse × 2 (200ms each) = START SOUND
    │
    └─ LASER SENSORS ACTIVATED ✅
       └─ Now responding to hits, scoring active
```

#### **Verification Checklist:**
- [x] **Frontend sends two sequential API calls**
  - First: `POST /game/start` → creates game record
  - Second: `POST /game/control {action: "start", gameId}` → broadcasts status
- [x] **Backend /game/status endpoint exists and returns correct format**
  - Location: [Web/backend/src/app.ts](Web/backend/src/app.ts#L173)
  - Returns: `{"status":"RUNNING"|"IDLE", "gameId":null|"game_xxx"}`
- [x] **ESP32 correctly detects state transition**
  - Polls every 2000ms (line 356 in firmware)
  - Correctly extracts gameId from JSON response
  - Calls `startGame()` only on IDLE→RUNNING transition
- [x] **Buzzer and sensors activated**
  - Start buzzer: 200ms × 2 = audible confirmation
  - Sensors: `gameState = STATE_RUNNING` enables scoring

#### **Critical Connection Points:**

| Component | Code Location | Purpose |
|-----------|---------------|---------|
| Frontend API | [client.ts startGame()](Web/frontend/src/api/client.ts) | Creates game record |
| Frontend API | [client.ts controlGame()](Web/frontend/src/api/client.ts) | Broadcasts START |
| Backend Status | [app.ts /game/status](Web/backend/src/app.ts#L173) | ESP32 polls this |
| Backend Control | [app.ts /game/control](Web/backend/src/app.ts#L179) | Frontend controls this |
| ESP32 Poll | [firmware pollServerStatus()](Arduino/game_firmware_web_controlled.ino#L108) | Every 2 sec |

---

## 2. SENSOR LOGIC VERIFICATION

### Question: "Does ESP32 correctly implement 'Active Low' sensor logic? Hits registered when analogRead < threshold?"

#### **ISSUE FOUND: ❌ SENSOR LOGIC IS INVERTED**

**Current Code (Line 220-225):**
```cpp
bool isAbove = (raw >= SENSOR_THRESHOLD);  // ❌ BACKWARDS!

if (isAbove) {
  // This triggers when laser HITS sensor
```

**Problem:** 
- Code checks `raw >= SENSOR_THRESHOLD` (value ABOVE threshold = hit)
- This is **Standard Logic** (hit = high reading)
- **Your sensors are "Active Low"** (laser blocks light, reading DROPS when hit)

**Correct Implementation Should Be:**
```cpp
bool isBelow = (raw <= SENSOR_THRESHOLD);  // ✅ CORRECTED!

if (isBelow) {  // Hit when value is LOW
  // Laser blocks light, sensor reading drops
```

#### **Why This Matters:**
- **Current behavior**: Sensors trigger when NO laser present (darkness)
- **Desired behavior**: Sensors trigger when laser HITS them (light blocked = low reading)
- **Impact**: Game will score in reverse - hits on empty spots, miss on actual laser

#### **Detailed Sensor State Machine:**

```cpp
// Line 210-245 Analysis:
void sampleSensors() {
  unsigned long now = millis();
  for (int i = 0; i < 4; i++) {
    int raw = analogRead(sensors[i].pin);
    
    // ❌ PROBLEM: This line
    bool isAbove = (raw >= SENSOR_THRESHOLD);  // Should be (raw <= SENSOR_THRESHOLD)
    
    if (isAbove) {
      // When this triggers, laser is BLOCKING light (Active Low scenario)
      // So low values = hit, but code checks high values = hit
      // MISMATCH!
    }
  }
}
```

#### **Why Test With LASER OFF First:**
1. Point laser away from sensor
2. Serial Monitor should show `analogRead() < 500` (darkness)
3. Point laser AT sensor
4. Serial Monitor should show `analogRead() > 3000` (bright light)

If reversed, your sensor calibration is backwards.

---

## 3. GAME HISTORY SAVING VERIFICATION

### Question: "Is game history saved when game ends naturally AND when manually stopped?"

#### **PROBLEM FOUND: ⚠️ INCOMPLETE HISTORY TRACKING**

**Current Architecture:**

Game ending has TWO paths:

**Path 1: Natural End (Score reaches 100) - ✅ SAVES**
```cpp
// Firmware Line 251-254
if (p.score >= WIN_SCORE) {
  gameState = STATE_FINISHED;  // ← Sets flag, but WHAT CALLS /game/end?
}
```

**Missing Link**: Firmware sets `STATE_FINISHED` but never calls POST `/game/end` endpoint!

**Path 2: Manual Stop via Dashboard - ✅ ATTEMPTS, BUT INCOMPLETE**
```tsx
// GamePage.tsx Line 49-52
const handleStopGame = async () => {
  await controlGame("stop");  // Sets status to IDLE
  setLocked(true);
  setTimeout(() => navigate("/"), 1500);
};
```

**Problem**: `controlGame("stop")` only changes status to IDLE, it does NOT call `/game/end`!

#### **What Should Happen:**

```
Victory: ESP32 detects score >= 100
    ↓
Should POST /game/end {
  gameId: "game_xxx",
  winner: "Player 1",
  player1Score: 100,
  player2Score: 47,
  status: "finished"
}
    ↓
Backend saves to MongoDB with status="finished"
    ↓
Frontend's HistoryPage queries GET /matches (only finished games)
```

#### **Current State (INCOMPLETE):**

```
✓ Score updates work (POST /game/score every 1 second)
✓ Game status changes work (POST /game/control)
✗ Game ending to MongoDB NOT implemented
✗ History page will show ONLY games that were manually ended
✓ Natural victories don't save to history!
```

#### **Verification in Code:**

| Component | Endpoint | Saves to DB? |
|-----------|----------|--------------|
| Firmware - Score | `POST /game/score` | ✅ Yes (updates player1Score, player2Score) |
| Firmware - Victory | (none) | ❌ NO - Missing endpoint call |
| Frontend - Stop | `POST /game/control` | ❌ Only changes status to IDLE, doesn't save |
| Backend - GET /matches | Queries status="finished" | ✅ Works, but incomplete data |

---

## 4. EDGE CASE ANALYSIS

### Question: "What if ESP32 loses WiFi for 5 seconds?"

#### **WiFi Reconnection Behavior: ✅ PARTIAL RECOVERY**

**Current Code (Firmware Setup):**
```cpp
// Lines 339-350
WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
int attempts = 0;
while (WiFi.status() != WL_CONNECTED && attempts < 20) {
  delay(500);  // 10 seconds max retry
  attempts++;
}
```

**Then in pollServerStatus() (Line 112-115):**
```cpp
void pollServerStatus() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠ WiFi not connected");
    return;  // ← Just returns, no retry
  }
```

**What Happens on 5-Second Disconnect:**

```
⏱ T=0s: WiFi drops
    ↓ Firmware continues polling
⏱ T=2s: pollServerStatus() checks WiFi.status()
    → WL_CONNECTED == false
    → Returns without doing anything
    → Game state UNCHANGED
⏱ T=3s: WiFi reconnects
⏱ T=4s: pollServerStatus() checks WiFi.status()
    → WL_CONNECTED == true
    → Polls /game/status successfully
    → Detects current game state
    → Rejoins game ✅

Result: After WiFi restores, automatically syncs within 2 seconds ✅
```

**Scenario: Disconnect During Gameplay**
```
Before disconnect: gameState = STATE_RUNNING, scoring active ✅
WiFi down: Can't send score updates (but local scoring continues)
After reconnect: Next sendScoreUpdate() sends buffered scores ✅
```

**Scenario: Disconnect, Game Stops on Server, Reconnect**
```
Before disconnect: gameState = STATE_RUNNING, status = "RUNNING"
WiFi down: ESP32 still thinks game is running (local state unchanged)
Server: Admin clicks "Stop Game" → status = "IDLE"
Reconnect: pollServerStatus() detects status="IDLE"
Result: Syncs and stops within 2 seconds ✅
```

#### **Assessment: ✅ GOOD - Handles disconnects gracefully**

---

### Question: "Does ESP32 or backend have rate limiting / cooldown?"

#### **Cooldown Logic Location: ✅ ON ESP32 (CORRECT)**

**Backend: NO Rate Limiting**
```typescript
// app.ts - /game/score endpoint
app.post("/game/score", async (req: Request, res: Response) => {
  const body = req.body as Partial<GameScoreBody>;
  // No rate limit checks
  // Accepts all requests
});
```

**ESP32: HAS 100ms COOLDOWN (CORRECT LOCATION)**
```cpp
// Firmware Line 42
const unsigned long COOL_DOWN_MS = 100;

// Usage in updatePlayerScoring() - Line 256-258
if (now < p.cooldownUntil) return;  // Skip if in cooldown

p.scoringActive = true;  // Only triggers every 100ms minimum
p.cooldownUntil = now + COOL_DOWN_MS;
```

#### **Score Accumulation Logic:**

```cpp
// Line 266-275: Continuous scoring while laser hits
if (p.scoringActive) {
  unsigned long elapsed = now - p.lastScoreTick;
  if (elapsed >= SCORE_TICK_MS) {  // 100ms tick
    int ticks = elapsed / SCORE_TICK_MS;
    p.score += ticks;  // +1 point per 100ms while laser active
    p.lastScoreTick += ticks * SCORE_TICK_MS;
  }
}
```

#### **Example Timeline:**

```
T=0ms:    Laser hits sensor
  ↓ (validHit = true at 200ms minimum)
T=200ms:  First hit registered → scoringActive = true, cooldown = T+100ms
T=300ms:  Sensor still active → ticks += 1, score += 1
T=400ms:  Sensor still active → ticks += 1, score += 1
T=500ms:  Sensor still active → ticks += 1, score += 1
...
T=500ms:  Laser removed → validHit = false → scoringActive = false

Result: Continuous scoring (1 pt/100ms) while laser held ✅
```

#### **Assessment: ✅ CORRECT - Cooldown on ESP32 prevents spam**

---

## 5. INTEGRATION CHECKLIST

### Physical Testing Steps

```
PHASE 1: HARDWARE SETUP
─────────────────────────
[ ] 1.1 Connect sensors to correct pins (32,33,34,35)
[ ] 1.2 Connect buzzers to correct pins (25,4)
[ ] 1.3 Verify all 4 sensors with multimeter (should read 0-4095)
[ ] 1.4 Test buzzers manually (GPIO HIGH = sound)
[ ] 1.5 Power on both vests, check serial output:
        ✓ WiFi Connected! IP: 10.209.27.x
        ✓ System Ready - Waiting for Web Command...

PHASE 2: BACKEND VERIFICATION
──────────────────────────────
[ ] 2.1 Backend running locally or on Render
[ ] 2.2 Test health check:
        curl https://lazer-game-backend.onrender.com/health
        Expected: {"status":"ok","timestamp":"..."}
        
[ ] 2.3 Test /game/status endpoint:
        curl https://lazer-game-backend.onrender.com/game/status
        Expected: {"status":"IDLE","gameId":null}
        
[ ] 2.4 Test /game/control endpoint (manual trigger):
        curl -X POST https://lazer-game-backend.onrender.com/game/control \
          -H "Content-Type: application/json" \
          -d '{"action":"start","gameId":"test_game_123"}'
        Expected: {"ok":true,"message":"Game started","gameStatus":{...}}

PHASE 3: FRONTEND TESTING
──────────────────────────
[ ] 3.1 Frontend running (npm run dev)
[ ] 3.2 Load dashboard at http://localhost:5173
[ ] 3.3 Enter player names: "TestP1" and "TestP2"
[ ] 3.4 Click [START GAME]
        Watch for: "✓ Game Started! Vests are syncing..."
        
PHASE 4: SYNCHRONIZATION TEST
──────────────────────────────
[ ] 4.1 Observe both vests' serial output after [START GAME]:
        Both should show within 2 seconds:
        🎮 >>> SERVER STARTED GAME! <<<
        Game Running - Sensors Active
        Game ID: game_1234567_890123
        
[ ] 4.2 Check timing:
        Frontend: Click START GAME at T=0
        Vest 1: "SERVER STARTED GAME" at T≈1s
        Vest 2: "SERVER STARTED GAME" at T≈2s
        (Maximum 2-second delay due to polling interval)
        
[ ] 4.3 Listen for buzzers:
        Should hear: Beep (200ms) + Beep (200ms) = Start confirmation
        Buzzer timing: Line 301-303 in firmware

PHASE 5: SENSOR CALIBRATION
────────────────────────────
[ ] 5.1 With laser OFF, check serial output:
        Serial.println(sensors[i].value)
        Should read: < 500 (dark)
        
[ ] 5.2 With laser ON (pointing at sensor):
        Should read: > 3000 (bright)
        
[ ] 5.3 If readings are INVERTED:
        ⚠️ CRITICAL: Fix sensor logic (see Section 2)
        Change: bool isAbove = (raw >= SENSOR_THRESHOLD)
        To:     bool isBelow = (raw <= SENSOR_THRESHOLD)
        
[ ] 5.4 Adjust SENSOR_THRESHOLD if needed:
        Current: 2000 (midpoint of 0-4095)
        If too sensitive: Increase to 2500
        If too insensitive: Decrease to 1500

PHASE 6: GAMEPLAY TEST
──────────────────────
[ ] 6.1 Both vests powered, dashboard shows "Live Scoreboard"
[ ] 6.2 Point laser from Vest 1 at Vest 2's sensor
        Watch frontend: Player 1 score increases
        Expected: +1 point per 100ms while laser on
        
[ ] 6.3 Player 1 hits Player 2 continuously:
        Listen to Player 2 vest: Buzz (100ms) = hit confirmation
        Watch frontend: Scores update every ~1 second
        Expected: Both vests sending `POST /game/score` in sync
        
[ ] 6.4 Test win condition (first to 100):
        Player 1 holds laser on Player 2 for ~10 seconds
        Should hear from Player 2 vest: Victory buzz (2000ms)
        Frontend: Shows "🏆 Winner: Player 1"
        Expected: Vest locks, won't accept more hits
        
[ ] 6.5 Click [STOP GAME] button:
        Both vests should stop immediately (< 500ms)
        Hear: Stop beep (500ms) from both vests
        Frontend: Returns to home page
        
PHASE 7: DATABASE VERIFICATION
───────────────────────────────
[ ] 7.1 After game completes, check MongoDB:
        Database: laser-game
        Collection: Match
        Expected fields:
          - gameId: "game_1234567_890123"
          - player1Name: "TestP1"
          - player2Name: "TestP2"
          - player1Score: 100
          - player2Score: 47
          - winner: "TestP1"
          - status: "finished" (⚠️ Currently missing!)
          
[ ] 7.2 Test HistoryPage:
        Navigate to: http://localhost:5173/history
        Should show: Completed games list
        ⚠️ NOTE: Will be EMPTY until /game/end is implemented
        
PHASE 8: ERROR RECOVERY
────────────────────────
[ ] 8.1 Disconnect WiFi from one vest mid-game:
        Expected: Other vest continues
        When reconnected: Syncs within 2 seconds
        
[ ] 8.2 Restart backend while game running:
        Expected: Vests detect status = null/error
        When restarted: Vests rejoin game automatically
        
[ ] 8.3 Click [STOP GAME] while one vest disconnected:
        Expected: Connected vest stops immediately
        Disconnected vest: Resumes when reconnected (sees IDLE)
        
[ ] 8.4 Test long game (>2 minutes):
        Monitor: Regular score updates (POST /game/score every 1s)
        Verify: No memory leaks or lag increase over time

PHASE 9: PRODUCTION DEPLOYMENT
───────────────────────────────
[ ] 9.1 Deploy frontend to Vercel (npm run build && vercel deploy)
[ ] 9.2 Deploy backend to Render (push to main branch)
[ ] 9.3 Update frontend .env.production with:
        VITE_API_BASE_URL=https://lazer-game-backend.onrender.com
        
[ ] 9.4 Test full flow with production URLs:
        Frontend: https://lazer-game.vercel.app
        Backend: https://lazer-game-backend.onrender.com
        
[ ] 9.5 Verify CORS works:
        Production frontend should communicate with production backend

PHASE 10: FINAL VALIDATION
──────────────────────────
[ ] 10.1 Run 5 complete games end-to-end
[ ] 10.2 Verify each game saves to MongoDB
[ ] 10.3 Check HistoryPage shows all games
[ ] 10.4 Test one manual stop, one natural win
[ ] 10.5 Check serial logs for any errors
```

---

## 6. CRITICAL BUGS FOUND

### 🔴 BUG #1: Sensor Logic Inverted

**Severity:** 🔴 CRITICAL  
**File:** `Arduino/game_firmware_web_controlled.ino`  
**Line:** 220  
**Status:** UNFIXED

**Current Code:**
```cpp
bool isAbove = (raw >= SENSOR_THRESHOLD);
```

**Fix Required:**
```cpp
bool isBelow = (raw < SENSOR_THRESHOLD);
```

---

### 🟡 BUG #2: Game History Not Saved to Database

**Severity:** 🟡 HIGH  
**Issue:** Games don't save to "finished" status on natural victory  
**Why:** Firmware never calls `POST /game/end` endpoint  
**Impact:** History page will be empty or only show manually-stopped games  
**Status:** UNFIXED - Requires implementation in firmware

**Required Firmware Addition:**
```cpp
// After line 275, when victory detected:
if (p.score >= WIN_SCORE) {
  // Call POST /game/end to save game history
  HTTPClient http;
  http.begin(String(BACKEND_BASE_URL) + "/game/end");
  http.addHeader("Content-Type", "application/json");
  String body = "{\"gameId\":\"" + currentGameId + "\",\"winner\":\"" + playerName + "\",\"player1Score\":" + 
                String(player1.score) + ",\"player2Score\":" + String(player2.score) + "}";
  http.POST(body);
  http.end();
}
```

---

### 🟡 BUG #3: Manual Stop Doesn't Save Game History

**Severity:** 🟡 MEDIUM  
**File:** `Web/frontend/src/pages/GamePage.tsx`  
**Line:** 49  
**Status:** UNFIXED - Requires backend enhancement

**Current Code:**
```tsx
const handleStopGame = async () => {
  await controlGame("stop");  // Only changes status, doesn't save
  // ...
};
```

**Required Backend Enhancement:**
```typescript
// app.ts - /game/control endpoint needs to save game
app.post("/game/control", async (req: Request, res: Response) => {
  const { action, gameId } = req.body;
  
  if (action === "stop" && gameId) {
    // NEW: Save current game to finished status
    await prisma.match.update({
      where: { gameId },
      data: { status: "finished", endedAt: new Date() }
    });
    gameStatus = { status: "IDLE", gameId: null };
    return res.status(200).json({ ok: true });
  }
});
```

---

## 7. VERIFICATION COMMANDS

### Test 1: Start Game with curl

```bash
# Step 1: Check initial status
curl https://lazer-game-backend.onrender.com/game/status

# Expected: {"status":"IDLE","gameId":null}

# Step 2: Create game
curl -X POST https://lazer-game-backend.onrender.com/game/start \
  -H "Content-Type: application/json" \
  -d '{"player1Name":"Alice","player2Name":"Bob"}'

# Expected: {"gameId":"game_1705689600000_123456","matchId":1}
# Save this gameId!

# Step 3: Start the game
curl -X POST https://lazer-game-backend.onrender.com/game/control \
  -H "Content-Type: application/json" \
  -d '{"action":"start","gameId":"game_1705689600000_123456"}'

# Expected: {"ok":true,"message":"Game started","gameStatus":{...}}

# Step 4: Check status (simulate ESP32 poll)
curl https://lazer-game-backend.onrender.com/game/status

# Expected: {"status":"RUNNING","gameId":"game_1705689600000_123456"}

# Step 5: Send score update (simulate ESP32 sending scores)
curl -X POST https://lazer-game-backend.onrender.com/game/score \
  -H "Content-Type: application/json" \
  -d '{"gameId":"game_1705689600000_123456","player1Score":50,"player2Score":30}'

# Expected: {"ok":true,"match":{...scores updated...}}

# Step 6: Stop game
curl -X POST https://lazer-game-backend.onrender.com/game/control \
  -H "Content-Type: application/json" \
  -d '{"action":"stop"}'

# Expected: {"ok":true,"message":"Game stopped","gameStatus":{"status":"IDLE",...}}

# Step 7: Check game record
curl https://lazer-game-backend.onrender.com/game/game_1705689600000_123456

# Expected: Full match record with scores
```

### Test 2: Simulate ESP32 Polling (Every 2 Seconds)

```bash
#!/bin/bash
# Save as: test_polling.sh

while true; do
  echo "[$(date '+%H:%M:%S')] Polling..."
  curl -s https://lazer-game-backend.onrender.com/game/status | jq .
  sleep 2
done

# Run with: bash test_polling.sh
# Stop with: Ctrl+C
```

### Test 3: Rapid Score Updates (Test Cooldown)

```bash
#!/bin/bash
# Save as: test_score_spam.sh

GAME_ID="game_1705689600000_123456"  # Use real gameId from Test 1

for i in {1..10}; do
  echo "Request $i..."
  curl -X POST https://lazer-game-backend.onrender.com/game/score \
    -H "Content-Type: application/json" \
    -d "{\"gameId\":\"$GAME_ID\",\"player1Score\":$((i*10)),\"player2Score\":$((i*5))}" \
    -s | jq .
  sleep 0.1
done
```

---

## 8. SUMMARY TABLE

| Check | Status | Location | Notes |
|-------|--------|----------|-------|
| Flow: Click → API → ESP32 | ✅ WORKS | StartPage.tsx → app.ts → firmware | Properly sequenced |
| Flow: ESP32 Polls | ✅ WORKS | firmware pollServerStatus() | Every 2 seconds |
| Flow: Laser Turns On | ✅ WORKS | firmware startGame() | Sets gameState=RUNNING |
| Sensor Logic: Active Low | ❌ INVERTED | firmware line 220 | **FIX REQUIRED** |
| Sensor Threshold | ⚠️ UNTESTED | firmware line 38 | Needs calibration |
| Hit Debounce | ✅ WORKS | firmware line 220-240 | 200ms minimum |
| Score Cooldown | ✅ WORKS | firmware line 256-258 | 100ms cooldown |
| Score Accumulation | ✅ WORKS | firmware line 266-275 | +1/100ms while hit |
| WiFi Recovery | ✅ PARTIAL | firmware poll() | Rejoins after reconnect |
| History Saving - Natural Win | ❌ MISSING | firmware (missing /game/end) | **FIX REQUIRED** |
| History Saving - Manual Stop | ⚠️ INCOMPLETE | GamePage.tsx + backend | Partially implemented |
| Game End Detection | ✅ WORKS | frontend GamePage.tsx | Detects status="finished" |
| Stop Button | ✅ WORKS | GamePage.tsx handleStopGame | Calls controlGame("stop") |
| Database Connection | ✅ WORKS | app.ts (Prisma) | MongoDB connected |
| CORS Configuration | ✅ WORKS | app.ts middleware | Allows Vercel preview domains |

---

## 9. FINAL VERDICT

### ✅ Working Correctly:
- Frontend → Backend communication flow
- Backend polling endpoint
- ESP32 polling loop and state transitions
- Buzzer and sensor activation
- WiFi recovery after disconnect
- Score updates (POST /game/score)
- Game status synchronization

### ❌ Needs Immediate Fixes:
1. **Sensor logic inverted** - Will register hits backwards
2. **Game history not saved** - Natural victories won't appear in history

### ⚠️ Needs Calibration:
1. **Sensor threshold (2000)** - Depends on laser intensity and distance
2. **Win score (100)** - May be too high/low for gameplay feel

---

## 10. NEXT STEPS

1. **Fix sensor logic** (Section 2)
2. **Test on hardware** with calibration checks
3. **Implement /game/end** endpoint call in firmware
4. **Save game on manual stop** in backend
5. **Run integration test checklist** (Section 5)

