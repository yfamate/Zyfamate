#include "mate.h"
#if defined(USE_MATE_DFPN)





#if defined(DFPN64) || defined(DFPN32)

#include <mutex>
#include <array>
#include <unordered_map>
#include "../position.h"
#include "../thread.h"
#include "mate_move_picker.h"
#include "mate_groups.h"

#if defined (DFPN64)

namespace Mate::Dfpn64
{
	
	
	

	
	

	template <typename NodeCountType , bool MoveOrdering>
	struct alignas(32) Node
	{
		
		
		
		static constexpr NodeCountType DNPN_INF  = std::numeric_limits<NodeCountType>::max();
		static constexpr NodeCountType DNPN_MATE = std::numeric_limits<NodeCountType>::max() - 2000;

		
		static constexpr u8 CHILDNUM_NOT_INIT = std::numeric_limits<u8>::max();

		
		Node* children;

		
		
		
		
		
		
		NodeCountType pn, dn;

		
		Move lastMove;

		
		
		

		
		
		
		u8 child_num;

		
		
		bool repeated;

		bool interposition;
		u8 padding[1]; 

		
		template <bool or_node>
		void init(ExtMove m)
		{
			lastMove = m.move;
            interposition = false;
			repeated = false;

			if (MoveOrdering)
			{
				
				pn = -m.value + 30000; 
				dn = +m.value + 30000; 
			}
			else {
				pn = dn = 1;
			}

			children = nullptr;
			child_num = CHILDNUM_NOT_INIT;
		}

		
		
		template <bool or_node>
		void init_mate_move(Move move)
		{
			this->lastMove = move;
            interposition = false;
			this->repeated = false;
			this->template set_mate<true>();

			children = nullptr;
			child_num = CHILDNUM_NOT_INIT;
		}

		
		
		template <bool or_node>
		void set_mate(int ply = 0)
		{
			
			
			this->pn = or_node ? 0                             : NodeCountType(DNPN_INF - ply);
			this->dn = or_node ? NodeCountType(DNPN_INF - ply): 0;
		}

		
		template <bool or_node>
		void set_mated(int ply = 0)
		{
			this->pn = or_node ? NodeCountType(DNPN_INF - ply): 0;
			this->dn = or_node ? 0                             :NodeCountType(DNPN_INF - ply);
		}

		
		void set_nomate(int ply = 0)
		{
			this->pn = NodeCountType(DNPN_INF - ply);
			this->dn = 0; 
		}

		
		void set_child(u8 child_num , Node* children = nullptr)
		{
			this->child_num = child_num;
			this->children = children;
		}
	};

#endif 

#if defined (DFPN32)


namespace Mate::Dfpn32
{
	
	
	

	
	
	
	

	template <typename NodeCountType , bool MoveOrdering>
	struct alignas(16) Node
	{
		
		static constexpr NodeCountType NodeCountTypeNull = std::numeric_limits<NodeCountType>::max();

		
		
		
		static constexpr NodeCountType DNPN_INF  = std::numeric_limits<NodeCountType>::max();
		static constexpr NodeCountType DNPN_MATE = std::numeric_limits<NodeCountType>::max() - 2000;

		
		static constexpr u8 CHILDNUM_NOT_INIT = std::numeric_limits<u8>::max();

		
		static bool is_nullptr(NodeCountType n) { return n == NodeCountTypeNull; }

		
		
		NodeCountType children;

		
		
		
		
		
		
		NodeCountType pn, dn;

		
		
		u32 lastMove  : 22;
        u32 interposition : 1;

		
		
		u32 repeated  : 1;

		
		
		
		
		
		u32 child_num :  8;

		
		

		
		template <bool or_node>
		void init(ExtMove m)
		{
			lastMove = m.move;
            interposition = false;
			repeated = false;

			if (MoveOrdering)
			{
				
				pn = -m.value + 30000; 
				dn = +m.value + 30000; 
			}
			else {
				pn = dn = 1;
			}

			
			children = NodeCountTypeNull;
			child_num = CHILDNUM_NOT_INIT;
		}

		
		
		template <bool or_node>
		void init_mate_move(Move move ,int ply = 0)
		{
			this->lastMove = move;
            interposition = false;
			this->repeated = false;
			this->template set_mate<true>(ply);

			
			children = NodeCountTypeNull;
			child_num = CHILDNUM_NOT_INIT;
		}

		
		
		template <bool or_node>
		void set_mate(int ply = 0)
		{
			
			
			this->pn = or_node ? 0                             : NodeCountType(DNPN_INF - ply);
			this->dn = or_node ? NodeCountType(DNPN_INF - ply): 0;
		}

		
		template <bool or_node>
		void set_mated(int ply = 0)
		{
			this->pn = or_node ? NodeCountType(DNPN_INF - ply): 0;
			this->dn = or_node ? 0                             :NodeCountType(DNPN_INF - ply);
		}

		
		void set_nomate(int ply = 0)
		{
			this->pn = NodeCountType(DNPN_INF - ply);
			this->dn = 0; 
		}

		
		void set_child(u8 child_num , NodeCountType children = NodeCountTypeNull)
		{
			this->child_num = child_num;
			this->children = children;
		}
	};

#endif 

	
	
	

	
	
	template <typename NodeCountType , bool MoveOrdering>
	struct NodeManager
	{
		
		typedef Node<NodeCountType,MoveOrdering> NodeType;

