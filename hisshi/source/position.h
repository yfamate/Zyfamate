#ifndef _POSITION_H_
#define _POSITION_H_
#include <deque>
#include <memory> 

#include "bitboard.h"
#include "evaluate.h"
#include "types.h"

#if defined(EVAL_NNUE)
#include "eval/nnue/nnue_accumulator.h"
#endif

#include "extra/key128.h"
#include "extra/long_effect.h"
#include "misc.h"




struct StateInfo {


	int pliesFromNull;




	HASH_KEY board_key_;
	HASH_KEY hand_key_;


	Key key()                     const { return long_key(); }
	Key board_key()               const { return board_long_key(); }
	Key hand_key()                const { return hand_long_key(); }


	HASH_KEY long_key()           const { return board_key_ + hand_key_; }
	HASH_KEY board_long_key()     const { return board_key_; }
	HASH_KEY hand_long_key()      const { return hand_key_; }

	Bitboard checkersBB;

	Piece capturedPiece;

	StateInfo* previous;


	Bitboard blockersForKing[COLOR_NB];

	Bitboard pinners[COLOR_NB];

	Bitboard checkSquares[PIECE_TYPE_NB];


	int continuousCheck[COLOR_NB];

	Hand hand;


#if defined (USE_PIECE_VALUE)
	Value materialValue;
#endif

#if defined(EVAL_KPPT) || defined(EVAL_KPP_KKPT)

	Eval::EvalSum sum;

#endif

#if defined(EVAL_NNUE)
	Eval::NNUE::Accumulator accumulator;
#endif

#if defined (USE_EVAL_LIST)
	Eval::DirtyPiece dirtyPiece;
#endif


#if defined(KEEP_LAST_MOVE)
	Move lastMove;

	PieceType lastMovedPieceType;
#endif

};

class Thread;


extern std::string SFEN_HIRATE;


typedef std::deque<StateInfo> StateList;
typedef std::unique_ptr<StateList> StateListPtr;


struct PackedSfen {
	u8 data[32];


	bool operator==(const PackedSfen& rhs) const {
		static_assert(sizeof(PackedSfen) % sizeof(u64) == 0);

		for (size_t i = 0; i < sizeof(PackedSfen); i += sizeof(size_t))
		{
			if (*(u64*)&data[i] != *(u64*)&rhs.data[i])
				return false;
		}
		return true;
	}

	bool operator!=(const PackedSfen& rhs) const {
		return !(this->operator==(rhs));
	}
};

struct PackedSfenHash {
	size_t operator()(const PackedSfen& ps) const
	{
		static_assert(sizeof(PackedSfen) % sizeof(size_t) == 0);

		size_t s = 0;
		for (size_t i = 0; i < sizeof(PackedSfen) ; i+= sizeof(size_t))
		{
			s ^= *(size_t*)&ps.data[i];
		}
		return s;
	}
};

class Position
{
public:

	Position() = default;

	Position(const Position&) = delete;
	Position& operator=(const Position&) = delete;

	static void init();

	void set(std::string sfen , StateInfo* si , Thread* th);

	const std::string sfen() const { return sfen(game_ply()); }
	const std::string sfen(int gamePly) const;

	void set_hirate(StateInfo*si,Thread* th) { set(SFEN_HIRATE,si,th); }


	Color side_to_move() const { return sideToMove; }

	int game_ply() const { return gamePly; }

	Thread* this_thread() const { return thisThread; }

	Piece piece_on(Square sq) const { ASSERT_LV3(sq <= SQ_NB); return board[sq]; }

	bool empty(Square sq) const { return piece_on(sq) == NO_PIECE; }

	Hand hand_of(Color c) const { ASSERT_LV3(is_ok(c));  return hand[c]; }

	FORCE_INLINE Square king_square(Color c) const { ASSERT_LV3(is_ok(c)); return kingSquare[c]; }

