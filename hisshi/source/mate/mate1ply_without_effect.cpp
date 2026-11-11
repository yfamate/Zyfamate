#include "mate.h"

#if defined(USE_MATE_1PLY) && !defined(LONG_EFFECT_LIBRARY)




#include "../position.h"




namespace {

	
	template <typename T> int sgn(T val) {
		return (T(0) < val) - (val < T(0));
	}

	
	enum PieceTypeCheck
	{
		PIECE_TYPE_CHECK_PAWN_WITH_NO_PRO, 
		PIECE_TYPE_CHECK_PAWN_WITH_PRO, 
		PIECE_TYPE_CHECK_LANCE,
		PIECE_TYPE_CHECK_KNIGHT,
		PIECE_TYPE_CHECK_SILVER,
		PIECE_TYPE_CHECK_GOLD,
		PIECE_TYPE_CHECK_BISHOP,
		PIECE_TYPE_CHECK_ROOK,
		PIECE_TYPE_CHECK_PRO_BISHOP,
		PIECE_TYPE_CHECK_PRO_ROOK,
		PIECE_TYPE_CHECK_NON_SLIDER, 

		PIECE_TYPE_CHECK_NB,
		PIECE_TYPE_CHECK_ZERO = 0,
	};

	ENABLE_FULL_OPERATORS_ON(PieceTypeCheck);

	
	Bitboard CHECK_CAND_BB[SQ_NB_PLUS1][PIECE_TYPE_CHECK_NB][COLOR_NB];

	
	
	Bitboard CHECK_AROUND_BB[SQ_NB_PLUS1][PIECE_RAW_NB][COLOR_NB];

	
	
	inline Bitboard check_cand_bb(Color us, PieceTypeCheck pc, Square sq_king)
	{
		return CHECK_CAND_BB[sq_king][pc][us];
	}

	
	
	inline Bitboard check_around_bb(Color us, PieceType pt, Square sq_king)
	{
		return CHECK_AROUND_BB[sq_king][pt - 1][us];
	}

	
	
	
	 u8 NextSquare[SQ_NB_PLUS1][SQ_NB_PLUS1];
	inline Square nextSquare(Square sq1, Square sq2) { return (Square)NextSquare[sq1][sq2]; }

	
	void init_check_bb()
	{
		for (PieceTypeCheck p = PIECE_TYPE_CHECK_ZERO; p < PIECE_TYPE_CHECK_NB; ++p)
			for (auto sq : SQ)
				for (auto c : COLOR)
				{
					Bitboard bb = ZERO_BB, tmp = ZERO_BB;
					Square to;

					
					Bitboard enemyBB = enemy_field(c);

					
					

					switch ((int)p)
					{
					case PIECE_TYPE_CHECK_PAWN_WITH_NO_PRO:
						

						bb = pawnEffect(~c, sq) & ~enemyBB;
						if (!bb)
							break;
						to = bb.pop();
						bb = pawnEffect(~c, to);
						break;

					case PIECE_TYPE_CHECK_PAWN_WITH_PRO:

						bb = goldEffect(~c, sq) & enemy_field(c);
						bb = pawnEffect(~c, bb);
						break;

					case PIECE_TYPE_CHECK_LANCE:

						
						bb = lanceStepEffect(~c, sq);
						if (enemy_field(c) ^ sq)
						{
							
							if (file_of(sq) != FILE_1)
								bb |= lanceStepEffect(~c, sq + SQ_R);
							if (file_of(sq) != FILE_9)
								bb |= lanceStepEffect(~c, sq + SQ_L);
						}

						break;

					case PIECE_TYPE_CHECK_KNIGHT:

						
						tmp = knightEffect(~c, sq);
						while (tmp)
						{
							to = tmp.pop();
							bb |= knightEffect(~c, to);
						}
						
						tmp = goldEffect(~c, sq) & enemyBB;
						while (tmp)
						{
							to = tmp.pop();
							bb |= knightEffect(~c, to);
						}
						break;

					case PIECE_TYPE_CHECK_SILVER:

						
						tmp = silverEffect(~c, sq);
						while (tmp)
						{
							to = tmp.pop();
							bb |= silverEffect(~c, to);
						}
						
						tmp = goldEffect(~c, sq) & enemyBB;
						while (tmp)
						{
							to = tmp.pop();
							bb |= silverEffect(~c, to);
						}
						
						
						{
							Rank r = (c == BLACK ? RANK_4 : RANK_6);
							if (r == rank_of(sq))
							{
								r = (c == BLACK ? RANK_3 : RANK_7);
								to = (file_of(sq) | r);
								bb |= to;
								bb |= cross45StepEffect(to);

								
								if (file_of(to) >= FILE_3)
									bb |= (to + SQ_R * 2);
								if (file_of(to) <= FILE_7)
									bb |= (to + SQ_L * 2);
							}

							
							if (rank_of(sq) == RANK_5)
								bb |= knightEffect(c, sq);
						}
						break;

					case PIECE_TYPE_CHECK_GOLD:
						
						tmp = goldEffect(~c, sq);
						while (tmp)
						{
							to = tmp.pop();
							bb |= goldEffect(~c, to);
						}
						break;

						
					case PIECE_TYPE_CHECK_BISHOP:
					case PIECE_TYPE_CHECK_PRO_BISHOP:
					case PIECE_TYPE_CHECK_ROOK:
					case PIECE_TYPE_CHECK_PRO_ROOK:
						
						break;

						
					case PIECE_TYPE_CHECK_NON_SLIDER:
						bb =  CHECK_CAND_BB[sq][PIECE_TYPE_CHECK_GOLD][c]
							| CHECK_CAND_BB[sq][PIECE_TYPE_CHECK_KNIGHT][c]
							| CHECK_CAND_BB[sq][PIECE_TYPE_CHECK_SILVER][c]
							| CHECK_CAND_BB[sq][PIECE_TYPE_CHECK_PAWN_WITH_NO_PRO][c]
							| CHECK_CAND_BB[sq][PIECE_TYPE_CHECK_PAWN_WITH_PRO][c];
						break;
					}
					bb &= ~Bitboard(sq); 
					CHECK_CAND_BB[sq][p][c] = bb;
				}


		for (PieceType p = PAWN; p <= KING; ++p)
			for (auto sq : SQ)
				for (auto c : COLOR)
				{
					Bitboard bb = ZERO_BB, tmp = ZERO_BB;
					Square to;
					bb = ZERO_BB;

					switch (p)
					{
					case PAWN:
						
						
						bb = kingEffect(sq);
						bb = pawnEffect(c, bb);
						
						bb &= ALL_BB; 
						break;

					case LANCE:
						
						bb = lanceStepEffect(~c, sq);
						if (file_of(sq) != FILE_1)
							bb |= lanceStepEffect(~c, sq + SQ_R) | (sq + SQ_R);
						if (file_of(sq) != FILE_9)
							bb |= lanceStepEffect(~c, sq + SQ_L) | (sq + SQ_L);
						break;

					case KNIGHT:
						
						tmp = kingEffect(sq);
						while (tmp)
						{
							to = tmp.pop();
							bb |= knightEffect(~c, to);
						}
						break;

					case SILVER:
						
						tmp = kingEffect(sq);
						while (tmp)
						{
							to = tmp.pop();
							bb |= silverEffect(~c, to);
						}
						break;

					case GOLD:
						
						tmp = kingEffect(sq);
						while (tmp)
						{
							to = tmp.pop();
							bb |= goldEffect(~c, to);
						}
						break;

					case BISHOP:
						
						tmp = kingEffect(sq);
						while (tmp)
						{
							to = tmp.pop();
							bb |= bishopStepEffect(to);
						}
						break;

					case ROOK:
						
						tmp = kingEffect(sq);
						while (tmp)
						{
							to = tmp.pop();
							bb |= rookStepEffect(to);
						}
						break;

						
					case KING:
						
						tmp = kingEffect(sq);
						while (tmp)
						{
							to = tmp.pop();
							bb |= kingEffect(to);
						}
						break;

					default:
						UNREACHABLE;
					}

					bb &= ~Bitboard(sq); 
										 
					CHECK_AROUND_BB[sq][p - 1][c] = bb;
				}

		
		
		
		

		for (auto s1 : SQ)
			for (auto s2 : SQ)
			{
				Square next_sq = SQ_NB;

				
				if (queenStepEffect(s1) & s2)
				{
					File vf = File(sgn(file_of(s2) - file_of(s1)));
					Rank vr = Rank(sgn(rank_of(s2) - rank_of(s1)));

					File s3f = file_of(s2) + vf;
					Rank s3r = rank_of(s2) + vr;
					
					if (is_ok(s3f) && is_ok(s3r))
						next_sq = s3f | s3r;
				}
				NextSquare[s1][s2] = next_sq;
			}

	}

	
	static Bitboard RANK3_5BB = RANK3_BB | RANK4_BB | RANK5_BB;
	static Bitboard RANK5_7BB = RANK5_BB | RANK6_BB | RANK7_BB;

	
	
	


	
	
