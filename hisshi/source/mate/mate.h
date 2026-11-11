#ifndef __MATE_H_INCLUDED__
#define __MATE_H_INCLUDED__

#include "../types.h"
#if defined (USE_MATE_1PLY)

#include <vector>
#include <functional>
#include <memory> 
#include <atomic> 

namespace Mate
{
	void init();

	Move mate_1ply(const Position& pos);

	template <Color Us>
	Move mate_1ply_imp(const Position& pos);

	Move weak_mate_3ply(const Position& pos, int ply);

	enum class MateRepetitionState
	{
		Mate   , 
		Mated  , 
		Unknown, 
	};

	constexpr int MAX_REPETITION_PLY = 16;

	MateRepetitionState mate_repetition(const Position& pos, const int plies_from_root , const int repetition_ply, bool or_node);


#if defined(USE_MATE_SOLVER)|| defined(USE_MATE_DFPN)


	struct alignas(16) MateHashEntry
	{
		u64 board_key : 48;

		u64 root_color : 1;

		Color side_to_move() const { return (Color)(board_key & 1); }


		u64 is_mate   : 1; 

		u64 ply       : 11;

		u64 padding1  : 2;

		Move get_move() const { return (Move)(move16 + (move8 << 16)); }
		void set_move(Move move) { move16 = (u16)move; move8 = move >> 16; }

		Hand get_hand() const { return (Hand)hand; }

		void lock();

		void unlock();

		void save(Key board_key, Color root_color,  Hand hand, bool is_mate, u32 ply, Move move);


	private:
		u16 move16;
		u8  move8;
		u32 hand;

		std::atomic<bool> mutex;

	};

	class MateHashTable
	{
	public:
		MateHashTable() = default;
		~MateHashTable();
		MateHashTable(const MateHashTable&) = delete;
		MateHashTable& operator=(const MateHashTable&) = delete;

		MateHashEntry* first_entry(const Key board_key) const;

		void resize(size_t mbSize);

		void clear();

	private:
		MateHashEntry* table = nullptr;

		size_t entryCount = 0;
	};

#endif 

#if defined(USE_MATE_SOLVER)

	class MateSolver
	{
	public:
		Move mate_odd_ply(Position& pos, const int ply, bool gen_all);

		void set_max_game_ply(int max_game_ply) { this->max_game_ply = max_game_ply; }

	private:

		template <bool INCHECK, bool GEN_ALL>
		Move mate_odd_ply(Position& pos, const int ply);

		Move mated_even_ply(Position& pos, const int ply, bool gen_all);

		template <bool GEN_ALL>
		Move mated_even_ply(Position& pos, const int ply);

		template <bool INCHECK, bool GEN_ALL>
		Move mate_3ply(Position& pos);

	private:
		int root_game_ply;

		int max_game_ply = 0;
	};

#endif
}

#if defined(USE_MATE_DFPN)

namespace Mate::Dfpn
{
    using ProofHandReducer = std::function<Hand(const Position&, bool, Move, const std::vector<Hand>&)>;
    struct RetainedProof {
        Key key{};
        u64 epoch{};
        int depth{};
        int plies{};
    };
    struct ThresholdResult {
        Move move{MOVE_NONE};
        u64 pn{1}, dn{1}, nodes{}, proof_reuse{};
        bool resumed{}, memory_reset{}, out_of_memory{}, repeated{};
    };

	class MateDfpnSolverInterface
	{
	public:
		virtual void alloc(size_t size_mb) = 0;

		virtual void alloc_by_nodes_limit(size_t nodes_limit) = 0;

		virtual void set_hash_table(MateHashTable* hash_table) = 0;

		virtual Move mate_dfpn(const Position& pos , u64 nodes_limit)= 0;
        virtual ThresholdResult mate_dfpn_threshold(const Position& pos, u64 pn, u64 dn,
                                                    u64 overall_remaining = 0) = 0;
        virtual void clear_threshold_search() = 0;
        virtual void set_interposition_grouping(bool enabled) = 0;
        virtual void set_attack_grouping(bool enabled) = 0;
        virtual RetainedProof retained_proof(const Position& root) const = 0;
        virtual bool replay_retained_proof(RetainedProof proof, Position& pos, u64 limit,
            u64& work, Move& first, const std::vector<Key>* forbidden = nullptr,
            const std::function<void(Key)>* visit = nullptr, int max_plies = INT32_MAX,
            const ProofHandReducer* reducer = nullptr, Hand* proof_hand = nullptr) const = 0;
        virtual void set_stop_callback(std::function<bool(u64)> callback) = 0;
        virtual void set_result_callback(std::function<void(const Position&, bool, Move, int)> callback) = 0;