	bool pos_is_ok() const;

	Piece moved_piece_before(Move m) const
	{
		ASSERT_LV3(is_ok(m));
#if defined( KEEP_PIECE_IN_GENERATE_MOVES)
		static_assert(MOVE_PROMOTE == (1 << 15) && PIECE_PROMOTE == 8, "");
		return (Piece)((m ^ ((m & MOVE_PROMOTE) << 4)) >> 16);

#else
		return is_drop(m) ? make_piece(sideToMove , move_dropped_piece(m)) : piece_on(from_sq(m));
#endif
	}

	Piece moved_piece_after(Move m) const
	{

		return Piece(m >> 16);
	}

	Move to_move(Move16 m) const;

	RepetitionState is_repetition(int rep_ply = 16) const;

#if defined(CUCKOO)
	bool has_game_cycle(int plies_from_root , int rep_ply = 16) const;
#endif


	Bitboard pieces() const { return byTypeBB[ALL_PIECES]; }

	Bitboard pieces(Color c) const { ASSERT_LV3(is_ok(c)); return byColorBB[c]; }

	Bitboard empties() const { return pieces() ^ ALL_BB; }


	Bitboard pieces(PieceType pr) const { ASSERT_LV3(pr < PIECE_BB_NB); return byTypeBB[pr]; }
	Bitboard pieces(PieceType pr1, PieceType pr2) const { return pieces(pr1) | pieces(pr2); }
	Bitboard pieces(PieceType pr1, PieceType pr2, PieceType pr3) const { return pieces(pr1) | pieces(pr2) | pieces(pr3); }
	Bitboard pieces(PieceType pr1, PieceType pr2, PieceType pr3, PieceType pr4) const { return pieces(pr1) | pieces(pr2) | pieces(pr3) | pieces(pr4); }
	Bitboard pieces(PieceType pr1, PieceType pr2, PieceType pr3, PieceType pr4, PieceType pr5) const { return pieces(pr1) | pieces(pr2) | pieces(pr3) | pieces(pr4) | pieces(pr5); }

	Bitboard pieces(Color c, PieceType pr) const { return pieces(pr) & pieces(c); }
	Bitboard pieces(Color c, PieceType pr1, PieceType pr2) const { return pieces(pr1, pr2) & pieces(c); }
	Bitboard pieces(Color c, PieceType pr1, PieceType pr2, PieceType pr3) const { return pieces(pr1, pr2, pr3) & pieces(c); }
	Bitboard pieces(Color c, PieceType pr1, PieceType pr2, PieceType pr3, PieceType pr4) const { return pieces(pr1, pr2, pr3, pr4) & pieces(c); }
	Bitboard pieces(Color c, PieceType pr1, PieceType pr2, PieceType pr3, PieceType pr4, PieceType pr5) const { return pieces(pr1, pr2, pr3, pr4, pr5) & pieces(c); }



	template<PieceType Pt> Square square(Color c) const
	{
		static_assert(Pt == KING,"Pt must be a KING in Position::square().");
		ASSERT_LV3(is_ok(c));
		return king_square(c);
	}


	Bitboard checkers() const { return st->checkersBB; }

	Bitboard blockers_for_king(Color c) const { return st->blockersForKing[c]; }

	Bitboard check_squares(PieceType pt) const { ASSERT_LV3(pt!= NO_PIECE_TYPE && pt < PIECE_TYPE_NB); return st->checkSquares[pt]; }


	bool is_discovery_check_on_king(Color c, Move m) const { return st->blockersForKing[c] & from_sq(m); }



	Bitboard attackers_to(Color c, Square sq) const { return attackers_to(c, sq, pieces()); }
	Bitboard attackers_to(Color c, Square sq, const Bitboard& occ) const;
	Bitboard attackers_to(Square sq) const { return attackers_to(sq, pieces()); }
	Bitboard attackers_to(Square sq, const Bitboard& occ) const;

