#ifndef TT_H_INCLUDED
#define TT_H_INCLUDED

#include "types.h"
#include "misc.h"



struct TTEntry {

	Move16 move() const { return Move16(move16); }
	Value value() const { return (Value)value16; }
	Value eval() const { return (Value)eval16; }
	Depth depth() const { return (Depth)depth8 + DEPTH_OFFSET; }
	bool is_pv() const { return (bool)(genBound8 & 0x4); }
	Bound bound() const { return (Bound)(genBound8 & 0x3); }

	void save(Key k, Value v, bool pv , Bound b, Depth d, Move m, Value ev);

private:
	friend struct TranspositionTable;

	uint16_t key16;

	uint16_t move16;

	int16_t value16;

	int16_t eval16;

	uint8_t genBound8;

	uint8_t depth8;
};

struct TranspositionTable {

	static constexpr int ClusterSize = 3;

	struct Cluster {
		TTEntry entry[ClusterSize];
		u8 padding[2]; 
	};

	static_assert(sizeof(Cluster) == 32, "Unexpected Cluster size");

public:

	void new_search() { generation8 += 8; } 

	TTEntry* probe(const Key key, bool& found) const;

	TTEntry* read_probe(const Key key, bool& found) const;

	int hashfull() const;

	void resize(size_t mbSize);

	void clear();

	TTEntry* first_entry(const Key key) const {



		ASSERT_LV3((clusterCount & 1) == 0);


		uint64_t index = mul_hi64((u64)key >> 1, clusterCount);

		return &table[(index << 1) | ((u64)key & 1)].entry[0];
	}

#if defined(EVAL_LEARN)
	void init_tt_per_thread();
#endif

private:
	friend struct TTEntry;

	size_t clusterCount = 0;

	Cluster* table = nullptr;


	uint8_t generation8;


	LargeMemory tt_memory;
};

extern TranspositionTable TT;

#endif 
