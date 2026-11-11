#ifndef _SEARCH_H_INCLUDED_
#define _SEARCH_H_INCLUDED_

#include "config.h"
#include "misc.h"
#include "movepick.h"
#include "position.h"


namespace Search {

#if defined(USE_MOVE_PICKER)
	
	constexpr int CounterMovePruneThreshold = 0;

	
	
	

	struct Stack {
		Move* pv;					
		PieceToHistory* continuationHistory;
		int ply;					
		Move currentMove;			
		Move excludedMove;			
		Move killers[2];			
		Value staticEval;			
		int statScore;				
		int moveCount;				

		bool inCheck;				
		bool ttPv;					
		bool ttHit;					
	};
#endif

	
	
	
	struct RootMove
	{
		
		explicit RootMove(Move m) : pv(1, m) {}

		
		
		
		
		bool extract_ponder_from_tt(Position& pos, Move ponder_candidate);

		
		bool operator==(const Move& m) const { return pv[0] == m; }

		
		
		bool operator<(const RootMove& m) const {
			return m.score != score ? m.score < score
								    : m.previousScore < previousScore;
		}

		
		Value score = -VALUE_INFINITE;

		
		
		Value previousScore = -VALUE_INFINITE;

		
		int selDepth = 0;

		
		
		

		
		std::vector<Move> pv;
	};

	typedef std::vector<RootMove> RootMoves;

	
	
	struct LimitsType {

		
		LimitsType() {
			time[WHITE] = time[BLACK] = inc[WHITE] = inc[BLACK] = npmsec = movetime = TimePoint(0);
			depth = mate = perft = infinite = 0;
			nodes = 0;

			

			byoyomi[WHITE] = byoyomi[BLACK] = TimePoint(0);
			max_game_ply = 100000;
			rtime = 0;
			enteringKingRule = EKR_NONE;
			silent = bench = consideration_mode = outout_fail_lh_pv = false;
			pv_interval = 0;
#if defined(USE_GENERATE_ALL_LEGAL_MOVES)
			generate_all_legal_moves = false;
#endif
		}

		
		
		
		bool use_time_management() const {
			return !(mate | movetime | depth | nodes | perft | infinite);
		}

		
		std::vector<Move> searchmoves;

		
		
		
		
		
		TimePoint time[COLOR_NB] , inc[COLOR_NB] , npmsec , movetime;

		
		
		
		
		
		
		
		
		
		int depth , mate, perft, infinite;

		
		int64_t nodes;

		

		
		TimePoint byoyomi[COLOR_NB];

		
		
		
		
		
		
		
		int max_game_ply;

		
		TimePoint rtime;

		
		EnteringKingRule enteringKingRule;

		
		
		bool silent;

		
		bool consideration_mode;

		
		bool outout_fail_lh_pv;

		
		bool bench;

		
		TimePoint pv_interval;

#if defined (USE_GENERATE_ALL_LEGAL_MOVES)
		
		
		bool generate_all_legal_moves;
#endif

#if defined(TANUKI_MATE_ENGINE)
		std::vector<Move16> pv_check;
#endif
	};

	extern LimitsType Limits;

	
	void init();

	
	
	void clear();

} 

#endif 

