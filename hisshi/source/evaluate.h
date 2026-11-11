#ifndef _EVALUATE_H_
#define _EVALUATE_H_

#include "config.h"
#include "types.h"


#if defined(EVAL_KPPT) || defined(EVAL_KPP_KKPT)
#include "eval/evalsum.h"
#endif


struct StateInfo;

namespace Eval {

	void init();

	Value compute_eval(const Position& pos);

	void evaluate_with_no_return(const Position& pos);

	void print_eval_stat(Position& pos);

	void load_eval();

	Value evaluate(const Position& pos);

	Value material(const Position& pos);


#if defined(EVAL_KPPT) || defined(EVAL_KPP_KKPT)
	u64 calc_check_sum();

	void print_softname(u64 check_sum);
#else
	static u64 calc_check_sum() { return 0; }
	static void print_softname(u64 check_sum) {}
#endif

#if defined (USE_PIECE_VALUE)

	enum {
		PawnValue = 90,
		LanceValue = 315,
		KnightValue = 405,
		SilverValue = 495,
		GoldValue = 540,
		BishopValue = 855,
		RookValue = 990,
		ProPawnValue = 540,
		ProLanceValue = 540,
		ProKnightValue = 540,
		ProSilverValue = 540,
		HorseValue = 945,
		DragonValue = 1395,
		KingValue = 15000,
	};

	extern int PieceValue[PIECE_NB];

	extern int CapturePieceValue[PIECE_NB];

	extern int ProDiffPieceValue[PIECE_NB];
#endif


#if defined(USE_EVAL_LIST)


	enum BonaPiece : int32_t
	{

		BONA_PIECE_NOT_INIT = -1,

		BONA_PIECE_ZERO = 0,


#if defined (EVAL_MATERIAL) || defined (EVAL_KPPT) || defined(EVAL_KPP_KKPT) || defined(EVAL_NNUE)

		f_hand_pawn = BONA_PIECE_ZERO + 1,
		e_hand_pawn = 20,
		f_hand_lance = 39,
		e_hand_lance = 44,
		f_hand_knight = 49,
		e_hand_knight = 54,
		f_hand_silver = 59,
		e_hand_silver = 64,
		f_hand_gold = 69,
		e_hand_gold = 74,
		f_hand_bishop = 79,
		e_hand_bishop = 82,
		f_hand_rook = 85,
		e_hand_rook = 88,
		fe_hand_end = 90,

#else 
		fe_hand_end = 0,
#endif                     


		f_pawn = fe_hand_end,
		e_pawn = f_pawn + 81,
		f_lance = e_pawn + 81,
		e_lance = f_lance + 81,
		f_knight = e_lance + 81,
		e_knight = f_knight + 81,
		f_silver = e_knight + 81,
		e_silver = f_silver + 81,
		f_gold = e_silver + 81,
		e_gold = f_gold + 81,
		f_bishop = e_gold + 81,
		e_bishop = f_bishop + 81,
		f_horse = e_bishop + 81,
		e_horse = f_horse + 81,
		f_rook = e_horse + 81,
		e_rook = f_rook + 81,
		f_dragon = e_rook + 81,
		e_dragon = f_dragon + 81,
		fe_old_end = e_dragon + 81,


#if defined(DISTINGUISH_GOLDS)
		f_pro_pawn = fe_old_end,
		e_pro_pawn = f_pro_pawn + 81,
		f_pro_lance = e_pro_pawn + 81,
		e_pro_lance = f_pro_lance + 81,
		f_pro_knight = e_pro_lance + 81,
		e_pro_knight = f_pro_knight + 81,
		f_pro_silver = e_pro_knight + 81,
		e_pro_silver = f_pro_silver + 81,
		fe_new_end = e_pro_silver + 81,
#else
		fe_new_end = fe_old_end,
#endif

		fe_end = fe_new_end,


		f_king = fe_end,
		e_king = f_king + SQ_NB,
		fe_end2 = e_king + SQ_NB, 

	};

	std::ostream& operator<<(std::ostream& os, BonaPiece bp);

