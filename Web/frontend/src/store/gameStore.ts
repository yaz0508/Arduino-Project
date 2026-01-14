import { create } from "zustand";

interface GameState {
  gameId: string | null;
  player1Name: string;
  player2Name: string;
  setGame: (gameId: string, p1: string, p2: string) => void;
  clearGame: () => void;
}

export const useGameStore = create<GameState>((set) => ({
  gameId: null,
  player1Name: "",
  player2Name: "",
  setGame: (gameId, p1, p2) => set({ gameId, player1Name: p1, player2Name: p2 }),
  clearGame: () => set({ gameId: null, player1Name: "", player2Name: "" })
}));

