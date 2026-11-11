
#include "hisshi_dfpn.hpp"

#include "answer.hpp"

namespace hisshi {

using namespace detail;

#ifdef HB_PROF
extern std::atomic<int> g_hb_phase;
void HbProfStart();
void HbProfStop();
struct HbProfGuard {
  HbProfGuard() { HbProfStart(); }
  ~HbProfGuard() { HbProfStop(); }
};
#endif

#ifdef HB_STATS
static void HbPrint(const char* phase, std::uint64_t ms) {
  static const char* nm[kHbN] = {"his_or", "his_and", "mate_or", "mate_and", "ttret_his", "ttret_mate", "mate1", "lazy_or",
                                 "lazy_and", "sim_try", "sim_hit", "cache_hit", "expand"};
  std::ostringstream os;
  os << "info string hbstats phase=" << phase << " ms=" << ms;
  for (int i = 0; i < kHbN; ++i) os << ' ' << nm[i] << '=' << g_hb[i];
  sync_cout << os.str() << sync_endl;
}
#endif


void Solver::Resize(std::size_t mb) {
  const std::size_t bytes = std::max<std::size_t>(mb, 1) * 1024 * 1024;
  std::size_t rep_entries = std::size_t(1) << 20;
  while (rep_entries * 2 * sizeof(std::uint64_t) <= bytes / 16) rep_entries *= 2;
  const std::size_t rep_bytes = rep_entries * sizeof(std::uint64_t);
  std::size_t clusters = (bytes > rep_bytes ? bytes - rep_bytes : 0) / sizeof(Cluster);
  if (clusters < 1024) clusters = 1024;
  if (clusters == cluster_count_ && !table_.empty() && rep_entries == rep_mask_ + 1) return;
  table_.clear();
  table_.shrink_to_fit();
  table_.resize(clusters);
  cluster_count_ = clusters;
  rep_table_.reset();
  rep_table_.reset(new std::atomic<std::uint64_t>[rep_entries]);
  rep_mask_ = rep_entries - 1;
  dirty_map_.assign(clusters, 0);
  rep_dirty_bits_.assign((rep_entries / 8 + 63) / 64, 0);
  full_clear_ = true;
  Clear();
}

void Solver::Clear() {
  const std::size_t nm = dirty_map_.size();
  const std::size_t nd = nm - static_cast<std::size_t>(std::count(dirty_map_.begin(), dirty_map_.end(), std::uint8_t{0}));
  if (!table_.empty()) {
    if (full_clear_ || nd * 8 > table_.size()) {
      std::memset(static_cast<void*>(table_.data()), 0, table_.size() * sizeof(Cluster));
    } else if (nd) {
      std::size_t k = 0;
      for (; k + 8 <= nm; k += 8) {
        std::uint64_t w;
        std::memcpy(&w, &dirty_map_[k], 8);
        if (!w) continue;
        for (std::size_t j = 0; j < 8; ++j)
          if (dirty_map_[k + j]) std::memset(static_cast<void*>(&table_[k + j]), 0, sizeof(Cluster));
      }
      for (; k < nm; ++k)
        if (dirty_map_[k]) std::memset(static_cast<void*>(&table_[k]), 0, sizeof(Cluster));
    }
  }
  if (nd) std::memset(dirty_map_.data(), 0, nm);
  if (rep_table_) {
    if (full_clear_) {
      for (std::size_t i = 0; i <= rep_mask_; ++i) rep_table_[i].store(0, std::memory_order_relaxed);
    } else {
      for (std::size_t k = 0; k < rep_dirty_bits_.size(); ++k)
        for (std::uint64_t w = rep_dirty_bits_[k]; w; w &= w - 1) {
          const std::size_t b = ((k << 6) + __builtin_ctzll(w)) << 3;
          for (std::size_t i = b; i < b + 8 && i <= rep_mask_; ++i) rep_table_[i].store(0, std::memory_order_relaxed);
        }
    }
  }
  std::fill(rep_dirty_bits_.begin(), rep_dirty_bits_.end(), 0);
  full_clear_ = false;
  dirty_ = false;
}

void Solver::KeepOnlyProofs() {
  for (Cluster& c : table_)
    for (int j = 0; j < kClusterSize; ++j)
      if (c.key[j] != 0 && c.d[j].pn != 0) c.key[j] = 0;
  proofs_only_ = true;
}

std::string Solver::StatsString() const {
  auto v = [](const std::atomic<std::uint64_t>& a) { return std::to_string(a.load(std::memory_order_relaxed)); };
  return "new=" + v(stat_new_) + " tt_return=" + v(stat_tt_return_) + " clamp32=" + v(stat_clamp32_) +
         " sat=" + v(stat_sat_);
}

int Solver::Hashfull() const {
  if (table_.empty()) return 0;
  int used = 0;
  const std::size_t sample = std::min<std::size_t>(table_.size(), 1000);
  for (std::size_t i = 0; i < sample; ++i) {
    Lock* l = shared_ ? &locks_[i & (kLocks - 1)] : nullptr;
    if (l)
      while (l->v.exchange(1, std::memory_order_acquire))
        while (l->v.load(std::memory_order_relaxed)) _mm_pause();
    for (int j = 0; j < kClusterSize; ++j) used += table_[i].key[j] != 0;
    if (l) l->v.store(0, std::memory_order_release);
  }
  return static_cast<int>(used * 1000 / (sample * kClusterSize));
}

namespace {


Options HelperOptions(const Options& opt, int t, int cache_slots) {
  static const int kLazyDn[] = {64, 32, 128, 48, 96, 80, 40, 160};
  static const int kStageDn[] = {16, 12, 24, 16, 20, 14, 16, 32};
  static const int kCost[] = {2, 3, 2, 2, 3, 2};
  static const int kCheckDn[] = {16, 8, 32, 16, 64, 12, 24};
  static const int kEps[] = {0, 50, 100, 25, 200};
  static const int kDeep[] = {16, 8, 24, 12, 32, 0, 20, 10, 16};
  Options h = opt;
  h.lazy_dn = kLazyDn[t % 8];
  h.and_stage_dn = kStageDn[t % 8];
  h.non_check_cost = kCost[t % 6];
  h.check_stage_dn = kCheckDn[t % 7];
  h.eps_percent = kEps[t % 5];
  if (opt.deep_pn > 0) h.deep_pn = kDeep[t % 9];
  h.cache_slots = cache_slots;
  return h;
}

void CacheSlots(std::size_t hash_bytes, const Options& opt, int nthreads, int& main_slots, int& helper_slots) {
  const std::size_t cache_bytes = hash_bytes / 4;
  auto slots = [](std::size_t bytes, int cap) {
    return static_cast<int>(std::min<std::size_t>(static_cast<std::size_t>(cap),
                                                  std::max<std::size_t>(64, bytes / sizeof(SearchImpl::CacheSlot))));
  };
  main_slots = slots(nthreads > 1 ? cache_bytes / 4 : cache_bytes, opt.cache_slots);
  helper_slots = nthreads > 1 ? slots(cache_bytes * 3 / 4 / static_cast<std::size_t>(nthreads - 1),
                                      std::min(opt.cache_slots, opt.helper_cache_slots))
                              : 0;
}

struct SolveContext {
  Solver& solver;
  Position& root;
  const std::string root_sfen;
  const Options& opt;
  const Limits& limits;
  bool (*should_stop)();
  int nthreads;
};


void VerifyDisproof(SearchImpl& s, Position& root, Result& res) {
  const std::uint64_t before = s.nodes;
  const auto t0 = s.ElapsedMs();
  DisproofVerifier v(s);
  int dep = DisproofVerifier::kNoDep;
  s.PushPath(root.key());
  const bool ok = v.Refuted(0, kModeHisshi, dep);
  s.PopPath();
  res.verified = ok;
  std::ostringstream os;
  if (!res.verify_info.empty()) os << res.verify_info << ' ';
  os << "check=" << ok << " visited=" << v.visited << " searches=" << v.searches
     << " trees=" << v.trees_disproven << "/" << v.trees << " tree_nodes=" << v.tree_nodes
     << " research_nodes=" << (s.nodes - before) << " time_ms=" << (s.ElapsedMs() - t0);
  if (!ok) os << " error=" << v.error;
  res.verify_info = os.str();
  res.status = ok ? Status::kDisproven : Status::kUnknown;
}

void ResolveSingleThread(const SolveContext& ctx, const Options& main_opt, Result& res) {
  ctx.solver.Clear();
  Options opt = main_opt;
  opt.threads = 1;
  if (opt.eps_percent == 0) opt.eps_percent = ctx.opt.single_thread_eps;
  Position p;
  StateInfo si;
  p.set(ctx.root_sfen, &si, ctx.root.this_thread());
  SearchImpl s(ctx.solver, p, opt, ctx.limits, ctx.should_stop, 0, nullptr);
  s.root_sfen = ctx.root_sfen;
  const NodeResult r = s.Run(kModeHisshi);
  s.FlushNodes();
  res.verify_info += std::string(" resolve=") + (r.pn == 0 ? "proof" : r.dn == 0 ? "disproof" : "open");
  if (r.pn == 0) {
    res.status = Status::kProven;
    res.pn = 0;
    res.dn = kInf;
  } else if (r.dn == 0) {
    VerifyDisproof(s, p, res);
  }
}

bool CollectVerifyTasks(SearchImpl& s, Position& root, int nthreads, std::vector<VerifyTask>& tasks,
                        std::uint64_t& visited) {
  for (int d = 2; d <= 20; d += 2) {
    tasks.clear();
    Verifier c(s);
    c.own_log2 = 18;  
    c.collect_depth = d;
    c.tasks = &tasks;
    s.PushPath(root.key());
    const bool ok = c.Verify(0, kModeHisshi);
    s.PopPath();
    if (!ok) return false;
    if (tasks.size() >= 1024u * static_cast<std::size_t>(nthreads) || d >= 20 || tasks.empty()) {
      visited += c.visited;
      return true;
    }
  }
  return false;
}

bool VerifyTasksInParallel(const SolveContext& ctx, Color attacker, const std::vector<VerifyTask>& tasks,
                           AtomicHandSet& done, std::uint64_t& visited) {
  std::atomic<std::size_t> next{0};
  std::atomic<bool> failed{false};
  std::atomic<std::uint64_t> total_visited{0};
  ctx.solver.SetShared(true);
  std::vector<std::thread> workers;
  for (int t = 0; t < ctx.nthreads; ++t) {
    workers.emplace_back([&]() {
      Options wopt = ctx.opt;
      wopt.cache_slots = 256;
      wopt.threads = 1;
      Limits wlim = ctx.limits;
      wlim.pv_interval_ms = 0;
      Position wp;
      StateInfo root_st;
      wp.set(ctx.root_sfen, &root_st, ctx.root.this_thread());
      SearchImpl w(ctx.solver, wp, wopt, wlim, ctx.should_stop, 0, nullptr, attacker);
      Verifier v(w);
      v.shared = &done;
      std::vector<StateInfo> mst(1024);
      for (std::size_t i; !failed && (i = next.fetch_add(1)) < tasks.size();) {
        const VerifyTask& task = tasks[i];
        if (task.moves.size() >= mst.size()) { failed = true; break; }
        for (std::size_t j = 0; j < task.moves.size(); ++j) {
          if (task.moves[j] == MOVE_NULL) wp.do_null_move(mst[j]);
          else wp.do_move(task.moves[j], mst[j]);
        }
        v.on_path = task.ancestors;
        v.raw_path = task.ancestors_raw;
        v.error.clear();
        w.PushPath(wp.key());
        const bool ok = v.Verify(static_cast<int>(task.moves.size()), task.mode);
        w.PopPath();
        for (std::size_t j = task.moves.size(); j-- > 0;) {
          if (task.moves[j] == MOVE_NULL) wp.undo_null_move();
          else wp.undo_move(task.moves[j]);
        }
        if (!ok) failed = true;
      }
      total_visited += v.visited;
    });
  }
  for (auto& th : workers) th.join();
  ctx.solver.SetShared(false);
  visited += total_visited;
  return !failed;
}

std::unique_ptr<AtomicHandSet> VerifyProof(const SolveContext& ctx, SearchImpl& s, Result& res) {
  const std::uint64_t before = s.nodes;
  const auto t0 = s.ElapsedMs();
  std::uint64_t visited = 0;
  std::string error;
  bool parallel_ok = false;
  std::unique_ptr<AtomicHandSet> done;
  int memo_log2 = 18;
  while (memo_log2 < 25 && (std::size_t(8) << (memo_log2 + 1)) <= ctx.solver.TableBytes() / 8) ++memo_log2;
  if (ctx.nthreads > 1) {
    std::vector<VerifyTask> tasks;
    if (CollectVerifyTasks(s, ctx.root, ctx.nthreads, tasks, visited)) {
      if (tasks.empty()) {
        parallel_ok = true;  
      } else if (tasks.size() >= 16u * static_cast<std::size_t>(ctx.nthreads)) {  
        done = std::make_unique<AtomicHandSet>(memo_log2);
        parallel_ok = VerifyTasksInParallel(ctx, s.atk, tasks, *done, visited);
      }
    }
  }
  bool ok = parallel_ok;
  if (!parallel_ok) {
    Verifier v(s);
    if (!done) done = std::make_unique<AtomicHandSet>(std::min(v.own_log2, memo_log2));
    v.shared = done.get();
    s.PushPath(ctx.root.key());
    ok = v.Verify(0, kModeHisshi);
    s.PopPath();
    visited = v.visited;
    error = v.error;
  }
  res.verified = ok;
  std::ostringstream os;
  os << "visited=" << visited << " research_nodes=" << (s.nodes - before) << " time_ms=" << (s.ElapsedMs() - t0)
     << (parallel_ok ? " parallel" : "");
  if (!ok) os << " error=" << error;
  res.verify_info = os.str();
  if (!ok) done.reset();
  return done;
}


void BuildAnswer(const SolveContext& ctx, SearchImpl& s, AtomicHandSet* verified, Result& res) {
  const auto t0 = s.ElapsedMs();
  s.FlushNodes();
  const std::uint64_t n0 = ctx.solver.TotalNodes();
  PvBuilder builder(ctx.solver, ctx.opt, s.atk);
  builder.verified = verified;
  std::unique_ptr<AnswerPool> pool;
  if (ctx.nthreads > 1) {
    pool = std::make_unique<AnswerPool>(ctx.solver, ctx.root_sfen, ctx.root.this_thread(), ctx.opt, ctx.limits,
                                        ctx.should_stop, ctx.nthreads, s.atk);
    builder.pool = pool.get();
  }
  res.pv = builder.Pv(s);
  pool.reset();  
  const int mate_tail = builder.AppendMateLine(s, res.pv);
  s.FlushNodes();
  if (mate_tail) res.verify_info += " mate_tail=" + std::to_string(mate_tail);
  res.verify_info += std::string(" answer=") + (builder.exact ? "longest" : "greedy") +
                     " answer_ms=" + std::to_string(s.ElapsedMs() - t0) +
                     " answer_nodes=" + std::to_string(ctx.solver.TotalNodes() - n0);
}

}  


Result Solver::Solve(Position& root, const Limits& given_limits, const Options& opt, bool (*should_stop)()) {
#ifdef HB_PROF
  HbProfGuard hb_prof_guard;
#endif
  Limits limits = given_limits;
  if (limits.origin == std::chrono::steady_clock::time_point{}) limits.origin = std::chrono::steady_clock::now();
  InitTables();
  if (table_.empty()) Resize(256);
  if (dirty_) Clear();
  dirty_ = true;
  proofs_only_ = false;
  Result res;
  if (root.king_square(~root.side_to_move()) == SQ_NB) {
    res.status = Status::kDisproven;
    res.pn = kInf;
    res.dn = 0;
    return res;
  }
  const int nthreads = std::max(1, opt.threads);
  const SolveContext ctx{*this, root, root.sfen(), opt, limits, should_stop, nthreads};
  int main_slots = 0, helper_slots = 0;
  CacheSlots(TableBytes(), opt, nthreads, main_slots, helper_slots);

  std::atomic<bool> done{false};
  std::atomic<bool> helper_rep_disproof{false};  
  std::vector<std::thread> helpers;
  total_nodes_ = 0;
  stat_new_ = stat_tt_return_ = stat_clamp32_ = stat_sat_ = 0;
#ifdef HB_STATS
  for (auto& x : g_hb) x = 0;
#endif
  shared_ = nthreads > 1;
  for (int t = 1; t < nthreads; ++t) {
    helpers.emplace_back([&, t]() {
      Position hp;
      StateInfo hsi;
      hp.set(ctx.root_sfen, &hsi, root.this_thread());
      Limits hlim = limits;
      hlim.pv_interval_ms = 0;
      SearchImpl h(*this, hp, HelperOptions(opt, t, helper_slots), hlim, should_stop, t, &done);
      const NodeResult hr = h.Run(kModeHisshi);
      if (hr.dn == 0 && hr.rep) helper_rep_disproof = true;
      if (hr.pn == 0 || hr.dn == 0) done = true;
    });
  }
  Options main_opt = opt;
  if (nthreads == 1 && main_opt.eps_percent == 0) main_opt.eps_percent = opt.single_thread_eps;
  main_opt.cache_slots = main_slots;
  SearchImpl s(*this, root, main_opt, limits, should_stop, 0, nthreads > 1 ? &done : nullptr, COLOR_NB, true);
  s.report = true;
  s.root_sfen = ctx.root_sfen;
  NodeResult r = s.Run(kModeHisshi);
  done = true;
  for (auto& th : helpers) th.join();
  shared_ = false;
  if (nthreads > 1) {
    s.shared_stop = nullptr;
    const Probe rp = s.Lookup(BoardKey(root), root.hand_of(s.atk), kModeHisshi);
    if (rp.pn == 0 || (rp.dn == 0 && !rp.rep)) {
      r.pn = rp.pn;
      r.dn = rp.dn;
      r.rep = false;
    } else if (r.pn != 0 && r.dn != 0 && helper_rep_disproof) {
      r.pn = kInf;
      r.dn = 0;
      r.rep = true;
    }
  }
  res.pn = r.pn;
  res.dn = r.dn;
#ifdef HB_PROF
  g_hb_phase = 1;
#endif
#ifdef HB_STATS
  HbPrint("search", s.ElapsedMs());
#endif
  s.FlushNodes();  
  res.nodes = TotalNodes();
  res.elapsed_ms = s.ElapsedMs();
  if (r.pn == 0) {
    res.status = Status::kProven;
  } else if (r.dn == 0) {
    if (r.rep) res.verify_info = "rep=1";
    VerifyDisproof(s, root, res);
    if (nthreads > 1 && !res.verified && !s.LimitReached()) ResolveSingleThread(ctx, main_opt, res);
    res.nodes = TotalNodes();
    res.elapsed_ms = s.ElapsedMs();
  }
  if (res.status != Status::kProven) return res;

  s.report = false;  
  if (limits.pv_interval_ms > 0)
    sync_cout << "info string proof found (" << s.ElapsedMs() << " ms), verifying" << sync_endl;
  if (Hashfull() > 300) KeepOnlyProofs();  
  const std::unique_ptr<AtomicHandSet> verified = VerifyProof(ctx, s, res);
  const bool cut_off = !res.verified && s.LimitReached();
  if (cut_off) res.verify_info += " stopped=limit";
  if (limits.pv_interval_ms > 0)
    sync_cout << "info string proof "
              << (res.verified ? "verified" : cut_off ? "verification stopped by the limit" : "NOT verified") << " ("
              << s.ElapsedMs() << " ms)" << (res.verified ? ", building the answer" : "") << sync_endl;
  if (res.verified) BuildAnswer(ctx, s, verified.get(), res);

  s.FlushNodes();  
  res.nodes = TotalNodes();
  res.elapsed_ms = s.ElapsedMs();
#ifdef HB_STATS
  HbPrint("all", res.elapsed_ms);
#endif
  return res;
}

}  

