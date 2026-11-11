#include "position.h"
#include "misc.h"
#include "tt.h"
#include "thread.h"
#include "mate/mate.h"

#include <iostream>
#include <sstream>
#include <cstring> 

#if defined(EVAL_KPPT) || defined(EVAL_KPP_KKPT) || defined(EVAL_NNUE)
#include "eval/evaluate_common.h"
#endif

using namespace std;
using namespace Effect8;

std::string SFEN_HIRATE = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";


namespace Zobrist {
	HASH_KEY zero;							
	HASH_KEY side;							
	HASH_KEY psq[SQ_NB_PLUS1][PIECE_NB];	
	HASH_KEY hand[COLOR_NB][PIECE_HAND_NB];	
	HASH_KEY depth[MAX_PLY];				
}






template <bool doNullMove>
void Position::set_check_info(StateInfo* si) const {

	
	

	
	

	if (!doNullMove)
	{
		
		si->blockersForKing[WHITE] = slider_blockers(BLACK, square<KING>(WHITE), si->pinners[WHITE]);
		si->blockersForKing[BLACK] = slider_blockers(WHITE, square<KING>(BLACK), si->pinners[BLACK]);
	}

	Square ksq = square<KING>(~sideToMove);

	

	
	

	Bitboard occ = pieces();
	Color them = ~sideToMove;

	
	si->checkSquares[PAWN]   = pawnEffect(them, ksq);
	si->checkSquares[KNIGHT] = knightEffect(them, ksq);
	si->checkSquares[SILVER] = silverEffect(them, ksq);
	si->checkSquares[BISHOP] = bishopEffect(ksq, occ);
	si->checkSquares[ROOK]   = rookEffect(ksq, occ);
	si->checkSquares[GOLD]   = goldEffect(them, ksq);

	
	si->checkSquares[LANCE]  = si->checkSquares[ROOK] & lanceStepEffect(them,ksq);

	
	si->checkSquares[KING]   = ZERO_BB;

	
	
	si->checkSquares[PRO_PAWN]   = si->checkSquares[GOLD];
	si->checkSquares[PRO_LANCE]  = si->checkSquares[GOLD];
	si->checkSquares[PRO_KNIGHT] = si->checkSquares[GOLD];
	si->checkSquares[PRO_SILVER] = si->checkSquares[GOLD];
	si->checkSquares[HORSE]      = si->checkSquares[BISHOP] | kingEffect(ksq);
	si->checkSquares[DRAGON]     = si->checkSquares[ROOK]   | kingEffect(ksq);
}





#if defined(CUCKOO)







inline int H1(Key h) { return h & 0x3fff; }
inline int H2(Key h) { return (h >> 16) & 0x3fff; }


Key cuckoo[8192*2];
Move cuckooMove[8192*2];




#endif

void Position::init() {
	PRNG rng(20151225); 

	
	SET_HASH(Zobrist::side, 1, 0, 0, 0);
	SET_HASH(Zobrist::zero, 0, 0, 0, 0);

	
	
	
	
	for (auto pc : Piece())
		for (auto sq : SQ)
			if (pc)
				SET_HASH(Zobrist::psq[sq][pc], rng.rand<Key>() & ~1ULL, rng.rand<Key>(), rng.rand<Key>(), rng.rand<Key>());

	
	for (auto c : COLOR)
		for (PieceType pr = NO_PIECE_TYPE; pr < PIECE_HAND_NB; ++pr)
			if (pr)
				SET_HASH(Zobrist::hand[c][pr], rng.rand<Key>() & ~1ULL, rng.rand<Key>(), rng.rand<Key>(), rng.rand<Key>());

	for (int i = 0; i < MAX_PLY; ++i)
		SET_HASH(Zobrist::depth[i], rng.rand<Key>() & ~1ULL, rng.rand<Key>(), rng.rand<Key>(), rng.rand<Key>());

#if defined(CUCKOO)
	
	std::memset(cuckoo, 0, sizeof(cuckoo));
	std::memset(cuckooMove, 0, sizeof(cuckooMove));
	int count = 0;
	
	int count2 = 0;
	for (auto pc : Piece())
	{
		auto pt = type_of(pc);
		if (!(pt == PAWN || pt == LANCE || pt == KNIGHT || pt == SILVER || pt == GOLD || pt == BISHOP || pt == ROOK || pt == KING
			|| pt == PRO_PAWN || pt == PRO_LANCE || pt == PRO_KNIGHT || pt == PRO_SILVER || pt == HORSE || pt == DRAGON))
			continue;

		
		
		for (auto s1 : SQ)
			for (Square s2 : SQ)
				if (effects_from(pc, s1, ZERO_BB) & s2)
				{
					Move move = (Move)(make_move(s1, s2) + (pc << 16));
					
					Key key = (Zobrist::psq[s2][pc] - Zobrist::psq[s1][pc]) >> 1;
					int i = H1(key);
					while (true)
					{
						std::swap(cuckoo[i], key);
						std::swap(cuckooMove[i], move);
						if (move == MOVE_NONE) 
							break;

						
						

						i = H2(key);
						std::swap(cuckoo[i], key);
						std::swap(cuckooMove[i], move);

						if (move != MOVE_NONE)
							count2++;

						break;
					}
					count++;
				}
	}
	

	
	
	
	ASSERT_LV3(count == 16456);
#endif
}


HASH_KEY DepthHash(int depth) { return Zobrist::depth[depth]; }






#if !defined (PRETTY_JP)
std::string pretty(Piece pc) { return std::string(USI_PIECE).substr(pc * 2, 2); }
#else

std::string USI_PIECE_KANJI[] = {
	" 口"," 歩"," 香"," 桂"," 銀"," 角"," 飛"," 金"," 玉"," と"," 杏"," 圭"," 全"," 馬"," 龍"," 菌"," 王",
		  "^歩","^香","^桂","^銀","^角","^飛","^金","^玉","^と","^杏","^圭","^全","^馬","^龍","^菌","^王"
};
std::string pretty(Piece pc) {
#if 1
	return USI_PIECE_KANJI[pc];
#else
	
	
	

	std::string result;
	if (pc != NO_PIECE)
		result = (color_of(pc) == BLACK) ? "\\e[32;40;1m" : "\\e[33;40;1m";
	result += USI_PIECE_KANJI[pc];
	if (pc != NO_PIECE)
		result += "\\e[m";

	return result;
#endif
}
#endif


