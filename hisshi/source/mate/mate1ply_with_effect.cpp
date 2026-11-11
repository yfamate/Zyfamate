#include "mate.h"


#if defined(USE_MATE_1PLY) && defined(LONG_EFFECT_LIBRARY)

#include "../position.h"
#include "../extra/long_effect.h"

using namespace Effect8; 

namespace {




	struct alignas(2) MateInfo
	{
	uint8_t hand_kind;

	Directions directions;
	};

	MateInfo  mate1ply_drop_tbl[0x10000][COLOR_NB];

} 



#define CHECK_PIECE(DROP_PIECE) \
  if (hk & HAND_KIND_ ## DROP_PIECE) {                                                                        \
    Piece pc = make_piece(Us,DROP_PIECE);                                                                     \
    Directions directions = mi.directions & piece_check_around8(pc) & info1;                                  \
    while (directions) {                                                                                      \
      Direct to_direct = pop_directions(directions);                                                          \
      Directions effect_not = piece_effect_not(pc, to_direct);                                                \
      if (effect_not & info2) continue;                                                   \
      to = themKing + DirectToDelta(to_direct);                                                               \
      Directions cut_dirs_from_king = cutoff_directions(to_direct, pos.long_effect.directions_of(Us, to))     \
                & effect_not & a8_board_mask & a8_them_movable;                                               \
      while (cut_dirs_from_king)                                                                              \
      {                                                                                                       \
        Direct cut_dir_from_king = pop_directions(cut_dirs_from_king);                                        \
        Square to2 = themKing + DirectToDelta(cut_dir_from_king);                                             \
        if (pos.board_effect[Us].e[to2] <= 1)                                                                 \
        goto Next ## DROP_PIECE;                                                                              \
      }                                                                                                       \
      return make_move_drop(DROP_PIECE, to , Us);                                                             \
    Next ## DROP_PIECE:;                                                                                      \
    }                                                                                                         \
  }