#ifdef HB_PROF
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdio>
#include <unordered_map>
namespace hisshi {
std::atomic<int> g_hb_phase{0};
namespace {
std::atomic<bool> g_hbp_run{false};
std::thread g_hbp_th;
std::unordered_map<std::uint64_t, std::uint32_t> g_hbp_hist;
}  
void HbProfStart() {
  HANDLE target = nullptr;
  if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &target,
                       THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, 0))
    return;
  g_hb_phase = 0;
  g_hbp_hist.clear();
  g_hbp_run = true;
  const std::uint64_t base = reinterpret_cast<std::uint64_t>(GetModuleHandleW(nullptr));
  g_hbp_th = std::thread([target, base] {
    HANDLE tm = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    while (g_hbp_run.load(std::memory_order_relaxed)) {
      if (tm) {
        LARGE_INTEGER due;
        due.QuadPart = -2000;  
        SetWaitableTimer(tm, &due, 0, nullptr, nullptr, FALSE);
        WaitForSingleObject(tm, INFINITE);
      } else {
        Sleep(1);
      }
      if (SuspendThread(target) == static_cast<DWORD>(-1)) continue;
      CONTEXT ctx;
      ctx.ContextFlags = CONTEXT_CONTROL;
      const bool got = GetThreadContext(target, &ctx) != 0;
      ResumeThread(target);  
      if (got) {
        const std::uint64_t off = static_cast<std::uint64_t>(ctx.Rip) - base;
        ++g_hbp_hist[(static_cast<std::uint64_t>(g_hb_phase.load()) << 32) | (off & 0xffffffffULL)];
      }
    }
    if (tm) CloseHandle(tm);
    CloseHandle(target);
  });
}
void HbProfStop() {
  if (!g_hbp_run) return;
  g_hbp_run = false;
  if (g_hbp_th.joinable()) g_hbp_th.join();
  char fn[64];
  std::snprintf(fn, sizeof fn, "hbprof_%lu.txt", static_cast<unsigned long>(GetCurrentProcessId()));
  if (FILE* f = std::fopen(fn, "a")) {
    for (const auto& [k, v] : g_hbp_hist)
      std::fprintf(f, "%u %x %u\n", static_cast<unsigned>(k >> 32), static_cast<unsigned>(k & 0xffffffffu), v);
    std::fclose(f);
  }
}
}  
#endif