void Position::set(std::string sfen , StateInfo* si , Thread* th)
{
	std::memset(this, 0, sizeof(Position));

	
	
	
	std::memset(si, 0, sizeof(StateInfo));
	st = si;

	
	

	

	
	File f = FILE_9;
	Rank r = RANK_1;

	std::istringstream ss(sfen);
	
	ss >> std::noskipws;

	uint8_t token;
	bool promote = false;
	size_t idx;

#if defined (USE_EVAL_LIST)
	
	evalList.clear();

	
	
	PieceNumber piece_no_count[KING] = { PIECE_NUMBER_ZERO,PIECE_NUMBER_PAWN,PIECE_NUMBER_LANCE,PIECE_NUMBER_KNIGHT,
	  PIECE_NUMBER_SILVER, PIECE_NUMBER_BISHOP, PIECE_NUMBER_ROOK,PIECE_NUMBER_GOLD };

	
	
#endif

	kingSquare[BLACK] = kingSquare[WHITE] = SQ_NB;

	while ((ss >> token) && !isspace(token))
	{
		
		if (isdigit(token))
			f -= File(token - '0');
		
		else if (token == '/')
		{
			f = FILE_9;
			++r;
		}
		
		else if (token == '+')
			promote = true;
		
		else if ((idx = PieceToCharBW.find(token)) != string::npos)
		{
			
			auto sq = f | r;
			auto pc = Piece(idx + (promote ? u32(PIECE_PROMOTE) : 0));
			put_piece(sq, pc);

#if defined (USE_EVAL_LIST)
			PieceNumber piece_no =
				(idx == B_KING) ? PIECE_NUMBER_BKING : 
				(idx == W_KING) ? PIECE_NUMBER_WKING : 
				piece_no_count[raw_type_of(Piece(idx))]++; 
			evalList.put_piece(piece_no, sq, pc); 
#endif

			
			--f;

			
			promote = false;
		}

	}

	
	
	update_bitboards();

	
	update_kingSquare();

	

	ss >> token;
	sideToMove = (token == 'w' ? WHITE : BLACK);
	ss >> token; 

	

	hand[BLACK] = hand[WHITE] = (Hand)0;
	int ct = 0;
	while ((ss >> token) && !isspace(token))
	{
		
		if (token == '-')
			break;

		if (isdigit(token))
			
			ct = (token - '0') + ct * 10;
		else if ((idx = PieceToCharBW.find(token)) != string::npos)
		{
			
			ct = max(ct, 1);
			add_hand(hand[color_of(Piece(idx))], type_of(Piece(idx)), ct);

			
			for (int i = 0; i < ct; ++i)
			{
				PieceType rpc = raw_type_of(Piece(idx));

#if defined (USE_EVAL_LIST)
				PieceNumber piece_no = piece_no_count[rpc]++;
				ASSERT_LV1(is_ok(piece_no));
				evalList.put_piece(piece_no, color_of(Piece(idx)), rpc, i);
#endif
			}
			ct = 0;
		}
	}

	

	
	
	
	gamePly = 0;
	ss >> std::skipws >> gamePly;

	

	set_state(st);

	
	

	

#if defined (LONG_EFFECT_LIBRARY)
  
	LongEffect::calc_effect(*this);
#endif

	

#if defined (USE_PIECE_VALUE)
	st->materialValue = Eval::material(*this);
#endif

	Eval::compute_eval(*this);

	

#if ASSERT_LV >= 3
  
	if (!is_ok(*this))
		std::cout << "info string Illigal Position?" << endl;
#endif

	thisThread = th;
}



const std::string Position::sfen(int gamePly_) const
{
	std::ostringstream ss;

	
	int emptyCnt;
	for (Rank r = RANK_1; r <= RANK_9; ++r)
	{
		for (File f = FILE_9; f >= FILE_1; --f)
		{
			
			
			for (emptyCnt = 0; f >= FILE_1 && piece_on(f | r) == NO_PIECE; --f)
				++emptyCnt;

			
			if (emptyCnt)
				ss << emptyCnt;

			
			if (f >= FILE_1)
				ss << (piece_on(f | r));
		}

		
		if (r < RANK_9)
			ss << '/';
	}

	
	ss << (sideToMove == WHITE ? " w " : " b ");

	
	int n;
	bool found = false;
	for (Color c = BLACK; c <= WHITE; ++c)
		for (int pn = 0 ; pn < 7; ++ pn)
		{
			
			
			
			

			const PieceType USI_Hand[7] = { ROOK,BISHOP,GOLD,SILVER,KNIGHT,LANCE,PAWN };
			auto p = USI_Hand[pn];

			
			n = hand_count(hand[c], p);
			
			if (n != 0)
			{
				
				found = true;

				
				if (n != 1)
					ss << n;

				ss << PieceToCharBW[make_piece(c, p)];
			}
		}

	
	ss << (found ? " " : "- ");

	
	ss << gamePly_;

	return ss.str();
}

void Position::set_state(StateInfo* si) const {

	

	
	st->checkersBB = attackers_to(~sideToMove, king_square(sideToMove));

	
	set_check_info<false>(si);

	
	si->board_key_ = sideToMove == BLACK ? Zobrist::zero : Zobrist::side;
	si->hand_key_ = Zobrist::zero;
	for (auto sq : pieces())
	{
		auto pc = piece_on(sq);
		si->board_key_ += Zobrist::psq[sq][pc];
	}
	for (auto c : COLOR)
		for (PieceType pr = PAWN; pr < PIECE_HAND_NB; ++pr)
			si->hand_key_ += Zobrist::hand[c][pr] * (int64_t)hand_count(hand[c], pr); 

	
	si->hand = hand[sideToMove];

}


void Position::update_bitboards()
{
	
	byTypeBB[HDK]			= pieces(KING , HORSE , DRAGON);

	
	byTypeBB[GOLDS]			= pieces(GOLD , PRO_PAWN , PRO_LANCE , PRO_KNIGHT , PRO_SILVER);

	

	
	byTypeBB[BISHOP_HORSE]	= pieces(BISHOP , HORSE);

	
	byTypeBB[ROOK_DRAGON]	= pieces(ROOK   , DRAGON);

	
	byTypeBB[SILVER_HDK]	= pieces(SILVER , HDK);

	
	byTypeBB[GOLDS_HDK]		= pieces(GOLDS  , HDK);
}


void Position::update_kingSquare()
{
	for (auto c : COLOR)
	{
		
		auto b = pieces(c, KING);
		kingSquare[c] = b ? b.pop() : SQ_NB;
	}
}






