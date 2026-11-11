#include "movepick.h"
#if defined(USE_MOVE_PICKER)

#include "thread.h"


#include "engine/yaneuraou-engine/yaneuraou-param-common.h"

#if defined(USE_SUPER_SORT) && defined(USE_AVX2)

extern void partial_super_sort(ExtMove* start, ExtMove* end, int limit);


#endif

namespace {








constexpr Value LVATable[PIECE_WHITE] = {
  Value(0), Value(1) , Value(2), Value(3), Value(4), Value(7), Value(8), Value(6),
  Value(10000), Value(5), Value(5), Value(5), Value(5), Value(9), Value(10),Value(11)
};
constexpr Value LVA(const PieceType pt) { return LVATable[pt]; }
  





enum Stages: int {

	
	
	

	MAIN_TT,					
	CAPTURE_INIT,				
	GOOD_CAPTURE,				
	REFUTATION,					
	QUIET_INIT,					
	QUIET_,						
	BAD_CAPTURE,				

	
	
	

	
	
	

	EVASION_TT,					
	EVASION_INIT,				
	EVASION_,					

	
	
	

	PROBCUT_TT,					
	PROBCUT_INIT,				
	PROBCUT_,					

	
	
	

	QSEARCH_TT,					
	QCAPTURE_INIT,				
	QCAPTURE_,					
	QCHECK_INIT,				
	QCHECK_						
};










void partial_insertion_sort(ExtMove* begin, ExtMove* end, int limit) {

	for (ExtMove *sortedEnd = begin, *p = begin + 1; p < end; ++p)
		if (p->value >= limit)
		{
			ExtMove tmp = *p, *q;
			*p = *++sortedEnd;
			for (q = sortedEnd; q != begin && *(q - 1) < tmp; --q)
				*q = *(q - 1);
			*q = tmp;
		}
}





bool pseudo_legal(const Position& pos, Move ttm)
{
	
	return Search::Limits.generate_all_legal_moves ? pos.pseudo_legal_s<true>(ttm) : pos.pseudo_legal_s<false>(ttm);
}


} 




MovePicker::MovePicker(const Position& p, Move ttm, Depth d, const ButterflyHistory* mh, const LowPlyHistory* lp,
	const CapturePieceToHistory* cph , const PieceToHistory** ch, Move cm, const Move* killers,int pl)
	: pos(p), mainHistory(mh), lowPlyHistory(lp),captureHistory(cph) , continuationHistory(ch),
	ttMove(ttm), refutations{ { killers[0], 0 },{ killers[1], 0 },{ cm, 0 } }, depth(d), ply(pl)
{
	
	ASSERT_LV3(d > 0);

	
	
	stage = (pos.in_check() ? EVASION_TT : MAIN_TT) +
		!(ttm && pseudo_legal(pos,ttm));

	
	
}



MovePicker::MovePicker(const Position& p, Move ttm, Depth d, const ButterflyHistory* mh,
						const CapturePieceToHistory* cph, const PieceToHistory** ch, Square rs)
	: pos(p), mainHistory(mh), captureHistory(cph) , continuationHistory(ch) , ttMove(ttm), recaptureSquare(rs), depth(d) {

	
	ASSERT_LV3(d <= 0);

	
	

	stage = (pos.in_check() ? EVASION_TT : QSEARCH_TT) +
		!(ttm
			&& (pos.in_check() || depth > DEPTH_QS_RECAPTURES || to_sq(ttm) == recaptureSquare)
			&& pseudo_legal(pos,ttm));

}



MovePicker::MovePicker(const Position& p, Move ttm, Value th , const CapturePieceToHistory* cph)
			: pos(p), captureHistory(cph) , ttMove(ttm),threshold(th) {

	ASSERT_LV3(!pos.in_check());

	
	
	
	stage = PROBCUT_TT + !(ttm && pos.capture(ttm)
		&& pseudo_legal(pos,ttm)
		&& pos.see_ge(ttm, threshold));

}


template<MOVE_GEN_TYPE Type>
void MovePicker::score()
{
	static_assert(Type == CAPTURES || Type == QUIETS || Type == EVASIONS, "Wrong type");

	for (auto& m : *this)
	{
		if (Type == CAPTURES)
		{
			
			
			
			
			
			
			

			

			m.value = int(Eval::CapturePieceValue[pos.piece_on(to_sq(m))])*6
					+ (*captureHistory)[to_sq(m)][pos.moved_piece_after(m)][type_of(pos.piece_on(to_sq(m)))];
		}
		else if (Type == QUIETS)
		{
			
			
			
			

			Piece movedPiece = pos.moved_piece_after(m);
			Square movedSq = to_sq(m);

#if 0
			m.value =     (*mainHistory)[from_to(m)][pos.side_to_move()]
					+ 2 * (*continuationHistory[0])[movedSq][movedPiece]
					+ 2 * (*continuationHistory[1])[movedSq][movedPiece]
					+ 2 * (*continuationHistory[3])[movedSq][movedPiece]
					+     (*continuationHistory[5])[movedSq][movedPiece]
					+ (ply < MAX_LPH ? std::min(4, depth / 3) * (*lowPlyHistory)[ply][from_to(m)] : 0);
#endif

			
			m.value = 16 * (*mainHistory)[from_to(m)][pos.side_to_move()]
				+ MOVE_PICKER_Q_PARAM1 * (*continuationHistory[0])[movedSq][movedPiece]
				+ MOVE_PICKER_Q_PARAM2 * (*continuationHistory[1])[movedSq][movedPiece]
				+ MOVE_PICKER_Q_PARAM3 * (*continuationHistory[3])[movedSq][movedPiece]
				+ MOVE_PICKER_Q_PARAM4 * (*continuationHistory[5])[movedSq][movedPiece]
				+ MOVE_PICKER_Q_PARAM5 * (ply < MAX_LPH ? std::min(4, depth / 3) * (*lowPlyHistory)[ply][from_to(m)] : 0);

		}
		else 
		{
			

			
			
			

			
			
			
			
			
			

			
			
			
			
			
			
			
			

			if (pos.capture(m))
				
				m.value = (Value)Eval::CapturePieceValue[pos.piece_on(to_sq(m))]
				        - (Value)(LVA(type_of(pos.moved_piece_before(m))));
			else
				
				m.value = (*mainHistory)[from_to(m)][pos.side_to_move()]
						+ (*continuationHistory[0])[to_sq(m)][pos.moved_piece_after(m)]
						- (1 << 28);

		}
	}
}





