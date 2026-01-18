# Critical Bug Fixes Required

## 🔴 BUG #1: Sensor Logic is Inverted

### The Problem
Your sensors are **Active Low** (laser hits → reading drops), but the code checks for **high readings**.

### Current Broken Code
**File:** `Arduino/game_firmware_web_controlled.ino`  
**Line:** 220

```cpp
bool isAbove = (raw >= SENSOR_THRESHOLD);  // ❌ WRONG!

if (isAbove) {
  if (!sensors[i].aboveThreshold) {
    sensors[i].aboveThreshold = true;
    sensors[i].aboveStartTime = now;
  }
  // ...will trigger hit on DARKNESS, not LASER
}
```

### The Fix

**Replace this entire section (lines 214-243):**

```cpp
void sampleSensors() {
  unsigned long now = millis();
  for (int i = 0; i < 4; i++) {
    int raw = analogRead(sensors[i].pin);
    sensors[i].value = raw;
    bool isBelow = (raw < SENSOR_THRESHOLD);  // ✅ CORRECTED: Low reading = laser blocked light

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
```

### What Changed
- `bool isAbove = (raw >= SENSOR_THRESHOLD)` → `bool isBelow = (raw < SENSOR_THRESHOLD)`
- Variable name changed to `isBelow` for clarity
- Logic now correctly triggers when laser BLOCKS light (low reading)

### How to Verify the Fix

**Before uploading fix:**
1. Open Serial Monitor (115200 baud)
2. Point laser AWAY from sensor
   - Serial should show: `analogRead() > 3000` (dark, high reading)
3. Point laser AT sensor
   - Serial should show: `analogRead() < 500` (bright, low reading)

**If readings are backwards:** Your sensor wiring or sensor type might need adjustment.

---

## 🟡 BUG #2: Games Don't Save to History (Natural Victory)

### The Problem
When a player reaches 100 points, the firmware detects victory but never tells the backend to save the game. Games only get saved when manually stopped or if they're already finished.

### Current Code - What's Missing
**File:** `Arduino/game_firmware_web_controlled.ino`  
**Line:** 275-277

```cpp
// Check win condition
if (p.score >= WIN_SCORE) {
  Serial.print(">>> ");
  Serial.print(playerName);
  Serial.println(" WINS! <<<");
  buzzerPulse(*p.myBuzzer, 2000);  // Victory buzz
  gameState = STATE_FINISHED;
  // ❌ BUT NO CALL TO /game/end ENDPOINT!
}
```

### The Fix

**Add this code AFTER line 277** (after `gameState = STATE_FINISHED;`):

```cpp
  // ✅ NEW: Save game to database
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
```

### Complete Fixed updatePlayerScoring() Function

Here's the entire function with both fixes integrated:

```cpp
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
        
        // ✅ NEW: Save game to database
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
```

---

## 🟡 BUG #3: Manual Stop Doesn't Save Game History

### The Problem
When you click "Stop Game" on the dashboard, it sets the game status to IDLE but doesn't mark the game as "finished" in the database. So it never appears in the History page.

### Current Code - Backend Issue
**File:** `Web/backend/src/app.ts`  
**Line:** 179-189

```typescript
// POST /game/control - Admin endpoint to start/stop game
app.post("/game/control", (req: Request, res: Response) => {
  const { action, gameId } = req.body;
  
  if (action === "start" && gameId) {
    gameStatus = { status: "RUNNING", gameId };
    return res.status(200).json({ ok: true, message: "Game started", gameStatus });
  } else if (action === "stop") {
    gameStatus = { status: "IDLE", gameId: null };
    // ❌ BUT DOESN'T SAVE GAME TO DATABASE!
    return res.status(200).json({ ok: true, message: "Game stopped", gameStatus });
  }
  // ...
});
```

### The Fix

**Replace the entire /game/control endpoint** (lines 179-189):

```typescript
// POST /game/control - Admin endpoint to start/stop game
app.post("/game/control", async (req: Request, res: Response) => {
  const { action, gameId } = req.body;
  
  if (action === "start" && gameId) {
    gameStatus = { status: "RUNNING", gameId };
    return res.status(200).json({ ok: true, message: "Game started", gameStatus });
  } else if (action === "stop") {
    gameStatus = { status: "IDLE", gameId: null };
    
    // ✅ NEW: Save game to finished status if gameId exists
    if (gameId) {
      try {
        await prisma.match.update({
          where: { gameId },
          data: { 
            status: "finished", 
            endedAt: new Date()
          }
        });
        console.log(`Game ${gameId} manually stopped and saved`);
      } catch (err) {
        console.error(`Failed to save game ${gameId}:`, err);
        // Continue anyway - status change is more important
      }
    }
    
    return res.status(200).json({ ok: true, message: "Game stopped", gameStatus });
  } else {
    return res.status(400).json({ error: "Invalid action or missing gameId" });
  }
});
```

### What Changed
- Added `async` to function signature (needed for `await prisma`)
- After setting status to "IDLE", now also updates the database record to status="finished"
- Wrapped in try-catch so errors don't break the stop functionality
- Added console.log for debugging

---

## Summary of All Fixes

| Bug | File | Line | Fix Type | Impact |
|-----|------|------|----------|--------|
| Sensor inverted | firmware | 220 | Change logic | **CRITICAL** - Game won't work at all |
| History not saved (victory) | firmware | 277 | Add endpoint call | **HIGH** - Lost game records |
| History not saved (manual stop) | app.ts | 182 | Add database update | **HIGH** - Lost game records |

---

## Testing After Fixes

### Test 1: Sensor Logic
```
1. Open Serial Monitor
2. Point laser AWAY → Should show: analogRead() > 3000 ✓
3. Point laser AT sensor → Should show: analogRead() < 500 ✓
4. If backwards, fix wiring or sensor orientation
```

### Test 2: Natural Victory Saves
```
1. Start game on dashboard
2. Player 1 continuously hits Player 2 for ~10 seconds
3. Watch for ">>> Player 2 WINS! <<<" on Vest 2 serial
4. Check serial for "✓ Game saved to database"
5. Query MongoDB or use HistoryPage - game should appear
```

### Test 3: Manual Stop Saves
```
1. Start game on dashboard
2. Let it run for 10-20 seconds
3. Click "Stop Game" button
4. Check backend console for: "Game game_xxx manually stopped and saved"
5. Query HistoryPage - game should appear with scores from when stopped
```

---

## How to Apply These Fixes

### Option 1: Manual Copy-Paste (Safest)
1. Open the files mentioned above
2. Find the exact line numbers
3. Replace with the fixed code
4. Save and re-upload

### Option 2: Using find/replace in VS Code
1. Press `Ctrl+H` (Find and Replace)
2. Find: `bool isAbove = (raw >= SENSOR_THRESHOLD);`
3. Replace: `bool isBelow = (raw < SENSOR_THRESHOLD);`
4. Click "Replace All"

### Option 3: Request I Apply Them
If you want me to apply these fixes automatically, just say "Apply all critical bug fixes"