std::ostream& operator<<(std::ostream& os, const Position& pos)
{
	
	for (Rank r = RANK_1; r <= RANK_9; ++r)
	{
		for (File f = FILE_9; f >= FILE_1; --f)
			os << pretty(pos.board[f | r]);
		os << endl;
	}

#if !defined (PRETTY_JP)
	
	os << "BLACK HAND : " << pos.hand[BLACK] << " , WHITE HAND : " << pos.hand[WHITE] << endl;

	
	os << "Turn = " << pos.sideToMove << endl;
#else
	os << "先手 手駒 : " << pos.hand[BLACK] << " , 後手 手駒 : " << pos.hand[WHITE] << endl;
	os << "手番 = " << pos.sideToMove << endl;
#endif

	
	os << "sfen " << pos.sfen() << endl;

	return os;
}

#if defined (KEEP_LAST_MOVE)
#include <stack>


std::string Position::moves_from_start(bool is_pretty) const
{
	StateInfo* p = st;
	std::stack<StateInfo*> states;
	while (p->previous != nullptr)
	{
		states.push(p);
		p = p->previous;
	}

	stringstream ss;
	while (states.size())
	{
		auto& top = states.top();
		if (is_pretty)
			ss << pretty(top->lastMove, top->lastMovedPieceType) << ' ';
		else
			ss << top->lastMove << ' ';
		states.pop();
	}
	return ss.str();
}
#endif












Bitboard Position::slider_blockers(Color c, Square s , Bitboard& pinners) const {

	Bitboard blockers = ZERO_BB;

	
	pinners = ZERO_BB;

	
	

	
	Bitboard snipers =
		( (pieces(ROOK_DRAGON)  & rookStepEffect(s))
		| (pieces(BISHOP_HORSE) & bishopStepEffect(s))
		
		| (pieces(LANCE) & lanceStepEffect(~c, s))
		) & pieces(c);

	

	
	
	
	

	while (snipers)
	{
		Square sniperSq = snipers.pop();
		Bitboard b = between_bb(s, sniperSq) & pieces() ;

		
		if (b && !more_than_one(b))
		{
			blockers |= b;
			if (b & pieces(~c))
				
				pinners |= sniperSq;
		}
	}
	return blockers;
}




Bitboard Position::attackers_to(Color c, Square sq, const Bitboard& occ) const
{
	ASSERT_LV3(is_ok(c) && sq <= SQ_NB);

	Color them = ~c;

	
	
	return
		(     (pawnEffect(them, sq)		&  pieces(PAWN)        )
			| (knightEffect(them, sq)	&  pieces(KNIGHT)      )
			| (silverEffect(them, sq)	&  pieces(SILVER_HDK)  )
			| (goldEffect(them, sq)		&  pieces(GOLDS_HDK)   )
			| (bishopEffect(sq, occ)	&  pieces(BISHOP_HORSE))
			| (rookEffect(sq, occ)		& (
					pieces(ROOK_DRAGON)
				|  (lanceStepEffect(them,sq) & pieces(LANCE))
			  ))
		
		
			) & pieces(c); 
		;

}




Bitboard Position::attackers_to(Square sq, const Bitboard& occ) const
{
	ASSERT_LV3(sq <= SQ_NB);

	
	return
		
		((    (pawnEffect(WHITE, sq)   & pieces(PAWN)        )
			| (knightEffect(WHITE, sq) & pieces(KNIGHT)      )
			| (silverEffect(WHITE, sq) & pieces(SILVER_HDK)  )
			| (goldEffect(WHITE, sq)   & pieces(GOLDS_HDK)   )
			) & pieces(BLACK))
		|

		
		((    (pawnEffect(BLACK, sq)   & pieces(PAWN)        )
			| (knightEffect(BLACK, sq) & pieces(KNIGHT)      )
			| (silverEffect(BLACK, sq) & pieces(SILVER_HDK)  )
			| (goldEffect(BLACK, sq)   & pieces(GOLDS_HDK)   )
			) & pieces(WHITE))

		
		| (bishopEffect(sq, occ) & pieces(BISHOP_HORSE) )
		| (rookEffect(sq, occ) & (
			   pieces(ROOK_DRAGON)
			| (pieces(BLACK , LANCE) & lanceStepEffect(WHITE , sq))
			| (pieces(WHITE , LANCE) & lanceStepEffect(BLACK , sq))
			
			));
}


inline Bitboard Position::attackers_to_pawn(Color c, Square pawn_sq) const
{
	ASSERT_LV3(is_ok(c) && pawn_sq <= SQ_NB);

	Color them = ~c;
	const Bitboard& occ = pieces();

	
	const Bitboard bb_hd = kingEffect(pawn_sq) & pieces(HORSE,DRAGON);
	
	

	
	
	return
		(     (knightEffect(them, pawn_sq) &  pieces(KNIGHT)          )
			| (silverEffect(them, pawn_sq) & (pieces(SILVER) | bb_hd) )
			| (goldEffect(them, pawn_sq)   & (pieces(GOLDS)  | bb_hd) )
			| (bishopEffect(pawn_sq, occ)  &  pieces(BISHOP_HORSE)    )
			| (rookEffect(pawn_sq, occ)    &  pieces(ROOK_DRAGON)     )
			) & pieces(c);
}




bool Position::gives_check(Move m) const
{
	
	ASSERT_LV3(is_ok(m));

	
	const Square to = to_sq(m);

	
	
	if (st->checkSquares[type_of(moved_piece_after(m))] & to)
		return true;

	

	
	const Square from = from_sq(m);

	
	
	return !is_drop(m)
		&& (((blockers_for_king(~sideToMove) ) & from)
		&& !aligned(from, to, square<KING>(~sideToMove)));
}

Bitboard Position::pinned_pieces(Color c, Square avoid) const {
	Bitboard b, pinners, result = ZERO_BB;
	Square ksq = king_square(c);

	
	Bitboard avoid_bb = ~Bitboard(avoid);

	pinners = (
		  (pieces(ROOK_DRAGON)   & rookStepEffect(ksq))
		| (pieces(BISHOP_HORSE)  & bishopStepEffect(ksq))
		| (pieces(LANCE)         & lanceStepEffect(c, ksq))
		) & avoid_bb & pieces(~c);

	while (pinners)
	{
		b = between_bb(ksq, pinners.pop()) & pieces() & avoid_bb;
		if (!more_than_one(b))
			result |= b & pieces(c);
	}
	return result;
}

Bitboard Position::pinned_pieces(Color c, Square from, Square to) const {
	Bitboard b, pinners, result = ZERO_BB;
	Square ksq = king_square(c);

	
	Bitboard avoid_bb = ~Bitboard(from);

	pinners = (
		(pieces(ROOK_DRAGON)    & rookStepEffect(ksq))
		| (pieces(BISHOP_HORSE) & bishopStepEffect(ksq))
		| (pieces(LANCE)        & lanceStepEffect(c, ksq))
		) & avoid_bb & pieces(~c);

	
	Bitboard new_pieces = (pieces() & avoid_bb) | to;
	while (pinners)
	{
		b = between_bb(ksq, pinners.pop()) & new_pieces;
		if (!more_than_one(b))
			result |= b & pieces(c);
	}
	return result;
}


