#ifndef HISSHI_DFPN_HPP_
#define HISSHI_DFPN_HPP_


#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../../position.h"

namespace hisshi {

using PnDn = std::uint64_t;
constexpr PnDn kInf = 1ULL << 60;
constexpr std::uint32_t kTTInf = 0xffffffffU;
inline std::uint32_t ToTT(PnDn v) {
  return v >= kInf ? kTTInf : static_cast<std::uint32_t>(v < kTTInf - 1 ? v : kTTInf - 1);
}
inline PnDn FromTT(std::uint32_t v) { return v == kTTInf ? kInf : v; }

struct Options {
  int max_ply = 400;
  bool full_width = false;   
  int non_check_cost = 2;    
  int lazy_dn = 64;          
  int lazy_pn = 0;           
  int sim_budget = 4096;     
  int check_stage_dn = 32;   
  int and_stage_dn = 16;     
  int cache_slots = 4096;    
  int helper_cache_slots = 1024;  
  int eps_percent = 0;       
  int single_thread_eps = 25;  
  int deep_pn = 16;          
  int threads = 1;           
  int futile = 0;
  int eager_mate1 = 1;  
  int eager_sim = 0;    
  int store_mate1 = 0;  
  int cache_min = 12;   
  int mate_eps = 50;    
};

enum HbStat { kHbHisOr, kHbHisAnd, kHbMateOr, kHbMateAnd, kHbTTRetHis, kHbTTRetMate, kHbMate1, kHbLazyOr, kHbLazyAnd,
              kHbSimTry, kHbSimHit, kHbCacheHit, kHbExpand, kHbN };
#ifdef HB_STATS
inline std::uint64_t g_hb[kHbN];
#define HB_CNT(i) (++::hisshi::g_hb[i])
#else
#define HB_CNT(i) ((void)0)
#endif

enum class Status { kUnknown, kProven, kDisproven };

struct Result {
  Status status = Status::kUnknown;
  PnDn pn = 1, dn = 1;
  std::uint64_t nodes = 0;
  std::uint64_t elapsed_ms = 0;
  std::vector<Move> pv;
  bool verified = false;
  std::string verify_info;
};

struct Limits {
  std::uint64_t nodes = 0;       
  std::int64_t time_ms = 0;      
  int pv_interval_ms = 1000;
  std::chrono::steady_clock::time_point origin{};  
};

struct EntryData {
  std::uint32_t hand;    
  std::uint32_t pn;      
  std::uint32_t dn;
  std::uint16_t best;    
  std::uint16_t lmr;     
  std::uint16_t amount;  
  std::uint16_t tag;     
  std::uint16_t Len() const { return lmr & 0x3fff; }
  std::uint8_t Mode() const { return (lmr >> 14) & 1; }  
  bool Rep() const { return (lmr >> 15) != 0; }           
};
static_assert(sizeof(EntryData) == 20, "entry size");

inline std::uint16_t EncodeAmount(std::uint64_t a) {
  int e = 0;
  while (a >= 4096 && e < 15) { a >>= 1; ++e; }
  if (a >= 4096) a = 4095;
  return static_cast<std::uint16_t>((e << 12) | a);
}
inline std::uint64_t DecodeAmount(std::uint16_t c) { return static_cast<std::uint64_t>(c & 4095) << (c >> 12); }

constexpr int kClusterSize = 8;
struct alignas(64) Cluster {
  std::uint32_t key[kClusterSize];  
  EntryData d[kClusterSize];
};
static_assert(sizeof(Cluster) == 192, "cluster size");

namespace detail {
struct SearchImpl;
}

class Solver {
 public:
  void Resize(std::size_t mb);
  void Clear();
  Result Solve(Position& root, const Limits& limits, const Options& opt,
               bool (*should_stop)());
  int Hashfull() const;
  std::size_t TableBytes() const { return table_.size() * sizeof(Cluster); }
  void KeepOnlyProofs();
  bool Shared() const { return shared_; }
  std::uint64_t TotalNodes() const { return total_nodes_.load(std::memory_order_relaxed); }
  std::string StatsString() const;
  void SetShared(bool v) { shared_ = v; }

  std::size_t ClusterOf(Key board) const {
    const std::uint64_t h = static_cast<std::uint64_t>(board) * 0x9E3779B97F4A7C15ULL;
    return static_cast<std::size_t>((static_cast<unsigned __int128>(h) * cluster_count_) >> 64);
  }
  void MarkDirty(std::size_t ci) { __atomic_store_n(&dirty_map_[ci], static_cast<std::uint8_t>(1), __ATOMIC_RELAXED); }
  void* MainCache(std::size_t bytes) {
    if (main_cache_bytes_ < bytes) {
      main_cache_.reset(new std::uint64_t[(bytes + 7) / 8]());
      main_cache_bytes_ = bytes;
    }
    return main_cache_.get();
  }

 private:
  friend struct detail::SearchImpl;
  std::vector<Cluster> table_;
  std::size_t cluster_count_ = 0;
  std::vector<std::uint8_t> dirty_map_;        
  std::vector<std::uint64_t> rep_dirty_bits_;  
  bool full_clear_ = true;                     
  std::unique_ptr<std::uint64_t[]> main_cache_;
  std::size_t main_cache_bytes_ = 0;

  struct alignas(64) Lock {
    std::atomic<std::uint32_t> v{0};
  };
  static constexpr std::size_t kLocks = 1 << 14;
  std::unique_ptr<Lock[]> locks_{new Lock[kLocks]};
  bool shared_ = false;  
  std::atomic<std::uint64_t> total_nodes_{0};  
  std::atomic<std::uint64_t> stat_new_{0}, stat_tt_return_{0}, stat_clamp32_{0}, stat_sat_{0};  
  bool dirty_ = false;   

  std::size_t rep_mask_ = 0;  
  std::unique_ptr<std::atomic<std::uint64_t>[]> rep_table_;
  void RepInsert(std::uint64_t path_key) {
    const std::size_t i = path_key & rep_mask_;
    std::uint64_t& w = rep_dirty_bits_[i >> 9];
    const std::uint64_t m = 1ULL << ((i >> 3) & 63);
    if (!(w & m)) {
      if (shared_) __atomic_fetch_or(&w, m, __ATOMIC_RELAXED);
      else w |= m;
    }
    rep_table_[i].store(path_key, std::memory_order_relaxed);
  }
  bool RepContains(std::uint64_t path_key) const {
    return path_key != 0 && rep_table_[path_key & rep_mask_].load(std::memory_order_relaxed) == path_key;
  }
  bool proofs_only_ = false;  
};

}  

#endif  
