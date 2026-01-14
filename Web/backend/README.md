# Laser Game Backend

Express.js backend with Prisma and MongoDB for the Laser Game project.

## 🚀 Quick Start (Local Development)

1. **Install dependencies:**
   ```bash
   npm install
   ```

2. **Set up environment variables:**
   Create a `.env` file in this directory:
   ```env
   DATABASE_URL="mongodb+srv://username:password@cluster0.xxxxx.mongodb.net/laser-game?retryWrites=true&w=majority"
   NODE_ENV="development"
   ```

3. **Generate Prisma Client:**
   ```bash
   npx prisma generate
   ```

4. **Push database schema:**
   ```bash
   npx prisma db push
   ```

5. **Start development server:**
   ```bash
   npm run dev
   ```

The server will run on `http://localhost:3000`

## 📦 Deployment to Render

### Option 1: Using render.yaml (Recommended)

1. Push your code to GitHub
2. In Render dashboard, click **"New +"** → **"Blueprint"**
3. Connect your repository
4. Render will automatically detect `render.yaml` and configure the service

### Option 2: Manual Configuration

1. Go to [Render Dashboard](https://dashboard.render.com/)
2. Click **"New +"** → **"Web Service"**
3. Connect your GitHub repository
4. Configure:
   - **Root Directory:** `Web/backend`
   - **Build Command:** `npm install && npm run build`
   - **Start Command:** `npm start`
5. Add environment variables:
   - `DATABASE_URL` - Your MongoDB connection string
   - `NODE_ENV` - `production`
   - `CORS_ORIGINS` (optional) - Comma-separated list of allowed origins

## 🔧 Environment Variables

| Variable | Description | Required |
|----------|-------------|----------|
| `DATABASE_URL` | MongoDB connection string | Yes |
| `NODE_ENV` | Environment (development/production) | No |
| `PORT` | Server port (auto-set by Render) | No |
| `CORS_ORIGINS` | Comma-separated allowed origins | No |

## 📡 API Endpoints

- `POST /game/start` - Start a new game
- `POST /game/score` - Update game scores
- `POST /game/end` - End a game
- `GET /game/:gameId` - Get game status
- `GET /matches` - Get match history
- `GET /health` - Health check

## 🏥 Health Check

The `/health` endpoint is used by Render to verify the service is running.

## 🔒 CORS Configuration

By default, the backend allows:
- `http://localhost:5173` (local frontend)
- All `*.vercel.app` domains (Vercel preview deployments)

To customize, set `CORS_ORIGINS` environment variable with comma-separated origins.