bool Position::is_mated() const
{
	
	return MoveList<LEGAL>(*this).size() == 0;
}





bool Position::legal_drop(const Square to) const
{
	const auto us = sideToMove;

	
	ASSERT_LV3(pawnEffect(us, to) == Bitboard(king_square(~us)));

	
	if (!effected_to(us, to))
		return true;

	
	
	Bitboard b = attackers_to_pawn(~us, to);

	
	Bitboard pinned = blockers_for_king(~us);

	
	if (b & (~pinned | FILE_BB[file_of(to)]))
		return true;

	
	
	
	
	
	
	
	

	
	
	

	
	
	
	

	
	
	
	
	

	
	

	
	Square sq_king = king_square(~us);

#ifndef LONG_EFFECT_LIBRARY
	

	Bitboard escape_bb = kingEffect(sq_king) & ~pieces(~us);
	escape_bb ^= to;
	auto occ = pieces() ^ to; 
	while (escape_bb)
	{
		Square king_to = escape_bb.pop();
		if (!attackers_to(us, king_to, occ))
			return true; 
	}

	
	return false;
#else
	

	auto a8_effet_us = board_effect[us].around8(sq_king);
	
	
	auto a8_them_movable = Effect8::around8(~pieces(~us), sq_king) & Effect8::board_mask(sq_king);

	
	
	if (~a8_effet_us & a8_them_movable)
		return true;

	
	
	auto a8_long_effect_to = long_effect.directions_of(us, to);
	auto to_dir = (us == BLACK) ? DIRECT_D : DIRECT_U;  
	auto a8_cutoff_dir = Effect8::cutoff_directions(to_dir, a8_long_effect_to);
	auto a8_target = a8_cutoff_dir & a8_them_movable & ~board_effect[us].around8_greater_than_one(sq_king);

	return a8_target != 0;
#endif
}


template <bool All>
bool Position::pseudo_legal_s(const Move m) const {

	const Color us = sideToMove;
	const Square to = to_sq(m); 

	if (is_drop(m))
	{
		const PieceType pr = move_dropped_piece(m);
		
		

		
		if (moved_piece_after(m) != Piece(pr + (us == WHITE ? u32(PIECE_WHITE) : 0) ))
			return false;

		ASSERT_LV3(PAWN <= pr && pr < KING);

		
		if (piece_on(to) != NO_PIECE || hand_count(hand[us], pr) == 0)
			return false;

		if (in_check())
		{
			
			Bitboard target = checkers();
			Square checksq = target.pop();

			
			
			if (target)
				return false;

			
			
			if (!(between_bb(checksq, king_square(us)) & to))
				return false;
		}

		
		if (pr == PAWN && !legal_pawn_drop(us, to))
			return false;

		

		
		
		

		
		
		

	}
	else {

		const Square from = from_sq(m);
		const Piece pc = piece_on(from);

		
		if (pc == NO_PIECE || color_of(pc) != us)
			return false;

		
		if (!(effects_from(pc, from, pieces()) & to))
			return false;

		
		if (pieces(us) & to)
			return false;

		PieceType pt = type_of(pc);
		if (is_promote(m))
		{
			

			
			static_assert(GOLD == 7, "GOLD must be 7.");
			if (pt >= GOLD)
				return false;

			
			
			if (moved_piece_after(m) != Piece(pc + PIECE_PROMOTE))
				return false;
		}
		else {

			

			
			if (moved_piece_after(m) != pc)
				return false;

			
			
			
			
			

			if (All)
			{
				
				
				

				if (pt == PAWN || pt == LANCE)
					if ((us == BLACK && rank_of(to) == RANK_1) || (us == WHITE && rank_of(to) == RANK_9))
						return false;
			}
			else {
				
				
				switch (pt)
				{
				case PAWN:
					if (enemy_field(us) & to)
						return false;
					break;

				case LANCE:
					if ((us == BLACK && rank_of(to) <= RANK_2) || (us == WHITE && rank_of(to) >= RANK_8))
						return false;
					break;

				case BISHOP:
				case ROOK:
					if (enemy_field(us) & (Bitboard(from) | Bitboard(to)))
						return false;
					break;

				default:
					break;
				}
			}

		}

		
		if (checkers())
		{
			

			
			if (type_of(pc) != KING)
			{
				
				if (more_than_one(checkers()))
					return false;

				
				
				
				
				
				if (!((between_bb(checkers().pop(), king_square(us)) | checkers()) & to))
					return false;
			}

			

		}
	}

	return true;
}


bool Position::legal(Move m) const
{
	if (is_drop(m))
		
		return true;
	else
	{
		Color us = sideToMove;
		Square from = from_sq(m);

		ASSERT_LV5(color_of(piece_on(from_sq(m))) == us);
		ASSERT_LV5(piece_on(square<KING>(us)) == make_piece(us, KING));

		
		if (type_of(piece_on(from)) == KING)
			return !effected_to(~us, to_sq(m), from);

		
		
		return   !(blockers_for_king(us) & from)
			|| aligned(from, to_sq(m), square<KING>(us));
	}
}



Move Position::to_move(Move16 m16) const
{
	
	

	
	

	Move m = (Move)m16.to_u16();

	
	
	
	
	if (!is_ok(m))
		return m;

	return
		Move(u16(m) +
			((is_drop(m) ? (Piece)(make_piece(sideToMove, move_dropped_piece(m)))
			: is_promote(m) ? (Piece)(piece_on(from_sq(m)) | PIECE_PROMOTE) : piece_on(from_sq(m))) << 16)
		
		
		
		
		

		
		
	);

}







