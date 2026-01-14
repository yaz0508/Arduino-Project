# Laser Game Frontend

React + TypeScript + Vite frontend for the Laser Game project.

## 🚀 Quick Start (Local Development)

1. **Install dependencies:**
   ```bash
   npm install
   ```

2. **Set up environment variables:**
   Create a `.env` file in this directory:
   ```env
   VITE_API_BASE_URL="http://localhost:3000"
   ```

3. **Start development server:**
   ```bash
   npm run dev
   ```

The app will run on `http://localhost:5173`

## 📦 Deployment to Vercel

### Method 1: Vercel Dashboard (Recommended)

1. Push your code to GitHub
2. Go to [Vercel Dashboard](https://vercel.com/dashboard)
3. Click **"Add New..."** → **"Project"**
4. Import your GitHub repository
5. Configure:
   - **Framework Preset:** `Vite`
   - **Root Directory:** `Web/frontend` ⚠️ **IMPORTANT!**
   - **Build Command:** `npm run build` (auto-detected)
   - **Output Directory:** `dist` (auto-detected)
6. Add environment variable:
   - `VITE_API_BASE_URL` - Your Render backend URL (e.g., `https://your-backend.onrender.com`)
7. Click **"Deploy"**

### Method 2: Vercel CLI

```bash
npm i -g vercel
cd Web/frontend
vercel
```

Follow the prompts and set environment variables when asked.

## 🔧 Environment Variables

| Variable | Description | Required |
|----------|-------------|----------|
| `VITE_API_BASE_URL` | Backend API base URL | Yes |

**Note:** Vite requires the `VITE_` prefix for environment variables to be exposed to the client.

## 🏗️ Build

```bash
npm run build
```

Output will be in the `dist/` directory.

## 📱 Features

- **Start Page:** Enter player names and start a game
- **Game Page:** Live scoreboard with polling
- **History Page:** View past matches

## 🔄 API Integration

The frontend uses Axios to communicate with the backend API. All API calls are centralized in `src/api/client.ts`.

## 🎨 Styling

- **Tailwind CSS** for styling
- Fully responsive design
- Dark theme optimized
