/* chess.c: Implementation of chess game logic */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chess.h"

/* Helper arrays for knight moves */
static const int knight_moves[8][2] = {
    { 2,  1}, { 2, -1}, {-2,  1}, {-2, -1},
    { 1,  2}, { 1, -2}, {-1,  2}, {-1, -2}
};

/* Initialize the board with standard setup */
void init_board(GameState *game) {
    game->turn = WHITE;
    game->whiteKingMoved = game->whiteRookAmoved = game->whiteRookH = 0;
    game->blackKingMoved = game->blackRookAmoved = game->blackRookH = 0;
    game->ep_row = -1; game->ep_col = -1;
    /* Black pieces (rank 8 and 7) */
    char row8[] = "rnbqkbnr";
    char row7[] = "pppppppp";
    char row2[] = "PPPPPPPP";
    char row1[] = "RNBQKBNR";
    for(int c = 0; c < BOARD_SIZE; c++) {
        game->board[0][c] = row8[c];
        game->board[1][c] = row7[c];
        game->board[6][c] = row2[c];
        game->board[7][c] = row1[c];
    }
    /* Empty middle */
    for(int r = 2; r < 6; r++)
        for(int c = 0; c < BOARD_SIZE; c++)
            game->board[r][c] = '.';
}

/* Print the board (with ranks/files) */
void print_board(const GameState *game) {
    printf("  a b c d e f g h\n");
    for(int r = 0; r < BOARD_SIZE; r++) {
        printf("%d ", BOARD_SIZE - r);
        for(int c = 0; c < BOARD_SIZE; c++) {
            char pc = game->board[r][c];
            printf("%c ", pc);
        }
        printf("%d\n", BOARD_SIZE - r);
    }
    printf("  a b c d e f g h\n");
}

/* Convert algebraic notation to array indices */
int parse_move(const char *move, int *src_row, int *src_col, int *dst_row, int *dst_col) {
    /* Expect format [file][rank][file][rank], e.g., e2e4 */
    if(strlen(move) < 4) return 0;
    char sc = move[0], sr = move[1];
    char dc = move[2], dr = move[3];
    if(sc < 'a' || sc > 'h' || dc < 'a' || dc > 'h') return 0;
    if(sr < '1' || sr > '8' || dr < '1' || dr > '8') return 0;
    *src_col = sc - 'a';
    *dst_col = dc - 'a';
    /* Convert rank to 0-based row (0 is rank 8, 7 is rank 1) */
    *src_row = BOARD_SIZE - (sr - '0');
    *dst_row = BOARD_SIZE - (dr - '0');
    /* Adjust: rank '1' -> row 7, '8' -> row 0 */
    *src_row = BOARD_SIZE - (sr - '0');
    *dst_row = BOARD_SIZE - (dr - '0');
    /* Actually do BOARD_SIZE - rank */
    *src_row = BOARD_SIZE - (sr - '0');
    *dst_row = BOARD_SIZE - (dr - '0');
    return 1;
}

/* Utility: check if coordinates are on board */
static int on_board(int r, int c) {
    return r >= 0 && r < BOARD_SIZE && c >= 0 && c < BOARD_SIZE;
}

/* Return 1 if square has a piece of color player (WHITE/BLACK) */
static int is_own_piece(const GameState *game, int player, int r, int c) {
    char pc = game->board[r][c];
    if(pc == '.') return 0;
    if(player == WHITE) return (pc >= 'A' && pc <= 'Z');
    else return (pc >= 'a' && pc <= 'z');
}

/* Return 1 if square has an enemy piece */
static int is_enemy_piece(const GameState *game, int player, int r, int c) {
    char pc = game->board[r][c];
    if(pc == '.') return 0;
    if(player == WHITE) return (pc >= 'a' && pc <= 'z');
    else return (pc >= 'A' && pc <= 'Z');
}