namespace Mate
{
	template <Color Us>
	Move mate_1ply_imp(const Position& pos)
	{
		Square from, to;
		auto them = ~Us;
		auto themKing = pos.king_square(them);


		Directions a8_effect_them = pos.board_effect[them].around8_greater_than_one(themKing);

		Directions a8_effect_us = pos.board_effect[Us].around8(themKing);

		Directions a8_them_movable = ~around8(pos.pieces(them), themKing);

		Directions a8_droppable = around8(~pos.pieces(), themKing);

		Directions a8_board_mask = board_mask(themKing);

		Directions info1 = ~a8_effect_them & a8_effect_us & a8_droppable & a8_board_mask;  
		Directions info2 = a8_them_movable & ~a8_effect_us & a8_board_mask; 

		uint16_t info = ((uint16_t)info2 << 8) | info1;

		auto& mi = mate1ply_drop_tbl[info][Us];

		if (!(mi.directions | mi.hand_kind))
			return MOVE_NONE;


		auto ourHand = toHandKind(pos.hand_of(Us));

		auto hk = (HandKind)(ourHand & ~(HAND_KIND_PAWN | HAND_KIND_KNIGHT | HAND_KIND_KING)) & mi.hand_kind;

		if (!hk) goto MOVE_MATE;

		{
			auto themKingWW = to_sqww(themKing);

			CHECK_PIECE(GOLD);
			CHECK_PIECE(SILVER);
			CHECK_PIECE(ROOK) else CHECK_PIECE(LANCE); 
			CHECK_PIECE(BISHOP);
		}

	MOVE_MATE:


		const auto& pinned = pos.pinned_pieces(pos.side_to_move());

		Directions a8_effect_us_gt1 = pos.board_effect[Us].around8_greater_than_one(themKing); 


		if (mi.hand_kind & HAND_KIND_KNIGHT)
		{
			auto drop_target = knightEffect(them, themKing) & ~pos.pieces(Us);
			while (drop_target) {
				to = drop_target.pop();
				if (!pos.board_effect[them].effect(to))
				{
					if ((ourHand & HAND_KIND_KNIGHT) && !(pos.pieces() & to)) {
						auto cut_directions = pos.long_effect.le16[to].dirs[Us];
						if (!cut_directions)
							return make_move_drop(KNIGHT, to, Us);

						auto direct = direct_knight_of<Us>(themKing, to);
						auto dec_effect = cutoff_directions(direct, cut_directions);
						if (!dec_effect)
							return make_move_drop(KNIGHT, to, Us);

						if (!(dec_effect & ~(a8_effect_us_gt1 | ~a8_board_mask | ~a8_them_movable)))
							return make_move_drop(KNIGHT, to, Us);

						continue;
					}

					auto froms = knightEffect(them, to) & pos.pieces(Us, KNIGHT);
					while (froms) {
						from = froms.pop();
						if (!(pinned & from))
						{
							auto cut_directions = pos.long_effect.le16[to].dirs[Us];
							if (!cut_directions)
								return make_move(from, to, Us, KNIGHT);

							auto direct = direct_knight_of<Us>(themKing, to);
							auto dec_effect = cutoff_directions(direct, cut_directions);
							if (!dec_effect)
								return make_move(from, to, Us, KNIGHT);

							if (!(dec_effect & ~(a8_effect_us_gt1 | ~a8_board_mask | ~a8_them_movable)))
								return make_move(from, to, Us, KNIGHT);

						}
					}
				}
			}
		}

		auto ourKing = pos.king_square(Us);



		auto is_mated_by_from_to = [&](Square from, Square to, Direct to_direct)
		{
			Piece pc = pos.piece_on(from);






			{
				if (type_of(pc) == KING)
					goto NextCandidate; 

				if (pos.discovered(from, to, ourKing, pinned))          
					goto NextCandidate;

				static_assert(GOLD == 7, "GOLD must be 7.");
				if ((type_of(pc) >= GOLD) || !((Bitboard(from) ^ Bitboard(to)) & enemy_field(Us)))
					goto NonProCheck2; 

				auto pro_pc = (Piece)(pc + PIECE_PROMOTE);

				if (!(piece_check_around8(pro_pc) & (1 << to_direct))) 
					goto NonProCheck;

				auto effect_us_not = (type_of(pro_pc) == DRAGON && is_diag(to_direct)) ?
					~around8(dragonEffect(to, pos.pieces()), themKing) : piece_effect_not(pro_pc, to_direct); 

				if (effect_us_not & info2)     
					goto NonProCheck;

				if (directions_of(from, to) & pos.long_effect.le16[from].dirs[~Us])  
					goto NextCandidate; 

				auto dec_effect = effects_from(pc, from, pos.pieces());
				auto dec_around8 = around8(dec_effect, themKing);

				auto cut_dirs = cutoff_directions(to_direct, pos.long_effect.directions_of(Us, to) & ~LongEffect::long_effect_of(pc));

				auto dec = (dec_around8 | cut_dirs) & effect_us_not & a8_board_mask & a8_them_movable & ~to_directions(to_direct);

				while (dec)
				{
					Direct cut_direct = pop_directions(dec);

					uint8_t dec_num = ((dec_around8 >> cut_direct) & 1) + ((cut_dirs >> cut_direct) & 1);

					Square to2 = themKing + DirectToDelta(cut_direct);
					if (pos.board_effect[Us].effect(to2) <= dec_num) 
						goto NonProCheck;
				}

				return make_move_promote(from, to, pc);
			}

		NonProCheck:;

		{
			PieceType pt = type_of(pc);

			if (pt == PAWN || pt == BISHOP || pt == ROOK)
				goto NextCandidate;

			if (pt == LANCE && (rank_of(to) == ((Us == BLACK) ? RANK_2 : RANK_8)))
				goto NextCandidate;
		}

	NonProCheck2:;


	{
		if (type_of(pc) == KNIGHT) 
			goto NextCandidate;

		if (pos.discovered(from, to, ourKing, pinned))
			goto NextCandidate;

		if (!(piece_check_around8(pc) & (1 << to_direct)))
			goto NextCandidate;

		auto effect_us_not = (type_of(pc) == DRAGON && is_diag(to_direct)) ?
			~around8(dragonEffect(to, pos.pieces()), themKing) : piece_effect_not(pc, to_direct);

		if (effect_us_not & info2)
			goto NextCandidate;

		if (directions_of(from, to) & pos.long_effect.le16[from].dirs[~Us])
			goto NextCandidate;

		auto dec_effect = effects_from(pc, from, pos.pieces());
		auto dec_around8 = around8(dec_effect, themKing);
		auto cut_dirs = cutoff_directions(to_direct, pos.long_effect.directions_of(Us, to) & ~LongEffect::long_effect_of(pc));
		auto dec = (dec_around8 | cut_dirs) & effect_us_not & a8_board_mask & a8_them_movable & ~to_directions(to_direct);

		while (dec)
		{
			Direct cut_direct = pop_directions(dec);
			uint8_t dec_num = ((dec_around8 >> cut_direct) & 1) + ((cut_dirs >> cut_direct) & 1);
			Square to2 = themKing + DirectToDelta(cut_direct);
			if (pos.board_effect[Us].effect(to2) <= dec_num)
				goto NextCandidate;
		}
		return make_move(from, to, Us, KNIGHT);
	}
NextCandidate:;

	return MOVE_NONE;
		};




		Directions a8_us_movable = ~around8(pos.pieces(Us), themKing);
		Bitboard froms;
		Directions to_candicate = a8_effect_us_gt1 & a8_board_mask & a8_us_movable & mi.directions & ~a8_effect_them; 

		while (to_candicate)
		{
			auto to_direct = pop_directions(to_candicate);
			to = themKing + DirectToDelta(to_direct);

			froms = pos.attackers_to(Us, to);
			while (froms)
			{
				from = froms.pop();
				Move m = is_mated_by_from_to(from, to, to_direct);
				if (m != MOVE_NONE)
					return m;
			}
		}



		auto a24_candidate = pos.long_effect.long_effect24_to_around9<Us>(themKing);

		auto a24_us_occ = Effect24::around24(pos.pieces(Us), themKing);

		auto a24_board_mask = Effect24::board_mask(themKing);

		auto froms24 = a24_candidate & a24_us_occ & a24_board_mask;

		while (froms24)
		{

			auto from_direct = Effect24::pop_directions(froms24);
			auto from = themKing + Effect24::DirectToDelta(from_direct);

			auto pc = pos.piece_on(from);

			auto target = effects_from(pc, from, pos.pieces()) & kingEffect(themKing) & ~pos.pieces(Us);

			while (target)
			{
				auto to = target.pop();

				if (!(directions_of(from, to) & pos.long_effect.directions_of(Us, from)))
					continue;

				if (pos.board_effect[Us].effect(to) != 1 || pos.board_effect[them].effect(to) != 1)
					continue;

				auto to_direct = (Direct)LSB32(directions_of(themKing, to));
				Move m = is_mated_by_from_to(from, to, to_direct);
				if (m != MOVE_NONE)
					return m;
			}
		}

		return MOVE_NONE;
	}

