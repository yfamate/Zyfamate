#include <algorithm> 

#include "thread.h"
#include "usi.h"

ThreadPool Threads;		

Thread::Thread(size_t n) : idx(n) , stdThread(&Thread::idle_loop, this)
{
	wait_for_search_finished();
}

Thread::~Thread()
{
	ASSERT_LV3(!searching);

	exit = true;
	start_searching();
	stdThread.join();
}

void Thread::clear()
{
#if defined(USE_MOVE_PICKER)
	counterMoves.fill(MOVE_NONE);
	mainHistory.fill(0);
	lowPlyHistory.fill(0);
	captureHistory.fill(0);


	for (bool inCheck : { false, true })
		for (StatsType c : { NoCaptures, Captures })
		{
			for (auto& to : continuationHistory[inCheck][c])
				for (auto& h : to)
					h->fill(0);
			continuationHistory[inCheck][c][SQ_ZERO][NO_PIECE]->fill(Search::CounterMovePruneThreshold - 1);
		}
#endif
}

void Thread::start_searching()
{
	std::lock_guard<std::mutex> lk(mutex);
	searching = true;
	cv.notify_one(); 
}

void Thread::wait_for_search_finished()
{
	std::unique_lock<std::mutex> lk(mutex);
	cv.wait(lk, [&] { return !searching; });
}

void Thread::idle_loop() {


#if !defined(FORCE_BIND_THIS_THREAD)
	if (Options.count("Threads")==0 || Options["Threads"] > 8)
#endif
		WinProcGroup::bindThisThread(idx);

	while (true)
	{
		std::unique_lock<std::mutex> lk(mutex);
		searching = false;
		cv.notify_one(); 
		cv.wait(lk, [&] { return searching; });

		if (exit)
			return;

		lk.unlock();

		search();
	}
}

void ThreadPool::set(size_t requested)
{
	if (size() > 0) { 
		main()->wait_for_search_finished();

		while (size() > 0)
			delete back(), pop_back();
	}

	if (requested > 0) { 
		push_back(new MainThread(0));

		while (size() < requested)
			push_back(new Thread(size()));
		clear();







		Search::init();

	}

#if defined(EVAL_LEARN)
	TT.init_tt_per_thread();
#endif

}

void ThreadPool::clear() {

	for (Thread* th : *this)
		th->clear();

	main()->callsCnt = 0;
	main()->bestPreviousScore = VALUE_INFINITE;
	main()->previousTimeReduction = 1.0;
}

void ThreadPool::start_thinking(const Position& pos, StateListPtr& states ,
								const Search::LimitsType& limits , bool ponderMode)
{
	main()->wait_for_search_finished();

	 stop = false;
	increaseDepth = true;
	main()->ponder = ponderMode;
	Search::Limits = limits;
	Search::RootMoves rootMoves;

	
#if defined (USE_ENTERING_KING_WIN)
	if (pos.DeclarationWin() == MOVE_WIN)
		rootMoves.emplace_back(MOVE_WIN);
#endif

#if defined(USE_GENERATE_ALL_LEGAL_MOVES)
	if (limits.generate_all_legal_moves)
	{
		for (auto m : MoveList<LEGAL_ALL>(pos))
			if (limits.searchmoves.empty()
				|| std::count(limits.searchmoves.begin(), limits.searchmoves.end(), m))
				rootMoves.emplace_back(m);

	} else
#endif
	{

		for (auto m : MoveList<LEGAL>(pos))
			if (limits.searchmoves.empty()
				|| std::count(limits.searchmoves.begin(), limits.searchmoves.end(), m))
				rootMoves.emplace_back(m);
	}


	ASSERT_LV3(states.get() || setupStates.get());

	if (states.get())
		setupStates = std::move(states);	

	  
	
	auto sfen = pos.sfen();
	for (Thread* th : *this)
	{
		th->nodes = th->bestMoveChanges =  th->nmpMinPly = 0;

		th->rootDepth = th->completedDepth = 0;
		th->rootMoves = rootMoves;
		th->rootPos.set(sfen, &th->rootState,th);
		th->rootState = setupStates->back();
	}

	main()->start_searching();
}


Thread* ThreadPool::get_best_thread() const {


	Thread* bestThread = front();
	std::map<Move, int64_t> votes;
	Value minScore = VALUE_NONE;

	for (Thread* th : *this)
		minScore = std::min(minScore, th->rootMoves[0].score);

	for (Thread* th : *this)
	{
		votes[th->rootMoves[0].pv[0]] +=
			(th->rootMoves[0].score - minScore + 14) * int(th->completedDepth);

		if (abs(bestThread->rootMoves[0].score) >= VALUE_TB_WIN_IN_MAX_PLY)
		{
			if (th->rootMoves[0].score > bestThread->rootMoves[0].score)
				bestThread = th;
		}
		else if (th->rootMoves[0].score >= VALUE_TB_WIN_IN_MAX_PLY
			|| (th->rootMoves[0].score > VALUE_TB_LOSS_IN_MAX_PLY
				&& votes[th->rootMoves[0].pv[0]] > votes[bestThread->rootMoves[0].pv[0]]))
			bestThread = th;
	}

	return bestThread;
}



void ThreadPool::start_searching() {

	for (Thread* th : *this)
		if (th != front())
			th->start_searching();
}



void ThreadPool::wait_for_search_finished() const {

	for (Thread* th : *this)
		if (th != front())
			th->wait_for_search_finished();
}

bool ThreadPool::search_finished() const
{
	for (Thread* th : *this)
		if (th != front())
			if (th->is_searching())
				return false;

	return true;
}
