import axios from "axios";

const API_BASE_URL = (import.meta.env.VITE_API_BASE_URL as string) || "http://localhost:3000";

// Create axios instance with default config
const apiClient = axios.create({
  baseURL: API_BASE_URL,
  timeout: 10000,
  headers: {
    "Content-Type": "application/json",
  },
});

// Add response interceptor for error handling
apiClient.interceptors.response.use(
  (response) => response,
  (error) => {
    if (error.response) {
      // Server responded with error status
      console.error("API Error:", error.response.status, error.response.data);
    } else if (error.request) {
      // Request made but no response received
      console.error("Network Error: No response from server");
    } else {
      // Something else happened
      console.error("Error:", error.message);
    }
    return Promise.reject(error);
  }
);

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

export async function startGame(player1Name: string, player2Name: string): Promise<GameStartResponse> {
  const res = await apiClient.post<GameStartResponse>("/game/start", {
    player1Name,
    player2Name
  });
  return res.data;
}

export async function getGame(gameId: string): Promise<Match> {
  const res = await apiClient.get<Match>(`/game/${gameId}`);
  return res.data;
}

export async function getMatches(): Promise<Match[]> {
  const res = await apiClient.get<Match[]>("/matches");
  return res.data;
}