	union ExtBonaPiece
	{
		struct {
			BonaPiece fb; 
			BonaPiece fw; 
		};
		BonaPiece from[2];

		ExtBonaPiece() {}
		ExtBonaPiece(BonaPiece fb_, BonaPiece fw_) : fb(fb_) , fw(fw_){}
	};

	inline std::ostream& operator<<(std::ostream& os, ExtBonaPiece bp) { os << bp.fb; return os; }

	struct ChangedBonaPiece
	{
		ExtBonaPiece old_piece;
		ExtBonaPiece new_piece;
	};

	extern ExtBonaPiece kpp_board_index[PIECE_NB];

	extern ExtBonaPiece kpp_hand_index[COLOR_NB][KING];

	struct EvalList
	{
		BonaPiece* piece_list_fb() const { return const_cast<BonaPiece*>(pieceListFb); }
		BonaPiece* piece_list_fw() const { return const_cast<BonaPiece*>(pieceListFw); }

		ExtBonaPiece bona_piece(PieceNumber piece_no) const
		{
			ExtBonaPiece bp;
			bp.fb = pieceListFb[piece_no];
			bp.fw = pieceListFw[piece_no];
			return bp;
		}

		void put_piece(PieceNumber piece_no, Square sq, Piece pc) {
			set_piece_on_board(piece_no, BonaPiece(kpp_board_index[pc].fb + sq), BonaPiece(kpp_board_index[pc].fw + Inv(sq)),sq);
		}

		void put_piece(PieceNumber piece_no, Color c, PieceType pt, int i) {
			set_piece_on_hand(piece_no, BonaPiece(kpp_hand_index[c][pt].fb + i), BonaPiece(kpp_hand_index[c][pt].fw + i));
		}

		PieceNumber piece_no_of_hand(BonaPiece bp) const { return piece_no_list_hand[bp]; }
		PieceNumber piece_no_of_board(Square sq) const { return piece_no_list_board[sq]; }

		void clear()
		{

			for (auto& p : pieceListFb)
				p = BONA_PIECE_ZERO;

			for (auto& p : pieceListFw)
				p = BONA_PIECE_ZERO;

			for (auto& v : piece_no_list_hand)
				v = PIECE_NUMBER_NB;

			for (auto& v : piece_no_list_board)
				v = PIECE_NUMBER_NB;
		}

		bool is_valid(const Position& pos);


	protected:

		inline void set_piece_on_board(PieceNumber piece_no, BonaPiece fb , BonaPiece fw, Square sq)
		{
			ASSERT_LV3(is_ok(piece_no));
			pieceListFb[piece_no] = fb;
			pieceListFw[piece_no] = fw;
			piece_no_list_board[sq] = piece_no;
		}

		inline void set_piece_on_hand(PieceNumber piece_no, BonaPiece fb, BonaPiece fw)
		{
			ASSERT_LV3(is_ok(piece_no));
			pieceListFb[piece_no] = fb;
			pieceListFw[piece_no] = fw;
			piece_no_list_hand[fb] = piece_no;
		}


	public:
		int length() const { return PIECE_NUMBER_KING; }

		static const int MAX_LENGTH = 40;

	private:

	#if defined(USE_AVX2)

		alignas(32) BonaPiece pieceListFb[MAX_LENGTH];
		alignas(32) BonaPiece pieceListFw[MAX_LENGTH];

	#else

		BonaPiece pieceListFb[MAX_LENGTH];
		BonaPiece pieceListFw[MAX_LENGTH];

	#endif

		PieceNumber piece_no_list_hand[fe_hand_end];

		PieceNumber piece_no_list_board[SQ_NB_PLUS1];

	};


	struct DirtyPiece
	{
		Eval::ChangedBonaPiece changed_piece[2];

		PieceNumber pieceNo[2];

		int dirty_num;
	};
#endif 

#if defined(USE_EVAL_HASH)
	extern void EvalHash_Resize(size_t mbSize);

	extern void EvalHash_Clear();
#endif

}

#endif 