	template <Color Us>
	Bitboard AttacksSlider(const Position& pos, const Bitboard& slide)
	{
		Bitboard bb, sum = ZERO_BB;
		Square from;

		bb = pos.pieces(Us, LANCE);
		while (bb)
		{
			from = bb.pop();
			sum |= lanceEffect(Us, from, slide);
		}
		bb = pos.pieces(Us, BISHOP_HORSE);
		while (bb)
		{
			from = bb.pop();
			sum |= bishopEffect(from, slide);
		}
		bb = pos.pieces(Us, ROOK_DRAGON);
		while (bb)
		{
			from = bb.pop();
			sum |= rookEffect(from, slide);
		}
		return sum;
	}

	
	
	template <Color Us>
	Bitboard AttacksSlider(const Position& pos, Square avoid_from, const Bitboard& occ)
	{
		Bitboard bb, sum = ZERO_BB;
		Bitboard avoid_bb = ~Bitboard(avoid_from);
		Square from;

		bb = pos.pieces(Us, LANCE) & avoid_bb;
		while (bb)
		{
			from = bb.pop();
			sum |= lanceEffect(Us, from, occ);
		}
		bb = pos.pieces(Us, BISHOP_HORSE) & avoid_bb;
		while (bb)
		{
			from = bb.pop();
			sum |= bishopEffect(from, occ);
		}
		bb = pos.pieces(Us, ROOK_DRAGON) & avoid_bb;
		while (bb)
		{
			from = bb.pop();
			sum |= rookEffect(from, occ);
		}
		return sum;
	}

	
	template<Color ourKing>
	Bitboard AttacksAroundKingNonSlider(const Position& pos)
	{
		Square sq_king = pos.king_square(ourKing);
		Color them = ~ourKing;
		Square from;
		Bitboard bb;

		
		Bitboard sum = pawnEffect(them, pos.pieces(them, PAWN));

		
		bb = pos.pieces(them, KNIGHT) & check_around_bb(them, KNIGHT, sq_king);
		while (bb)
		{
			from = bb.pop();
			sum |= knightEffect(them, from);
		}
		bb = pos.pieces(them, SILVER) & check_around_bb(them, SILVER, sq_king);
		while (bb)
		{
			from = bb.pop();
			sum |= silverEffect(them, from);
		}
		bb = pos.pieces(them, GOLDS) & check_around_bb(them, GOLD, sq_king);
		while (bb)
		{
			from = bb.pop();
			sum |= goldEffect(them, from);
		}
		bb = pos.pieces(them, HDK) & check_around_bb(them, KING, sq_king);
		while (bb)
		{
			from = bb.pop();
			sum |= kingEffect(from);
		}
		return sum;
	}

	
	template <Color ourKing>
	Bitboard AttacksAroundKingSlider(const Position& pos)
	{
		Square sq_king = pos.king_square(ourKing);
		Color them = ~ourKing;
		Square from;
		Bitboard bb;
		Bitboard sum = ZERO_BB;

		bb = pos.pieces(them, LANCE) & check_around_bb(them, LANCE, sq_king);
		while (bb)
		{
			from = bb.pop();
			sum |= lanceEffect(them, from, pos.pieces());
		}
		bb = pos.pieces(them, BISHOP_HORSE) & check_around_bb(them, BISHOP, sq_king);
		while (bb)
		{
			from = bb.pop();
			sum |= bishopEffect(from, pos.pieces());
		}
		bb = pos.pieces(them, ROOK_DRAGON) & check_around_bb(them, ROOK, sq_king);
		while (bb)
		{
			from = bb.pop();
			sum |= rookEffect(from, pos.pieces());
		}
		return sum;
	}

	template <Color Us>
	Bitboard AttacksAroundKingNonSliderInAvoiding(const Position& pos, Square avoid_from)
	{
		Square sq_king = pos.king_square(Us);
		Color them = ~Us;
		Bitboard bb;
		Bitboard avoid_bb = ~Bitboard(avoid_from);
		Square from;

		
		Bitboard sum = pawnEffect(them, pos.pieces(them, PAWN));

		
		bb = pos.pieces(them, KNIGHT) & check_around_bb(them, KNIGHT, sq_king) & avoid_bb;
		while (bb)
		{
			from = bb.pop();
			sum |= knightEffect(them, from);
		}
		bb = pos.pieces(them, SILVER) & check_around_bb(them, SILVER, sq_king) & avoid_bb;
		while (bb)
		{
			from = bb.pop();
			sum |= silverEffect(them, from);
		}
		bb = pos.pieces(them, GOLDS) & check_around_bb(them, GOLD, sq_king) & avoid_bb;
		while (bb)
		{
			from = bb.pop();
			sum |= goldEffect(them, from);
		}
		bb = pos.pieces(them, HDK) & check_around_bb(them, KING, sq_king) & avoid_bb;
		while (bb)
		{
			from = bb.pop();
			sum |= kingEffect(from);
		}
		return sum;
	}


	
	
	
	
	
	template <Color Us>
	inline Bitboard AttacksAroundKingInAvoiding(const Position& pos, Square from, const Bitboard& occ)
	{
		return AttacksAroundKingNonSliderInAvoiding<Us>(pos, from) | AttacksSlider<~Us>(pos, from, occ);
	}

	
	
