#ifndef THREAD_H_INCLUDED
#define THREAD_H_INCLUDED

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include "movepick.h"
#include "position.h"
#include "search.h"
#include "thread_win32_osx.h"

#if defined(EVAL_LEARN)
#include "tt.h"
#endif


class Thread
{
	std::mutex mutex;

	std::condition_variable cv;

	size_t idx;

	bool exit = false , searching = true;

	NativeThread stdThread;

public:

	explicit Thread(size_t n);
	virtual ~Thread();

	virtual void search();

	void clear();

	void idle_loop();


	void start_searching();

	void wait_for_search_finished();


	bool is_searching() const { return searching; }


	size_t pvIdx ;

	uint64_t ttHitAverage;

	int selDepth ,nmpMinPly;
	Color nmpColor;

	std::atomic<uint64_t> nodes, bestMoveChanges;


	Position rootPos;

	StateInfo rootState;

	Search::RootMoves rootMoves;

	Depth rootDepth, completedDepth;

#if defined(USE_MOVE_PICKER)
	CounterMoveHistory counterMoves;
	LowPlyHistory lowPlyHistory;
	ButterflyHistory mainHistory;
	CapturePieceToHistory captureHistory;

	ContinuationHistory continuationHistory[2][2];
#endif


	int failedHighCnt;


	size_t thread_id() const { return idx; }

#if defined(EVAL_LEARN)
	TranspositionTable tt;
#endif

};
  

struct MainThread: public Thread
{
	using Thread::Thread;

	void search() override;

	void thread_search() { Thread::search(); }

	void check_time();

	double previousTimeReduction;

	Value bestPreviousScore;

	Value iterValue[4];

	int callsCnt;


	std::atomic_bool ponder;


	TimePoint lastPvInfoTime;

	Move ponder_candidate;

	std::string game_root_sfen;

	std::vector<Move> moves_from_game_root;

	std::string last_position_cmd_string = "position startpos";
	std::string last_go_cmd_string;
	bool position_is_dirty = false;
};


struct ThreadPool: public std::vector<Thread*>
{
	void start_thinking(const Position& pos, StateListPtr& states , const Search::LimitsType& limits , bool ponderMode = false);

	void clear();

	void set(size_t requested);

	MainThread* main() { return static_cast<MainThread*>(at(0)); }

	uint64_t nodes_searched() { return accumulate(&Thread::nodes); }

	Thread* get_best_thread() const;

	void start_searching();

	void wait_for_search_finished() const;

	std::atomic_bool stop , increaseDepth;


	bool search_finished() const;
	
private:

	StateListPtr setupStates;

	uint64_t accumulate(std::atomic<uint64_t> Thread::* member) const {

		uint64_t sum = 0;
		for (Thread* th : *this)
			sum += (th->*member).load(std::memory_order_relaxed);
		return sum;
	}
};

extern ThreadPool Threads;

#endif 