	Bitboard attackers_to_pawn(Color c, Square pawn_sq) const;

#if !defined(LONG_EFFECT_LIBRARY)
	bool effected_to(Color c, Square sq) const { return attackers_to(c, sq, pieces()); }
	bool effected_to(Color c, Square sq, Square kingSq) const { return attackers_to(c, sq, pieces() ^ kingSq); }
#else
	bool effected_to(Color c, Square sq) const { return board_effect[c].effect(sq) != 0; }
	bool effected_to(Color c, Square sq, Square kingSq) const {
		return board_effect[c].effect(sq) != 0 ||
			((long_effect.directions_of(c, kingSq) & Effect8::directions_of(kingSq, sq)) != 0); 
	}
#endif

	Bitboard slider_blockers(Color c, Square s, Bitboard& pinners) const;


	void do_move(Move m, StateInfo& newSt, bool givesCheck);

	void do_move(Move m, StateInfo& newSt) { do_move(m, newSt, gives_check(m)); }

	void undo_move(Move m);

	void do_null_move(StateInfo& st);
	void undo_null_move();

	void steal_hand(PieceType pr);
	void give_hand(PieceType pr);


	bool legal(Move m) const;

	bool pseudo_legal(const Move m) const { return pseudo_legal_s<true>(m); }

	template <bool All> bool pseudo_legal_s(const Move m) const;

	bool legal_drop(const Square to) const;

	bool legal_pawn_drop(const Color us, const Square to) const
	{
		return !((pieces(us, PAWN) & FILE_BB[file_of(to)])                             
			|| ((pawnEffect(us, to) == Bitboard(king_square(~us)) && !legal_drop(to)))); 
	}


	StateInfo* state() const { return st; }


#if defined(USE_EVAL_LIST)
	const Eval::EvalList* eval_list() const { return &evalList; }
#endif

#if defined (USE_SEE)
	bool see_ge(Move m, Value threshold = VALUE_ZERO) const;

#endif


	Key key() const { return st->key(); }

#if defined(USE_KEY_AFTER)
	Key key_after(Move m) const;
#endif

#if defined(USE_BOARD_KEY_AFTER)
  Key board_key_after(Move m) const;
#endif


	bool in_check() const { return checkers(); }

	Bitboard pinned_pieces(Color c) const { ASSERT_LV3(is_ok(c)); return st->blockersForKing[c] & pieces(c); }

	Bitboard pinned_pieces(Color c, Square avoid) const;

	Bitboard pinned_pieces(Color c, Square from, Square to) const;


	bool gives_check(Move m) const;

	bool discovered(Square from, Square to, Square ourKing, const Bitboard& pinned) const
	{
		return pinned                        
			&& (pinned & from)               
			&& !aligned(from, to , ourKing); 
	}

	bool is_mated() const;

	Piece captured_piece() const { return st->capturedPiece; }

	bool capture_or_promotion(Move m) const { return is_promote(m) || capture(m); }

	bool pawn_promotion(Move m) const
	{
		return (is_promote(m) && raw_type_of(moved_piece_after(m)) == PAWN);
	}

	bool capture_or_pawn_promotion(Move m) const
	{
		return pawn_promotion(m) || capture(m);
	}

	bool capture_or_valuable_promotion(Move m) const
	{
		auto pr = raw_type_of(moved_piece_after(m));
		return (is_promote(m) && (pr == PAWN || pr == BISHOP || pr == ROOK)) || capture(m);
	}

	bool capture(Move m) const { return !is_drop(m) && piece_on(to_sq(m)) != NO_PIECE; }

#if defined(USE_ENTERING_KING_WIN)
	Move DeclarationWin() const;
#endif

#if defined(USE_SFEN_PACKER)
	void sfen_pack(PackedSfen& sfen);

	static std::string sfen_unpack(const PackedSfen& sfen);