	void init_mate_1ply()
	{
		for (auto c : COLOR)
			for (uint32_t info = 0; info < 0x10000; ++info)
			{
				Directions info1 = Directions(info & 0xff);

				Directions info2 = Directions(info >> 8);

				Directions directions = DIRECTIONS_ZERO;
				HandKind hk = HAND_KIND_ZERO;

				for (int i = 0; i < 8; ++i)
				{
					Direct dir = Direct(i);
					bool droppable = (info1 & (1 << i));

					const PieceType drop_pieces[6] = { LANCE,SILVER,GOLD,BISHOP,ROOK,QUEEN };
					for (auto pt : drop_pieces)
					{
						if (!droppable && pt != QUEEN)
							continue;

						Directions effect_not = piece_effect_not(make_piece(c, pt), dir);

						Directions king_movable = effect_not & info2;

						if (!king_movable)
						{
							if (pt == QUEEN)
								directions |= to_directions(dir);
							else
								hk |= HandKind(1 << (pt - 1));
						}
					}
					if (!info2)
						hk |= HandKind(1 << (KNIGHT - 1));
				}

				mate1ply_drop_tbl[info][c].hand_kind = hk;
				mate1ply_drop_tbl[info][c].directions = directions;
			}
	}

	Move mate_1ply(const Position& pos)
	{
		return pos.side_to_move() == BLACK ? Mate::mate_1ply_imp<BLACK>(pos) : Mate::mate_1ply_imp<WHITE>(pos);
	}

} 


#endif
