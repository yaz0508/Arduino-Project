import express from "express";
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
    origin: (origin, callback) => {
      // Allow requests with no origin (like mobile apps or curl requests)
      if (!origin) return callback(null, true);
      
      // Check if origin matches any allowed origin
      const isAllowed = allowedOrigins.some((allowed) => {
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
app.post("/game/start", async (req, res) => {
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
app.post("/game/score", async (req, res) => {
  const body = req.body as Partial<GameScoreBody>;
  if (!body.gameId) {
    return res.status(400).json({ error: "gameId is required" });
  }

  try {
    const match = await prisma.match.update({
      where: { gameId: body.gameId },
      data: {
        player1Score: body.player1Score ?? undefined,
        player2Score: body.player2Score ?? undefined
      }
    });

    return res.status(200).json({ ok: true, match });
  } catch (err) {
    console.error(err);
    return res.status(404).json({ error: "Match not found" });
  }
});

// POST /game/end
app.post("/game/end", async (req, res) => {
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

    return res.status(200).json({ ok: true, match });
  } catch (err) {
    console.error(err);
    return res.status(404).json({ error: "Match not found" });
  }
});

// GET /matches - list all finished matches (history)
app.get("/matches", async (_req, res) => {
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

// GET /game/:gameId - live status for a specific game (for Game Page polling)
app.get("/game/:gameId", async (req, res) => {
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

// Health check endpoint for Render
app.get("/health", (_req, res) => {
  res.status(200).json({ status: "ok", timestamp: new Date().toISOString() });
});

export default app;

