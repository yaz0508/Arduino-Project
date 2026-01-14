import React from "react";
import { Routes, Route, Link, NavLink } from "react-router-dom";
import StartPage from "./pages/StartPage";
import GamePage from "./pages/GamePage";
import HistoryPage from "./pages/HistoryPage";

const App: React.FC = () => {
  return (
    <div className="min-h-screen flex flex-col">
      <header className="border-b border-slate-800 bg-slate-950/60 backdrop-blur">
        <div className="max-w-4xl mx-auto px-4 py-3 flex items-center justify-between">
          <Link to="/" className="text-lg font-bold text-sky-400">
            Laser Duel
          </Link>
          <nav className="flex gap-4 text-sm">
            <NavLink
              to="/"
              className={({ isActive }) =>
                `hover:text-sky-300 ${isActive ? "text-sky-400" : "text-slate-300"}`
              }
            >
              Start
            </NavLink>
            <NavLink
              to="/game"
              className={({ isActive }) =>
                `hover:text-sky-300 ${isActive ? "text-sky-400" : "text-slate-300"}`
              }
            >
              Live Game
            </NavLink>
            <NavLink
              to="/history"
              className={({ isActive }) =>
                `hover:text-sky-300 ${isActive ? "text-sky-400" : "text-slate-300"}`
              }
            >
              History
            </NavLink>
          </nav>
        </div>
      </header>

      <main className="flex-1">
        <div className="max-w-4xl mx-auto px-4 py-6">
          <Routes>
            <Route path="/" element={<StartPage />} />
            <Route path="/game" element={<GamePage />} />
            <Route path="/history" element={<HistoryPage />} />
          </Routes>
        </div>
      </main>
    </div>
  );
};

export default App;

