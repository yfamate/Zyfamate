#include <cstring>	
#include "misc.h"
#include "thread.h"
#include "tt.h"

TranspositionTable TT; 

void TTEntry::save(Key k, Value v, bool pv , Bound b, Depth d, Move m , Value ev)
{



	u16 pos_key = (u16)(k >> 1);
	if (m || pos_key != key16)
		move16 = (uint16_t)m;

	if ( b == BOUND_EXACT
		|| pos_key != key16
		|| d - DEPTH_OFFSET > depth8 - 4
		)
	{
		ASSERT_LV3(d > DEPTH_OFFSET);
		ASSERT_LV3(d < 256 + DEPTH_OFFSET);

		key16     = pos_key;
		depth8    = (uint8_t)(d - DEPTH_OFFSET); 
		genBound8 = (uint8_t)(TT.generation8 | uint8_t(pv) << 2 | b);
		value16   = (int16_t)v;
		eval16    = (int16_t)ev;
	}
}

void TranspositionTable::resize(size_t mbSize) {

#if defined(TANUKI_MATE_ENGINE) || defined(YANEURAOU_MATE_ENGINE)
	return;
#endif
	if (Threads.size() == 0)
		return;

	Threads.main()->wait_for_search_finished();

	size_t newClusterCount = mbSize * 1024 * 1024 / sizeof(Cluster);

	ASSERT_LV3((newClusterCount & 1) == 0);



	if (newClusterCount == clusterCount)
		return;

	clusterCount = newClusterCount;


	table = static_cast<Cluster*>(tt_memory.alloc(clusterCount * sizeof(Cluster),32));



#if defined(EVAL_LEARN)
	init_tt_per_thread();
#endif
}

void TranspositionTable::clear()
{
#if defined(TANUKI_MATE_ENGINE) || defined(YANEURAOU_MATE_ENGINE)
	return;
#endif

	auto size = clusterCount * sizeof(Cluster);

#if !defined(EVAL_LEARN)
	Tools::memclear("USI_Hash" , table, size);
#else
	std::memset(table, 0, size);
#endif
}

TTEntry* TranspositionTable::probe(const Key key, bool& found) const
{
	ASSERT_LV3(clusterCount != 0);

#if defined(USE_GLOBAL_OPTIONS)
	if (!GlobalOptions.use_hash_probe)
	{
		return found = false, first_entry(0);
	}
#endif

	TTEntry* const tte = first_entry(key);

	const uint16_t key16 = (u16)(key >> 1);

	for (int i = 0; i < ClusterSize; ++i)
	{



		if (tte[i].key16 == key16 || !tte[i].depth8)
		{
			tte[i].genBound8 = uint8_t(generation8 | (tte[i].genBound8 & 0x7)); 

			return found = (bool)tte[i].depth8, &tte[i];
		}
	}


	TTEntry* replace = tte;
	for (int i = 1; i < ClusterSize; ++i)


		if (replace->depth8 - ((263 + generation8 - replace->genBound8) & 0xF8)
		  >   tte[i].depth8 - ((263 + generation8 -   tte[i].genBound8) & 0xF8))
			replace = &tte[i];


	return found = false, replace;
}

TTEntry* TranspositionTable::read_probe(const Key key, bool& found) const
{
	ASSERT_LV3(clusterCount != 0);

#if defined(USE_GLOBAL_OPTIONS)
	if (!GlobalOptions.use_hash_probe)
		return found = false, first_entry(0);
#endif

	TTEntry* const tte = first_entry(key);
	const uint16_t key16 = (u16)(key >> 1);

	for (int i = 0; i < ClusterSize; ++i)
	{
		if (tte[i].key16 == key16 || !tte[i].depth8)
				return found = (bool)tte[i].depth8, &tte[i];
	}
	return found = false, nullptr;
}

int TranspositionTable::hashfull() const
{

	
	int cnt = 0;
	for (int i = 0; i < 1000 / ClusterSize; ++i)
		for (int j = 0; j < ClusterSize; ++j)
			cnt += table[i].entry[j].depth8 && (table[i].entry[j].genBound8 & 0xF8) == generation8;

	return cnt * 1000 / (ClusterSize * (1000 / ClusterSize));
}

#if defined(EVAL_LEARN)
void TranspositionTable::init_tt_per_thread()
{
	size_t thread_size = Threads.size();

	if (thread_size == 0)
		return;

	size_t clusterCountPerThread = (clusterCount / thread_size) & ~(size_t)1;
	 
	ASSERT_LV3((clusterCountPerThread & 1) == 0);

	for (size_t i = 0; i < thread_size; ++i)
	{
		auto& tt = Threads[i]->tt;
		tt.clusterCount = clusterCountPerThread;
		tt.table = this->table + clusterCountPerThread * i;
	}
}
#endif
