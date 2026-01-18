import express, { Request, Response } from "express";
import cors from "cors";
import helmet from "helmet";
import morgan from "morgan";
import { prisma } from "./prisma";
import { GameStartBody, GameScoreBody, GameEndBody } from "./types";

const app = express();

// Middleware
app.use(express.json());

// CORS configuration - allow all Vercel preview deployments
const allowedOrigins = process.env.CORS_ORIGINS
  ? process.env.CORS_ORIGINS.split(",")
  : [
      "http://localhost:5173",
      "http://localhost:3000",
      /^https:\/\/.*\.vercel\.app$/, // Allow all Vercel preview deployments
    ];

app.use(
  cors({
    origin: (
      origin: string | undefined,
      callback: (err: Error | null, allow?: boolean) => void
    ) => {
      // Allow requests with no origin (like mobile apps or curl requests)
      if (!origin) return callback(null, true);
      
      // Check if origin matches any allowed origin
      const isAllowed = allowedOrigins.some((allowed: string | RegExp) => {
        if (typeof allowed === "string") {
          return origin === allowed;
        }
        if (allowed instanceof RegExp) {
          return allowed.test(origin);
        }
        return false;
      });
      
      if (isAllowed) {
        callback(null, true);
      } else {
        callback(new Error("Not allowed by CORS"));
      }
    },
    methods: ["GET", "POST", "OPTIONS"],
    credentials: false,
  })
);

// Helmet configuration - adjust for API needs
app.use(
  helmet({
    crossOriginResourcePolicy: { policy: "cross-origin" },
  })
);

app.use(morgan(process.env.NODE_ENV === "production" ? "combined" : "dev"));

// POST /game/start
app.post("/game/start", async (req: Request, res: Response) => {
  const body = req.body as Partial<GameStartBody>;
  if (!body.player1Name || !body.player2Name) {
    return res.status(400).json({ error: "player1Name and player2Name are required" });
  }

  try {
    const gameId = `game_${Date.now()}_${Math.floor(Math.random() * 1e6)}`;

    const match = await prisma.match.create({
      data: {
        gameId,
        player1Name: body.player1Name,
        player2Name: body.player2Name
      }
    });

    return res.status(201).json({
      gameId: match.gameId,
      matchId: match.id
    });
  } catch (err) {
    console.error(err);
    return res.status(500).json({ error: "Internal server error" });
  }
});

// POST /game/score
app.post("/game/score", async (req: Request, res: Response) => {
  const body = req.body as Partial<GameScoreBody>;
  if (!body.gameId) {
    return res.status(400).json({ error: "gameId is required" });
  }

  try {
    // Build update object with only provided fields (don't overwrite with undefined or 0)
    const updateData: any = {};
    if (body.player1Score !== undefined) updateData.player1Score = body.player1Score;
    if (body.player2Score !== undefined) updateData.player2Score = body.player2Score;

    const match = await prisma.match.update({
      where: { gameId: body.gameId },
      data: updateData
    });

    return res.status(200).json({ ok: true, match });
  } catch (err) {
    console.error(err);
    return res.status(404).json({ error: "Match not found" });
  }
});

// POST /game/end
app.post("/game/end", async (req: Request, res: Response) => {
  const body = req.body as Partial<GameEndBody>;
  if (!body.gameId || !body.winner) {
    return res.status(400).json({ error: "gameId and winner are required" });
  }

  try {
    const match = await prisma.match.update({
      where: { gameId: body.gameId },
      data: {
        player1Score: body.player1Score ?? undefined,
        player2Score: body.player2Score ?? undefined,
        winner: body.winner,
        status: "finished",
        endedAt: new Date()
      }
    });

    // Reset global game status so ESP32s stop when they poll
    gameStatus = { status: "IDLE", gameId: null };
    console.log(`Game ${body.gameId} finished with winner: ${body.winner}`);

    return res.status(200).json({ ok: true });
  } catch (err) {
    console.error(err);
    return res.status(404).json({ error: "Match not found" });
  }
});

// GET /matches - list all finished matches (history)
app.get("/matches", async (_req: Request, res: Response) => {
  try {
    const matches = await prisma.match.findMany({
      where: { status: "finished" },
      orderBy: { startedAt: "desc" }
    });
    return res.json(matches);
  } catch (err) {
    console.error(err);
    return res.status(500).json({ error: "Internal server error" });
  }
});

// GET /game/status - Returns current game status (RUNNING or IDLE)
// IMPORTANT: This must come BEFORE /game/:gameId to avoid route matching issues
// ESP32s poll this endpoint to know when to start/stop
let gameStatus: { status: "IDLE" | "RUNNING"; gameId: string | null } = { status: "IDLE", gameId: null };

app.get("/game/status", (_req: Request, res: Response) => {
  res.status(200).json(gameStatus);
});

// GET /game/:gameId - live status for a specific game (for Game Page polling)
app.get("/game/:gameId", async (req: Request, res: Response) => {
  const { gameId } = req.params;
  try {
    const match = await prisma.match.findUnique({
      where: { gameId }
    });
    if (!match) {
      return res.status(404).json({ error: "Match not found" });
    }
    return res.json(match);
  } catch (err) {
    console.error(err);
    return res.status(500).json({ error: "Internal server error" });
  }
});

// POST /game/control - Admin endpoint to start/stop game
app.post("/game/control", async (req: Request, res: Response) => {
  const { action, gameId } = req.body;
  
  if (action === "start" && gameId) {
    gameStatus = { status: "RUNNING", gameId };
    return res.status(200).json({ ok: true, message: "Game started", gameStatus });
  } else if (action === "stop") {
    gameStatus = { status: "IDLE", gameId: null };
    
    // Save game to finished status if gameId exists
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

// Health check endpoint for Render
app.get("/health", (_req: Request, res: Response) => {
  res.status(200).json({ status: "ok", timestamp: new Date().toISOString() });
});

export default app;

