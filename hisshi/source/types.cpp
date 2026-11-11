#include "types.h"
#include "usi.h"
#include "search.h"
#include "tt.h"


const char* USI_PIECE = ". P L N S B R G K +P+L+N+S+B+R+G+.p l n s b r g k +p+l+n+s+b+r+g+k";


namespace Effect8 {
	Directions direc_table[SQ_NB_PLUS1][SQ_NB_PLUS1];
}

std::string PieceToCharBW(" PLNSBRGK        plnsbrgk");


std::string pretty(File f) { return pretty_jp ? std::string("１２３４５６７８９").substr((size_t)f * 2, 2) : std::to_string((size_t)f + 1); }
std::string pretty(Rank r) { return pretty_jp ? std::string("一二三四五六七八九").substr((size_t)r * 2, 2) : std::to_string((size_t)r + 1); }

std::string pretty(Move m)
{
	if (is_drop(m))
		return pretty(to_sq(m)  ) + pretty2(Piece(from_sq(m))) + (pretty_jp ? "打" : "*");
	else
		return pretty(from_sq(m)) + pretty(to_sq(m))           + (is_promote(m) ? (pretty_jp ? "成" : "+") : "");
}

std::string pretty(Move m, Piece movedPieceType)
{
	if (is_drop(m))
		return pretty(to_sq(m)) + pretty2(movedPieceType) + (pretty_jp ? "打" : "*");
	else
		return pretty(to_sq(m)) + pretty2(movedPieceType) + (is_promote(m) ? (pretty_jp ? "成" : "+") : "") + "[" + pretty(from_sq(m)) + "]";
}

std::string to_usi_string(Move   m){ return USI::move(m); }
std::string to_usi_string(Move16 m){ return USI::move(m); }

std::ostream& operator<<(std::ostream& os, Color c) { os << ((c == BLACK) ? (pretty_jp ? "先手" : "BLACK") : (pretty_jp ? "後手" : "WHITE")); return os; }

std::ostream& operator<<(std::ostream& os, Piece pc)
{
	auto s = usi_piece(pc);
	if (s[1] == ' ') s.resize(1); 
	os << s;
	return os;
}

std::ostream& operator<<(std::ostream& os, Hand hand)
{
	for (PieceType pr = PAWN; pr < PIECE_HAND_NB; ++pr)
	{
		int c = hand_count(hand, pr);
		if (c != 0)
		{
			const std::string cs = (c != 1) ? std::to_string(c) : "";
			os << (pretty_jp ? "" : cs) << pretty(pr) << (pretty_jp ? cs : "");
		}
	}
	return os;
}

std::string to_usi_string(RepetitionState rs)
{
#if !defined(PV_OUTPUT_DRAW_ONLY)
	return ((rs == REPETITION_NONE) ? "rep_none" : 
		(rs == REPETITION_WIN)      ? "rep_win" :
		(rs == REPETITION_LOSE)     ? "rep_lose" :
		(rs == REPETITION_DRAW)     ? "rep_draw" :
		(rs == REPETITION_SUPERIOR) ? "rep_sup" :
		(rs == REPETITION_INFERIOR) ? "rep_inf" :
		"")
		;
#else
	return "rep_draw";
#endif
}

std::ostream& operator<<(std::ostream& os, RepetitionState rs)
{
	os << to_usi_string(rs);
	return os;
}


namespace Search {
	LimitsType Limits;


	bool RootMove::extract_ponder_from_tt(Position& pos, Move ponder_candidate)
	{
		StateInfo st;
		bool ttHit;


		if (!is_ok(pv[0]))
			return false;

		pos.do_move(pv[0], st, pos.gives_check(pv[0]));
		TTEntry* tte = TT.read_probe(pos.state()->key(), ttHit);
		Move m;
		if (ttHit)
		{
			m = pos.to_move(tte->move()); 
			if (MoveList<LEGAL_ALL>(pos).contains(m))
				goto FOUND;
		}
		m = ponder_candidate;
		if (MoveList<LEGAL_ALL>(pos).contains(m))
			goto FOUND;

		pos.undo_move(pv[0]);
		return false;
	FOUND:;
		pos.undo_move(pv[0]);
		pv.push_back(m);
		return true;
	}
}

Value drawValueTable[REPETITION_NB][COLOR_NB] =
{
	{  VALUE_ZERO        ,  VALUE_ZERO        }, 
	{  VALUE_MATE        ,  VALUE_MATE        }, 
	{ -VALUE_MATE        , -VALUE_MATE        }, 
	{  VALUE_ZERO        ,  VALUE_ZERO        }, 
	{  VALUE_SUPERIOR    ,  VALUE_SUPERIOR    }, 
	{ -VALUE_SUPERIOR    , -VALUE_SUPERIOR    }, 
};

#if defined(USE_GLOBAL_OPTIONS)
GlobalOptions_ GlobalOptions;
#endif