		virtual void set_max_game_ply(int max_game_ply) = 0;

		virtual void set_move_hint(std::function<Move(const Position&)> hint) = 0;

		virtual std::vector<Move> get_pv() const = 0;

		virtual std::vector<Move> get_current_pv() const = 0;

		virtual bool replay_last_proof(Position& pos,
		                               u64 work_limit,
		                               u64& work,
		                               Move& first_move,
		                               const std::vector<Key>* forbidden_keys = nullptr,
		                               const std::function<void(Key)>* visit = nullptr) const = 0;

		virtual u64 last_proof_generation() const = 0;

		virtual u64 get_nodes_searched() const = 0;

		virtual int get_mate_ply() const= 0;

		virtual bool is_out_of_memory() const= 0;

		virtual int hashfull() const = 0;

		virtual ~MateDfpnSolverInterface() {}
	};

	enum class DfpnSolverType
	{
		None,              

		Node32bit        , 
		Node16bitOrdering, 
		Node64bit        , 
		Node48bitOrdering, 

		Node32bitWithHash,
		Node16bitOrderingWithHash,
		Node64bitWithHash,
		Node48bitOrderingWithHash,


	};

	class MateDfpnSolver : MateDfpnSolverInterface
	{
	public:
		MateDfpnSolver(DfpnSolverType t);

		void ChangeSolverType(DfpnSolverType t);

		virtual void alloc(size_t size_mb) { impl->alloc(size_mb); }

		virtual void alloc_by_nodes_limit(size_t nodes_limit) { impl->alloc_by_nodes_limit(nodes_limit); }

		virtual void set_hash_table(Mate::MateHashTable* table) { impl->set_hash_table(table); }

		virtual Move mate_dfpn(const Position& pos, u64 nodes_limit) { return impl->mate_dfpn(pos, nodes_limit); }
        virtual ThresholdResult mate_dfpn_threshold(const Position& pos, u64 pn, u64 dn,
                                                    u64 overall_remaining = 0) {
            return impl->mate_dfpn_threshold(pos, pn, dn, overall_remaining);
        }
        virtual void clear_threshold_search() { impl->clear_threshold_search(); }
        virtual void set_interposition_grouping(bool enabled) {
            impl->set_interposition_grouping(enabled);
        }
        virtual void set_attack_grouping(bool enabled) {
            impl->set_attack_grouping(enabled);
        }
        virtual RetainedProof retained_proof(const Position& root) const {
            return impl->retained_proof(root);
        }
        virtual bool replay_retained_proof(RetainedProof proof, Position& pos, u64 limit,
            u64& work, Move& first, const std::vector<Key>* forbidden = nullptr,
            const std::function<void(Key)>* visit = nullptr, int max_plies = INT32_MAX,
            const ProofHandReducer* reducer = nullptr, Hand* proof_hand = nullptr) const {
            return impl->replay_retained_proof(proof, pos, limit, work, first, forbidden, visit, max_plies,
                                              reducer, proof_hand);
        }
        virtual void set_stop_callback(std::function<bool(u64)> callback) {
            impl->set_stop_callback(std::move(callback));
        }
        virtual void set_result_callback(std::function<void(const Position&, bool, Move, int)> callback) {
            impl->set_result_callback(std::move(callback));
        }


		virtual void set_max_game_ply(int max_game_ply) { impl->set_max_game_ply(max_game_ply); }
		virtual void set_move_hint(std::function<Move(const Position&)> hint) {
			impl->set_move_hint(std::move(hint));
		}

		virtual std::vector<Move> get_pv() const { return impl->get_pv(); }

		virtual std::vector<Move> get_current_pv() const { return impl->get_current_pv(); }

		virtual bool replay_last_proof(Position& pos,
		                               u64 work_limit,
		                               u64& work,
		                               Move& first_move,
		                               const std::vector<Key>* forbidden_keys = nullptr,
		                               const std::function<void(Key)>* visit = nullptr) const {
			return impl->replay_last_proof(
			    pos, work_limit, work, first_move, forbidden_keys, visit);
		}

		virtual u64 last_proof_generation() const { return impl->last_proof_generation(); }

		virtual u64 get_nodes_searched() const { return impl->get_nodes_searched(); }

		virtual int get_mate_ply() const { return impl->get_mate_ply(); }

		virtual bool is_out_of_memory() const { return impl->is_out_of_memory(); }

		virtual int hashfull() const { return impl->hashfull(); }

	private:
		std::unique_ptr<MateDfpnSolverInterface> impl;
	};

} 
#endif


#endif 

#endif 
