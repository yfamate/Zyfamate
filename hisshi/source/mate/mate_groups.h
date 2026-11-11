#ifndef MATE_GROUPS_H_INCLUDED
#define MATE_GROUPS_H_INCLUDED

#include "../position.h"

namespace Mate {
inline int AttackDirectionGroup(const Position& pos, Move move, Color attacker) {
    const Square to = to_sq(move);
    const Square king = pos.king_square(~attacker);
    if (king == SQ_NB) return -1;
    const bool before = pos.side_to_move() == attacker;
    const Piece moved = before ? pos.moved_piece_after(move) : pos.piece_on(to);
    const PieceType type = raw_type_of(moved);
    if (type != ROOK && type != BISHOP && type != LANCE) return -1;
    Bitboard occupied = pos.pieces() | to;
    if (before && !is_drop(move)) occupied ^= from_sq(move);
    Bitboard targets = effects_from(moved, to, occupied) & (kingEffect(king) | king);
    Square target = SQ_NB;
    while (targets) {
        const Square square = targets.pop();
        if (target == SQ_NB || dist(square, king) < dist(target, king) ||
            (dist(square, king) == dist(target, king) && dist(to, square) < dist(to, target)))
            target = square;
    }
    if (target == SQ_NB) return -1;
    const int df = static_cast<int>(file_of(to)) - static_cast<int>(file_of(target));
    const int dr = static_cast<int>(rank_of(to)) - static_cast<int>(rank_of(target));
    return 81 + ((dr > 0) - (dr < 0) + 1) * 3 + (df > 0) - (df < 0) + 1;
}

inline int CheckingAttackGroup(const Position& pos, Move move) {
    const Square to = to_sq(move);
    if (pos.piece_on(to) != NO_PIECE) return static_cast<int>(to);
    return AttackDirectionGroup(pos, move, pos.side_to_move());
}
} 
#endif