template <Color Us>
void Position::do_move_impl(Move m, StateInfo& new_st, bool givesCheck)
{
	
	ASSERT_LV3(is_ok(m));

	ASSERT_LV3(&new_st != st);

	
	
	
#if !defined(USER_ENGINE)
	thisThread->nodes.fetch_add(1, std::memory_order_relaxed);
#endif

	

	
	
	

	

	
	Key64 k = st->board_key_ ^ Zobrist::side;
	Key64 h = st->hand_key_;

	
	

	
	

	
	StateInfo* prev;
	new_st.previous = prev = st;
	st = &new_st;

	

	
	++gamePly;

	
	st->pliesFromNull = prev->pliesFromNull + 1;

	

#if defined(EVAL_KPPT) || defined(EVAL_KPP_KKPT)
	st->sum.p[0][0] = VALUE_NOT_EVALUATED;
#endif
#if defined(EVAL_NNUE)
	st->accumulator.computed_accumulation = false;
	st->accumulator.computed_score = false;
#endif

#if defined(USE_BOARD_EFFECT_PREV)
	
	
	std::memcpy(board_effect_prev, board_effect, sizeof(board_effect));
#endif

	

#if defined (KEEP_LAST_MOVE)
	st->lastMove = m;
	st->lastMovedPieceType = is_drop(m) ? (PieceType)from_sq(m) : type_of(piece_on(from_sq(m)));
#endif

	
	
	

	
	Square to = to_sq(m);
	ASSERT_LV2(is_ok(to));

#if defined (USE_PIECE_VALUE)
	
	int materialDiff;
#endif

#if defined (USE_EVAL_LIST)
	auto& dp = st->dirtyPiece;
#endif

	if (is_drop(m))
	{
		

		
		ASSERT_LV2(piece_on(to) == NO_PIECE);

		Piece pc = moved_piece_after(m);
		PieceType pr = raw_type_of(pc);
		ASSERT_LV2(PAWN <= pr && pr < PIECE_HAND_NB);

		
		h -= Zobrist::hand[Us][pr];
		k += Zobrist::psq[to][pc];

		
		
		const Key key = k + h;
		prefetch(TT.first_entry(key));
#if defined(USE_EVAL_HASH)
		Eval::prefetch_evalhash(key);
#endif

		put_piece(to, pc);

		
#if defined (USE_EVAL_LIST)
		PieceNumber piece_no = piece_no_of(Us, pr);
		ASSERT_LV3(is_ok(piece_no));

		
		dp.dirty_num = 1; 
		dp.pieceNo[0] = piece_no;
		dp.changed_piece[0].old_piece = evalList.bona_piece(piece_no);
		evalList.put_piece(piece_no , to, pc);
		dp.changed_piece[0].new_piece = evalList.bona_piece(piece_no);
#endif

		
		sub_hand(hand[Us], pr);

		
		
		if (givesCheck)
		{
			st->checkersBB = Bitboard(to);
			st->continuousCheck[Us] = prev->continuousCheck[Us] + 2;

			
			
			

		} else {
			st->checkersBB = ZERO_BB;
			st->continuousCheck[Us] = 0;
		}

		
		st->capturedPiece = NO_PIECE;

		
		update_bitboards();

#if defined(USE_PIECE_VALUE)
		
		materialDiff = 0;
#endif

#if defined(LONG_EFFECT_LIBRARY)
		
		LongEffect::update_by_dropping_piece<Us>(*this, to, pc);
#endif

	} else {

		
		Square from = from_sq(m);
		ASSERT_LV2(is_ok(from));

		
		Piece moved_pc = piece_on(from);
		ASSERT_LV2(moved_pc != NO_PIECE);

		
		
		Piece moved_after_pc = moved_piece_after(m);

#if defined (USE_PIECE_VALUE)
		materialDiff = is_promote(m) ? Eval::ProDiffPieceValue[moved_pc] : 0;
#endif

		
		Piece to_pc = piece_on(to);
		if (to_pc != NO_PIECE)
		{
			

#if defined(LONG_EFFECT_LIBRARY)
	  
			LongEffect::update_by_capturing_piece<Us>(*this, from, to, moved_pc, moved_after_pc, to_pc);
#endif

			
			

			ASSERT_LV1(type_of(to_pc) != KING);

			PieceType pr = raw_type_of(to_pc);

			
#if defined (USE_EVAL_LIST)
			
			PieceNumber piece_no = piece_no_of(to);
			ASSERT_LV3(is_ok(piece_no));
			dp.dirty_num = 2; 
			dp.pieceNo[1] = piece_no;
			dp.changed_piece[1].old_piece = evalList.bona_piece(piece_no);
			evalList.put_piece(piece_no, Us, pr, hand_count(hand[Us], pr));
			dp.changed_piece[1].new_piece = evalList.bona_piece(piece_no);
#endif

			
			add_hand(hand[Us], pr);

			
			remove_piece(to);

			
			k -= Zobrist::psq[to][to_pc];
			h += Zobrist::hand[Us][pr];

			
			st->capturedPiece = to_pc;

#if defined (USE_PIECE_VALUE)
			
			materialDiff += Eval::CapturePieceValue[to_pc];
#endif

		} else {
			

			st->capturedPiece = NO_PIECE;

#if defined (LONG_EFFECT_LIBRARY)
			
			LongEffect::update_by_no_capturing_piece<Us>(*this, from, to, moved_pc, moved_after_pc);
#endif
#if defined (USE_EVAL_LIST)
			dp.dirty_num = 1; 
#endif
		}

#if defined (USE_EVAL_LIST)
		
		PieceNumber piece_no2 = piece_no_of(from);
		dp.pieceNo[0] = piece_no2;
		dp.changed_piece[0].old_piece = evalList.bona_piece(piece_no2);
#endif

		
		remove_piece(from);
		
		put_piece(to, moved_after_pc);

#if defined (USE_EVAL_LIST)
		evalList.put_piece(piece_no2, to, moved_after_pc);
		dp.changed_piece[0].new_piece = evalList.bona_piece(piece_no2);
#endif

		
		
		
		
		if (type_of(moved_pc) == KING)
			kingSquare[Us] = to;

		
		k -= Zobrist::psq[from][moved_pc];
		k += Zobrist::psq[to][moved_after_pc];

		
		const Key key = k + h;
		prefetch(TT.first_entry(key));
#if defined(USE_EVAL_HASH)
		Eval::prefetch_evalhash(key);
#endif

		
		
		update_bitboards();

		
		if (givesCheck)
		{
			
#if 1
			const StateInfo* prevSt = st->previous;

			
			st->checkersBB = prevSt->checkSquares[type_of(moved_after_pc)] & to;

			
			const Square ksq = king_square(~Us);
			
			
			

			if (discovered(from, to, ksq, prevSt->blockersForKing[~Us] & from))
			{
				auto directions = directions_of(from, ksq);
				switch (pop_directions(directions)) {

					
					
					

					
					

				case DIRECT_U: case DIRECT_D:
					st->checkersBB |= rookFileEffect(from, pieces()) & pieces(Us); break;

					
					

				case DIRECT_R: case DIRECT_L:
					st->checkersBB |= rookRankEffect(from, pieces()) & pieces(Us); break;


#if defined(USE_OLD_YANEURAOU_EFFECT)
					
					

				case DIRECT_RU: case DIRECT_LD:
					st->checkersBB |= bishopEffect0(from, pieces()) & pieces(Us); break;

				case DIRECT_RD: case DIRECT_LU:
					st->checkersBB |= bishopEffect1(from, pieces()) & pieces(Us); break;
#else

				

				
				
				
				

				case DIRECT_RU: case DIRECT_LD:
				case DIRECT_RD: case DIRECT_LU:
					st->checkersBB |= bishopEffect(ksq, pieces()) & pieces(Us, BISHOP_HORSE); break;

#endif
				default: UNREACHABLE;
				}
			}

			
			ASSERT_LV3(st->checkersBB == attackers_to(Us, king_square(~Us)));
#else
			
			st->checkersBB = attackers_to(Us, king_square(~Us));
#endif
			st->continuousCheck[Us] = prev->continuousCheck[Us] + 2;

		} else {

			st->checkersBB = ZERO_BB;
			st->continuousCheck[Us] = 0;
		}
	}
	
	st->continuousCheck[~Us] = prev->continuousCheck[~Us];

#if defined (USE_PIECE_VALUE)
	st->materialValue = (Value)(st->previous->materialValue + (Us == BLACK ? materialDiff : -materialDiff));
	
#endif

	
	sideToMove = ~Us;

	
	st->board_key_ = k;
	st->hand_key_ = h;

	st->hand = hand[sideToMove];

	
	set_check_info<false>(st);

	

	
	
}