		NodeManager() :node_index(0), nodes_num(0) {}

		
		void alloc(size_t size_) {
			
			release();

			this->nodes_num = (NodeCountType)size_;
			nodes = std::make_unique<NodeType[]>(size_);
		}

		
		void release() { nodes.reset(); }

		
		NodeCountType size() const { return nodes_num; }
        size_t offset(const NodeType* node) const { return static_cast<size_t>(node - nodes.get()); }

		
		
		NodeType* new_node(size_t size = 1)
		{
			
			

			if (is_out_of_memory())
				return nullptr;

			NodeType* node = &nodes[node_index];
			
			
			node_index.store(node_index.load(std::memory_order_relaxed) + (NodeCountType)size,
			                 std::memory_order_relaxed);
			return node;
		}

		
		
		
		void reset_counter() { node_index = 0; }

		
		bool is_out_of_memory() const {
			
			return node_index + MaxCheckMoves >= nodes_num;
		}

		
		int hashfull() const { return (int)((u64)node_index * 1000 / nodes_num); }


#if defined(DFPN32)
		
		
		
		

		
		NodeType* get_children(const NodeType* node) const
		{
			return node_index_to_node(node->children);
		}

		
		void set_children(NodeType* node, const NodeType* next) const
		{
			node->children = node_to_node_index(next);
		}

		
		
		NodeType* node_index_to_node(NodeCountType n) const {
			return ! NodeType::is_nullptr(n) ? &nodes[n] : nullptr;
		}

		
		
		NodeCountType node_to_node_index(const NodeType*node) const
		{
			
			return (node == nullptr) ? NodeType::NodeCountTypeNull : NodeCountType(node - nodes.get());
		}
#endif

#if defined(DFPN64)
		

		NodeType* get_children(const NodeType* node) const { return node->children; }
		void set_children(NodeType* node, const NodeType* next) const { node->children = next; }
		NodeType* node_index_to_node(NodeType* n) const { return n; }
		NodeType* node_to_node_index(NodeType* node) const { return node; }
#endif

	private:

		
		
		std::unique_ptr<Node<NodeCountType,MoveOrdering>[]> nodes;

		
		NodeCountType nodes_num;

		
		std::atomic<NodeCountType> node_index;

		
		
	};


	
	
	

	
	
	template <typename NodeCountType , bool MoveOrdering , bool WithHash >
	class MateDfpnPn : public Mate::Dfpn::MateDfpnSolverInterface
	{
	public:
		
		typedef Node<NodeCountType,MoveOrdering> NodeType;

		
		
		virtual void alloc(size_t size_mb) {
			
			release();

			
			size_t size = size_mb * 1024 * 1024;
			node_manager.alloc(size / sizeof(Node<NodeCountType,MoveOrdering>));
            if (group_attacks) attack_groups.assign(node_manager.size(), 0);
		}

		
		
		virtual void alloc_by_nodes_limit(size_t nodes_limit)
		{
			
			release();

			node_manager.alloc(nodes_limit);
            if (group_attacks) attack_groups.assign(node_manager.size(), 0);
		}

		
		
		
		
		virtual void set_hash_table(MateHashTable* hash_table)
		{
			this->hash_table = hash_table;
		}

		
		void release()
		{
			threshold_roots.clear();
            proven_nodes.clear();
            ++proof_epoch;
			node_manager.release();
			current_root = nullptr;
			std::vector<ReplayMemoEntry>{}.swap(replay_memo);
            std::vector<u8>{}.swap(attack_groups);
		}

		
		
		
		virtual void set_move_hint(std::function<Move(const Position&)> hint) {
			move_hint = std::move(hint);
		}

		virtual void set_max_game_ply(int max_game_ply)
		{
			this->max_game_ply = max_game_ply;
		}

		
		
		

