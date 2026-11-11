#include "../types.h"
#include "../position.h"
#include "../evaluate.h"
#include "../misc.h"
#include <map>

namespace Eval
{
#if !defined(USE_EVAL)

	void init() {}
	Value compute_eval(const Position& pos) { return VALUE_ZERO; }
	void evaluate_with_no_return(const Position& pos) {}
	void print_eval_stat(Position& pos) {}
	void load_eval() {}
	Value evaluate(const Position& pos) { return VALUE_ZERO; }

#endif

#if defined(USE_PIECE_VALUE)

	Value material(const Position& pos)
	{
		int v = VALUE_ZERO;

		for (auto i : SQ)
			v = v + PieceValue[pos.piece_on(i)];

		for (auto c : COLOR)
			for (auto pc = PAWN; pc < PIECE_HAND_NB; ++pc)
				v += (c == BLACK ? 1 : -1) * Value(hand_count(pos.hand_of(c), pc) * PieceValue[pc]);

		return (Value)v;
	}
#endif

#if defined(EVAL_KPPT) || defined(EVAL_KPP_KKPT)

	void print_softname(u64 check_sum)
	{
		std::string softname = "unknown";

		std::map<u64, std::string> list = {
			{ 0x0000d6799926233 , "ShinYane(20161010)"    } , 
			{ 0x0000da0f36d1b4b , "Apery(WCSC26)"         } , 
			{ 0x0000d7213c45779 , "Ukamuse(sdt4)"         } , 

			{ 0x0000c9e81cef72b , "elmo(WCSC27)"          } , 
			{ 0x0000900f1fbe7a6 , "Yomita(WCSC27)"        } , 
			{ 0x0000d5214c4e6d8 , "Qhapaq(WCSC27)"        } , 
			{ 0x0000d291a9942bb , "tanuki(WCSC27)"        } , 

			{ 0x000000000000000 , "rezero epoch0"         } , 
			{ 0x00008b66a4ecea9 , "rezero epoch0.1"       } , 
			{ 0x000093a87cc91ad , "rezero epoch1"         } , 
			{ 0x000096a5706b9ec , "rezero epoch2"         } , 
			{ 0x000096b3df75de8 , "rezero epoch3"         } , 
			{ 0x0000964a98dc84b , "rezero epoch4"         } , 
			{ 0x000095f8443a079 , "rezero epoch5"         } , 
			{ 0x0000953fbdc48d6 , "rezero epoch6"         } , 
			{ 0x0000a4896775602 , "rezero epoch7"         } , 
			{ 0x0000a516345897e , "rezero epoch8"         } , 
			{ 0x0000a6eea7db958 , "yaseiyomita(20170626)" } , 
			{ 0x0000a6fbc5087ce , "yaseiyomita(20170703)" } , 
			{ 0x0000a63c328ca42 , "yaseiyomita(20170802)" } , 
			{ 0x0000c8e89414b88 , "elmoqhapaq ver1.0"     } , 
			{ 0x0000c8ac0b10c78 , "elmoqhapaq ver1.1"     } , 

			{ 0x0000c62cf4f6b91 , "relmo8"                } , 
			{ 0x0000c85005252ee , "yaselmo(20170705)"     } , 

			{ 0x0000574a685513c , "kppkkpt rezero epoch4" } , 

		};

		if (list.count(check_sum))
			softname = list[check_sum];

		sync_cout << "info string Eval Check Sum = " << std::hex << check_sum << std::dec
			<< " , Eval File = " << softname << sync_endl;
	}
#endif

#if defined(USE_EVAL_LIST)
	bool EvalList::is_valid(const Position& pos)
	{
		int hand_max[KING] = { 0,18,4,4,4,2,2,4 };

		for (int i = 0; i < length() ; ++i)
		{
			BonaPiece fb = pieceListFb[i];

			if (!( 0 <= fb && fb < fe_end))
				return false;

			if (fb < fe_hand_end)
			{
				for(auto c : COLOR)
					for (PieceType pr = PAWN; pr < KING; ++pr)
					{
						auto s = kpp_hand_index[c][pr].fb;
						if (s <= fb && fb < s + hand_max[pr])
						{
							int n = (int)(fb - s) + 1;
							if (hand_count(pos.hand_of(c),pr) < n)
								return false;

							goto Found;
						}
					}
			}
			else {
				for (Piece pc = NO_PIECE; pc < PIECE_NB; ++pc)
				{
					PieceType pt = type_of(pc);
					if (pt == NO_PIECE_TYPE || pt == QUEEN) 
						continue;

					auto s = BonaPiece(kpp_board_index[pc].fb);
					if (s <= fb && fb < s + SQ_NB)
					{
						Square sq = (Square)(fb - s);
						Piece pc2 = pos.piece_on(sq);

						PieceType pt2 = type_of(pc2);
						if (pt2 == PRO_PAWN || pt2 == PRO_LANCE || pt2 == PRO_KNIGHT || pt2 == PRO_SILVER)
							pc2 = make_piece(color_of(pc2), GOLD);

						if (pc2 != pc)
							return false;

						goto Found;
					}
				}
			}
			return false;
		Found:;
		}

		return true;
	}
#endif

} 
