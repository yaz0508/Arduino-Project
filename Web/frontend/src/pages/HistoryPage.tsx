import React, { useEffect, useState } from "react";
import { getMatches, Match } from "../api/client";

const HistoryPage: React.FC = () => {
  const [matches, setMatches] = useState<Match[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    let cancelled = false;

    async function load() {
      try {
        const data = await getMatches();
        if (!cancelled) {
          setMatches(data);
          setError(null);
        }
      } catch (err) {
        console.error(err);
        if (!cancelled) setError("Failed to load match history.");
      } finally {
        if (!cancelled) setLoading(false);
      }
    }

    load();
    return () => {
      cancelled = true;
    };
  }, []);

  return (
    <div className="space-y-6">
      <h1 className="text-2xl font-bold text-sky-300">Match History</h1>

      {loading && <p className="text-sm text-slate-300">Loading...</p>}
      {error && <p className="text-sm text-red-400">{error}</p>}

      <div className="space-y-3">
        {matches.length === 0 && !loading && (
          <p className="text-sm text-slate-400">No matches recorded yet.</p>
        )}

        {matches.map((m) => (
          <div
            key={m.id}
            className="flex flex-col sm:flex-row sm:items-center justify-between bg-slate-900/80 border border-slate-800 rounded-lg px-4 py-3 gap-2"
          >
            <div className="space-y-1">
              <div className="text-sm font-semibold">
                {m.player1Name} <span className="text-slate-400">vs</span> {m.player2Name}
              </div>
              <div className="text-xs text-slate-400">
                {new Date(m.startedAt).toLocaleString()}
              </div>
            </div>
            <div className="text-right space-y-1">
              <div className="text-sm">
                <span className="text-sky-300 font-semibold">{m.player1Score}</span>{" "}
                <span className="text-slate-400">:</span>{" "}
                <span className="text-pink-300 font-semibold">{m.player2Score}</span>
              </div>
              {m.winner && (
                <div className="text-xs text-emerald-300">
                  Winner: <span className="font-semibold">{m.winner}</span>
                </div>
              )}
            </div>
          </div>
        ))}
      </div>
    </div>
  );
};

export default HistoryPage;