        virtual Mate::Dfpn::RetainedProof retained_proof(const Position& root) const {
            const auto found = threshold_roots.find(root.key());
            const int depth = max_game_ply ? max_game_ply - root.game_ply() : INT32_MAX;
            if (found == threshold_roots.end() || found->second.root->pn != 0 ||
                found->second.root->repeated) return {};
            const int plies = static_cast<int>(NodeType::DNPN_INF - found->second.root->dn);
            if (plies > depth) return {};
            return {root.key(), proof_epoch, found->second.depth, plies};
        }
        virtual bool replay_retained_proof(Mate::Dfpn::RetainedProof proof, Position& pos,
                u64 limit, u64& work, Move& first, const std::vector<Key>* forbidden = nullptr,
                const std::function<void(Key)>* visit = nullptr, int max_plies = INT32_MAX,
                const Mate::Dfpn::ProofHandReducer* reducer = nullptr, Hand* proof_hand = nullptr) const {
            work = 0;
            first = MOVE_NONE;
            if (proof_hand) *proof_hand = Hand{};
            if (!proof.epoch || proof.epoch != proof_epoch || !limit) return false;
            const auto found = threshold_roots.find(proof.key);
            if (found == threshold_roots.end() || found->second.depth != proof.depth ||
                found->second.root->pn != 0) return false;
            return ReplayProof<true>(pos, found->second.root, limit, work, first,
                                     true, forbidden, visit, max_plies, reducer, proof_hand,
                                     visit == nullptr && BeginReplayMemo());
        }
        virtual void clear_threshold_search() {
            threshold_roots.clear();
            proven_nodes.clear();
            ++proof_epoch;
            node_manager.reset_counter();
            current_root = nullptr;
            ++proof_generation;
        }
        virtual void set_interposition_grouping(bool enabled) {
            if (group_interpositions != enabled) {
                clear_threshold_search();
                group_interpositions = enabled;
            }
        }
        virtual void set_attack_grouping(bool enabled) {
            if (group_attacks != enabled) {
                clear_threshold_search();
                group_attacks = enabled;
                if (enabled) attack_groups.assign(node_manager.size(), 0);
                else std::vector<u8>{}.swap(attack_groups);
            }
        }
        virtual void set_stop_callback(std::function<bool(u64)> callback) {
            stop_callback = std::move(callback);
        }
        virtual void set_result_callback(std::function<void(const Position&, bool, Move, int)> callback) {
            result_callback = std::move(callback);
        }
        virtual Mate::Dfpn::ThresholdResult mate_dfpn_threshold(
                const Position& pos_, u64 pn, u64 dn, u64 overall_remaining = 0) {
            Mate::Dfpn::ThresholdResult result;
            auto& pos = *const_cast<Position*>(&pos_);
            ++proof_generation;
            nodes_searched = 0;
            proof_reuses = 0;
            nodes_limit = overall_remaining;
            threshold_search = true;
            out_of_memory = false;
            if (node_manager.is_out_of_memory()) {
                clear_threshold_search();
                result.memory_reset = true;
            }
            if (node_manager.is_out_of_memory()) {
                result.out_of_memory = true;
                return result;
            }
            root_game_ply = pos.game_ply();
            root_color = pos.side_to_move();
            root_pos = &pos;
            const int remaining_depth = max_game_ply ? max_game_ply - root_game_ply : INT32_MAX;
            
            
            const auto found = threshold_roots.find(pos.key());
            
            
            
            
            const bool reusable = found != threshold_roots.end() &&
                (found->second.depth == remaining_depth ||
                 (found->second.max_expanded_depth <= found->second.depth &&
                  found->second.max_expanded_depth <= remaining_depth));
            max_expanded_depth = reusable ? found->second.max_expanded_depth : 0;
            if (reusable) {
                current_root = found->second.root;
                result.resumed = true;
            } else {
                ExpandRoot(pos);
            }
            threshold_roots[pos.key()] = {current_root, remaining_depth, max_expanded_depth};
            
            
            const auto bound = [](u64 value) {
                return static_cast<NodeCountType>(std::min<u64>(
                    value ? value - 1 : 0, NodeType::DNPN_MATE - 1));
            };
            if (!out_of_memory && current_root->pn && current_root->dn &&
                current_root->pn <= bound(pn) && current_root->dn <= bound(dn))
                ParallelSearch<true>(pos, current_root, bound(pn), bound(dn));
            result.nodes = nodes_searched.load();
            result.proof_reuse = proof_reuses;
            result.out_of_memory = out_of_memory;
            if (!result.out_of_memory) {
                threshold_roots[pos.key()].max_expanded_depth = max_expanded_depth;
                result.repeated = current_root->repeated;
                result.pn = current_root->pn >= NodeType::DNPN_MATE
                    ? std::numeric_limits<u64>::max() : current_root->pn;
                result.dn = current_root->dn >= NodeType::DNPN_MATE
                    ? std::numeric_limits<u64>::max() : current_root->dn;
                if (!current_root->pn) {
                    auto node = current_root;
                    result.move = pick_the_best<true, true, false>(node);
                } else if (!current_root->dn) result.move = MOVE_NULL;
            } else {
                
                
                clear_threshold_search();
            }
            return result;
        }

		virtual Move mate_dfpn(const Position& pos_, u64 nodes_limit)
		{
			++proof_generation;
			
			auto& pos = *const_cast<Position*>(&pos_);

			nodes_searched = 0;
			this->nodes_limit = nodes_limit;
            threshold_search = false;
            threshold_roots.clear();
            proven_nodes.clear();
            ++proof_epoch;
			bestmove = MOVE_NONE;

			
			node_manager.reset_counter();
			out_of_memory = false;

			
			ExpandRoot(pos);

			
			ParallelSearch(pos);

			
			if (current_root->pn == 0 && current_root->dn >= NodeType::DNPN_MATE)
			{
#if 0
				std::cout << "solved! ply = " << get_mate_ply() << ", nodes_searched = " << nodes_searched << std::endl;
				
#endif

				
				auto node = current_root;
				return pick_the_best<true, true, false>(node);
			}

			
			if (current_root->pn >= NodeType::DNPN_MATE && current_root->dn == 0)
				return MOVE_NULL;

			
			
			return MOVE_NONE;
		}

		
		
		virtual std::vector<Move> get_pv() const { return get_pv<true,false>(); }

		
		virtual std::vector<Move> get_current_pv() const { return get_pv<true,true>(); }

		virtual bool replay_last_proof(Position& pos,
		                               u64 work_limit,
		                               u64& work,
		                               Move& first_move,
		                               const std::vector<Key>* forbidden_keys = nullptr,
		                               const std::function<void(Key)>* visit = nullptr) const {
			work = 0;
			first_move = MOVE_NONE;
			if (current_root == nullptr || current_root->pn != 0 ||
			    current_root->dn < NodeType::DNPN_MATE || work_limit == 0)
				return false;
			return ReplayProof<true>(
			    pos, current_root, work_limit, work, first_move, true, forbidden_keys, visit);
		}