#if defined(USE_KEY_AFTER)


Key Position::key_after(Move m) const {

	Color Us = side_to_move();
	auto k = st->board_key_ ^ Zobrist::side;
	auto h = st->hand_key_;

	
	Square to = to_sq(m);
	ASSERT_LV2(is_ok(to));

	if (is_drop(m))
	{
		
		PieceType pr = move_dropped_piece(m);
		ASSERT_LV2(PAWN <= pr && pr < PIECE_HAND_NB);

		Piece pc = make_piece(Us, pr);

		
		h -= Zobrist::hand[Us][pr];
		k += Zobrist::psq[to][pc];
	}
	else
	{
		
		Square from = from_sq(m);
		ASSERT_LV2(is_ok(from));

		
		Piece moved_pc = piece_on(from);
		ASSERT_LV2(moved_pc != NO_PIECE);

		
		
		Piece moved_after_pc;

		if (is_promote(m))
		{
			moved_after_pc = moved_pc + PIECE_PROMOTE;
		}
		else {
			moved_after_pc = moved_pc;
		}

		
		Piece to_pc = piece_on(to);
		if (to_pc != NO_PIECE)
		{
			PieceType pr = raw_type_of(to_pc);

			
			k -= Zobrist::psq[to][to_pc];
			h += Zobrist::hand[Us][pr];
		}

		
		k -= Zobrist::psq[from][moved_pc];
		k += Zobrist::psq[to][moved_after_pc];
	}

	return k + h;
}
#endif

#if defined(USE_BOARD_KEY_AFTER)

Key Position::board_key_after(Move m) const {

  Color Us = side_to_move();
  auto k = st->board_key_ ^ Zobrist::side;

  
  Square to = to_sq(m);
  ASSERT_LV2(is_ok(to));

  if (is_drop(m))
  {
    
    PieceType pr = move_dropped_piece(m);
    ASSERT_LV2(PAWN <= pr && pr < PIECE_HAND_NB);

    Piece pc = make_piece(Us, pr);

    
    k += Zobrist::psq[to][pc];
  }
  else
  {
    
    Square from = from_sq(m);
    ASSERT_LV2(is_ok(from));

    
    Piece moved_pc = piece_on(from);
    ASSERT_LV2(moved_pc != NO_PIECE);

    
    
    Piece moved_after_pc;

    if (is_promote(m))
    {
      moved_after_pc = moved_pc + PIECE_PROMOTE;
    }
    else {
      moved_after_pc = moved_pc;
    }

    
    Piece to_pc = piece_on(to);
    if (to_pc != NO_PIECE)
    {
      PieceType pr = raw_type_of(to_pc);

      
      k -= Zobrist::psq[to][to_pc];
    }

    
    k -= Zobrist::psq[from][moved_pc];
    k += Zobrist::psq[to][moved_after_pc];
  }

  return k;
}
#endif


template <Color Us>
void Position::undo_move_impl(Move m)
{
	

	auto to = to_sq(m);
	ASSERT_LV2(is_ok(to));

	

	Piece moved_after_pc = moved_piece_after(m);

#if defined (USE_EVAL_LIST)
	PieceNumber piece_no = piece_no_of(to); 
	ASSERT_LV3(is_ok(piece_no));
#endif

	
	

	
	static_assert((u32)MOVE_PROMOTE / (u32)PIECE_PROMOTE == 4096,"");
	
	Piece moved_pc = Piece(moved_after_pc ^ ((m & MOVE_PROMOTE) >> 12));

	if (is_drop(m))
	{
		

		
		PieceType pt = raw_type_of(moved_after_pc);

#if defined (USE_EVAL_LIST)
		evalList.put_piece(piece_no, Us, pt, hand_count(hand[Us], pt));
#endif

		add_hand(hand[Us], pt);

		
		remove_piece(to);

#if defined(LONG_EFFECT_LIBRARY)
		
		LongEffect::rewind_by_dropping_piece<Us>(*this, to, moved_after_pc);
#endif

	} else {

		

		auto from = from_sq(m);
		ASSERT_LV2(is_ok(from));

		
		remove_piece(to);

		
		
		
		if (st->capturedPiece != NO_PIECE)
		{
			Piece to_pc = st->capturedPiece;

			
			put_piece(to, to_pc);
			put_piece(from, moved_pc);

#if defined (USE_EVAL_LIST)
			PieceNumber piece_no2 = piece_no_of(Us, raw_type_of(to_pc)); 
			ASSERT_LV3(is_ok(piece_no2));

			evalList.put_piece(piece_no2, to, to_pc);

			
			sub_hand(hand[Us], raw_type_of(to_pc));

			
			
			
			evalList.put_piece(piece_no, from , moved_pc);
#else
			
			sub_hand(hand[Us], raw_type_of(to_pc));
#endif

#if defined(LONG_EFFECT_LIBRARY)
			
			LongEffect::rewind_by_capturing_piece<Us>(*this, from, to, moved_pc, moved_after_pc, to_pc);
#endif

		}
		else {

			put_piece(from, moved_pc);

#if defined (USE_EVAL_LIST)
			
			evalList.put_piece(piece_no, from, moved_pc);
#endif

#if defined(LONG_EFFECT_LIBRARY)
			
			
			LongEffect::rewind_by_no_capturing_piece<Us>(*this, from, to, moved_pc, moved_after_pc);
#endif
		}

		if (type_of(moved_pc) == KING)
			kingSquare[Us] = from;
	}

	
	
	update_bitboards();

	
	sideToMove = Us; 

	
	st = st->previous;

	--gamePly;

	
	
}