template<MovePicker::PickType T, typename Pred>
Move MovePicker::select(Pred filter) {

	while (cur < endMoves)
	{
		
		
		if (T == Best)
			std::swap(*cur, *std::max_element(cur, endMoves));

		
		if (*cur != ttMove && filter())
			return *cur++;
		cur++;
	}
	return MOVE_NONE;
}





Move MovePicker::next_move(bool skipQuiets) {

top:
	switch (stage) {

	
	case MAIN_TT:
	case EVASION_TT:
	case QSEARCH_TT:
	case PROBCUT_TT:
		++stage;
		return ttMove;

	
	case CAPTURE_INIT:
	case PROBCUT_INIT:
	case QCAPTURE_INIT:
		cur = endBadCaptures = moves;

		endMoves = Search::Limits.generate_all_legal_moves ? generateMoves<CAPTURES_PRO_PLUS_ALL>(pos, cur) : generateMoves<CAPTURES_PRO_PLUS>(pos, cur);

		
		score<CAPTURES>();

		++stage;
		goto top;

	
	
	
	case GOOD_CAPTURE:
		if (select<Best>([&]() {
				
				return pos.see_ge(*cur, Value(-69 * cur->value / 1024)) ?
						
						true : (*endBadCaptures++ = *cur, false); }))
			return *(cur -1);

			
			cur = std::begin(refutations);
			endMoves = std::end(refutations);

			
			
			if (   refutations[0].move == refutations[2].move
				|| refutations[1].move == refutations[2].move)
				--endMoves;

			++stage;
			[[fallthrough]];

	
	case REFUTATION:

		
		
		
		
		if (select<Next>([&]() { return    *cur != MOVE_NONE
										&& !pos.capture_or_pawn_promotion(*cur)
										&&  pseudo_legal(pos,*cur); }))
			return *(cur - 1);

		++stage;
		[[fallthrough]];

	
	case QUIET_INIT:

		if (!skipQuiets)
		{
			cur = endBadCaptures;

#if defined(USE_AVX2)
			
			
			
			cur = (ExtMove*)Math::align((size_t)cur, 32);
#endif

			endMoves = Search::Limits.generate_all_legal_moves ? generateMoves<NON_CAPTURES_PRO_MINUS_ALL>(pos, cur) : generateMoves<NON_CAPTURES_PRO_MINUS>(pos, cur);

			
			score<QUIETS>();

			
			

#if defined(USE_SUPER_SORT) && defined(USE_AVX2)

			
			
			
			
			
			partial_super_sort(cur, endMoves , -3000 * depth);

			

#else

			
			
			
			partial_insertion_sort(cur, endMoves, -3000 * depth);
#endif
		}

		++stage;
		[[fallthrough]];

	
	
	
	case QUIET_:
		if (   !skipQuiets
			&& select<Next>([&]() {return  *cur != refutations[0].move
										&& *cur != refutations[1].move
										&& *cur != refutations[2].move; }))

			return *(cur - 1);

		
		
		cur = moves;
		endMoves = endBadCaptures;

		++stage;
		

	
	case BAD_CAPTURE:
		return select<Next>([]() { return true; });

	
	case EVASION_INIT:
		cur = moves;

		endMoves = Search::Limits.generate_all_legal_moves ? generateMoves<EVASIONS_ALL>(pos, cur) : generateMoves<EVASIONS>(pos, cur);

		
		score<EVASIONS>();

		++stage;
		[[fallthrough]];

	
	case EVASION_:
		
		return select<Best>([](){ return true; });

	
	case PROBCUT_:
		return select<Best>([&]() { return pos.see_ge(*cur, threshold); });
		

	
	case QCAPTURE_:
		
		if (select<Best>([&]() { return    depth > DEPTH_QS_RECAPTURES
										|| to_sq(*cur) == recaptureSquare; }))
			return *(cur - 1);

		
		
		if (depth != DEPTH_QS_CHECKS)
			return MOVE_NONE;

		++stage;
		[[fallthrough]];

	
	case QCHECK_INIT:
		
		
		
		cur = moves;

		endMoves = Search::Limits.generate_all_legal_moves ? generateMoves<QUIET_CHECKS_ALL>(pos, cur) : generateMoves<QUIET_CHECKS>(pos, cur);

		++stage;
		[[fallthrough]];

	
	case QCHECK_:
		
		return select<Next>([&]() { return !pos.pawn_promotion(*cur); });

	default:
		UNREACHABLE;
		return MOVE_NONE;
	}

	ASSERT(false);
	return MOVE_NONE;
}

#endif 