		virtual u64 last_proof_generation() const { return proof_generation; }
		
		
		
		
		std::vector<Move> get_unproof_pv() const { return get_pv<false,false>(); }

		
		int get_mate_ply() const {
			if (current_root->pn != 0 || current_root->dn < NodeType::DNPN_MATE)
				return -1; 
			return int(NodeType::DNPN_INF - current_root->dn);
		}

		
		virtual u64 get_nodes_searched() const {
			return (u64)nodes_searched;
		}

		
		virtual bool is_out_of_memory() const { return node_manager.is_out_of_memory(); }

		
		virtual int hashfull() const { return node_manager.hashfull(); }

	protected:

		
		
		
		
		
		
        struct ReplayMemoEntry {
            Key key{};
            const NodeType* children{};
            u64 epoch{};
            u32 child_num{};
            int plies_left{};
            Move move{MOVE_NONE};
            Hand hand{};
        };

        
        
        
        bool BeginReplayMemo() const {
            if (replay_memo.empty()) {
                const size_t capacity = static_cast<size_t>(node_manager.size()) * sizeof(NodeType)
                    / (16 * sizeof(ReplayMemoEntry));
                if (!capacity) return false;
                size_t size = 1;
                while (size <= capacity / 2) size *= 2;
                replay_memo.resize(size);
            }
            if (++replay_memo_epoch == 0) {
                std::fill(replay_memo.begin(), replay_memo.end(), ReplayMemoEntry{});
                ++replay_memo_epoch;
            }
            return true;
        }

		template <bool or_node>
		bool ReplayProof(Position& pos,
		                 const NodeType* node,
		                 u64 work_limit,
		                 u64& work,
		                 Move& first_move,
		                 bool at_root,
		                 const std::vector<Key>* forbidden_keys,
		                 const std::function<void(Key)>* visit, int plies_left = INT32_MAX,
                         const Mate::Dfpn::ProofHandReducer* reducer = nullptr,
                         Hand* proof_hand = nullptr, bool memoize = false) const {
            if (plies_left < 0) return false;
			if (node == nullptr || node->pn != 0 || work >= work_limit || Threads.stop)
				return false;
            if (work % 1024 == 0 && stop_callback && stop_callback(work)) return false;
			++work;
			if (forbidden_keys != nullptr &&
			    std::find(forbidden_keys->begin(), forbidden_keys->end(), pos.key()) !=
			        forbidden_keys->end())
				return false;
			if (visit != nullptr) (*visit)(pos.key());
			if constexpr (WithHash) {
				
				
				if (node->repeated)
					return false;
			}

            NodeType* children = node_manager.get_children(node);
            const u32 child_num = node->child_num;
            ReplayMemoEntry* memo = nullptr;
            if (memoize) {
                
                
                
                const auto hash = pos.key() * 11400714819323198485ULL;
                memo = &replay_memo[(hash ^ (hash >> 32)) & (replay_memo.size() - 1)];
                if (memo->epoch == replay_memo_epoch && memo->key == pos.key() &&
                    memo->children == children && memo->child_num == child_num &&
                    memo->plies_left <= plies_left) {
                    if (at_root) first_move = memo->move;
                    if (reducer && proof_hand) *proof_hand = memo->hand;
                    return true;
                }
            }
            const auto remember = [&](Move move) {
                if (memo) *memo = {pos.key(), children, replay_memo_epoch, child_num, plies_left, move,
                                  reducer && proof_hand ? *proof_hand : Hand{}};
            };
			if constexpr (or_node) {
				if (children == nullptr || child_num == 0 ||
				    child_num == NodeType::CHILDNUM_NOT_INIT)
					return false;

				for (u32 i = 0; i < child_num; ++i) {
					const auto& child = children[i];
					if (child.pn != 0)
						continue;
					const Move move = pos.to_move(Move16{static_cast<Move>(child.lastMove)});
					if (move == MOVE_NONE || move == MOVE_NULL || !pos.pseudo_legal(move) ||
					    !pos.legal(move) || !pos.gives_check(move))
						continue;

					StateInfo state;
					pos.do_move(move, state);
					Move ignored = MOVE_NONE;
					Hand child_hand{};
					const bool proven = ReplayProof<false>(
					    pos, &child, work_limit, work, ignored, false, forbidden_keys, visit, plies_left - 1,
                        reducer, &child_hand, memoize);
					pos.undo_move(move);
					if (proven) {
                        if (reducer && proof_hand)
                            *proof_hand = (*reducer)(pos, true, move, {child_hand});
						if (at_root)
							first_move = move;
                        remember(move);
						return true;
					}
					if (work >= work_limit || Threads.stop)
						return false;
				}
				return false;
			} else {
				
				
				if (!pos.in_check())
					return false;

				const auto evasions = MoveList<LEGAL_ALL>{pos};
				if (evasions.size() == 0) {
                    if (reducer && proof_hand)
                        *proof_hand = (*reducer)(pos, false, MOVE_NONE, {});
                    remember(MOVE_NONE);
					return true;
                }
				if (children == nullptr || child_num == 0 ||
				    child_num == NodeType::CHILDNUM_NOT_INIT)
					return false;

                std::vector<Hand> child_hands;
				for (const auto& ext_move : evasions) {
					const Move evasion = ext_move.move;
					const NodeType* matching = nullptr;
					for (u32 i = 0; i < child_num; ++i) {
						const auto& child = children[i];
						if (child.pn != 0)
							continue;
						const Move stored =
						    pos.to_move(Move16{static_cast<Move>(child.lastMove)});
						if (stored == evasion) {
							matching = &child;
							break;
						}
					}
					if (matching == nullptr)
						return false;

					StateInfo state;
					pos.do_move(evasion, state);
					Move ignored = MOVE_NONE;
                    Hand child_hand{};
					const bool proven = ReplayProof<true>(
					    pos, matching, work_limit, work, ignored, false, forbidden_keys, visit, plies_left - 1,
                        reducer, &child_hand, memoize);
					pos.undo_move(evasion);
					if (!proven)
						return false;
                    if (reducer && proof_hand) child_hands.push_back(child_hand);
				}
                if (reducer && proof_hand)
                    *proof_hand = (*reducer)(pos, false, MOVE_NONE, child_hands);
                remember(MOVE_NONE);
				return true;
			}
		}

		
		
		
		template <bool proof , bool current>
		std::vector<Move> get_pv() const {
			std::vector<Move> pv;

			auto node = current_root;
			
			bool or_node = true;
			while (node) {
				
				
				
				Move move = or_node ? pick_the_best<true,proof,current>(node) : pick_the_best<false,proof,current>(node);
				if (move == MOVE_NONE)
					break;

				pv.push_back(move);
				or_node ^= true; 
			}
			return pv;
		}

		
		
		
		
		
		