void Position::do_move(Move m, StateInfo& newSt, bool givesCheck)
{
	if (sideToMove == BLACK)
		do_move_impl<BLACK>(m, newSt, givesCheck);
	else
		do_move_impl<WHITE>(m, newSt, givesCheck);
}


void Position::undo_move(Move m)
{
	if (sideToMove == BLACK)
		undo_move_impl<WHITE>(m); 
	else
		undo_move_impl<BLACK>(m);
}


void Position::do_null_move(StateInfo& newSt) {

	ASSERT_LV3(!checkers());
	ASSERT_LV3(&newSt != st);

	
	
	std::memcpy(&newSt, st, sizeof(StateInfo));

	
	

	newSt.previous = st;
	st = &newSt;

#if defined(EVAL_NNUE)
	
	st->accumulator.computed_score = false;
#endif

	st->board_key_ ^= Zobrist::side;

	
	
	
	

	const Key key = st->key();
	prefetch(TT.first_entry(key));

	
	

#if defined(EVAL_NNUE) && defined(USE_EVAL_HASH)
	
	Eval::prefetch_evalhash(key);
#endif

	

	st->pliesFromNull = 0;

	sideToMove = ~sideToMove;

	set_check_info<true>(st);

	

	

}

void Position::undo_null_move()
{
	ASSERT_LV3(!checkers());

	st = st->previous;
	sideToMove = ~sideToMove;
}

void Position::steal_hand(PieceType pr) {
	auto us = side_to_move();
	auto them = ~us;
	auto& our_hand = hand[us];
	auto& their_hand = hand[them];

	if (hand_exists(their_hand, pr)) {
		auto h = st->hand_key_;
		sub_hand(their_hand, pr);
		h -= Zobrist::hand[them][pr];

		add_hand(our_hand, pr);
		h += Zobrist::hand[us][pr];

		st->hand_key_ = h;
		st->hand = our_hand;
	}
}

void Position::give_hand(PieceType pr) {
	auto us = side_to_move();
	auto them = ~us;
	auto& our_hand = hand[us];
	auto& their_hand = hand[them];

	if (hand_exists(our_hand, pr)) {
		auto h = state()->hand_key_;
		sub_hand(our_hand, pr);
		h -= Zobrist::hand[us][pr];

		add_hand(their_hand, pr);
		h += Zobrist::hand[them][pr];

		st->hand_key_ = h;
		st->hand = our_hand;
	}
}


#if defined (USE_SEE)

namespace {

	using namespace Eval;
	using namespace Effect8;

	
	
	

	
	

	
	
	
	
	
	

	

	PieceType min_attacker(const Position& pos, const Square& to
		, const Bitboard& stmAttackers, Bitboard& occupied, Bitboard& attackers
	) {

		
	

		
		

		Bitboard b;
		b = stmAttackers & pos.pieces(PAWN);   if (b) goto found;
		b = stmAttackers & pos.pieces(LANCE);  if (b) goto found;
		b = stmAttackers & pos.pieces(KNIGHT); if (b) goto found;
		b = stmAttackers & pos.pieces(SILVER); if (b) goto found;
		b = stmAttackers & pos.pieces(GOLDS);  if (b) goto found;
		b = stmAttackers & pos.pieces(BISHOP); if (b) goto found;
		b = stmAttackers & pos.pieces(ROOK);   if (b) goto found;
		b = stmAttackers & pos.pieces(HORSE);  if (b) goto found;
		b = stmAttackers & pos.pieces(DRAGON); if (b) goto found;

		
		
		

		return KING;

	found:;

		

		Square sq = b.pop();
		occupied ^= sq;

		
		

		

		auto dirs = directions_of(to, sq);
		if (dirs) switch (pop_directions(dirs))
		{

#if defined(USE_OLD_YANEURAOU_EFFECT)
		case DIRECT_RU: case DIRECT_LD:
			
			attackers |= bishopEffect0(to, occupied) & pos.pieces(BISHOP_HORSE);

			ASSERT_LV3((bishopStepEffect(to) & sq));
			break;

		case DIRECT_RD: case DIRECT_LU:
			attackers |= bishopEffect1(to, occupied) & pos.pieces(BISHOP_HORSE);
			ASSERT_LV3((bishopStepEffect(to) & sq));
			break;
#else
		
		

		
		case DIRECT_RU: case DIRECT_LD:
		case DIRECT_RD: case DIRECT_LU:
			attackers |= bishopEffect(to, occupied) & pos.pieces(BISHOP_HORSE);
			ASSERT_LV3((bishopStepEffect(to) & sq));
			break;

#endif
		case DIRECT_U:
			
			attackers |= lanceEffect(BLACK, to, occupied) & (pos.pieces(ROOK_DRAGON) | pos.pieces(WHITE, LANCE));

			ASSERT_LV3((lanceStepEffect(BLACK, to) & sq));
			break;

		case DIRECT_D:
			
			attackers |= lanceEffect(WHITE, to, occupied) & (pos.pieces(ROOK_DRAGON) | pos.pieces(BLACK, LANCE));

			ASSERT_LV3((lanceStepEffect(WHITE, to) & sq));
			break;

		case DIRECT_L: case DIRECT_R:
			

#if defined(USE_OLD_YANEURAOU_EFFECT)
			attackers |= rookRankEffect(to, occupied) & pos.pieces(ROOK_DRAGON);
#else
			attackers |= rookEffect(to, occupied) & pos.pieces(ROOK_DRAGON);
#endif

			ASSERT_LV3(((rookStepEffect(to) & sq)));
			break;

		default:
			UNREACHABLE;
		}
		else {
			
			ASSERT_LV3(!(bishopStepEffect(to) & sq));
			ASSERT_LV3(!((rookStepEffect(to) & sq)));
		}

		
		
		attackers &= occupied;

		
		
		return type_of(pos.piece_on(sq));
	}

} 

























