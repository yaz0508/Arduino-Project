# Web-Controlled Laser Game System

## Overview

This is a **web-controlled laser tag game** where:
- **Frontend (Web Dashboard)**: Control start/stop from your laptop/phone
- **Backend (Node.js/Express)**: Manages game state and syncs with ESP32s
- **ESP32 Vests (Embedded)**: Poll the server for game status and execute gameplay

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    WEB DASHBOARD                            │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ StartPage: Enter player names → Click "Start Game"  │   │
│  │ GamePage: Watch live scores → Click "Stop Game"     │   │
│  └─────────────────────────────────────────────────────┘   │
└────────────────────────────┬────────────────────────────────┘
                             │ HTTP POST /game/control
                             ▼
         ┌─────────────────────────────────┐
         │      BACKEND (Render)           │
         │  ┌───────────────────────────┐  │
         │  │ /game/start    (CREATE)   │  │ Updates Database
         │  │ /game/score    (UPDATE)   │  │
         │  │ /game/status   (POLL)     │  │ Stores game state
         │  │ /game/control  (ADMIN)    │  │
         │  │ /matches       (HISTORY)  │  │
         │  └───────────────────────────┘  │
         │         ▲         ▲              │
         └─────────┼─────────┼──────────────┘
                   │         │
           Poll    │         │ Send Score
           Status  │         │ Updates
                   │         │
         ┌─────────┴─────────▼──────────────┐
         │    ESP32 VEST (Player 1)         │
         │  ┌───────────────────────────┐   │
         │  │ Every 2 secs: Poll Status │   │
         │  │ If RUNNING: Read Sensors  │   │
         │  │ If IDLE: Sleep (Wait)     │   │
         │  │ Beep on Hit               │   │
         │  │ Send Score Updates        │   │
         │  └───────────────────────────┘   │
         └──────────────────────────────────┘

         ┌─────────────────────────────────┐
         │    ESP32 VEST (Player 2)        │
         │  (Same as Player 1)             │
         └─────────────────────────────────┘
```

## Game Flow

### 1. **Startup** (ESP32 Initialization)
```
ESP32 Powers On
  ├─ Connects to WiFi
  ├─ Sets gameState = IDLE
  ├─ Starts polling server every 2 seconds
  │  └─ GET /game/status
  └─ Waits for command from server
     └─ Lasers OFF, Sensors OFF, Ready for game
```

### 2. **Game Start** (User Clicks "Start Game")
```
User on Web Dashboard
  ├─ Enters player names (e.g., "John" & "Jane")
  ├─ Clicks [START GAME]
  │
  ├─ Frontend calls POST /game/start
  │  └─ Backend creates game in MongoDB
  │     └─ Returns gameId (e.g., "game_abc123")
  │
  └─ Frontend calls POST /game/control with action="start"
     └─ Backend updates internal gameStatus = "RUNNING"
        └─ Sets gameId
        
Next Poll Cycle (0-2 seconds):
  ESP32 calls GET /game/status
    ├─ Server responds: {"status": "RUNNING", "gameId": "game_abc123"}
    └─ ESP32:
       ├─ Detects status changed to RUNNING
       ├─ Turns ON lasers
       ├─ Activates sensors
       ├─ Resets scores to 0
       ├─ Plays start beep (buzzer)
       └─ Starts reading sensor inputs
```

### 3. **Gameplay** (Players Shooting)
```
Player 1 Shoots Player 2 (Laser hits sensor)
  │
  ESP32 Player 1:
    ├─ Sensor detects laser (value > 2000)
    ├─ Confirms hit for 200ms (debounce)
    ├─ score += 1 per 100ms
    ├─ Buzzer beeps (100ms)
    ├─ Every 1 second, sends:
    │  POST /game/score
    │  Body: {gameId, player1Score, player2Score}
    │
    └─ Backend updates database
       └─ Frontend polls and updates live scoreboard
```

### 4. **Game End** (Winner or Manual Stop)

#### Option A: Automatic Win (Score reaches 100)
```
ESP32 Player 1 reaches score >= 100
  ├─ Victory buzzer (2000ms)
  ├─ Calls POST /game/end
  │  └─ Body: {gameId, player1Score, player2Score, winner: "Player 1"}
  │
  └─ gameState = FINISHED
     └─ Waits for next RUNNING status from server
```

#### Option B: Manual Stop (User clicks "Stop Game")
```
User clicks [STOP GAME] on dashboard
  │
  └─ Frontend calls POST /game/control with action="stop"
     └─ Backend sets gameStatus = "IDLE"
        └─ Clears gameId

Next Poll Cycle:
  ESP32 calls GET /game/status
    ├─ Server responds: {"status": "IDLE", "gameId": null}
    └─ ESP32:
       ├─ Detects status changed to IDLE
       ├─ Turns OFF lasers
       ├─ Deactivates sensors
       ├─ Plays stop beep (500ms)
       └─ gameState = IDLE (back to waiting)