		template <bool or_node, bool proof,bool current>
		Move pick_the_best(NodeType*& node) const
		{
			NodeType* children = node_manager.get_children(node);
			if ( children == nullptr || node->child_num == 0)
			{
				node = nullptr;
				return MOVE_NONE; 
			}

			
			int child_num = node->child_num;

			int selected_index = 0;
			if (proof)
			{
				for (int i = 1;  i < child_num ; ++i)
					if (or_node)
					{
						if (current)
						{
							
							if (children[i].pn < children[selected_index].pn
								|| (children[i].pn == children[selected_index].pn && children[i].dn > children[selected_index].dn))
								selected_index = i;
						}
						else {
							
							
							if (children[i].dn > children[selected_index].dn)
								selected_index = i;
						}
					}
					else {
						if (current)
						{
							
							if (children[i].pn < children[selected_index].pn
								|| (children[i].pn == children[selected_index].pn && children[i].dn < children[selected_index].dn))
								selected_index = i;
						}
						else {
							
							
							if (children[i].dn < children[selected_index].dn)
								selected_index = i;
						}
					}
			}
			else {
				
				
				for (int i = 1;  i < child_num ; ++i)
					if (or_node)
					{
						if (children[i].pn > children[selected_index].pn)
							selected_index = i;
					}
					else {
						if (children[i].pn < children[selected_index].pn)
							selected_index = i;
					}
			}

			auto& selected = children[selected_index];
			node = &selected;

			return (Move)selected.lastMove;
		}

		
		void dump_tree(NodeType* node , std::string pv = "")
		{
			NodeType* children = node_manager.get_nodeptr(node);
			if (children == nullptr)
			{
				std::cout << pv << std::endl;
				return;
			}
			int child_num = node->child_num;
			if (child_num == 0)
			{
				std::cout << pv << std::endl;
				return;
			}

			bool found = false;
			for (int i = 0; i < child_num; ++i)
			{
				if (node->children[i].pn == 0)
				{
					found = true;
					
					auto pv2 = pv + "[" + std::to_string(node->dn) + "] " + to_usi_string(children[i].move);
					NodeType* next = node->children[i].children;
					dump_tree(next, pv2);
				}
			}

			if (!found)
				std::cout << "mate not found : " << pv << std::endl;
		}

		
		void ParallelSearch(Position& pos)
		{
			ParallelSearch<true>(pos, current_root , NodeType::DNPN_INF , NodeType::DNPN_INF);
		}

		
		
		
		template <bool or_node>
		void ParallelSearch(Position& pos , NodeType* node , NodeCountType second_pn , NodeCountType second_dn)
		{
			ASSERT_LV3(node->pn <= second_pn && node->dn <= second_dn);

			
			
			 while (
				    node->pn <= second_pn
				 && node->dn <= second_dn
				 && node->pn
				 && node->dn
				 && !out_of_memory
				 
                && (!nodes_limit || (nodes_searched < nodes_limit && (threshold_search ||
                        ( MoveOrdering && (nodes_limit - nodes_searched > (std::max(current_root->pn, node->pn) >>16))) ||
                        (!MoveOrdering && (nodes_limit - nodes_searched > std::max(current_root->pn, node->pn))))))
                && (!(nodes_searched.load(std::memory_order_relaxed) % 1024 == 0) ||
                    !stop_callback || !stop_callback(nodes_searched.load(std::memory_order_relaxed)))
				 
				 
				 
				 && !Threads.stop 
				)
			{
#if 0
				 
				 std::cout << pos << std::endl;
#endif

				u8 child_num = node->child_num;
				if (child_num == NodeType::CHILDNUM_NOT_INIT)
				{
					ExpandNode<or_node>(pos, node);
					

					continue;
				}

				NodeCountType second_pn2 = second_pn;
				NodeCountType second_dn2 = second_dn;
				auto best_child = select_the_best_child<or_node>(node, second_pn2, second_dn2);

				
				StateInfo si;
				Move m = pos.to_move(best_child->lastMove);
				pos.do_move(m, si);

				
				ParallelSearch<!or_node>(pos, best_child ,second_pn2 , second_dn2);

				
				SummarizeNode<or_node>(node);

				pos.undo_move(m);

				RememberProof(pos, node);
				if (result_callback && (node->dn == 0 || node->pn == 0) && !node->repeated) {
                    auto selected = node;
                    const bool mate = node->pn == 0;
                    const Move first = mate ? pick_the_best<or_node, true, false>(selected)
                                            : pick_the_best<or_node, false, false>(selected);
                    const int plies = static_cast<int>(NodeType::DNPN_INF - (mate ? node->dn : node->pn));
                    result_callback(pos, mate, first, plies);
                }

				
				if (WithHash && (node->dn == 0 || node->pn == 0) && !node->repeated)
				{
					auto key = pos.state()->board_key();
					auto entry = hash_table->first_entry(key);

					
					bool is_mate = node->pn == 0; 
					auto ply = is_mate ? (u32)(NodeType::DNPN_INF - node->dn) : (u32)(NodeType::DNPN_INF - node->pn);
					entry->save(key, root_color , pos.state()->hand , is_mate , ply, m);

#if 0
					
					std::cout << pos << std::endl
							  << " is_mate = " << is_mate << std::endl
							  << " ply     = " << ply << std::endl
							  << " move    = " << m << std::endl;
#endif
				}

			 }
		}

		
		
		
		
