import React, { useEffect, useState } from "react";
import { useGameStore } from "../store/gameStore";
import { getGame, controlGame, Match } from "../api/client";
import { useNavigate } from "react-router-dom";

const POLL_INTERVAL_MS = 1000;

const GamePage: React.FC = () => {
  const { gameId, player1Name, player2Name } = useGameStore();
  const navigate = useNavigate();
  const [match, setMatch] = useState<Match | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [locked, setLocked] = useState(false);
  const [stopping, setStopping] = useState(false);

  useEffect(() => {
    if (!gameId) return;

    let cancelled = false;
    let timer: number | undefined;

    async function poll() {
      try {
        const data = await getGame(gameId);
        if (cancelled) return;
        setMatch(data);
        setError(null);

        if (data.status === "finished") {
          setLocked(true);
          return;
        }

        timer = window.setTimeout(poll, POLL_INTERVAL_MS);
      } catch (err) {
        console.error(err);
        if (!cancelled) {
          setError("Failed to fetch game status.");
          timer = window.setTimeout(poll, POLL_INTERVAL_MS * 2);
        }
      }
    }

    poll();

    return () => {
      cancelled = true;
      if (timer) window.clearTimeout(timer);
    };
  }, [gameId]);

  const handleStopGame = async () => {
    try {
      setStopping(true);
      await controlGame("stop");
      setLocked(true);
      setTimeout(() => {
        navigate("/");
      }, 1500);
    } catch (err) {
      console.error(err);
      setError("Failed to stop game.");
      setStopping(false);
    }
  };

  if (!gameId) {
    return <p className="text-sm text-slate-300">No active game selected.</p>;
  }

  const p1Score = match?.player1Score ?? 0;
  const p2Score = match?.player2Score ?? 0;
  const finished = match?.status === "finished";
  const winner = match?.winner ?? null;

  return (
    <div className="space-y-6">
      <h1 className="text-2xl font-bold text-sky-300">Live Scoreboard</h1>

      {error && <p className="text-sm text-red-400">{error}</p>}

      <div className="bg-slate-900/80 border border-slate-800 rounded-xl p-6 shadow-xl shadow-black/40">
        <div className="grid grid-cols-2 gap-4 text-center">
          <div className="space-y-2">
            <div className="text-xs uppercase tracking-wide text-slate-400">Player 1</div>
            <div className="text-lg font-semibold">{player1Name || match?.player1Name}</div>
            <div className="text-4xl font-black text-sky-300">{p1Score}</div>
          </div>
          <div className="space-y-2">
            <div className="text-xs uppercase tracking-wide text-slate-400">Player 2</div>
            <div className="text-lg font-semibold">{player2Name || match?.player2Name}</div>
            <div className="text-4xl font-black text-pink-300">{p2Score}</div>
          </div>
        </div>

        <div className="mt-6 h-px bg-gradient-to-r from-sky-500/40 via-slate-700 to-pink-500/40" />

        <div className="mt-4 flex items-center justify-between text-xs text-slate-400">
          <span>Game ID: {gameId}</span>
          <span>Status: {finished ? "Finished" : "Running 🎮"}</span>
        </div>

        {finished && winner && (
          <div className="mt-4 rounded-lg bg-emerald-600/20 border border-emerald-500/50 px-4 py-3 text-center">
            <p className="text-sm font-semibold text-emerald-300">
              🏆 Winner: <span className="font-bold">{winner}</span>
            </p>
            <p className="text-xs text-emerald-200/80 mt-1">
              Game finished. Returning to home in a moment...
            </p>
          </div>
        )}

        {!finished && !locked && (
          <button
            onClick={handleStopGame}
            disabled={stopping}
            className="mt-4 w-full rounded-lg bg-red-600 px-4 py-2 text-sm font-semibold text-white hover:bg-red-500 disabled:opacity-60 disabled:cursor-not-allowed transition"
          >
            {stopping ? "Stopping..." : "🛑 Stop Game"}
          </button>
        )}

        {locked && !finished && (
          <p className="mt-4 text-xs text-yellow-300 text-center">
            Game stopping... Vests will sync within 2 seconds.
          </p>
        )}
      </div>
    </div>
  );
};

export default GamePage;

