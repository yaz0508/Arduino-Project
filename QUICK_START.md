# Quick Start Guide - Web-Controlled Laser Game

## 🚀 In 5 Minutes

### Step 1: Upload New ESP32 Firmware
1. Open `Arduino/game_firmware_web_controlled.ino` in Arduino IDE
2. Update WiFi (lines 14-15):
   ```cpp
   const char* WIFI_SSID     = "ADAPT LNG";
   const char* WIFI_PASSWORD = "sigepo123";
   ```
3. Click **Upload**
4. Open Serial Monitor (115200 baud) and verify:
   ```
   ✓ WiFi Connected! IP: 10.209.27.x
   ✓ System Ready - Waiting for Web Command...
   ```

### Step 2: Deploy Backend (if not already done)
```bash
cd Web/backend
npm install
npm run dev
```

### Step 3: Start Frontend
```bash
cd Web/frontend
npm install
npm run dev
```
Visit: `http://localhost:5173`

### Step 4: Play!

1. **Open Dashboard** → `http://localhost:5173`
2. **Enter player names** (e.g., "Player 1" and "Player 2")
3. **Click [START GAME]**
   - Within 2 seconds, both vests will sync
   - Buzzers will beep (start sound)
   - Lasers will turn ON
   - Game is LIVE!
4. **Watch scores update in real-time** 📊
5. **Click [STOP GAME]** to end (or wait for someone to reach 100 points)

---

## 🔧 How It Works

```
Your Laptop
    ↓
[Click "Start Game"]
    ↓
Backend: "Game is RUNNING!"
    ↓
ESP32 Vest (polls every 2 sec)
    ↓
"Status is RUNNING? → GAME ON!"
    ↓
Laser & Sensors Activate
```

**Both players start at almost the same time** ✓

---

## 📱 Testing Without Hardware

### Option 1: Mock the ESP32
Use Postman or curl to simulate ESP32:

```bash
# ESP32 checks game status (you do this manually)
curl https://lazer-game-backend.onrender.com/game/status
# Returns: {"status":"IDLE","gameId":null}

# Then you start game from frontend, immediately:
curl https://lazer-game-backend.onrender.com/game/status
# Returns: {"status":"RUNNING","gameId":"game_abc123"}

# Simulate score update
curl -X POST https://lazer-game-backend.onrender.com/game/score \
  -H "Content-Type: application/json" \
  -d '{"gameId":"game_abc123","player1Score":50,"player2Score":30}'
```

### Option 2: Run ESP32 Simulator
If you have Python:
```python
import requests
import time

backend = "http://localhost:3000"

while True:
    status = requests.get(f"{backend}/game/status").json()
    print(f"Game Status: {status}")
    
    if status["status"] == "RUNNING":
        # Simulate scoring
        requests.post(f"{backend}/game/score", json={
            "gameId": status["gameId"],
            "player1Score": 25,
            "player2Score": 10
        })
    
    time.sleep(2)
```

---

## 🐛 Debugging

### Check ESP32 Serial Output
```
=== LASER GAME SYSTEM STARTING (Web-Controlled) ===
Connecting to WiFi: ADAPT LNG
✓ WiFi Connected! IP: 10.209.27.x
Backend URL: https://lazer-game-backend.onrender.com
✓ System Ready - Waiting for Web Command...

Button States - START: released | RESET: released
Button States - START: released | RESET: released

🎮 >>> SERVER STARTED GAME! <<<
Game Running - Sensors Active
Game ID: game_1768755249699_852291
✓ Game ID received: game_1768755249699_852291
=== GAME READY FOR SCORING ===
```

### Check Backend Logs
```
GET /game/status → 200
POST /game/control {"action":"start"} → 200
POST /game/score → 200
```

### Check Frontend Console
Open DevTools (F12) and check for API errors

---

## ⚙️ Configuration

### Change Polling Interval
In `game_firmware_web_controlled.ino`:
```cpp
const unsigned long POLL_INTERVAL_MS = 2000;  // Change to 1000 for 1 second
```

### Change Win Score
In `game_firmware_web_controlled.ino`:
```cpp
const int WIN_SCORE = 100;  // Change to 50 for shorter games
```

### Change Sensor Sensitivity
In `game_firmware_web_controlled.ino`:
```cpp
const int SENSOR_THRESHOLD = 2000;  // Lower = more sensitive, Higher = less sensitive
```

---

## 🎯 Common Issues

| Problem | Solution |
|---------|----------|
| ESP32 not connecting to WiFi | Check SSID/password, ensure 2.4GHz network |
| ESP32 doesn't receive "START" signal | Check backend is running, test `/game/status` endpoint |
| Scores not showing in frontend | Verify gameId matches, check `/game/score` endpoint |
| Game won't stop | Manually call `POST /game/control {"action":"stop"}` via curl |
| Buzzer not beeping | Check buzzer pin connection, verify pin configuration |

---

## 🎮 Game Rules (Default)

- **Win Score**: 100 points
- **Points per hit**: 1 point per 100ms while laser hits sensor
- **Hit debounce**: Must hold laser for 200ms minimum
- **Cooldown**: 100ms between consecutive hits
- **Polling frequency**: Every 2 seconds (both vests sync)

---

## 📊 Monitoring

### Check All Active Games
```bash
curl https://lazer-game-backend.onrender.com/matches
```

### Check Specific Game
```bash
curl https://lazer-game-backend.onrender.com/game/GAME_ID_HERE
```

---

## ✅ Checklist Before Playing

- [ ] Backend running (npm run dev in Web/backend)
- [ ] Frontend running (npm run dev in Web/frontend)
- [ ] ESP32 uploaded with latest firmware
- [ ] ESP32 shows "✓ System Ready" in Serial Monitor
- [ ] WiFi connected
- [ ] Both vests in range
- [ ] Batteries charged
- [ ] Laser aligned with sensors

---

## 🎬 Demo Scenario

1. **Power on both vests** → Serial shows "Waiting for Web Command..."
2. **Open dashboard** → `http://localhost:5173`
3. **Enter:** "Alice" vs "Bob" → Click [START GAME]
4. **Wait 2 seconds** → Both vests start beeping, lasers ON
5. **Players shoot each other** → Scores update live on dashboard
6. **First to 100 wins** → Vest plays victory sound, game ends
7. **Click [STOP GAME]** → Both vests turn off, reset
8. **View match history** → See game recorded in database

---

## 📞 Support

If something doesn't work:
1. Check Serial Monitor output
2. Test endpoints manually with curl
3. Check browser console for API errors
4. Verify WiFi connectivity
5. Review backend logs

Good luck! 🚀
