# Firmware Setup Guide - Two Player Configuration

## ✅ Files Created

You now have TWO separate firmware files, one for each player:

### **File 1: game_firmware_player1.ino**
- For **PLAYER 1 VEST** only
- Sensors: GPIO 32, 33
- Buzzer: GPIO 25
- Sends: `player1Score` to backend

### **File 2: game_firmware_player2.ino**
- For **PLAYER 2 VEST** only
- Sensors: GPIO 34, 35
- Buzzer: GPIO 4
- Sends: `player2Score` to backend

---

## 📋 Upload Instructions

### **For VEST 1 (Player 1):**

1. **Open Arduino IDE**
2. **File → Open → game_firmware_player1.ino**
3. **Select Board:** ESP32 Dev Module
4. **Select Port:** COM[X] (your first ESP32)
5. **Click Upload**
6. **Open Serial Monitor (115200 baud)**
7. Should show:
   ```
   === LASER GAME SYSTEM STARTING (Web-Controlled - PLAYER 1) ===
   ✓ WiFi Connected! IP: 10.209.27.x
   ✓ System Ready - Waiting for Web Command...
   ```

### **For VEST 2 (Player 2):**

1. **Open Arduino IDE** (or new window)
2. **File → Open → game_firmware_player2.ino**
3. **Select Board:** ESP32 Dev Module
4. **Select Port:** COM[Y] (your second ESP32 - different COM port!)
5. **Click Upload**
6. **Open Serial Monitor (115200 baud)**
7. Should show:
   ```
   === LASER GAME SYSTEM STARTING (Web-Controlled - PLAYER 2) ===
   ✓ WiFi Connected! IP: 10.209.27.y
   ✓ System Ready - Waiting for Web Command...
   ```

---

## 🎯 Key Differences Between Firmware

| Feature | Player 1 | Player 2 |
|---------|----------|----------|
| **Sensor 1** | GPIO 32 | GPIO 34 |
| **Sensor 2** | GPIO 33 | GPIO 35 |
| **Buzzer Pin** | GPIO 25 | GPIO 4 |
| **Startup Message** | PLAYER 1 | PLAYER 2 |
| **Score Report** | `player1Score` | `player2Score` |
| **Victory Message** | "Player 1 WINS!" | "Player 2 WINS!" |

---

## ⚙️ Pin Wiring Reference

### **Vest 1 (Player 1)**
```
Sensor 1 → GPIO 32
Sensor 2 → GPIO 33
Buzzer   → GPIO 25
```

### **Vest 2 (Player 2)**
```
Sensor 1 → GPIO 34
Sensor 2 → GPIO 35
Buzzer   → GPIO 4
```

---

## 🔍 How to Verify Correct Upload

After uploading each firmware, check Serial Monitor:

**Player 1 correct:**
```
=== LASER GAME SYSTEM STARTING (Web-Controlled - PLAYER 1) ===
Connecting to WiFi: ADAPT LNG
. . . .
✓ WiFi Connected! IP: 10.209.27.247
Backend URL: https://lazer-game-backend.onrender.com
✓ System Ready - Waiting for Web Command...
```

**Player 2 correct:**
```
=== LASER GAME SYSTEM STARTING (Web-Controlled - PLAYER 2) ===
Connecting to WiFi: ADAPT LNG
. . . .
✓ WiFi Connected! IP: 10.209.27.248
Backend URL: https://lazer-game-backend.onrender.com
✓ System Ready - Waiting for Web Command...
```

---

## 🚀 Testing Both Vests

### Quick Test Procedure

1. **Power on both vests** (they'll boot and show startup messages)
2. **Open dashboard:** http://localhost:5173
3. **Enter player names:** "Vest1" vs "Vest2"
4. **Click [START GAME]**
5. **Watch both Serial Monitors:**
   - Both should show: `🎮 >>> SERVER STARTED GAME! <<<`
   - Both should show: `Game Running - Sensors Active`
6. **Point laser from Vest 1 at Vest 2:**
   - Vest 2 Serial shows: `🎯 HIT DETECTED`
   - Vest 2 Serial shows: `📊 My Score: 1`
   - Dashboard updates: Player 1: 0 | Player 2: 1

---

## ❌ Common Issues & Fixes

| Problem | Cause | Fix |
|---------|-------|-----|
| Uploading wrong file | Mixed up the files | Check filename before uploading |
| Both vests sending P1 scores | Uploaded P1 firmware to both | Upload P2 firmware to second vest |
| Wrong GPIO pins trigger | Using generic filename | Verify filename matches (player1 or player2) |
| Buzzer on wrong pin | Wrong firmware uploaded | Check Serial output shows correct player number |
| Sensors not working | Wrong pins configured | Verify `game_firmware_player2.ino` uses GPIO 34,35 |

---

## 📝 File Organization

```
Arduino/
├── game_firmware_player1.ino    ← VEST 1 firmware
├── game_firmware_player2.ino    ← VEST 2 firmware
└── (old files archived)
```

---

## ✅ Checklist Before Playing

- [ ] Upload game_firmware_player1.ino to Vest 1
- [ ] Upload game_firmware_player2.ino to Vest 2
- [ ] Verify Serial output shows correct player number (1 or 2)
- [ ] Both vests show WiFi connected
- [ ] Both vests show "Waiting for Web Command..."
- [ ] Backend is running
- [ ] Frontend is running
- [ ] Sensors are physically connected (GPIO 32,33 for P1 and 34,35 for P2)
- [ ] Buzzers are connected (GPIO 25 for P1 and 4 for P2)

---

## 🎮 You're Ready!

Once both vests are running the correct firmware:
1. Each vest only monitors its own sensors
2. Each vest only controls its own buzzer
3. Each vest sends only its own score
4. No interference between vests
5. Perfect synchronization via polling

**System is now ready for testing!** 🚀