	template<Color Us>
	inline bool can_pawn_drop(const Position& pos, Square sq)
	{
		
		return hand_count(pos.hand_of(Us), PAWN) > 0 && !((pos.pieces(Us, PAWN) & FILE_BB[file_of(sq)]));
	}

}

using namespace Effect8;

namespace {

	
	
	bool can_king_escape(const Position& pos, Color us, Square to, const Bitboard& bb_avoid, const Bitboard& slide_)
	{
		
		
		Bitboard slide = slide_ | to;

		Square sq_king = pos.king_square(us);
		

		
		Bitboard bb = kingEffect(sq_king) & ~(bb_avoid | to | pos.pieces(us));

		while (bb)
		{
			Square escape = bb.pop();

			if (!pos.attackers_to(~us, escape, slide))
				return true;
			

		}
		return false;
	}

	
	
	
	bool can_king_escape(const Position& pos, Color us, Square from, Square to, const Bitboard& bb_avoid, const Bitboard& slide_)
	{
		
		Bitboard slide = slide_ | to;

		Square sq_king = pos.king_square(us);
		
		slide ^= sq_king;
		
		

		
		Bitboard bb = kingEffect(sq_king) & ~(bb_avoid | to | pos.pieces(us));

		while (bb)
		{
			Square escape = bb.pop();

			if (!(pos.attackers_to(~us, escape, slide) & ~Bitboard(from)))
				
				return true;
			

		}
		return false;
	}

	
	
	
	
	bool can_king_escape_cangoto(const Position& pos, Color us, Square from, Square to, const Bitboard& bb_avoid, const Bitboard& slide_)
	{
		
		Bitboard slide = slide_ | to;

		Square sq_king = pos.king_square(us);
		
		slide ^= sq_king;
		
		

		
		Bitboard bb = kingEffect(sq_king) & ~((bb_avoid  | pos.pieces(us)) & ~Bitboard(to));

		
		

		while (bb)
		{
			Square escape = bb.pop();

			if (!(pos.attackers_to(~us, escape, slide) & ~Bitboard(from)))
				
				return true;
			

		}
		return false;
	}

	
	bool can_piece_capture(const Position& pos, Color us, Square to, const Bitboard& pinned, const Bitboard& slide)
	{
		Square sq_king = pos.king_square(us);

		
		Bitboard sum = pos.attackers_to(us, to, slide) & ~pos.pieces(KING);
		while (sum)
		{
			Square from = sum.pop();

			
			if (!pinned
				|| !(pinned & from)
				|| aligned(from, to, sq_king)
				)
				return true;
		}

		return false;
	}

	
	
	bool can_piece_capture(const Position& pos, Color us, Square to, Square avoid, const Bitboard& pinned, const Bitboard& slide)
	{
		ASSERT_LV3(is_ok(to));

		Square sq_king = pos.king_square(us);

		
		Bitboard sum = pos.attackers_to(us, to, slide) & ~(pos.pieces(KING) | Bitboard(avoid));
		while (sum)
		{
			Square from = sum.pop();

			
			if (!pinned
				|| !(pinned & from)
				|| aligned(from, to, sq_king)
				)
				return true;
		}

		return false;
	}

}

