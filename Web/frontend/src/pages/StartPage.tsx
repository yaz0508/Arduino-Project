import React, { useState } from "react";
import { useNavigate } from "react-router-dom";
import { startGame, controlGame } from "../api/client";
import { useGameStore } from "../store/gameStore";

const StartPage: React.FC = () => {
  const [player1, setPlayer1] = useState("");
  const [player2, setPlayer2] = useState("");
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [gameStarted, setGameStarted] = useState(false);

  const navigate = useNavigate();
  const setGame = useGameStore((s) => s.setGame);

  async function handleSubmit(e: React.FormEvent) {
    e.preventDefault();
    setError(null);
    if (!player1.trim() || !player2.trim()) {
      setError("Please enter both player names.");
      return;
    }

    try {
      setLoading(true);
      
      // Step 1: Create game in database
      const res = await startGame(player1.trim(), player2.trim());
      setGame(res.gameId, player1.trim(), player2.trim());
      
      // Step 2: Tell backend to start the game (broadcast to ESP32s)
      await controlGame("start", res.gameId);
      
      setGameStarted(true);
      setTimeout(() => {
        navigate("/game");
      }, 1000);
    } catch (err) {
      console.error(err);
      setError("Failed to start game. Please try again.");
    } finally {
      setLoading(false);
    }
  }

  return (
    <div className="space-y-6">
      <h1 className="text-2xl font-bold text-sky-300">Start New Game</h1>
      
      {gameStarted && (
        <div className="rounded-lg bg-emerald-600/20 border border-emerald-500/50 px-4 py-3">
          <p className="text-sm font-semibold text-emerald-300">
            ✓ Game Started! Vests are syncing...
          </p>
        </div>
      )}
      
      <form
        onSubmit={handleSubmit}
        className="space-y-4 bg-slate-900/80 border border-slate-800 rounded-xl p-6 shadow-xl shadow-black/40"
      >
        <div className="grid gap-4 sm:grid-cols-2">
          <div>
            <label className="block text-sm text-slate-300 mb-1">Player 1 Name</label>
            <input
              className="w-full rounded-lg border border-slate-700 bg-slate-950 px-3 py-2 text-sm focus:outline-none focus:border-sky-400 focus:ring-1 focus:ring-sky-400"
              value={player1}
              onChange={(e) => setPlayer1(e.target.value)}
              disabled={loading}
            />
          </div>
          <div>
            <label className="block text-sm text-slate-300 mb-1">Player 2 Name</label>
            <input
              className="w-full rounded-lg border border-slate-700 bg-slate-950 px-3 py-2 text-sm focus:outline-none focus:border-sky-400 focus:ring-1 focus:ring-sky-400"
              value={player2}
              onChange={(e) => setPlayer2(e.target.value)}
              disabled={loading}
            />
          </div>
        </div>

        {error && <p className="text-sm text-red-400">{error}</p>}

        <button
          type="submit"
          disabled={loading}
          className="inline-flex items-center justify-center rounded-lg bg-sky-500 px-4 py-2 text-sm font-semibold text-slate-950 hover:bg-sky-400 disabled:opacity-60 disabled:cursor-not-allowed transition"
        >
          {loading ? "Starting Game..." : "Start Game"}
        </button>

        <p className="text-xs text-slate-400">
          💡 Both vests will automatically sync within 2 seconds. Get ready!
        </p>
      </form>
    </div>
  );
};

export default StartPage;