/* Check if 'player' has a move attacking (r,c). Used for check detection. */
static int attacks_square(const GameState *game, int player, int r, int c) {
    /* Pawn attacks */
    int dir = (player == WHITE) ? -1 : 1;
    /* White pawns move up (decreasing row), Black down */
    int pawnRow = r + dir;
    if(on_board(pawnRow, c-1) && ((player == WHITE && game->board[pawnRow][c-1] == 'p') ||
                                 (player == BLACK && game->board[pawnRow][c-1] == 'P')))
        return 1;
    if(on_board(pawnRow, c+1) && ((player == WHITE && game->board[pawnRow][c+1] == 'p') ||
                                 (player == BLACK && game->board[pawnRow][c+1] == 'P')))
        return 1;
    /* Knight attacks */
    for(int k = 0; k < 8; k++) {
        int nr = r + knight_moves[k][0];
        int nc = c + knight_moves[k][1];
        if(on_board(nr, nc)) {
            char pc = game->board[nr][nc];
            if((player == WHITE && pc == 'n') || (player == BLACK && pc == 'N'))
                return 1;
        }
    }
    /* Rook/Queen straight-line attacks */
    int dirs[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
    for(int d = 0; d < 8; d++) {
        int dr = dirs[d][0], dc = dirs[d][1];
        int nr = r + dr, nc = c + dc;
        while(on_board(nr, nc)) {
            char pc = game->board[nr][nc];
            if(pc != '.') {
                /* If same player piece, block */
                if(is_own_piece(game, 1-player, nr, nc)) break;
                /* Check if enemy rook/queen (straight) or bishop/queen (diagonal) */
                if(d < 4) { /* straight dirs (0-3) */
                    if((player==WHITE && (pc=='r' || pc=='q')) ||
                       (player==BLACK && (pc=='R' || pc=='Q')))
                        return 1;
                } else { /* diagonal dirs (4-7) */
                    if((player==WHITE && (pc=='b' || pc=='q')) ||
                       (player==BLACK && (pc=='B' || pc=='Q')))
                        return 1;
                }
                break;
            }
            nr += dr; nc += dc;
        }
    }
    /* King adjacency (should not happen in legal games) */
    for(int dr = -1; dr <= 1; dr++) {
        for(int dc = -1; dc <= 1; dc++) {
            if(dr==0 && dc==0) continue;
            int nr = r + dr, nc = c + dc;
            if(on_board(nr,nc)) {
                char pc = game->board[nr][nc];
                if((player==WHITE && pc=='k') || (player==BLACK && pc=='K'))
                    return 1;
            }
        }
    }
    return 0;
}

/* Check if the given player is in check. Return 1 if king is attacked. */
int is_in_check(const GameState *game, int player) {
    /* Find king coordinates */
    char kingChar = (player == WHITE) ? 'K' : 'k';
    int kr=-1, kc=-1;
    for(int r=0;r<BOARD_SIZE;r++) {
        for(int c=0;c<BOARD_SIZE;c++) {
            if(game->board[r][c] == kingChar) { kr=r; kc=c; break; }
        }
        if(kr!=-1) break;
    }
    if(kr < 0) return 0; /* should not happen */
    /* Check if any enemy attacks king's square */
    return attacks_square(game, 1-player, kr, kc);
}

/* Copy game state */
void copy_game(const GameState *src, GameState *dst) {
    memcpy(dst, src, sizeof(GameState));
}

/* Try to make a move; returns 1 if valid, 0 otherwise */
int make_move(GameState *game, int src_row, int src_col, int dst_row, int dst_col) {
    /* Basic range checks */
    if(!on_board(src_row, src_col) || !on_board(dst_row, dst_col)) return 0;
    char piece = game->board[src_row][src_col];
    if(piece == '.') return 0;
    int player = (piece >= 'A' && piece <= 'Z') ? WHITE : BLACK;
    if(player != game->turn) return 0;  /* wrong side to move */
    /* Destination not own piece */
    if(is_own_piece(game, player, dst_row, dst_col)) return 0;

    /* Determine piece type for move rules */
    char pc = piece;
    int dr = dst_row - src_row, dc = dst_col - src_col;

    /* Pawn moves */
    if(pc == 'P' || pc == 'p') {
        int dir = (pc == 'P') ? -1 : 1;  /* White pawns go up (decreasing row) */
        /* Single step forward */
        if(dc == 0 && dr == dir && game->board[dst_row][dst_col] == '.') {
            /* Normal move */
        }
        /* Double step from start rank */
        else if(dc == 0 && dr == 2*dir) {
            int startRow = (pc == 'P') ? 6 : 1;
            if(src_row == startRow && game->board[src_row+dir][src_col] == '.' &&
               game->board[dst_row][dst_col] == '.') {
                /* Set en passant target square */
                game->ep_row = src_row + dir;
                game->ep_col = src_col;
            } else return 0;
        }
        /* Capture move */
        else if(abs(dc) == 1 && dr == dir) {
            /* Normal capture */
            if(is_enemy_piece(game, player, dst_row, dst_col)) {
                /* OK */
            }
            /* En passant capture */
            else if(dst_row == game->ep_row && dst_col == game->ep_col) {
                /* Capture the pawn that just moved two squares */
                int cap_row = src_row;
                game->board[cap_row][dst_col] = '.';
            } else return 0;
        } else {
            return 0;
        }
        /* Move pawn */
        game->board[dst_row][dst_col] = piece;
        game->board[src_row][src_col] = '.';
        /* Promotion: if pawn reaches last rank */
        if((pc == 'P' && dst_row == 0) || (pc == 'p' && dst_row == 7)) {
            game->board[dst_row][dst_col] = (player==WHITE) ? 'Q' : 'q';  /* auto-queen */
        }
    }
    /* Knight moves */
    else if(pc == 'N' || pc == 'n') {
        int valid = 0;
        for(int k=0;k<8;k++) {
            if(dr == knight_moves[k][0] && dc == knight_moves[k][1]) { valid = 1; break; }
        }
        if(!valid) return 0;
        game->board[dst_row][dst_col] = piece;
        game->board[src_row][src_col] = '.';
    }
    /* Bishop moves */
    else if(pc == 'B' || pc == 'b') {
        if(abs(dr) != abs(dc) || dr == 0) return 0;
        int stepR = (dr > 0) ? 1 : -1;
        int stepC = (dc > 0) ? 1 : -1;
        int r=src_row+stepR, c=src_col+stepC;
        while(r != dst_row && c != dst_col) {
            if(game->board[r][c] != '.') return 0;
            r += stepR; c += stepC;
        }
        game->board[dst_row][dst_col] = piece;
        game->board[src_row][src_col] = '.';
    }
    /* Rook moves */
    else if(pc == 'R' || pc == 'r') {
        if(dr != 0 && dc != 0) return 0;
        int stepR = (dr==0) ? 0 : (dr>0 ? 1 : -1);
        int stepC = (dc==0) ? 0 : (dc>0 ? 1 : -1);
        int r=src_row+stepR, c=src_col+stepC;
        while(r != dst_row || c != dst_col) {
            if(game->board[r][c] != '.') return 0;
            r += stepR; c += stepC;
        }
        /* If rook move, may affect castling rights */
        if(player == WHITE && src_row==7 && src_col==0) game->whiteRookAmoved = 1;
        if(player == WHITE && src_row==7 && src_col==7) game->whiteRookH = 1;
        if(player == BLACK && src_row==0 && src_col==0) game->blackRookAmoved = 1;
        if(player == BLACK && src_row==0 && src_col==7) game->blackRookH = 1;
        game->board[dst_row][dst_col] = piece;
        game->board[src_row][src_col] = '.';
    }
    /* Queen moves (combination of R and B) */
    else if(pc == 'Q' || pc == 'q') {
        if(abs(dr) == abs(dc) && dr != 0) {
            /* diagonal (bishop) */
            int stepR = (dr > 0) ? 1 : -1;
            int stepC = (dc > 0) ? 1 : -1;
            int r=src_row+stepR, c=src_col+stepC;
            while(r != dst_row && c != dst_col) {
                if(game->board[r][c] != '.') return 0;
                r += stepR; c += stepC;
            }
        } else if(dr == 0 || dc == 0) {
            /* straight (rook) */
            int stepR = (dr==0) ? 0 : (dr>0 ? 1 : -1);
            int stepC = (dc==0) ? 0 : (dc>0 ? 1 : -1);
            int r=src_row+stepR, c=src_col+stepC;
            while(r != dst_row || c != dst_col) {
                if(game->board[r][c] != '.') return 0;
                r += stepR; c += stepC;
            }
        } else {
            return 0;
        }
        game->board[dst_row][dst_col] = piece;
        game->board[src_row][src_col] = '.';
    }
    /* King moves (one square or castling) */
    else if(pc == 'K' || pc == 'k') {
        /* Castling: move two squares horizontally */
        if(dr == 0 && abs(dc) == 2) {
            /* Check castling rights and path */
            if(player == WHITE && src_row == 7) {
                /* King must not have moved and not in check */
                if(game->whiteKingMoved) return 0;
                if(is_in_check(game, WHITE)) return 0;
                /* Castling kingside */
                if(dc == 2 && !game->whiteRookH) {
                    /* Path must be clear and not attacked */
                    if(game->board[7][5] != '.' || game->board[7][6] != '.') return 0;
                    /* Check if squares  f1/g1 are safe */
                    GameState temp; copy_game(game, &temp);
                    /* simulate king move to f1 */
                    temp.board[7][4]='.'; temp.board[7][5]='K';
                    if(is_in_check(&temp, WHITE)) return 0;
                    /* simulate king move to g1 */
                    temp.board[7][5]='.'; temp.board[7][6]='K';
                    if(is_in_check(&temp, WHITE)) return 0;
                    /* Perform castling */
                    game->board[7][6] = 'K'; game->board[7][4] = '.';
                    game->board[7][5] = 'R'; game->board[7][7] = '.';
                    game->whiteKingMoved = game->whiteRookH = 1;
                }
                /* Castling queenside */
                else if(dc == -2 && !game->whiteRookAmoved) {
                    if(game->board[7][1] != '.' || game->board[7][2] != '.' || game->board[7][3] != '.') return 0;
                    GameState temp; copy_game(game, &temp);
                    temp.board[7][4]='.'; temp.board[7][3]='K';
                    if(is_in_check(&temp, WHITE)) return 0;
                    temp.board[7][3]='.'; temp.board[7][2]='K';
                    if(is_in_check(&temp, WHITE)) return 0;
                    game->board[7][2] = 'K'; game->board[7][4] = '.';
                    game->board[7][3] = 'R'; game->board[7][0] = '.';
                    game->whiteKingMoved = game->whiteRookAmoved = 1;
                } else return 0;
            }
            else if(player == BLACK && src_row == 0) {
                if(game->blackKingMoved) return 0;
                if(is_in_check(game, BLACK)) return 0;
                if(dc == 2 && !game->blackRookH) {
                    if(game->board[0][5] != '.' || game->board[0][6] != '.') return 0;
                    GameState temp; copy_game(game, &temp);
                    temp.board[0][4]='.'; temp.board[0][5]='k';
                    if(is_in_check(&temp, BLACK)) return 0;
                    temp.board[0][5]='.'; temp.board[0][6]='k';
                    if(is_in_check(&temp, BLACK)) return 0;
                    game->board[0][6] = 'k'; game->board[0][4] = '.';
                    game->board[0][5] = 'r'; game->board[0][7] = '.';
                    game->blackKingMoved = game->blackRookH = 1;
                }
                else if(dc == -2 && !game->blackRookAmoved) {
                    if(game->board[0][1] != '.' || game->board[0][2] != '.' || game->board[0][3] != '.') return 0;
                    GameState temp; copy_game(game, &temp);
                    temp.board[0][4]='.'; temp.board[0][3]='k';
                    if(is_in_check(&temp, BLACK)) return 0;
                    temp.board[0][3]='.'; temp.board[0][2]='k';
                    if(is_in_check(&temp, BLACK)) return 0;
                    game->board[0][2] = 'k'; game->board[0][4] = '.';
                    game->board[0][3] = 'r'; game->board[0][0] = '.';
                    game->blackKingMoved = game->blackRookAmoved = 1;
                } else return 0;
            }
            else return 0;
        }
        /* Normal king move (one square any direction) */
        else if(abs(dr) <= 1 && abs(dc) <= 1) {
            /* Move the king */
            if(player == WHITE) {
                game->whiteKingMoved = 1;
            } else {
                game->blackKingMoved = 1;
            }
            game->board[dst_row][dst_col] = piece;
            game->board[src_row][src_col] = '.';
        }
        else {
            return 0;
        }
        /* After moving king or castling, ensure king not in check */
        if(is_in_check(game, player)) {
            /* Undo move (simple approach: reject) */
            return 0;
        }
    }
    else {
        /* Unknown piece */
        return 0;
    }

    /* Reset en passant if not used */
    if(!(pc=='P' || pc=='p') || abs(dst_row - src_row) != 2) {
        game->ep_row = game->ep_col = -1;
    }
    /* Toggle turn */
    game->turn = 1 - game->turn;
    return 1;
}

/* Check if the player has any legal move (used for checkmate/stalemate) */
int has_valid_moves(GameState *game, int player) {
    GameState temp;
    /* Try all pieces */
    for(int r=0; r<BOARD_SIZE; r++) {
        for(int c=0; c<BOARD_SIZE; c++) {
            char pc = game->board[r][c];
            if(pc == '.') continue;
            if((player==WHITE && pc>='A'&&pc<='Z') || (player==BLACK && pc>='a'&&pc<='z')) {
                /* Try every possible destination */
                for(int tr=0; tr<BOARD_SIZE; tr++) {
                    for(int tc=0; tc<BOARD_SIZE; tc++) {
                        copy_game(game, &temp);
                        if(make_move(&temp, r, c, tr, tc)) {
                            /* Move applied successfully */
                            if(!is_in_check(&temp, player)) {
                                return 1;
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}
