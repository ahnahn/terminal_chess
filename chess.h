/* chess.h: Declarations for the chess game logic */
#ifndef CHESS_H
#define CHESS_H

#define BOARD_SIZE 8

/* Piece and board representation: uppercase = White, lowercase = Black, '.' = empty */
enum {WHITE, BLACK};
typedef struct {
    char board[BOARD_SIZE][BOARD_SIZE];
    int turn; /* WHITE or BLACK */
    /* Castling rights: 1 = not moved, 0 = moved */
    int whiteKingMoved, whiteRookAmoved, whiteRookH, blackKingMoved, blackRookAmoved, blackRookH;
    /* En passant target: file ('a' to 'h') and rank (1 to 8), or 0 if none */
    int ep_row, ep_col;
} GameState;

/* Initialize board to starting position */
void init_board(GameState *game);

/* Print board to stdout (for local display or sending to client) */
void print_board(const GameState *game);

/* Parse move string like "e2e4" into source/destination indices (0-7).
   Returns 1 on success, 0 on invalid input. */
int parse_move(const char *move, int *src_row, int *src_col, int *dst_row, int *dst_col);

/* Attempt to make a move; return 1 if move is valid and applied, 0 if invalid.
   Handles pawn promotion (auto to Queen), castling, en passant, etc. */
int make_move(GameState *game, int src_row, int src_col, int dst_row, int dst_col);

/* Check whether the side 'player' (WHITE or BLACK) is in check */
int is_in_check(const GameState *game, int player);

/* Check if the player has any valid moves. Used to detect checkmate or stalemate. */
int has_valid_moves(GameState *game, int player);

/* Copy game state (for simulating moves) */
void copy_game(const GameState *src, GameState *dst);

#endif /* CHESS_H */