	Tools::Result set_from_packed_sfen(const PackedSfen& sfen , StateInfo * si , Thread* th, bool mirror=false , int gamePly_ = 0);

	static std::string sfen_from_rawdata(Piece board[81], Hand hands[2], Color turn, int gamePly);
#endif

#if defined(LONG_EFFECT_LIBRARY)

	LongEffect::ByteBoard board_effect[COLOR_NB];

#if defined(USE_BOARD_EFFECT_PREV)

	LongEffect::ByteBoard board_effect_prev[COLOR_NB];
#endif

	LongEffect::WordBoard long_effect;

#endif


#if defined(KEEP_LAST_MOVE)
	std::string moves_from_start() const { return moves_from_start(false); }
	std::string moves_from_start_pretty() const { return moves_from_start(true); }
	std::string moves_from_start(bool is_pretty) const;
#endif

	friend std::ostream& operator<<(std::ostream& os, const Position& pos);

	template <MOVE_GEN_TYPE gen_type, bool gen_all>
	friend struct MoveGenerator;

private:
	void set_state(StateInfo* si) const;

	template <bool doNullMove>
	void set_check_info(StateInfo* si) const;

	template <Color Us> void do_move_impl(Move m, StateInfo& st, bool givesCheck);

	template <Color Us> void undo_move_impl(Move m);


	Bitboard byColorBB[COLOR_NB];

	Bitboard byTypeBB[PIECE_BB_NB];


	void put_piece(Square sq, Piece pc);

	void remove_piece(Square sq);

	void xor_piece(Piece pc, Square sq);

	void update_bitboards();

	void update_kingSquare();

#if defined (USE_EVAL_LIST)

	Eval::BonaPiece bona_piece_of(Color c, PieceType pt) const {
		int ct = hand_count(hand[c], pt);
		ASSERT_LV3(ct > 0);
		return (Eval::BonaPiece)(Eval::kpp_hand_index[c][pt].fb + ct - 1);
	}

	PieceNumber piece_no_of(Color c, PieceType pt) const { return evalList.piece_no_of_hand(bona_piece_of(c, pt)); }

	PieceNumber piece_no_of(Square sq) const
	{
		ASSERT_LV3(piece_on(sq) != NO_PIECE);
		PieceNumber n = evalList.piece_no_of_board(sq);
		ASSERT_LV3(is_ok(n));
		return n;
	}
#else
	PieceNumber piece_no_of(Color c, Piece pt) const { return PIECE_NUMBER_ZERO; }
	PieceNumber piece_no_of(Piece pc, Square sq) const { return PIECE_NUMBER_ZERO; }
	PieceNumber piece_no_of(Square sq) const { return PIECE_NUMBER_ZERO; }
#endif

	Piece board[SQ_NB_PLUS1];

	Hand hand[COLOR_NB];

	Color sideToMove;

	Square kingSquare[COLOR_NB];

	int gamePly;

	Thread* thisThread;

	StateInfo* st;

#if defined(USE_EVAL_LIST)
	Eval::EvalList evalList;
#endif
};

inline void Position::xor_piece(Piece pc, Square sq)
{
	byColorBB[color_of(pc)] ^= sq;

	byTypeBB[ALL_PIECES] ^= sq;

	byTypeBB[type_of(pc)] ^= sq;
}

inline void Position::put_piece(Square sq, Piece pc)
{
	ASSERT_LV2(board[sq] == NO_PIECE);
	board[sq] = pc;
	xor_piece(pc, sq);
}

inline void Position::remove_piece(Square sq)
{
	Piece pc = board[sq];
	ASSERT_LV3(pc != NO_PIECE);
	board[sq] = NO_PIECE;
	xor_piece(pc, sq);
}

inline bool is_ok(Position& pos) { return pos.pos_is_ok(); }

std::ostream& operator<<(std::ostream& os, const Position& pos);

HASH_KEY DepthHash(int depth);

#endif 