bool Position::see_ge(Move m, Value threshold) const
{
	

	

	bool drop = is_drop(m);
	
	
	Square from = drop ? SQ_NB : from_sq(m);
	Square to = to_sq(m);

	
	
	PieceType nextVictim = drop ? move_dropped_piece(m) : type_of(piece_on(from));

	
	
	
	
	Color us = color_of(moved_piece_after(m));
	Color stm = ~us;

	
	
	Value balance = (Value)Eval::CapturePieceValue[piece_on(to)] - threshold;

	
	if (balance < VALUE_ZERO)
		return false;

	
	
	

	balance -= (Value)Eval::CapturePieceValue[nextVictim];

	if (balance >= VALUE_ZERO)
		return true;

	
	bool relativeStm = true;

	
	Bitboard stmAttackers;

	
	
	Bitboard occupied = pieces() ^ from ^ to;

	
	Bitboard attackers = attackers_to(to, occupied) & occupied;

	while (true)
	{
		stmAttackers = attackers & pieces(stm);

		

		if (!(st->pinners[~stm] & occupied))
			stmAttackers &= ~st->blockersForKing[stm];

		
		if (!stmAttackers)
			break;

		

		nextVictim = min_attacker(*this, to, stmAttackers, occupied, attackers);

		stm = ~stm; 

		
		
		
		
		
		ASSERT_LV3(balance < VALUE_ZERO);

		balance = -balance - 1 - CapturePieceValue[nextVictim];

		
		
		if (balance >= VALUE_ZERO)
		{
			if (nextVictim == KING && (attackers & pieces(stm)))
				stm = ~stm;
			break;
		}
		ASSERT_LV3(nextVictim != KING);
	}
	return us != stm; 
}

#endif 






RepetitionState Position::is_repetition(int repPly ) const
{
	
	

	
	
	
	
	

	
	
	
	
	
	

	

	
	ASSERT_LV3(st->pliesFromNull >= 0);

	
	
	int end = std::min(repPly, st->pliesFromNull);

	
	if (end < 4)
		return REPETITION_NONE;

	StateInfo* stp = st->previous->previous;

	for (int i = 4; i <= end ; i += 2)
	{
		stp = stp->previous->previous;

		
		
		if (stp->board_key() == st->board_key())
		{
			
			if (stp->hand == st->hand)
			{
				
				if (i <= st->continuousCheck[sideToMove])
					return REPETITION_LOSE;

				
				if (i <= st->continuousCheck[~sideToMove])
					return REPETITION_WIN;

				return REPETITION_DRAW;
			}
			else {
				
				if (hand_is_equal_or_superior(st->hand, stp->hand))
					return REPETITION_SUPERIOR;
				if (hand_is_equal_or_superior(stp->hand, st->hand))
					return REPETITION_INFERIOR;
			}
		}
	}

	
	return REPETITION_NONE;
}

#if defined(CUCKOO)


bool Position::has_game_cycle(int plies_from_root, int rep_ply ) const
{
	int j;

	int end = std::min( rep_ply , st->pliesFromNull);

	if (end < 3)
		return false;

	Key originalKey = st->key();
	StateInfo* stp = st->previous;

	for (int i = 3; i <= end; i += 2)
	{
		stp = stp->previous->previous;

		
		
		Key moveKey = (stp->key() - originalKey) >> 1;
		if ((j = H1(moveKey), cuckoo[j] == moveKey)
			|| (j = H2(moveKey), cuckoo[j] == moveKey))
		{
			Move move = cuckooMove[j];
			Square s1 = from_sq(move);
			Square s2 = to_sq(move);
			Piece pc = (Piece)(move >> 16);
			if (piece_on(s1) != pc)
				continue;

			
			

			
			

			if (!(between_bb(s1, s2) & pieces()))
			{
				if (plies_from_root > i)
					return true;

				
				

				
				
				

				
				StateInfo* next_stp = stp;
				for (int k = i + 2; k <= end; k += 2)
				{
					next_stp = next_stp->previous->previous;
					if (next_stp->key() == stp->key())
						return true;
				}

			}
		}
	}
	return false;
}
#endif





#if defined(USE_ENTERING_KING_WIN)
Move Position::DeclarationWin() const
{
	auto rule = Search::Limits.enteringKingRule;

	switch (rule)
	{
		
	case EKR_NONE: return MOVE_NONE;

		
		
		
	case EKR_24_POINT: 
	case EKR_27_POINT: 
	{
		
		
		

		Color us = sideToMove;

		
		Bitboard ef = enemy_field(us);

		
		if (!(ef & king_square(us)))
			return MOVE_NONE;

		
		if (checkers())
			return MOVE_NONE;


		
		int p1 = (pieces(us) & ef).pop_count();
		
		if (p1 < 11)
			return MOVE_NONE;

		
		int p2 = ((pieces(us, BISHOP_HORSE, ROOK_DRAGON)) & ef).pop_count();

		
		

		
		
		
		Hand h = hand[us];
		int score = p1 + p2 * 4 - 1
			+ hand_count(h, PAWN) + hand_count(h, LANCE) + hand_count(h, KNIGHT) + hand_count(h, SILVER)
			+ hand_count(h, GOLD) + (hand_count(h, BISHOP) + hand_count(h, ROOK)) * 5;

		
		if (score < (rule == EKR_27_POINT ? (us == BLACK ? 28 : 27) : 31))
			return MOVE_NONE;

		
		return MOVE_WIN;
	}

	
	case EKR_TRY_RULE:
	{
		Color us = sideToMove;
		Square king_try_sq = (us == BLACK ? SQ_51 : SQ_59);
		Square king_sq = king_square(us);

		
		if (!(kingEffect(king_sq) & king_try_sq))
			return MOVE_NONE;

		
		if (pieces(us) & king_try_sq)
			return MOVE_NONE;

		
		if (effected_to(~us, king_try_sq, king_sq))
			return MOVE_NONE;

		
		return make_move(king_sq, king_try_sq, us,KING);
	}

	default:
		UNREACHABLE;
		return MOVE_NONE;
	}
}
#endif





bool Position::pos_is_ok() const
{
	

#if 0
	
	

	
	const int ptc0[KING] = { 2,18,4,4,4,2,2,4 };
	
	int ptc[PIECE_WHITE] = { 0 };

	int count = 0;
	for (auto sq : SQ)
	{
		Piece pc = piece_on(sq);
		if (pc != NO_PIECE)
		{
			++count;
			++ptc[raw_type_of(pc)];
		}
	}
	for (auto c : COLOR)
		for (Piece pr = PIECE_HAND_ZERO; pr < PIECE_HAND_NB; ++pr)
		{
			int ct = hand_count(hand[c], pr);
			count += ct;
			ptc[pr] += ct;
		}

	if (count != 40)
		return false;

	
	for (Piece pt = PIECE_ZERO; pt < KING; ++pt)
		if (ptc[pt] != ptc0[pt])
			return false;
#endif

	
	if (st->checkersBB != attackers_to(~sideToMove, king_square(sideToMove)))
		return false;

	
	if (effected_to(sideToMove, king_square(~sideToMove)))
		return false;

	
	if ((pieces() != (pieces(BLACK) | pieces(WHITE))) || (pieces(BLACK) & pieces(WHITE)))
		return false;

	
	if (checkers() & pieces(side_to_move()))
		return false;

	

	return true;
}


template bool Position::pseudo_legal_s<false>(const Move m) const;
template bool Position::pseudo_legal_s< true>(const Move m) const;