namespace Mate {

	
	void init_mate_1ply()
	{
		
		init_check_bb();
	}

	
	template <Color Us>
	Move mate_1ply_imp(const Position& pos)
	{
		ASSERT_LV3(!pos.checkers());

		Color them = ~Us;
		Square sq_king = pos.king_square(them);

		
		Bitboard dcCandidates = pos.blockers_for_king(them) & pos.pieces(Us);

		
		Bitboard pinned = pos.blockers_for_king(them) & pos.pieces(them);

		Square from, to;

		

		
		Bitboard bb_drop = ~pos.pieces();

		
		Bitboard bb;

		
		Bitboard bb_attacks;

		Hand ourHand = pos.hand_of(Us);
		Hand themHand = pos.hand_of(them);

		
		if (hand_count(ourHand, ROOK))
		{
			
			bb = rookStepEffect(sq_king) & kingEffect(sq_king) & bb_drop;

			while (bb)
			{
				to = bb.pop();
				
				if (!pos.attackers_to(Us, to))
					continue;

				
				bb_attacks = rookStepEffect(to);

				if (can_king_escape(pos, them, to, bb_attacks, pos.pieces())) { continue; }
				if (can_piece_capture(pos, them, to, pinned, pos.pieces())) { continue; }
				return make_move_drop(ROOK, to , Us);
			}
		}

		
		if (hand_count(ourHand, LANCE))
		{
			bb = pawnEffect(them, sq_king) & bb_drop;
			if (bb)
			{
				to = bb.pop();
				if (pos.attackers_to(Us, to))
				{
					bb_attacks = lanceStepEffect(Us, to);
					if (can_king_escape(pos, them, to, bb_attacks, pos.pieces())) { goto SKIP_LANCE; }
					if (can_piece_capture(pos, them, to, pinned, pos.pieces())) { goto SKIP_LANCE; }
					return make_move_drop(LANCE, to, Us);

				SKIP_LANCE:;
				}
			}
		}

		
		if (hand_count(ourHand, BISHOP))
		{
			
			bb = cross45StepEffect(sq_king) & bb_drop;

			while (bb)
			{
				to = bb.pop();
				
				if (!pos.attackers_to(Us, to))
					continue;

				
				bb_attacks = bishopStepEffect(to);

				if (can_king_escape(pos, them, to, bb_attacks, pos.pieces())) { continue; }
				if (can_piece_capture(pos, them, to, pinned, pos.pieces())) { continue; }
				return make_move_drop(BISHOP, to , Us);
			}
		}

		
		if (hand_count(ourHand, GOLD))
		{
			bb = goldEffect(them, sq_king) & bb_drop;

			
			
			if (hand_count(ourHand, ROOK))
				bb &= ~pawnEffect(Us, sq_king);

			while (bb)
			{
				to = bb.pop();
				
				if (!pos.attackers_to(Us, to))
					continue;

				bb_attacks = goldEffect(Us, to);

				if (can_king_escape(pos, them, to, bb_attacks, pos.pieces())) { continue; }
				if (can_piece_capture(pos, them, to, pinned, pos.pieces())) { continue; }
				return make_move_drop(GOLD, to , Us);
			}
		}
		
		if (hand_count(ourHand, SILVER))
		{
			
			if (hand_count(ourHand, GOLD))
			{
				
				if (hand_count(ourHand, BISHOP))
					goto SILVER_DROP_END;

				
				bb = silverEffect(them, sq_king) & ~goldEffect(them, sq_king) & bb_drop;
			}
			else {
				bb = silverEffect(them, sq_king) & bb_drop;
			}
			while (bb)
			{
				to = bb.pop();
				
				if (!pos.attackers_to(Us, to))
					continue;

				bb_attacks = silverEffect(Us, to);

				if (can_king_escape(pos, them, to, bb_attacks, pos.pieces())) { continue; }
				if (can_piece_capture(pos, them, to, pinned, pos.pieces())) { continue; }
				return make_move_drop(SILVER, to , Us);
			}
		}
	SILVER_DROP_END:;

		
		if (hand_count(ourHand, KNIGHT))
		{
			bb = knightEffect(them, sq_king) & bb_drop;

			while (bb)
			{
				to = bb.pop();
				
				

				if (can_king_escape(pos, them, to, ZERO_BB, pos.pieces())) { continue; }
				if (can_piece_capture(pos, them, to, pinned, pos.pieces())) { continue; }
				return make_move_drop(KNIGHT, to , Us);
			}
		}

		

		
		Bitboard bb_move = ~pos.pieces(Us);

		
		Bitboard bb_check;

		
		Bitboard our_pinned = pos.blockers_for_king(Us) & pos.pieces(Us);

		
		Square our_king = pos.king_square(Us);

		
		bb = pos.pieces(Us, DRAGON);
		while (bb)
		{
			from = bb.pop();
			Bitboard slide = pos.pieces() ^ from;
			bb_check = dragonEffect(from, slide) & bb_move & kingEffect(sq_king);
			

			
			

			
			
			
			
			

			Bitboard new_pin = pos.pinned_pieces(them, from);
			
			

			while (bb_check)
			{
				to = bb_check.pop();
				
				if (!(pos.attackers_to(Us, to, slide) ^ from)) { continue; }

				

				
				if (pos.discovered(from, to, our_king, our_pinned)) { continue; }

				
				if (cross45StepEffect(sq_king) & to)
					bb_attacks = dragonEffect(to, slide);
				else
					bb_attacks = rookStepEffect(to) | kingEffect(to);

				if (can_king_escape(pos, them, from, to, bb_attacks, slide)) { continue; }

				
				if (can_piece_capture(pos, them, to, new_pin, slide)) { continue; }
				return make_move(from, to , Us , DRAGON);
			}
		}

		
		bb = pos.pieces(Us, ROOK);
		while (bb)
		{
			from = bb.pop();
			Bitboard slide = pos.pieces() ^ from;
			bb_check = rookEffect(from, slide) & bb_move & kingEffect(sq_king);

			
			

			
			
			
			
			

			Bitboard new_pin = pos.pinned_pieces(them, from);

			while (bb_check)
			{
				to = bb_check.pop();
				if (!(pos.attackers_to(Us, to, slide) ^ from)) { continue; }

				
				if (canPromote(Us, from, to))
				{
					if (cross45StepEffect(sq_king) & to)
						bb_attacks = dragonEffect(to, slide);
					else
						bb_attacks = rookStepEffect(to) | kingEffect(to);
				}
				else
					bb_attacks = rookStepEffect(to);

				
				if (!(bb_attacks & sq_king)) { continue; }

				if (pos.discovered(from, to, our_king, our_pinned)) { continue; }
				if (can_king_escape(pos, them, from, to, bb_attacks, slide)) { continue; }

				
				if (dcCandidates & from)
					;
				else if (can_piece_capture(pos, them, to, new_pin, slide)) { continue; }

				if (!canPromote(Us, from, to))
					return make_move(from, to ,Us, ROOK);
				else
					return make_move_promote(from, to, Us, ROOK);
			}
		}

		

		
		bb = pos.pieces(Us, HORSE);
		while (bb)
		{
			from = bb.pop();
			Bitboard slide = pos.pieces() ^ from;
			bb_check = horseEffect(from, slide) & bb_move & kingEffect(sq_king);
			

			
			
			
			
			

			

			
			
			
			
			

			Bitboard new_pin = pos.pinned_pieces(them, from);

			while (bb_check)
			{
				to = bb_check.pop();
				if (!(pos.attackers_to(Us, to, slide) ^ from)) { continue; }

				

				if (pos.discovered(from, to, our_king, our_pinned)) { continue; }
				bb_attacks = bishopStepEffect(to) | kingEffect(to);
				if (can_king_escape(pos, them, from, to, bb_attacks, slide)) { continue; }
				
				if ((dcCandidates & from) && !aligned(from, to, sq_king))
					;
				else if (can_piece_capture(pos, them, to, new_pin, slide)) { continue; }

				return make_move(from, to , Us, HORSE);
			}
		}

		
		bb = pos.pieces(Us, BISHOP);
		while (bb)
		{
			from = bb.pop();
			Bitboard slide = pos.pieces() ^ from;
			bb_check = bishopEffect(from, slide) & bb_move & kingEffect(sq_king);
			Bitboard new_pin = pos.pinned_pieces(them, from);

			while (bb_check)
			{
				to = bb_check.pop();
				if (!(pos.attackers_to(Us, to, slide) ^ from)) { continue; }

				
				if (canPromote(Us, from, to))
					bb_attacks = bishopStepEffect(to) | kingEffect(to);
				else
					bb_attacks = bishopStepEffect(to);

				
				if (!(bb_attacks & sq_king)) { continue; }
				if (pos.discovered(from, to, pos.king_square(Us), our_pinned)) { continue; }
				if (can_king_escape(pos, them, from, to, bb_attacks, slide)) { continue; }
				
				if (dcCandidates & from)
					;
				else if (can_piece_capture(pos, them, to, new_pin, slide)) { continue; }

				if (!canPromote(Us, from, to))
					return make_move(from, to , Us , BISHOP);
				else
					return make_move_promote(from, to , Us , BISHOP);
			}
		}

		
		bb = check_cand_bb(Us, PIECE_TYPE_CHECK_LANCE, sq_king) & pos.pieces(Us, LANCE);
		while (bb)
		{
			from = bb.pop();
			Bitboard slide = pos.pieces() ^ from;
			bb_attacks = lanceEffect(Us, from, slide);

			
			bb_check = bb_attacks & bb_move & goldEffect(them, sq_king);

			

			while (bb_check)
			{
				to = bb_check.pop();

				if (canPromote(Us, to))
					bb_attacks = goldEffect(Us, to);
				else
					bb_attacks = lanceStepEffect(Us, to);

				if (!(bb_attacks & sq_king)) { goto LANCE_NO_PRO; }
				if (!(pos.attackers_to(Us, to, slide) ^ from)) { goto LANCE_NO_PRO; }
				if (pos.discovered(from, to, our_king, our_pinned)) { goto LANCE_NO_PRO; }
				if (can_king_escape(pos, them, from, to, bb_attacks, slide)) { goto LANCE_NO_PRO; }
				
				if (dcCandidates & from)
					;
				else if (can_piece_capture(pos, them, to, pinned, slide)) { goto LANCE_NO_PRO; }

				if (!canPromote(Us, to))
					return make_move(from, to , Us , LANCE);
				else
					return make_move_promote(from, to , Us , LANCE);

				
			LANCE_NO_PRO:;
				if ((Us == BLACK ? RANK3_BB : RANK7_BB) & to)
				{
					bb_attacks = lanceStepEffect(Us, to);
					if (!(bb_attacks & sq_king)) { continue; }
					if (!(pos.attackers_to(Us, to, slide) ^ from)) { continue; }
					if (pos.discovered(from, to, our_king, our_pinned)) { continue; }
					if (can_king_escape(pos, them, from, to, bb_attacks, slide)) { continue; }
					
					if (can_piece_capture(pos, them, to, pinned, slide)) { continue; }
					return make_move(from, to , Us , LANCE);
				}
			}
		}

		
		
	#if 0

		
		

		
		
		
		
		

		

		
		

		if (hand_count(themHand, PAWN) == (int)themHand)
		{
			
			Bitboard bb_king_movable = ~pos.pieces(them) & kingEffect(sq_king);

			
			
			Bitboard aakns = AttacksAroundKingNonSlider<~Us>(pos); 
			Bitboard aaks = AttacksAroundKingSlider<~Us>(pos);
			Bitboard aak = aakns | aaks;

			Bitboard escape_bb = bb_king_movable & ~aak; 

														 
														 

			int esc_count = escape_bb.pop_count();
			if (esc_count >= 4)
				goto NEXT1; 
							

							
							
			Bitboard bb2 = ~pos.pieces() & kingEffect(sq_king);

			
			
			
			if (esc_count == 2)
			{
				
				bb = escape_bb;
				from = bb.pop();
				to = bb.pop();
				
			}

			while (bb2)
			{
				
				Square one = bb2.pop();

				
				
				
				
				

				
				
				

				
				
				to = nextSquare(sq_king, one);
				if (to == SQ_NB) continue; 

				
				if (pos.piece_on(to) != NO_PIECE && color_of(pos.piece_on(to)) == Us) continue;

				
				if (can_pawn_drop<~Us>(pos, one)) continue; 

				
				
				if (type_of(pos.piece_on(to)) == PAWN && file_of(to) == file_of(one) && hand_count(themHand, PAWN) >= 1) continue;

				auto dr = directions_of(sq_king, one);
				PieceType pt;
				bool canLanceAttack = false;
				if (dr & DIRECTIONS_DIAG)
				{
					pt = BISHOP;

					
					if (hand_count(ourHand, BISHOP) == 0)
						goto NEXT2;
				}
				else {
					pt = ROOK;

					
					
					canLanceAttack = (Us == BLACK ? dr == DIRECTIONS_D : dr == DIRECTIONS_U);
					if (canLanceAttack && hand_count(ourHand, LANCE) >= 1)
					{
						pt = LANCE;
					}
					else if (hand_count(ourHand, ROOK) == 0)
						goto NEXT2;
				}

				if (pos.piece_on(to)) goto NEXT2;
				

				
				if (can_piece_capture(pos, them, one, pinned, pos.pieces())) goto NEXT2;

				
				if (can_piece_capture(pos, them, to, pinned, pos.pieces())) goto NEXT2;

				
				

				
				
				
				escape_bb = bb_king_movable & ~(aakns | AttacksSlider<Us>(pos, pos.pieces() | to));

				if (dr & DIRECTIONS_DIAG) 
				{
					if (!(~bishopStepEffect(to) & escape_bb))
						return make_move_drop(pt, to , Us);
				}
				else 
				{
					
					if (!(~rookStepEffect(to) & escape_bb))
						return make_move_drop(pt, to , Us);
				}

				
				
				if (esc_count <= 2)
				{
					Square nextTo = nextSquare(one, to);
					if (nextTo == SQ_NB) goto NEXT2;
					if (pos.piece_on(nextTo)) goto NEXT2;
					if (can_pawn_drop<~Us>(pos, to)) goto NEXT2;
					if (can_piece_capture(pos, them, nextTo, pinned, pos.pieces())) goto NEXT2;

					escape_bb = bb_king_movable & ~(aakns | AttacksSlider<Us>(pos, pos.pieces() | nextTo));

					if (dr & DIRECTIONS_DIAG) 
					{
						if (!(~bishopStepEffect(nextTo) & escape_bb))
							return make_move_drop(pt, nextTo, Us);
					}
					else 
					{
						if (!(~rookStepEffect(nextTo) & escape_bb))
							return make_move_drop(pt, nextTo, Us);
					}
				}

			NEXT2:;
				

				if (!(dr & DIRECTIONS_DIAG)) 
				{
					
					
					bool is_rook = rookStepEffect(to) & pos.pieces(Us, ROOK_DRAGON);
					bool is_dragon = kingEffect(to) & pos.pieces(Us, DRAGON);
					bool is_lance = (canLanceAttack) ? (lanceStepEffect(them, to) & pos.pieces(Us, LANCE)) : false;

					if (is_rook || is_dragon || is_lance)
					{
						
						bb = ZERO_BB;
						if (is_rook)
							bb = rookEffect(to, pos.pieces()) & pos.pieces(Us, ROOK_DRAGON);
						if (is_dragon)
							bb |= kingEffect(to) & pos.pieces(Us, DRAGON);
						if (is_lance)
							bb |= lanceEffect(them, to, pos.pieces()) & pos.pieces(Us, LANCE);

						while (bb)
						{
							from = bb.pop();
							

							
							if (pos.discovered(from, to, our_king, our_pinned)) continue;

							Bitboard slide = pos.pieces() ^ from;

							
							if (can_piece_capture(pos, them, to, pinned, slide)) continue;
							
							
							
							
							if (can_piece_capture(pos, them, one, to, pinned, slide)) continue;

							if (type_of(pos.piece_on(from)) == LANCE)
							{
								bb_attacks = rookStepEffect(to);
								
								
							}
							else if (canPromote(Us, from, to) || type_of(pos.piece_on(from)) == DRAGON)
							{
								bb_attacks = queenStepEffect(to);
							}
							else
								bb_attacks = rookStepEffect(to);
							

							Bitboard new_slide = (pos.pieces() ^ from) | to;

							
							

							if (!(kingEffect(sq_king)
								& ~(pos.pieces(them) | AttacksAroundKingInAvoiding<~Us>(pos, from, new_slide) | bb_attacks)))
							{
								
								PieceType pt = type_of(pos.piece_on(from));
								
								if (pt != LANCE && canPromote(Us, from, to) && !(pt & PIECE_PROMOTE))
									return make_move_promote(from, to, Us , pt);
								else
									return make_move(from, to, Us, pt);
							}
						}
					}
				}
				else {
					

					bool is_bishop = bishopStepEffect(to) & pos.pieces(Us, BISHOP_HORSE);
					bool is_horse = kingEffect(to) & pos.pieces(Us, HORSE);
					if (is_bishop || is_horse)
					{
						
						bb = ZERO_BB;
						if (is_bishop)
							bb = bishopEffect(to, pos.pieces()) & pos.pieces(Us, BISHOP_HORSE);
						if (is_horse)
							bb |= kingEffect(to) & pos.pieces(Us, HORSE);

						while (bb)
						{
							from = bb.pop();
							

							
							if (pos.discovered(from, to, our_king, our_pinned)) continue;

							Bitboard slide = pos.pieces() ^ from;
							
							
							if (can_piece_capture(pos, them, one, to, pinned, slide)) continue;

							
							if (can_piece_capture(pos, them, to, pinned, slide)) continue;

							
							
							
							
							

							
							

							Bitboard new_slide = (pos.pieces() ^ from) | to;

							if (!(kingEffect(sq_king)
								&  ~(pos.pieces(them) | AttacksAroundKingInAvoiding<~Us>(pos, from, new_slide) | queenStepEffect(to)
									)))
								
							{
								
								Piece pc = pos.piece_on(from);
								if (canPromote(Us, from, to) && !(pc & PIECE_PROMOTE))
									return make_move_promote(from, to , pc);
								else
									return make_move(from, to , pc);
							}
						}
					}
				}
				

			}
		}
	NEXT1:;
	#endif

		
		

		bb = check_cand_bb(Us, PIECE_TYPE_CHECK_NON_SLIDER, sq_king)
			& (pos.pieces(Us, GOLDS, SILVER, KNIGHT, PAWN));
		if (!bb)
			goto DC_CHECK;

		
		bb = check_cand_bb(Us, PIECE_TYPE_CHECK_GOLD, sq_king)  & pos.pieces(Us, GOLDS);
		while (bb)
		{
			from = bb.pop();
			bb_check = goldEffect(Us, from) & bb_move & goldEffect(them, sq_king);
			

			if (!bb_check) { continue; }

			Bitboard slide = pos.pieces() ^ from;
			Bitboard new_pin = pos.pinned_pieces(them, from);

			while (bb_check)
			{
				to = bb_check.pop();
				if (!(pos.attackers_to(Us, to, slide) ^ from)) { continue; }
				if (pos.discovered(from, to, our_king, our_pinned)) { continue; }
				bb_attacks = goldEffect(Us, to);
				if (can_king_escape(pos, them, from, to, bb_attacks, slide)) { continue; }
				if ((dcCandidates & from) && !aligned(from, to, sq_king))
					;
				else if (can_piece_capture(pos, them, to, new_pin, slide)) { continue; }
				return make_move(from, to , pos.piece_on(from) );
			}
		}


		
		bb = check_cand_bb(Us, PIECE_TYPE_CHECK_SILVER, sq_king)  & pos.pieces(Us, SILVER);
		while (bb)
		{
			from = bb.pop();
			bb_check = silverEffect(Us, from) & bb_move & kingEffect(sq_king);
			
			if (!bb_check) { continue; }

			Bitboard slide = pos.pieces() ^ from;
			Bitboard new_pin = pos.pinned_pieces(them, from);

			while (bb_check)
			{
				to = bb_check.pop();
				bb_attacks = silverEffect(Us, to);
				
				if (!(bb_attacks & sq_king)) { goto PRO_SILVER; }
				if (!(pos.attackers_to(Us, to, slide) ^ from)) { goto PRO_SILVER; }
				if (pos.discovered(from, to, our_king, our_pinned)) { goto PRO_SILVER; }
				if (can_king_escape(pos, them, from, to, bb_attacks, slide)) { goto PRO_SILVER; }
				if ((dcCandidates & from) && !aligned(from, to, sq_king))
					;
				else
					if (can_piece_capture(pos, them, to, new_pin, slide)) { goto PRO_SILVER; }
				
				
				

				return make_move(from, to, Us , SILVER );

			PRO_SILVER:;
				

				
				if (!(canPromote(Us, from, to)))
					continue;

				bb_attacks = goldEffect(Us, to);
				if (!(bb_attacks & sq_king)) { continue; }
				if (!(pos.attackers_to(Us, to, slide) ^ from)) { continue; }
				if (pos.discovered(from, to, our_king, our_pinned)) { continue; }
				if (can_king_escape(pos, them, from, to, bb_attacks, slide)) { continue; }
				if ((dcCandidates & from) && !aligned(from, to, sq_king))
					;
				else if (can_piece_capture(pos, them, to, new_pin, slide)) { continue; }
				return make_move_promote(from, to , Us , SILVER);
			}
		}

		
		bb = check_cand_bb(Us, PIECE_TYPE_CHECK_KNIGHT, sq_king)  & pos.pieces(Us, KNIGHT);
		while (bb)
		{
			from = bb.pop();
			bb_check = knightEffect(Us, from) & bb_move;
			if (!bb_check) { continue; }

			Bitboard slide = pos.pieces() ^ from;
			Bitboard new_pin = pos.pinned_pieces(them, from);

			while (bb_check)
			{
				to = bb_check.pop();
				bb_attacks = knightEffect(Us, to);
				
				if (!(bb_attacks & sq_king)) { goto PRO_KNIGHT; }
				
				
				

				if (pos.discovered(from, to, our_king, our_pinned)) { continue; }
				if (can_king_escape(pos, them, from, to, bb_attacks, slide)) { continue; }
				
				if (dcCandidates & from)
					;
				else if (can_piece_capture(pos, them, to, new_pin, slide)) { continue; }
				return make_move(from, to , Us , KNIGHT);

			PRO_KNIGHT:;
				

				if (!(canPromote(Us, from, to))) { continue; }
				bb_attacks = goldEffect(Us, to);
				if (!(bb_attacks & sq_king)) { continue; }
				if (!(pos.attackers_to(Us, to, slide) ^ from)) { continue; }
				if (pos.discovered(from, to, our_king, our_pinned)) { continue; }
				if (can_king_escape(pos, them, from, to, bb_attacks, slide)) { continue; }
				
				if (dcCandidates & from)
					;
				else if (can_piece_capture(pos, them, to, new_pin, slide)) { continue; }
				return make_move_promote(from, to , Us , KNIGHT);
			}
		}

		
		if (check_cand_bb(Us, PIECE_TYPE_CHECK_PAWN_WITH_NO_PRO, sq_king) & pos.pieces(Us, PAWN))
		{
			
			to = sq_king + (Us == BLACK ? SQ_D : SQ_U);
			if (pos.piece_on(to) != NO_PIECE && color_of(pos.piece_on(to)) != ~Us) { goto SKIP_PAWN; }
			from = to + (Us == BLACK ? SQ_D : SQ_U);

			
			if (canPromote(Us, to)) { goto SKIP_PAWN; }

			Bitboard slide = pos.pieces() ^ from;
			if (!(pos.attackers_to(Us, to, slide) ^ from)) { goto SKIP_PAWN; }
			if (pos.discovered(from, to, our_king, our_pinned)) { goto SKIP_PAWN; }
			if (can_king_escape(pos, them, from, to, ZERO_BB, slide)) { goto SKIP_PAWN; }
			
			if (can_piece_capture(pos, them, to, pinned, slide)) { goto SKIP_PAWN; }
			return make_move(from, to , Us , PAWN);
		}
	SKIP_PAWN:;

		
		bb = check_cand_bb(Us, PIECE_TYPE_CHECK_PAWN_WITH_PRO, sq_king) & pos.pieces(Us, PAWN);
		while (bb)
		{
			from = bb.pop();
			to = from + (Us == BLACK ? SQ_U : SQ_D);
			if (pos.piece_on(to) != NO_PIECE && color_of(pos.piece_on(to)) != ~Us) { continue; }
			bb_attacks = goldEffect(Us, to);

			if (!(bb_attacks & sq_king)) { continue; }

			Bitboard slide = pos.pieces() ^ from;
			if (!(pos.attackers_to(Us, to, slide) ^ from)) { continue; }
			if (pos.discovered(from, to, our_king, our_pinned)) { continue; }
			if (can_king_escape(pos, them, from, to, bb_attacks, slide)) { continue; }
			if (can_piece_capture(pos, them, to, pinned, slide)) { continue; }
			return make_move_promote(from, to , Us , PAWN);
		}

	DC_CHECK:;
#if 0
		
		if (dcCandidates)
		{
			
			

			
			Bitboard enemyBB = enemy_field(Us);

			auto bb_candidates = dcCandidates;

			while (bb_candidates)
			{
				from = bb_candidates.pop();
				PieceType pt = type_of(pos.piece_on(from));
				switch (pt)
				{
					
					

				case PAWN:
				{
					
					if (file_of(from) == file_of(sq_king)) { continue; }

					
					to = from + (Us == BLACK ? SQ_U : SQ_D);
					if (pos.piece_on(to) != NO_PIECE && color_of(pos.piece_on(to)) != ~Us) { continue; }

					
					if (!canPromote(Us, to)) continue;

					
					
					

					
					bb_attacks = goldEffect(Us, to);
					if (!(bb_attacks & sq_king)) continue;

					
					if (pos.discovered(from, to, our_king, our_pinned)) { continue; }

					
					Bitboard slide = pos.pieces() ^ from;
					if (can_king_escape_cangoto(pos, them, from, to, bb_attacks, slide)) { continue; }

					
					return make_move_promote(from, to , Us , PAWN);
				}
				ASSERT_LV3(false); 
				
				case LANCE:
					continue; 

				case KNIGHT:
					
					if (!(check_around_bb(Us, KNIGHT, sq_king) & from)) continue;

					bb = knightEffect(Us, from) &knightEffect(them, sq_king) & bb_move;
					while (bb)
					{
						to = bb.pop();
						if (aligned(from, to, sq_king)) { continue; }
						bb_attacks = knightEffect(Us, to);
						if (bb_attacks & sq_king) { continue; }
						if (pos.discovered(from, to, our_king, our_pinned)) { continue; }
						Bitboard slide = pos.pieces() ^ from;
						if (can_king_escape_cangoto(pos, them, from, to, bb_attacks, slide)) { continue; }
						return make_move(from, to , Us , KNIGHT);
					}

					bb = knightEffect(Us, from) &goldEffect(them, sq_king);
					while (bb)
					{
						to = bb.pop();
						if (aligned(from, to, sq_king)) { continue; }
						if (!(canPromote(Us, from, to))) { continue; }
						bb_attacks = goldEffect(Us, to);
						if (bb_attacks & sq_king) { continue; }
						if (pos.discovered(from, to, our_king, our_pinned)) { continue; }
						Bitboard slide = pos.pieces() ^ from;
						if (can_king_escape_cangoto(pos, them, from, to, bb_attacks, slide)) { continue; }
						return make_move_promote(from, to , Us, KNIGHT);
					}

					continue; 

				case SILVER:
					
					if (!(check_around_bb(Us, SILVER, sq_king) & from)) continue;
					

					bb = silverEffect(Us, from) & silverEffect(them, sq_king) & bb_move;;
					while (bb)
					{
						to = bb.pop();
						if (aligned(from, to, sq_king)) { continue; }
						bb_attacks = silverEffect(Us, to);
						if (bb_attacks & sq_king) { continue; }
						if (pos.discovered(from, to, our_king, our_pinned)) { continue; }
						Bitboard slide = pos.pieces() ^ from;
						if (can_king_escape_cangoto(pos, them, from, to, bb_attacks, slide)) { continue; }
						return make_move(from, to , Us, SILVER);
					}

					bb = silverEffect(Us, from) & goldEffect(them, sq_king) & bb_move;;
					while (bb)
					{
						to = bb.pop();
						if (aligned(from, to, sq_king)) { continue; }
						if (!(canPromote(Us, from, to))) { continue; }
						bb_attacks = goldEffect(Us, to);
						if (bb_attacks & sq_king) { continue; }
						if (pos.discovered(from, to, our_king, our_pinned)) { continue; }
						Bitboard slide = pos.pieces() ^ from;
						if (can_king_escape_cangoto(pos, them, from, to, bb_attacks, slide)) { continue; }
						return make_move_promote(from, to , Us , SILVER);
					}
					continue;

				case PRO_PAWN:
				case PRO_LANCE:
				case PRO_KNIGHT:
				case PRO_SILVER:
					pt = GOLD; 
					
				case GOLD:
					
					if (!(check_around_bb(Us, GOLD, sq_king) & from)) continue;

					
					bb = goldEffect(Us, from) & goldEffect(them, sq_king);
					
					break;

				case BISHOP:

					bb = bishopEffect(sq_king, pos.pieces()) |
						(kingEffect(sq_king) & (canPromote(Us, from) ? ALL_BB : enemyBB));
					
					
					
					bb &= bishopEffect(from, pos.pieces());

					

					break;

				case HORSE:
					bb = horseEffect(from, pos.pieces()) & horseEffect(sq_king, pos.pieces());

					

					break;

				case ROOK:
					
					bb = rookEffect(sq_king, pos.pieces()) |
						(kingEffect(sq_king) & (canPromote(Us, from) ? ALL_BB : enemyBB));
					bb &= rookEffect(from, pos.pieces());

					
					

					
					

					break;

				case DRAGON:

					bb = dragonEffect(from, pos.pieces()) & dragonEffect(sq_king, pos.pieces());

					

					break;

				default:
					ASSERT_LV3(pt == KING);
					continue;
				}

				bb &= bb_move;

				bool is_enemy_from = canPromote(Us, from);

				
				while (bb)
				{
					to = bb.pop();
					bool promo = is_enemy_from || canPromote(Us, to);

					
					if (aligned(from, to, sq_king)) { continue; }

					if (pos.discovered(from, to, our_king, our_pinned)) { continue; }

					
					
					

					Bitboard slide = pos.pieces() ^ from;
					switch (pt)
					{
					case SILVER:
						if (!promo) goto DC_SILVER_NO_PRO;
						bb_attacks = goldEffect(Us, to); break;
					case GOLD: bb_attacks = goldEffect(Us, to); break;
					case BISHOP:
						bb_attacks = bishopStepEffect(to);
						if (promo)
							bb_attacks |= kingEffect(to);
						break;
					case HORSE: bb_attacks = bishopStepEffect(to) | kingEffect(to); break;
					case ROOK:
						bb_attacks = rookStepEffect(to);
						if (promo)
							bb_attacks |= kingEffect(to);
						break;
					case DRAGON: bb_attacks = rookStepEffect(to) | kingEffect(to); break;
					default:
						ASSERT_LV3(false);
						bb_attacks = ZERO_BB;
					}

					if (!can_king_escape_cangoto(pos, them, from, to, bb_attacks, slide))
					{
						if (promo && !(pt & PIECE_PROMOTE) && pt != GOLD)
							return make_move_promote(from, to , Us , pt);
						return make_move(from, to , Us , pt);
					}

				DC_SILVER_NO_PRO:
					if (pt == SILVER)
					{
						
						

						bb_attacks = silverEffect(Us, to);
						if (!can_king_escape_cangoto(pos, them, from, to, bb_attacks, slide))
						{
							return make_move(from, to , Us , SILVER);
						}
					}
				}
			}
		}

		
		
		

		
		

		if (dcCandidates && hand_count(themHand, PAWN) == (int)themHand)
		{
			
			
			

			bb = dcCandidates & around24_bb(sq_king);

			while (bb)
			{
				from = bb.pop();

				
				
				
				
				

				Bitboard atk = pos.attackers_to(them, from) & ~Bitboard(sq_king);
				if (atk)
				{
					if (atk.pop_count() >= 2)
						continue; 

								  
								  
				}
				else {
					
					atk = around24_bb(sq_king) & bb_move; 
				}

				PieceType pt = type_of(pos.piece_on(from));
				switch ((int)pt) 
				{
				case PAWN:
				case LANCE:
					
					continue;

				case KNIGHT:
					
					if (!((Us == BLACK ? RANK3_5BB : RANK5_7BB) & from))
						continue;

					bb_attacks = knightEffect(Us, from) & ~goldEffect(them, sq_king);
					break;

				case SILVER:
					bb_attacks = silverEffect(Us, from);
					
					break;

				case PRO_PAWN:
				case PRO_LANCE:
				case PRO_KNIGHT:
				case PRO_SILVER:
				case GOLD:

					bb_attacks = goldEffect(Us, from) & ~goldEffect(them, sq_king);
					pt = GOLD;
					break;

				case BISHOP:
					bb_attacks = bishopEffect(from, pos.pieces());
					break;

				case HORSE:
					bb_attacks = horseEffect(from, pos.pieces()) & ~kingEffect(sq_king);
					
					break;

				case ROOK:
					bb_attacks = rookEffect(from, pos.pieces());
					break;

				case DRAGON:
					bb_attacks = dragonEffect(from, pos.pieces()) & ~kingEffect(sq_king);
					break;

					
					
				case KING:
					continue;
				}

				
				
				
				

				
				Bitboard bb2 = bb_attacks & atk;

				Bitboard slide = pos.pieces() ^ from;
				while (bb2)
				{
					to = bb2.pop();

					
					if (aligned(from, to, sq_king))
						continue;

					
					if (pos.discovered(from, to, our_king, our_pinned))
						continue;

					
					if (can_pawn_drop<~Us>(pos, from))
						continue;

					
					
					bool capPawn = type_of(pos.piece_on(to)) == PAWN;
					if (capPawn && file_of(from) == file_of(to))
						continue;

					Bitboard new_slide = slide | to;

					Bitboard new_pinned = pos.pinned_pieces(them, from, to);

					
					
					
					if (can_piece_capture(pos, them, from, to, new_pinned, new_slide))
						continue;

					
					if (kingEffect(sq_king) & to)
					{
						
						if (!(pos.attackers_to(Us, to) ^ from))
							continue;
					}

					
					switch (pt)
					{

					case SILVER:
						

						
						if (!canPromote(Us, from, to))
							goto DISCOVER_ATTACK_CONTINUE_SILVER;
						bb_attacks = goldEffect(Us, to);
						break;

					case KNIGHT:
						
					case GOLD:
						bb_attacks = goldEffect(Us, to);
						break;

					case BISHOP:
						if (canPromote(Us, from, to))
							bb_attacks = horseEffect(to, new_slide);
						else
							bb_attacks = bishopEffect(to, new_slide);
						break;

					case HORSE:
						bb_attacks = horseEffect(to, new_slide);
						break;

					case ROOK:
						if (canPromote(Us, from, to))
							bb_attacks = dragonEffect(to, new_slide);
						else
							bb_attacks = rookEffect(to, new_slide);
						break;

					case DRAGON:
						bb_attacks = dragonEffect(to, new_slide);
						break;

					default:
						ASSERT_LV3(false);
						continue;
					}

					
					if (bb_attacks & sq_king)
						goto DISCOVER_ATTACK_CONTINUE_SILVER;

					
					
					if (kingEffect(sq_king)
						& ~(bb_attacks | AttacksAroundKingInAvoiding<~Us>(pos, from, new_slide ^ sq_king) | pos.pieces(them)))
						goto DISCOVER_ATTACK_CONTINUE_SILVER;

					
					

					


					
					
					

					{
						Square s1 = sq_king;
						Square s2 = s1 + (nextSquare(s1, from) - from);
						do
						{
							

							
							
							if (can_piece_capture(pos, them, s2, to, new_pinned, new_slide))
								goto DISCOVER_ATTACK_CONTINUE_SILVER;

							
							
							if (s2 != from && pos.piece_on(s2)) 
								break;

							
							if (can_pawn_drop<~Us>(pos, s2) || (capPawn && file_of(s2) == file_of(to)))
								goto DISCOVER_ATTACK_CONTINUE_SILVER;

							Square s3 = nextSquare(s1, s2);
							s1 = s2;
							s2 = s3;
						} while (s2 != SQ_NB);

						
						
						
						if (canPromote(Us, from, to) && !(pos.piece_on(from) & PIECE_PROMOTE) && pt != GOLD)
							return make_move_promote(from, to , Us , pt);
						else
							return make_move(from, to , Us , pt);
					}

				DISCOVER_ATTACK_CONTINUE_SILVER:;

					if (pt == SILVER)
					{
						
						
						bb_attacks = silverEffect(Us, to);
						if (bb_attacks & sq_king)
							goto DISCOVER_ATTACK_CONTINUE;

						if (kingEffect(sq_king)
							& ~(bb_attacks | AttacksAroundKingInAvoiding<~Us>(pos, from, new_slide ^ sq_king) | pos.pieces(them)))
							goto DISCOVER_ATTACK_CONTINUE;

						Square s1 = sq_king;
						Square s2 = s1 + (nextSquare(s1, from) - from);
						do
						{
							if (can_piece_capture(pos, them, s2, to, new_pinned, new_slide))
								goto DISCOVER_ATTACK_CONTINUE;
							if (s2 != from && pos.piece_on(s2))
								break;
							if (can_pawn_drop<~Us>(pos, s2) || (capPawn && file_of(s2) == file_of(to)))
								goto DISCOVER_ATTACK_CONTINUE;
							Square s3 = nextSquare(s1, s2);
							s1 = s2;
							s2 = s3;
						} while (s2 != SQ_NB);
						return make_move(from, to , Us , SILVER);
					}

				DISCOVER_ATTACK_CONTINUE:;

				}
			}
		}
#endif

		
		
		
		

		return MOVE_NONE;
	}

	
	Move mate_1ply(const Position& pos)
	{
		return pos.side_to_move() == BLACK ? Mate::mate_1ply_imp<BLACK>(pos) : Mate::mate_1ply_imp<WHITE>(pos);
	}

} 






#endif 