		template <bool or_node>
		NodeType* select_the_best_child(NodeType* node,NodeCountType& second_pn,NodeCountType& second_dn)
		{
			auto children  = node_manager.get_children(node);
			u32 child_num = node->child_num;

			u32 selected_index = 0;
			NodeCountType pn2 = second_pn;
			NodeCountType dn2 = second_dn;

			if (or_node)
			{
				
				for (u32 i = 1; i < child_num; ++i)
					if (children[i].pn < children[selected_index].pn)
						selected_index = i;

				
				
				for (u32 i = 0; i < child_num; ++i)
					if (i != selected_index)
					{
						if (children[i].pn < pn2)
							pn2 = children[i].pn;

						
						
						
						
						
						

						
						if (children[i].dn < NodeType::DNPN_MATE)
							dn2 -= children[i].dn;
					}

				
				dn2 = std::min(dn2, second_dn);
                if (group_attacks) {
                    NodeCountType contribution = children[selected_index].dn;
                    const u8 group = AttackGroup(&children[selected_index]);
                    if (group) {
                        contribution = 0;
                        for (u32 i = 0; i < child_num; ++i)
                            if (AttackGroup(&children[i]) == group)
                                contribution = std::max(contribution, children[i].dn);
                    }
                    
                    
                    dn2 = second_dn - (node->dn - contribution);
                }
			}
			else {
				
				for (u32 i = 1; i < child_num; ++i)
					if (children[i].dn < children[selected_index].dn)
						selected_index = i;

				for (u32 i = 0; i < child_num; ++i)
					if (i != selected_index)
					{
						if (children[i].dn < dn2)
							dn2 = children[i].dn;

						if (children[i].pn < NodeType::DNPN_MATE)
							pn2 -= children[i].pn;
					}

				pn2 = std::min(pn2, second_pn);
                if (group_interpositions) {
                    NodeCountType contribution = children[selected_index].pn;
                    if (children[selected_index].interposition) {
                        contribution = 0;
                        for (u32 i = 0; i < child_num; ++i)
                            if (children[i].interposition)
                                contribution = std::max(contribution, children[i].pn);
                    }
                    
                    
                    pn2 = second_pn - (node->pn - contribution);
                }

			}

			
			second_pn = pn2;
			second_dn = dn2;

			return &children[selected_index];
		}

		
		template <bool or_node>
		void SummarizeNode(NodeType* node)
		{
			NodeCountType pn = or_node ? NodeType::DNPN_INF : 0                 ;
			NodeCountType dn = or_node ? 0                  : NodeType::DNPN_INF;
            NodeCountType interposition_pn = 0;
            std::array<NodeCountType, 91> attack_dn{};

			u32 child_num = node->child_num;
			ASSERT_LV3(child_num != NodeType::CHILDNUM_NOT_INIT);
			NodeType* children = node_manager.get_children(node); 

			for (u32 i = 0; i < child_num ; ++i)
			{
				
				NodeCountType p = children[i].pn;
				NodeCountType d = children[i].dn;

				

				if (p >= NodeType::DNPN_MATE) --p;
				if (d >= NodeType::DNPN_MATE) --d;

				
				if (or_node)
				{
					
					
					
					

					pn = std::min(pn, p);

					
					
					

					if (dn >= NodeType::DNPN_MATE || d >= NodeType::DNPN_MATE)
						dn = std::max(dn, d);
					else
                        if (group_attacks && AttackGroup(&children[i])) {
                            const u8 group = AttackGroup(&children[i]);
                            attack_dn[group] = std::max(attack_dn[group], d);
                        } else
                            dn += d;
				}
				else {
					
					
					if (pn >= NodeType::DNPN_MATE || p >= NodeType::DNPN_MATE)
						pn = std::max(pn, p);
					else
                        if (group_interpositions && children[i].interposition)
                            interposition_pn = std::max(interposition_pn, p);
                        else
                            pn += p;


					dn = std::min(dn, d);
				}
			}
            if (!or_node && pn < NodeType::DNPN_MATE)
                pn += interposition_pn;
            if (or_node && group_attacks && dn < NodeType::DNPN_MATE)
                for (const auto contribution : attack_dn) dn += contribution;
			node->pn = pn;
			node->dn = dn;

			if (WithHash)
			{
				
				if (node->pn == 0 || node->dn == 0)
				{
					
					
					

					
					
					


					if (or_node)
						if (node->pn == 0) {
							for (u32 i = 0; i < child_num; ++i)
								if (children[i].pn == 0) {
									node->repeated = children[i].repeated;
									break;
								}
						}
						else {
							bool repeated = false;
							for (u32 i = 0; i < child_num; ++i)
								repeated |= children[i].repeated;
							node->repeated = repeated;
						}
					else 
						if (node->dn == 0) {
							for (u32 i = 0; i < child_num; ++i)
								if (children[i].dn == 0) {
									node->repeated = children[i].repeated;
									break;
								}
						}
						else {
							bool repeated = false;
							for (u32 i = 0; i < child_num; ++i)
								repeated |= children[i].repeated;
							node->repeated = repeated;
						}
				}
				else {
					
					
				}
			}

		}

		
		void ExpandRoot(Position& pos)
		{
			
			
			
			root_game_ply = pos.game_ply();
			root_color = pos.side_to_move();
			root_pos = &pos;

			current_root = new_node();
			
			
			ASSERT_LV3(current_root != nullptr);

			
			constexpr bool or_node = true;
			current_root->repeated = false;
			current_root->child_num = NodeType::CHILDNUM_NOT_INIT;
			ExpandNode<or_node>(pos, current_root);
		}

		
		
		
		
		
		template <bool or_node>
		void ExpandNode(Position& pos, NodeType* node)
		{
			pos.in_check() ? ExpandNode<or_node,true>(pos, node) : ExpandNode<or_node,false>(pos, node);
            RememberProof(pos, node);

			
			nodes_searched.store(nodes_searched.load(std::memory_order_relaxed) + 1,
			                     std::memory_order_relaxed);
		}

