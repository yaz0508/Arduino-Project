# Deployment Tutorial: Laser Game Project

This tutorial will guide you through deploying the **Backend** to **Render** and the **Frontend** to **Vercel**.

---

## 📋 Prerequisites

- GitHub account with the project repository
- MongoDB Atlas account (free tier works)
- Render account (free tier available)
- Vercel account (free tier available)
- Git installed locally

---

## 🗄️ Part 1: MongoDB Atlas Setup

### Step 1: Create MongoDB Cluster

1. Go to [MongoDB Atlas](https://www.mongodb.com/cloud/atlas)
2. Sign up or log in
3. Click **"Create"** → **"Create a Deployment"**
4. Choose **FREE** (M0) tier
5. Select a cloud provider and region (choose closest to you)
6. Click **"Create"**

### Step 2: Configure Network Access

1. In MongoDB Atlas dashboard, go to **"Network Access"** (left sidebar)
2. Click **"Add IP Address"**
3. Click **"Allow Access from Anywhere"** (for development) or add specific IPs
4. Click **"Confirm"**

### Step 3: Create Database User

1. Go to **"Database Access"** (left sidebar)
2. Click **"Add New Database User"**
3. Choose **"Password"** authentication
4. Enter username (e.g., `pao`) and generate a secure password
5. Set privileges to **"Atlas Admin"** (or custom with read/write access)
6. Click **"Add User"**
7. **⚠️ IMPORTANT:** Save the username and password - you'll need them!

### Step 4: Get Connection String

1. Go to **"Database"** → **"Connect"**
2. Choose **"Connect your application"**
3. Copy the connection string
4. Replace `<password>` with your database user password
5. Replace `<dbname>` with `laser-game` (or your preferred database name)
6. **Example format:**
   ```
   mongodb+srv://pao:YOUR_PASSWORD@cluster0.xxxxx.mongodb.net/laser-game?retryWrites=true&w=majority
   ```
7. **Save this connection string** - you'll use it in Render!

---

## 🚀 Part 2: Backend Deployment on Render

### Step 1: Prepare Backend for Deployment

1. **Ensure your backend code is pushed to GitHub:**
   ```bash
   cd "Z:\Arduino Project\Web\backend"
   git add .
   git commit -m "Prepare for Render deployment"
   git push origin main
   ```

2. **Verify your `package.json` has these scripts:**
   ```json
   {
     "scripts": {
       "dev": "ts-node-dev --respawn --transpile-only src/index.ts",
       "build": "tsc",
       "start": "node dist/index.js",
       "prisma:generate": "prisma generate",
       "prisma:migrate": "prisma db push"
     }
   }
   ```

### Step 2: Create Render Web Service

1. Go to [Render Dashboard](https://dashboard.render.com/)
2. Click **"New +"** → **"Web Service"**
3. Connect your GitHub account if not already connected
4. Select your repository: `yaz0508/Arduino-Project`
5. Configure the service:

   **Basic Settings:**
   - **Name:** `laser-game-backend` (or your preferred name)
   - **Region:** Choose closest to you (e.g., `Oregon (US West)`)
   - **Branch:** `main`
   - **Root Directory:** `Web/backend`
   - **Runtime:** `Node`
   - **Build Command:** 
     ```
     npm install && npm run prisma:generate && npm run build
     ```
   - **Start Command:**
     ```
     npm start
     ```

### Step 3: Configure Environment Variables

In the Render dashboard, go to **"Environment"** tab and add:

| Key | Value |
|-----|-------|
| `DATABASE_URL` | Your MongoDB connection string from Part 1, Step 4 |
| `NODE_ENV` | `production` |
| `PORT` | Leave empty (Render sets this automatically) |

**Example:**
```
DATABASE_URL=mongodb+srv://pao:YOUR_PASSWORD@cluster0.xxxxx.mongodb.net/laser-game?retryWrites=true&w=majority
NODE_ENV=production
```

### Step 4: Deploy Backend

1. Click **"Create Web Service"**
2. Render will:
   - Clone your repository
   - Install dependencies
   - Generate Prisma Client
   - Build TypeScript
   - Start the server
3. Wait for deployment to complete (usually 2-5 minutes)
4. Once deployed, you'll see a URL like: `https://laser-game-backend.onrender.com`
5. **⚠️ Save this URL** - you'll need it for the frontend!

### Step 5: Test Backend Deployment

1. Open your browser and go to: `https://your-backend-url.onrender.com/matches`
2. You should see `[]` (empty array) - this means the backend is working!
3. If you get an error, check the Render logs:
   - Go to your service → **"Logs"** tab
   - Look for error messages

### Step 6: Update CORS Settings (Important!)

1. In your local `Web/backend/src/app.ts`, update CORS to include your Vercel URL:
   ```typescript
   app.use(
     cors({
       origin: [
         "http://localhost:5173",
         "https://your-frontend-name.vercel.app"  // Add your Vercel URL here
       ],
       methods: ["GET", "POST"],
       credentials: false
     })
   );
   ```
2. Commit and push:
   ```bash
   git add Web/backend/src/app.ts
   git commit -m "Update CORS for Vercel frontend"
   git push origin main
   ```
3. Render will automatically redeploy

---

## 🎨 Part 3: Frontend Deployment on Vercel

### Step 1: Prepare Frontend for Deployment

1. **Ensure your frontend code is pushed to GitHub:**
   ```bash
   cd "Z:\Arduino Project\Web\frontend"
   git add .
   git commit -m "Prepare for Vercel deployment"
   git push origin main
   ```

2. **Verify `vercel.json` exists in `Web/frontend/`** (should already be there)

### Step 2: Create Vercel Project

1. Go to [Vercel Dashboard](https://vercel.com/dashboard)
2. Click **"Add New..."** → **"Project"**
3. Import your GitHub repository: `yaz0508/Arduino-Project`
4. Configure the project:

   **Project Settings:**
   - **Framework Preset:** `Vite`
   - **Root Directory:** `Web/frontend` ⚠️ **IMPORTANT!**
   - **Build Command:** `npm run build` (auto-detected)
   - **Output Directory:** `dist` (auto-detected)
   - **Install Command:** `npm install` (auto-detected)

### Step 3: Configure Environment Variables

In Vercel project settings, go to **"Environment Variables"** and add:

| Key | Value |
|-----|-------|
| `VITE_API_BASE_URL` | Your Render backend URL (from Part 2, Step 4) |

**Example:**
```
VITE_API_BASE_URL=https://lazer-game-backend.onrender.com
```

**Note:** Your actual backend URL is `https://lazer-game-backend.onrender.com` and frontend is `https://lazer-game.vercel.app/`

**⚠️ Important:** 
- Make sure to add this for **Production**, **Preview**, and **Development** environments
- Click **"Save"** after adding

### Step 4: Deploy Frontend

1. Click **"Deploy"**
2. Vercel will:
   - Install dependencies
   - Build the React app
   - Deploy to production
3. Wait for deployment (usually 1-2 minutes)
4. Once deployed, you'll get a URL like: `https://laser-game-frontend.vercel.app`
5. **🎉 Your frontend is live!**

### Step 5: Test Frontend

1. Open your Vercel URL in a browser
2. You should see the Laser Duel app
3. Try starting a game to test the connection to the backend

---

## 🔧 Part 4: Update ESP32 Firmware

Now that your backend is deployed, update the ESP32 firmware to use the Render URL:

### Step 1: Update Backend URL in Firmware

1. Open `Arduino/game_firmware.ino`
2. Find this line:
   ```cpp
   const char* BACKEND_BASE_URL = "http://YOUR_BACKEND_HOST";
   ```
3. Replace with your Render backend URL:
   ```cpp
   const char* BACKEND_BASE_URL = "https://laser-game-backend.onrender.com";
   ```
   **⚠️ Note:** Use `https://` not `http://` for Render!

### Step 2: Update WiFi Credentials

Make sure your WiFi SSID and password are correct:
```cpp
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
```

### Step 3: Upload to ESP32

1. Connect your ESP32 to your computer
2. Select the correct board and port in Arduino IDE
3. Click **Upload**
4. Open Serial Monitor (115200 baud) to see connection status

---

## ✅ Part 5: Verification Checklist

### Backend (Render)
- [ ] Backend URL is accessible (e.g., `https://your-backend.onrender.com/matches`)
- [ ] Returns empty array `[]` or match data
- [ ] CORS is configured for Vercel frontend URL
- [ ] Environment variables are set correctly
- [ ] Prisma schema is pushed to MongoDB

### Frontend (Vercel)
- [ ] Frontend URL loads correctly
- [ ] Can navigate between pages (Start, Game, History)
- [ ] Environment variable `VITE_API_BASE_URL` is set
- [ ] Root directory is set to `Web/frontend`

### ESP32
- [ ] WiFi credentials are correct
- [ ] Backend URL points to Render (with `https://`)
- [ ] Can connect to WiFi
- [ ] Can send requests to backend (check Serial Monitor)

---

## 🐛 Troubleshooting

### Backend Issues

**Problem:** Render deployment fails
- **Solution:** Check logs in Render dashboard → Logs tab
- Common issues:
  - Missing environment variables
  - Build command errors
  - TypeScript compilation errors

**Problem:** Backend returns 500 errors
- **Solution:** 
  - Check MongoDB connection string is correct
  - Verify MongoDB network access allows Render IPs
  - Check Render logs for Prisma errors

**Problem:** CORS errors in browser console
- **Solution:** 
  - Update CORS in `Web/backend/src/app.ts` to include your Vercel URL
  - Redeploy backend after changes

### Frontend Issues

**Problem:** Vercel build fails
- **Solution:**
  - Verify Root Directory is set to `Web/frontend`
  - Check that `package.json` exists in `Web/frontend/`
  - Review build logs in Vercel dashboard

**Problem:** Frontend can't connect to backend
- **Solution:**
  - Verify `VITE_API_BASE_URL` environment variable is set
  - Check that backend URL is correct (include `https://`)
  - Ensure backend CORS allows your Vercel domain

**Problem:** Environment variables not working
- **Solution:**
  - Vite requires `VITE_` prefix for environment variables
  - Redeploy after adding/changing environment variables
  - Clear browser cache

### ESP32 Issues

**Problem:** ESP32 can't connect to WiFi
- **Solution:**
  - Verify SSID and password are correct
  - Check Serial Monitor for error messages
  - Ensure WiFi is 2.4GHz (ESP32 doesn't support 5GHz)

**Problem:** ESP32 can't reach backend
- **Solution:**
  - Verify backend URL uses `https://` (not `http://`)
  - Check that Render backend is accessible from your network
  - Review Serial Monitor for HTTP error codes

---

## 📝 Quick Reference

### Render Backend URLs
- **Dashboard:** https://dashboard.render.com/
- **Your Service:** Check your Render dashboard

### Vercel Frontend URLs
- **Dashboard:** https://vercel.com/dashboard
- **Your Project:** Check your Vercel dashboard

### MongoDB Atlas
- **Dashboard:** https://cloud.mongodb.com/
- **Connection String Format:**
  ```
  mongodb+srv://USERNAME:PASSWORD@cluster0.xxxxx.mongodb.net/DATABASE_NAME?retryWrites=true&w=majority
  ```

---

## 🎉 Success!

Once everything is deployed:
1. ✅ Backend running on Render
2. ✅ Frontend running on Vercel
3. ✅ ESP32 configured to use Render backend
4. ✅ MongoDB storing match data

Your Laser Game is now live and ready to play! 🎮

---

## 📞 Need Help?

- **Render Docs:** https://render.com/docs
- **Vercel Docs:** https://vercel.com/docs
- **MongoDB Atlas Docs:** https://docs.atlas.mongodb.com/
- **Prisma Docs:** https://www.prisma.io/docs/

---

**Last Updated:** 2024
**Project:** Laser Duel Game - ESP32 + Web App
