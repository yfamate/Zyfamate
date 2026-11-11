#ifndef MOVEPICK_H_INCLUDED
#define MOVEPICK_H_INCLUDED

#include "config.h"
#if defined(USE_MOVE_PICKER)

#include <array>
#include <limits>

#include "types.h"



template<typename T, int D>
class StatsEntry {

	T entry;

public:
	void operator=(const T& v) { entry = v; }
	T* operator&() { return &entry; }
	T* operator->() { return &entry; }
	operator const T&() const { return entry; }

	void operator<<(int bonus) {
		ASSERT_LV3(abs(bonus) <= D); 
		static_assert(D <= std::numeric_limits<T>::max(), "D overflows T");


		entry += bonus - entry * abs(bonus) / D;


		ASSERT_LV3(abs(entry) <= D);
	}
};

template <typename T, int D, int Size, int... Sizes>
struct Stats : public std::array<Stats<T, D, Sizes...>, Size>
{
	typedef Stats<T, D, Size, Sizes...> stats;

	void fill(const T& v) {

		ASSERT_LV3(std::is_standard_layout<stats>::value);
		
		typedef StatsEntry<T, D> entry;
		entry* p = reinterpret_cast<entry*>(this);
		std::fill(p, p + sizeof(*this) / sizeof(entry), v);
	}
};

template <typename T, int D, int Size>
struct Stats<T, D, Size> : public std::array<StatsEntry<T, D>, Size> {};

enum StatsParams { NOT_USED = 0 };
enum StatsType { NoCaptures, Captures };

typedef Stats<int16_t, 13365, int(SQ_NB + 7) * int(SQ_NB) , COLOR_NB> ButterflyHistory;


constexpr int MAX_LPH = 4; 
typedef Stats<int16_t, 10692, MAX_LPH,int(SQ_NB + 7)* int(SQ_NB)> LowPlyHistory;

typedef Stats<Move, NOT_USED, SQ_NB , PIECE_NB> CounterMoveHistory;

typedef Stats<int16_t, 10692, SQ_NB, PIECE_NB , PIECE_TYPE_NB> CapturePieceToHistory;

typedef Stats<int16_t, 29952, SQ_NB , PIECE_NB> PieceToHistory;

typedef Stats<PieceToHistory, NOT_USED, SQ_NB , PIECE_NB> ContinuationHistory;



class MovePicker
{
	enum PickType { Next, Best };

public:
	MovePicker(const MovePicker&) = delete;
	MovePicker& operator=(const MovePicker&) = delete;

	MovePicker(const Position& pos_, Move ttMove_, Value threshold_ ,
		const CapturePieceToHistory* cph);

	MovePicker(const Position& pos_, Move ttMove_, Depth depth_, const ButterflyHistory* mh ,
		const CapturePieceToHistory* cph , 
		const PieceToHistory** ch,
		Square recapSq);

	MovePicker(const Position& pos_, Move ttMove_, Depth depth_, const ButterflyHistory* mh,
		const LowPlyHistory* lph,
		const CapturePieceToHistory* cph ,
		const PieceToHistory** ch,
		Move cm,
		const Move* killers_p,
		int ply);


	Move next_move(bool skipQuiets = false);

private:
	template <PickType T, typename Pred> Move select(Pred);

	template<MOVE_GEN_TYPE> void score();

	ExtMove* begin() { return cur; }
	ExtMove* end() { return endMoves; }

	const Position& pos;

	const ButterflyHistory* mainHistory;
	const LowPlyHistory* lowPlyHistory;
	const CapturePieceToHistory* captureHistory;
	const PieceToHistory** continuationHistory;

	Move ttMove;

	ExtMove refutations[3] , *cur, *endMoves, *endBadCaptures;

	int stage;


	Square recaptureSquare;

	Value threshold;

	Depth depth;

	int ply;

	ExtMove moves[MAX_MOVES + 11];
};

#endif 

#endif 