		template <bool or_node , bool INCHECK>
		void ExpandNode(Position& pos, NodeType* node)
		{
			ASSERT_LV3(node != nullptr && node->child_num == NodeType::CHILDNUM_NOT_INIT);
            if (threshold_search)
                max_expanded_depth = std::max(max_expanded_depth, pos.game_ply() - root_game_ply);

			
			
			
			const bool over_limit = max_game_ply && max_game_ply < pos.game_ply();
			if (or_node && over_limit)
			{
				node->set_child(0);
				node->set_nomate();
				if (WithHash)
					node->repeated = true;
				return;
			}

			const int plies_from_root = pos.game_ply() - root_game_ply;
			switch (mate_repetition(pos, plies_from_root, MAX_REPETITION_PLY, or_node))
			{
			case MateRepetitionState::Mate:
				node->set_child(0);
				node->template set_mate<or_node>();
				if (WithHash)
					node->repeated = true;
				return;

			case MateRepetitionState::Mated:
				node->set_child(0);
				node->template set_mated<or_node>();
				if (WithHash)
					node->repeated = true;
				return;

			case MateRepetitionState::Unknown:
				break;
			}

            if (threshold_search) {
                const auto found = proven_nodes.find(pos.key());
                if (found != proven_nodes.end() && found->second.attacker == root_color &&
                    found->second.node != node) {
                    
                    
                    
                    std::vector<Key> forbidden;
                    const auto* state = pos.state()->previous;
                    for (int ply = pos.game_ply() - root_game_ply; state && ply > 0;
                         --ply, state = state->previous)
                        forbidden.push_back(state->key());
                    const u64 prior = nodes_searched.load(std::memory_order_relaxed);
                    
                    const u64 remaining = !nodes_limit ? std::numeric_limits<u64>::max()
                        : prior < nodes_limit ? nodes_limit - prior - 1 : 0;
                    u64 work = 0;
                    Move ignored = MOVE_NONE;
                    const auto saved_callback = stop_callback;
                    if (saved_callback)
                        stop_callback = [saved_callback, prior](u64 work) {
                            return saved_callback(prior + 1 + work);
                        };
                    const std::function<void(Key)> visit = [&](Key) {
                        max_expanded_depth = std::max(max_expanded_depth,
                                                     pos.game_ply() - root_game_ply);
                    };
                    const bool replayed = ReplayProof<or_node>(pos, found->second.node,
                        remaining, work, ignored, true, &forbidden, &visit,
                        max_game_ply ? std::max(0, max_game_ply - pos.game_ply()) : INT32_MAX);
                    stop_callback = saved_callback;
                    nodes_searched.store(prior + work, std::memory_order_relaxed);
                    if (replayed) {
                        const auto last = node->lastMove;
                        const bool interposition = node->interposition;
                        *node = *found->second.node;
                        node->lastMove = last;
                        node->interposition = interposition;
                        ++proof_reuses;
                        return;
                    }
                }
            }

			
			
			
			
			
			if (WithHash)
			{
				

				auto key = pos.state()->board_key();
				auto entry = hash_table->first_entry(key);

				
				bool proven = false;

				entry->lock();
				if (   entry->board_key  == (key & 0xffffffffffff) 
					&& entry->root_color == root_color
					)
				{
#if 0
					
					std::cout << pos << std::endl
						      << entry->get_move() << std::endl
							  << entry->get_hand() << std::endl;
#endif

					
					if (entry->is_mate) 
					{
						
						
						
						
						
						if (   ( or_node && hand_is_equal_or_superior(pos.state()->hand , entry->get_hand()))
							|| (!or_node && hand_is_equal_or_superior(entry->get_hand() , pos.state()->hand))
							)
						{
							

#if 0
							
							if (or_node)
							{
								Mate::Dfpn::MateDfpnSolver dfpn(Mate::Dfpn::DfpnSolverType::Node64bit);
								dfpn.alloc(1024);
								Move move = dfpn.mate_dfpn(pos, 1000000);
								if (move == MOVE_NONE || move == MOVE_NULL)
								{
									
									std::cout << pos << std::endl;
								}
							}
#endif
						}
					}
					else { 
						
						
						
						
						
						if (   ( or_node && hand_is_equal_or_superior(entry->get_hand() , pos.state()->hand))
							|| (!or_node && hand_is_equal_or_superior(pos.state()->hand , entry->get_hand()))
							)
						{
							node->set_nomate(entry->ply);
							proven = true;

#if 0
							
							Mate::Dfpn::MateDfpnSolver dfpn(Mate::Dfpn::DfpnSolverType::Node64bit);
							dfpn.alloc(1024);
							if (dfpn.mate_dfpn(pos, 1000000) != MOVE_NULL)
							{
								
								std::cout << pos << std::endl;
							}
#endif
						}
					}
				}
				
				Move move = entry->get_move();

				
				entry->unlock();

				if (proven)
				{
					
					
					if (node == current_root)
					{
						NodeType* child = new_node(1);
						
						if (child == nullptr)
							return;

						child->template init_mate_move<or_node>(move);
						node->set_child(1,node_manager.node_to_node_index(child));
					}
					else {
						
						
						node->set_child(0);
					}

					
					node->repeated = false;
					return;
				}

			}

#if 1
			
			
			if (or_node && !pos.in_check())
			{
				Move mate_move = Mate::mate_1ply(pos);
				if (mate_move != MOVE_NONE)
				{
					
					NodeType* child = new_node(1);
					
					if (child == nullptr)
						return;

					child->template init_mate_move<or_node>(mate_move);

					node->set_child(1,node_manager.node_to_node_index(child));
					node->template set_mate<or_node>(1 );

					if (WithHash)
						node->repeated = false;
					
					return;
				}
			}
#endif

			
			auto mp = MovePicker<or_node, INCHECK, true , MoveOrdering>(pos);
			u32 child_num = (u32)mp.size();
			if constexpr (or_node) {
				if (move_hint && child_num > 1) {
					const Move preferred = move_hint(pos);
					const auto found = std::find_if(mp.begin(), mp.end(),
					    [&](const auto& candidate) { return candidate.move == preferred; });
					if (found != mp.end()) std::rotate(mp.begin(), found, found + 1);
				}
			}

			if (!or_node && over_limit && child_num)
			{
				
				
				
				
				
				node->set_child(0);
				node->set_nomate();
				if (WithHash)
					node->repeated = true;
				return;
			}


			node->child_num = child_num;

			if (child_num == 0)
			{
				
				
				node->set_child(0);
				node->template set_mated<or_node>();
			}
			else {
				NodeType* children = new_node(child_num);
				
				if (children == nullptr)
					return;

				
				node->set_child(child_num , node_manager.node_to_node_index(children) );

                Bitboard interpositions = ZERO_BB;
                if (!or_node && group_interpositions && pos.checkers().pop_count() == 1) {
                    auto checkers = pos.checkers();
                    interpositions = between_bb(checkers.pop(), pos.king_square(pos.side_to_move()));
                }
                for (auto m : mp) {
                    children->template init<or_node>(m);
                    children->interposition = interpositions.test(to_sq(m.move));
                    if (or_node && group_attacks)
                        attack_groups[node_manager.offset(children)] =
                            static_cast<u8>(Mate::CheckingAttackGroup(pos, m.move) + 1);
                    ++children;
                }

				
				
				SummarizeNode<or_node>(node);
			}
		}

		
		NodeType* new_node(size_t size = 1) {
			auto node = node_manager.new_node(size);
			if (node == nullptr)
				out_of_memory = true;
            else if (group_attacks)
                std::fill_n(attack_groups.begin() + node_manager.offset(node), size, u8{0});
			return node;
		}