```

## New Endpoints

### 1. GET `/game/status` - Check Current Game Status
**Called by**: ESP32 (every 2 seconds)  
**Response**:
```json
{
  "status": "RUNNING" | "IDLE",
  "gameId": "game_abc123" | null
}
```

**Usage in ESP32**:
```cpp
GET /game/status
Response → {"status":"RUNNING","gameId":"game_abc123"}
  ↓
ESP32: "Game is RUNNING! Start playing!"
```

### 2. POST `/game/control` - Admin Control
**Called by**: Frontend (admin dashboard)  
**Body**:
```json
{
  "action": "start" | "stop",
  "gameId": "game_abc123" (only needed for "start")
}
```

**Response**:
```json
{
  "ok": true,
  "message": "Game started",
  "gameStatus": {
    "status": "RUNNING",
    "gameId": "game_abc123"
  }
}
```

## Installation & Setup

### Backend
```bash
cd Web/backend
npm install
npm run dev
```

Backend will start on `http://localhost:3000`

### Frontend
```bash
cd Web/frontend
npm install
npm run dev
```

Frontend will start on `http://localhost:5173`

### ESP32
1. Open `Arduino/game_firmware_web_controlled.ino` in Arduino IDE
2. Update WiFi credentials:
   ```cpp
   const char* WIFI_SSID     = "ADAPT LNG";
   const char* WIFI_PASSWORD = "sigepo123";
   ```
3. Update backend URL (if not using Render):
   ```cpp
   const char* BACKEND_BASE_URL = "https://lazer-game-backend.onrender.com";
   ```
4. Upload to ESP32

## Deployment

### Deploy Backend to Render
1. Create new Web Service on Render
2. Connect GitHub repo
3. Set Environment Variable:
   ```
   DATABASE_URL=mongodb+srv://...
   ```
4. Deploy

### Deploy Frontend to Vercel
1. Create new project on Vercel
2. Connect GitHub repo
3. Set Environment Variable:
   ```
   VITE_API_BASE_URL=https://your-backend.onrender.com
   ```
4. Deploy

## Polling Explained

**Why polling instead of WebSockets?**
- Simpler for ESP32 (no persistent connection overhead)
- Works reliably on any network
- 2-second delay is acceptable for a game

**Polling Timeline**:
```
Time 0s:  ESP32 calls GET /game/status → "IDLE"
Time 2s:  User clicks "Start Game" → Server sets status = "RUNNING"
Time 2.5s: ESP32 calls GET /game/status → "RUNNING" ✓ Game starts
```

**Both players sync within 2 seconds** (one polling cycle)

## Troubleshooting

### ESP32 not starting game?
- Check Serial Monitor (115200 baud)
- Verify WiFi connection
- Ensure backend is running and `/game/status` endpoint works
- Test with: `curl https://your-backend.onrender.com/game/status`

### Scores not updating in frontend?
- Check that ESP32 is sending POST `/game/score` requests
- Verify `currentGameId` matches between ESP32 and frontend
- Backend logs should show incoming requests

### Frontend not controlling game?
- Ensure `/game/control` endpoint is working
- Check browser console for API errors
- Verify backend is responding

## Game Tuning

**Edit `game_firmware_web_controlled.ino`**:

```cpp
const int SENSOR_THRESHOLD = 2000;           // Laser hit sensitivity
const int WIN_SCORE = 100;                   // Points to win
const unsigned long POLL_INTERVAL_MS = 2000; // How often ESP32 checks server
const unsigned long HIT_MIN_DURATION_MS = 200; // Debounce time for sensor
```

## Pin Configuration

### ESP32 Player 1
- Sensors: GPIO 32, 33
- Buzzer: GPIO 25

### ESP32 Player 2
- Sensors: GPIO 34, 35
- Buzzer: GPIO 4

## Files Modified

- ✅ `Web/backend/src/app.ts` - Added `/game/status` and `/game/control` endpoints
- ✅ `Web/frontend/src/api/client.ts` - Added new API functions
- ✅ `Web/frontend/src/pages/StartPage.tsx` - Calls `/game/control` to start
- ✅ `Web/frontend/src/pages/GamePage.tsx` - Added "Stop Game" button
- ✅ `Arduino/game_firmware_web_controlled.ino` - New polling-based firmware

## Testing

### Test Endpoint Manually
```bash
# Check game status
curl https://lazer-game-backend.onrender.com/game/status

# Start a game (requires gameId)
curl -X POST https://lazer-game-backend.onrender.com/game/control \
  -H "Content-Type: application/json" \
  -d '{"action":"start","gameId":"game_abc123"}'

# Stop a game
curl -X POST https://lazer-game-backend.onrender.com/game/control \
  -H "Content-Type: application/json" \
  -d '{"action":"stop"}'
```

## Summary

The new system is:
1. **Hands-free**: No buttons on vests
2. **Web-controlled**: Start/stop from dashboard
3. **Auto-synced**: Both players start within 2 seconds
4. **Scalable**: Can add more vests easily
5. **Remote**: Works over the internet

Now you can control entire games from your laptop while players wear their vests! 🎮✨
