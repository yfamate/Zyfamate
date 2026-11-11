#ifndef __MATE_MOVE_PICKER_H_INCLUDED__
#define __MATE_MOVE_PICKER_H_INCLUDED__

#include "../config.h"

#if defined(USE_MATE_SOLVER) || defined(USE_MATE_DFPN)

#include "../position.h"
#include "../evaluate.h" 

namespace Mate
{


	const constexpr size_t MaxCheckMoves = 100;

	template <bool or_node, bool INCHECK , bool GEN_ALL , bool MoveOrdering>
	class MovePicker {
	public:
		explicit MovePicker(const Position& pos)
		{
			bool doGivesCheck = false;
			if (or_node) {


				if (INCHECK)
				{
					last = GEN_ALL ? generateMoves<EVASIONS_ALL>(pos, moveList) : generateMoves<EVASIONS>(pos, moveList);
					doGivesCheck = true;
				}
				else {
					last = GEN_ALL ? generateMoves<CHECKS_ALL>(pos, moveList) : generateMoves<CHECKS>(pos, moveList);
				}
			}
			else {
				last = GEN_ALL ? generateMoves<EVASIONS_ALL>(pos, moveList): generateMoves<EVASIONS>(pos, moveList);
			}

			last = std::remove_if(moveList, last, [&](const auto& ml) {
				return (doGivesCheck && !pos.gives_check(ml.move)) || !pos.legal(ml.move);
			});

			if (MoveOrdering)
			{
#if !defined (USE_PIECE_VALUE)
				std::cout << "Error! : define USE_PIECE_VALUE" << std::endl;
#else


				int value = pos.state()->materialValue;

				auto us = pos.side_to_move();

				Color king_color = ((us == BLACK) ^ or_node) ? BLACK : WHITE;
				auto ksq = pos.king_square(king_color);

				constexpr int king_rank_bonus[COLOR_NB][10] = {
					{ -600,-300,-200,  0,  0,  0,   0,  50, 100,0 },
					{  100,  50,   0,  0,  0,  0,-200,-300,-600,0 }
				};
				int king_bonus = king_rank_bonus[king_color][rank_of(ksq)];

#if 0

				constexpr int hand_value[8] = { 0,1,2,2,5,4,4,6 };

				auto our_hand = pos.hand_of(us);
				int our_hand_value =
					hand_count(our_hand, PAWN) * hand_value[1]
					+ hand_count(our_hand, LANCE) * hand_value[2]
					+ hand_count(our_hand, KNIGHT) * hand_value[3]
					+ hand_count(our_hand, SILVER) * hand_value[4]
					+ hand_count(our_hand, BISHOP) * hand_value[5]
					+ hand_count(our_hand, ROOK) * hand_value[6]
					+ hand_count(our_hand, GOLD) * hand_value[7];

				auto their_hand = pos.hand_of(~us);
				int there_hand_value =
					hand_count(their_hand, PAWN) * hand_value[1]
					+ hand_count(their_hand, LANCE) * hand_value[2]
					+ hand_count(their_hand, KNIGHT) * hand_value[3]
					+ hand_count(their_hand, SILVER) * hand_value[4]
					+ hand_count(their_hand, BISHOP) * hand_value[5]
					+ hand_count(their_hand, ROOK) * hand_value[6]
					+ hand_count(their_hand, GOLD) * hand_value[7];
#endif

				Square lastTo = SQ_NB;
				int our_effect = 0 , their_effect = 0;

				for (auto& m : *this)
				{
					Move move = m.move;

					auto to = to_sq(move);
					auto cap = pos.piece_on(to);
					int v = value + (cap == NO_PIECE ? 0 : - Eval::PieceValue[cap]*2);

					auto pc = pos.moved_piece_after(move);

					if (is_promote(move))
						v += Eval::PieceValue[pc] - Eval::PieceValue[raw_of(pc)];

#if 1


					bool drop = is_drop(move);

					if (lastTo != to)
					{
						lastTo = to;

						our_effect = pos.attackers_to(us, to).pop_count();
						their_effect = pos.attackers_to(~us, to).pop_count();
					}

					if (  ( drop && our_effect     < their_effect)
						||(!drop && our_effect - 1 < their_effect))
					{
						v -= Eval::PieceValue[pc] + Eval::PieceValue[raw_of(pc)]; 
					}
#endif

					if (or_node ^ (us == BLACK))
						v = -v;


					if (or_node)
						v += -dist(ksq, to);
					else
						v += +dist(ksq, to);


					v += king_bonus;

					m.value = v;
				}

#if 0
				std::cout << pos << std::endl;
				for (auto& m : *this)
				{
					std::cout << m.move << " , " << m.value << std::endl;
				}
#endif
#endif
			}

			ASSERT_LV3(size() <= MaxCheckMoves);
		}

		size_t size() const { return static_cast<size_t>(last - moveList); }
		ExtMove* begin() { return &moveList[0]; }
		ExtMove* end() { return last; }
		bool empty() const { return size() == 0; }

	private:
		ExtMove moveList[MaxCheckMoves];
		ExtMove* last;
	};
}
#endif


#endif 
