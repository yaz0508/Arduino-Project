export interface GameStartBody {
  player1Name: string;
  player2Name: string;
}

export interface GameScoreBody {
  gameId: string;
  player1Score: number;
  player2Score: number;
}

export interface GameEndBody {
  gameId: string;
  player1Score: number;
  player2Score: number;
  winner: string;
}

