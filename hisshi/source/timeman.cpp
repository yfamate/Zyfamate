#include "config.h"

#if defined(USE_TIME_MANAGEMENT)

#include "misc.h"
#include "search.h"
#include "thread.h"
#include "usi.h"

namespace {

	const int MoveHorizon = 160;

	PRNG prng;

} 


void Timer::init(const Search::LimitsType& limits, Color us, int ply)
{
#if 0
	TimePoint npmsec = Options["nodestime"];

	if (npmsec)
	{
		if (!availableNodes)
			availableNodes = npmsec * limits.time[us];
		
		limits.time[us] = TimePoint(availableNodes);
		for (auto c : COLOR)
		{
			limits.inc[c] *= npmsec;
			limits.byoyomi[c] *= npmsec;
		}
		limits.rtime *= npmsec;
		limits.npmsec = npmsec;

	}
#endif

	network_delay = (int)Options["NetworkDelay"];

	search_end = 0;

	remain_time = limits.time[us] + limits.byoyomi[us] + limits.inc[us] - (TimePoint)Options["NetworkDelay2"];
	remain_time = std::max(remain_time, (TimePoint)100);

	minimum_thinking_time = (int)Options["MinimumThinkingTime"];

	int slowMover = (int)Options["SlowMover"];

	if (limits.rtime)
	{

		auto r = limits.rtime;
#if 1
		if (ply)
			r += (int)prng.rand((int)std::min(r * 0.5f, r * 10.0f / (ply)));
#endif

		remain_time = minimumTime = optimumTime = maximumTime = r;
		return;
	}

	if (limits.movetime)
	{
		remain_time = minimumTime = optimumTime = maximumTime = limits.movetime;
		return;
	}

	const int MTG = std::min((limits.max_game_ply - ply + 2) / 2, MoveHorizon);

	if (MTG <= 0)
	{
		sync_cout << "info string max_game_ply is too small." << sync_endl;
		return;
	}
	if (MTG == 1)
	{
		minimumTime = optimumTime = maximumTime = remain_time;
		return;
	}


	{
		minimumTime = std::max(minimum_thinking_time - network_delay, (TimePoint)1000);

		optimumTime = maximumTime = remain_time;


		TimePoint remain_estimate = limits.time[us]
			+ limits.inc[us] * MTG
			+ limits.byoyomi[us] * MTG;

		remain_estimate -= (MTG + 1) * 1000;
		remain_estimate = std::max(remain_estimate, TimePoint(0));

		TimePoint t1 = minimumTime + remain_estimate / MTG;

#if !defined(YANEURAOU_ENGINE_DEEP)
		float max_ratio = 3.0f;
#else
		float max_ratio = 5.0f;
#endif


		if (limits.inc[us] == 0 && limits.byoyomi[us] == 0)
		{
			max_ratio = std::min(max_ratio, std::max(float(limits.time[us]) / (60 * 1000), 1.0f));
		}
		TimePoint t2 = minimumTime + (int)(remain_estimate * max_ratio / MTG);

		t2 = std::min(t2, (TimePoint)(remain_estimate * 0.3));

		optimumTime = std::min(t1, optimumTime) * slowMover / 100;
		maximumTime = std::min(t2, maximumTime);

#if !defined(YANEURAOU_ENGINE_DEEP)
		if ( Options["USI_Ponder"])
			optimumTime += optimumTime / 4;
#endif
	}

	if (limits.byoyomi[us])
	{
		if (limits.time[us] < (int)(limits.byoyomi[us] * 1.2))
			minimumTime = optimumTime = maximumTime = limits.byoyomi[us] + limits.time[us];
	}

	minimumTime = std::min(round_up(minimumTime), remain_time);
	optimumTime = std::min(optimumTime, remain_time);
	maximumTime = std::min(round_up(maximumTime), remain_time);

}

#endif