	private:
		
		NodeType* current_root{nullptr};
		u64 proof_generation{};
		Position* root_pos;
		int root_game_ply; 
		Color root_color;  

		
        u64 nodes_limit{};
        bool threshold_search{};
        bool group_interpositions{};
        bool group_attacks{};
        
        std::vector<u8> attack_groups;
        u8 AttackGroup(const NodeType* node) const { return attack_groups[node_manager.offset(node)]; }
        u64 proof_epoch{1};
        void RememberProof(const Position& pos, NodeType* node) {
            if (threshold_search && !out_of_memory && node->pn == 0 && !node->repeated)
                proven_nodes[pos.key()] = {node, root_color};
        }
        struct ProvenNode { NodeType* node; Color attacker; };
        std::unordered_map<Key, ProvenNode> proven_nodes;
        u64 proof_reuses{};
        
        
        mutable std::vector<ReplayMemoEntry> replay_memo;
        mutable u64 replay_memo_epoch{};
        struct RetainedRoot { NodeType* root; int depth; int max_expanded_depth; };
        std::unordered_map<Key, RetainedRoot> threshold_roots;
        int max_expanded_depth{};
        std::function<bool(u64)> stop_callback;
        std::function<void(const Position&, bool, Move, int)> result_callback;

		
		std::atomic<NodeCountType> nodes_searched;

		
		std::atomic<bool> out_of_memory;

		
		Move bestmove;

		
		
		
		int max_game_ply;

		
		MateHashTable* hash_table;
		std::function<Move(const Position&)> move_hint;

	private:
		
		NodeManager<NodeCountType,MoveOrdering> node_manager;
	};

} 

#endif 

#endif 
