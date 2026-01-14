import axios from "axios";

const API_BASE_URL = import.meta.env.VITE_API_BASE_URL || "http://localhost:3000";

export interface Match {
  id: string;
  gameId: string;
  player1Name: string;
  player2Name: string;
  player1Score: number;
  player2Score: number;
  winner: string | null;
  status: string;
  startedAt: string;
  endedAt?: string | null;
}

export interface GameStartResponse {
  gameId: string;
  matchId: string;
}

export async function startGame(player1Name: string, player2Name: string) {
  const res = await axios.post<GameStartResponse>(`${API_BASE_URL}/game/start`, {
    player1Name,
    player2Name
  });
  return res.data;
}

export async function getGame(gameId: string) {
  const res = await axios.get<Match>(`${API_BASE_URL}/game/${gameId}`);
  return res.data;
}

export async function getMatches() {
  const res = await axios.get<Match[]>(`${API_BASE_URL}/matches`);
  return res.data;
}

