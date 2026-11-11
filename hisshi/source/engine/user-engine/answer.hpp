
#ifndef HISSHI_ANSWER_HPP_
#define HISSHI_ANSWER_HPP_

#include "verify.hpp"

namespace hisshi {
namespace detail {

struct AnswerPool {
  struct Task {
    std::string sfen;  
    Move move;
    std::uint64_t budget, threat_budget;
    bool result = false;
    std::uint64_t nodes = 0;
  };

  Solver& solver;
  std::vector<std::thread> threads;
  std::mutex mu;
  std::condition_variable wake, finished_cv;
  std::vector<Task>* batch = nullptr;
  std::atomic<std::size_t> next{0}, finished{0};
  std::uint64_t generation = 0;
  int active = 0;  
  bool quit = false;
  bool prev_shared;

  AnswerPool(Solver& s, const std::string& root_sfen, Thread* th, const Options& opt, const Limits& lim,
             bool (*should_stop)(), int n, Color atk)
      : solver(s), prev_shared(s.Shared()) {
    solver.SetShared(true);
    for (int t = 0; t < n; ++t) {
      threads.emplace_back([this, root_sfen, th, opt, lim, should_stop, t, atk]() {
        Options wopt = opt;
        wopt.cache_slots = 256;
        wopt.threads = 1;
        Limits wlim = lim;  
        wlim.pv_interval_ms = 0;
        Position wp;
        StateInfo root_st, st1, st2;
        wp.set(root_sfen, &root_st, th);
        SearchImpl w(solver, wp, wopt, wlim, should_stop, 100 + t, nullptr, atk);
        std::uint64_t seen = 0;
        for (;;) {
          std::vector<Task>* current;
          {
            std::unique_lock<std::mutex> lk(mu);
            wake.wait(lk, [&] { return quit || generation != seen; });
            if (quit) return;
            seen = generation;
            current = batch;
            if (!current) continue;  
            ++active;
          }
          std::vector<Task>& tasks = *current;
          for (std::size_t i; (i = next.fetch_add(1)) < tasks.size();) {
            Task& task = tasks[i];
            const std::uint64_t n0 = w.nodes;
            wp.set(task.sfen, &root_st, th);
            const bool check = wp.gives_check(task.move);
            wp.do_move(task.move, st1);
            bool threat = check;  
            if (!check) {
              wp.do_null_move(st2);
              threat = w.SubSearch(kModeMate, task.threat_budget).pn == 0;
              wp.undo_null_move();
            }
            task.result = threat && w.SubSearch(kModeHisshi, task.budget).pn == 0;
            task.nodes = w.nodes - n0;
            finished.fetch_add(1);
          }
          {
            std::lock_guard<std::mutex> lk(mu);
            --active;
            finished_cv.notify_all();
          }
        }
      });
    }
  }
  ~AnswerPool() {
    {
      std::lock_guard<std::mutex> lk(mu);
      quit = true;
    }
    wake.notify_all();
    for (auto& t : threads) t.join();
    solver.SetShared(prev_shared);
  }
  void Run(std::vector<Task>& tasks) {
    if (tasks.empty()) return;
    std::unique_lock<std::mutex> lk(mu);
    batch = &tasks;
    next = 0;
    finished = 0;
    ++generation;
    wake.notify_all();
    finished_cv.wait(lk, [&] { return finished.load() >= tasks.size() && active == 0; });
    batch = nullptr;
  }
};

struct PvBuilder {
  Solver& solver;
  const Options& opt;
  Color atk;
  PvBuilder(Solver& s, const Options& o, Color attacker) : solver(s), opt(o), atk(attacker) {}
  static constexpr std::uint64_t kMateProbe = 10000;  
  static constexpr int kMaxDepth = 9;                 


  AnswerPool* pool = nullptr;  
  AtomicHandSet* verified = nullptr;  
  std::unordered_map<Key, bool> mated_memo;
  std::unordered_set<Key> not_known_mated;  

  bool ReplyMated(SearchImpl& h, Move m, StateInfo& st) {
    Position& p = h.pos;
    p.do_move(m, st);
    const Key k = p.key();
    bool mated;
    const auto it = mated_memo.find(k);
    if (it != mated_memo.end()) {
      mated = it->second;
    } else {
      Probe pm = h.Lookup(BoardKey(p), p.hand_of(atk), kModeMate);
      if (pm.pn != 0 && !p.in_check() && Mate::mate_1ply(p) != MOVE_NONE) pm.pn = 0;
      if (pm.pn != 0 && pm.dn != 0) {
        h.SubSearch(kModeMate, kMateProbe);
        pm = h.Lookup(BoardKey(p), p.hand_of(atk), kModeMate);
      }
      mated = pm.pn == 0;
      mated_memo[k] = mated;
    }
    p.undo_move(m);
    return mated;
  }

  bool IsInterposition(const Position& p, Move r, Square slider) const {
    if (!opt.futile || !is_drop(r) || slider == SQ_NB) return false;
    const Piece pc = p.piece_on(slider);
    if (pc == NO_PIECE || color_of(pc) != atk || !IsSlider(type_of(pc))) return false;
    return static_cast<bool>(effects_from(pc, slider, p.pieces()) & Bitboard(to_sq(r)));
  }

  std::unordered_map<Key, std::vector<Move>> replies_cache;
  const std::vector<Move>& Replies(SearchImpl& h) {
    const auto it = replies_cache.find(h.pos.key());
    if (it != replies_cache.end()) return it->second;
    return replies_cache[h.pos.key()] = RepliesUncached(h);
  }

  std::vector<Move> RepliesUncached(SearchImpl& h) {
    Position& p = h.pos;
    StateInfo st;
    std::vector<std::pair<int, Move>> replies;
    for (const auto& em : MoveList<LEGAL_ALL>(p)) {
      p.do_move(em.move, st);
      const Probe ph = h.Lookup(BoardKey(p), p.hand_of(atk), kModeHisshi);
      p.undo_move(em.move);
      replies.emplace_back(ph.pn == 0 ? ph.len : 0, em.move);
    }
    std::stable_sort(replies.begin(), replies.end(),
                     [](const auto& x, const auto& y) { return x.first > y.first; });
    std::vector<Move> out;
    for (const auto& rm : replies) out.push_back(rm.second);
    return out;
  }

  bool KnownMated(SearchImpl& h, Move m, StateInfo& st) {
    Position& p = h.pos;
    p.do_move(m, st);
    const auto it = mated_memo.find(p.key());
    bool mated;
    if (it != mated_memo.end()) {
      mated = it->second;
    } else if (not_known_mated.count(p.key())) {
      mated = false;
    } else {
      mated = h.Lookup(BoardKey(p), p.hand_of(atk), kModeMate).pn == 0 ||
              (!p.in_check() && Mate::mate_1ply(p) != MOVE_NONE);
      if (mated) mated_memo[p.key()] = true;
      else not_known_mated.insert(p.key());
    }
    p.undo_move(m);
    return mated;
  }

  Move FirstRealDefence(SearchImpl& h, Square slider, int depth) {
    StateInfo st;
    std::vector<Move> open, inter;
    for (const Move m : Replies(h))
      if (!KnownMated(h, m, st)) (IsInterposition(h.pos, m, slider) ? inter : open).push_back(m);
    for (const Move m : open)
      if (!ReplyMated(h, m, st)) return m;
    for (const Move m : inter) {
      if (ReplyMated(h, m, st)) continue;
      if (depth < kMaxDepth && FutileAtOnce(h, m, slider, depth)) continue;
      return m;
    }
    return MOVE_NONE;
  }

  std::unordered_map<Key, bool> at_once_memo;

  bool FutileAtOnce(SearchImpl& h, Move r, Square slider, int depth) {
    Position& p = h.pos;
    StateInfo s1, s2;
    bool futile = false;
    p.do_move(r, s1);
    for (const Move cap : Captures(p, slider, to_sq(r))) {
      p.do_move(cap, s2);
      const Key k = Mix(p.key(), to_sq(r));
      const auto it = at_once_memo.find(k);
      if (it != at_once_memo.end()) {
        futile = it->second;
      } else {
        futile = FirstRealDefence(h, to_sq(r), depth + 1) == MOVE_NONE;
        at_once_memo[k] = futile;
      }
      p.undo_move(cap);
      if (futile) break;
    }
    p.undo_move(r);
    return futile;
  }

  static Key Mix(Key k, std::uint64_t a) { return k ^ ((a + 1) * 0x9E3779B97F4A7C15ULL); }

  std::vector<Move> Captures(const Position& p, Square slider, Square sq) const {
    std::vector<Move> out;
    for (const bool promote : {true, false}) {
      const Move cap = p.to_move(promote ? make_move_promote16(slider, sq) : make_move16(slider, sq));
      if (cap != MOVE_NONE && p.pseudo_legal_s<true>(cap) && p.legal(cap)) out.push_back(cap);
    }
    return out;
  }

  static constexpr int kMaxLength = 12;               
  static constexpr int kMaxLengthChain = 40;          
  static constexpr int kMaxAnswer = 255;              
  static constexpr std::uint64_t kBudget = 3000000;   
  static constexpr std::uint64_t kReprove = 5000000;       
  static constexpr std::uint64_t kReproveFutile = 200000;  
  int futility_depth = 0;  
  std::uint64_t ReproveBudget() const { return futility_depth ? kReproveFutile : kReprove; }
  std::uint64_t visits = 0;
  bool exact = false;  
  bool limit_reached = false;  
  bool OutOfBudget(SearchImpl& h) {
    if ((visits & 1023) == 0 && !limit_reached && h.LimitReached()) limit_reached = true;
    if (limit_reached) visits = kBudget;
    return visits >= kBudget;
  }
  struct Bounds {
    int lo = -1;
    int hi = 1 << 30;
  };
  std::unordered_map<Key, Bounds> bounds;

  std::unordered_map<Key, std::vector<std::pair<int, Move>>> proven_cache;
  std::unordered_set<Key> reprove_tried;  
  const std::vector<std::pair<int, Move>>& ProvenMoves(SearchImpl& h) {
    const Key key = futility_depth ? Mix(h.pos.key(), 29) : h.pos.key();
    const auto it = proven_cache.find(key);
    if (it != proven_cache.end()) return it->second;
    return proven_cache[key] = ProvenMovesUncached(h);
  }

  std::vector<std::pair<int, Move>> ProvenMovesUncached(SearchImpl& h) {
    Position& p = h.pos;
    std::vector<std::pair<int, Move>> moves;
    for (int attempt = 0; attempt < 2 && moves.empty(); ++attempt) {
      if (attempt == 1) {
        const Probe self = h.Lookup(BoardKey(p), p.hand_of(atk), kModeHisshi);
        if (self.dn == 0) break;
        if (futility_depth && !reprove_tried.insert(Mix(p.key(), ReproveBudget())).second) break;
        if (self.pn != 0) {
          if (h.SubSearch(kModeMate, kMateProbe).pn != 0) h.SubSearch(kModeHisshi, ReproveBudget());
        } else {
          const Move b = p.to_move(Move16(self.best));
          if (b == MOVE_NONE || !p.pseudo_legal_s<true>(b) || !p.legal(b)) break;
          StateInfo st;
          p.do_move(b, st);
          h.SubSearch(kModeHisshi, ReproveBudget());
          p.undo_move(b);
        }
      }
      if (!p.in_check()) {
        const Move mate = Mate::mate_1ply(p);
        if (mate != MOVE_NONE) moves.emplace_back(0, mate);
      }
      for (const auto& em : MoveList<LEGAL_ALL>(p)) {
        Child c{};
        c.move = em.move;
        const Probe pr = h.ProbeChild(c, kModeHisshi);
        if (pr.pn == 0) moves.emplace_back(pr.len, em.move);
      }
    }
    std::stable_sort(moves.begin(), moves.end(), [](const auto& x, const auto& y) { return x.first < y.first; });
    return moves;
  }

  bool OrWithin(SearchImpl& h, Square slider, int k) {
    if (k <= 0) return false;
    Position& p = h.pos;
    Bounds& bd = bounds[Mix(Mix(p.key(), slider), chain_eval ? 11 : 1)];
    if (k >= bd.hi) return true;
    if (k <= bd.lo || OutOfBudget(h)) return false;
    ++visits;
    const std::vector<std::pair<int, Move>> moves = ProvenMoves(h);
    bool ok = false;
    StateInfo st;
    for (const auto& lm : moves) {
      const Move m = lm.second;
      const Square next = IsSlider(type_of(p.moved_piece_after(m))) ? to_sq(m) : slider;
      p.do_move(m, st);
      ok = AndWithin(h, next, k - 1);
      p.undo_move(m);
      if (ok) break;
    }
    Bounds& b2 = bounds[Mix(Mix(p.key(), slider), chain_eval ? 11 : 1)];
    if (ok) b2.hi = std::min(b2.hi, k);
    else if (visits < kBudget) b2.lo = std::max(b2.lo, k);
    return ok;
  }

  bool AndWithin(SearchImpl& h, Square slider, int k) {
    if (k < 0) return false;
    Position& p = h.pos;
    Bounds& bd = bounds[Mix(Mix(p.key(), slider), chain_eval ? 12 : 2)];
    if (k >= bd.hi) return true;
    if (k <= bd.lo || OutOfBudget(h)) return false;
    ++visits;
    bool ok = true;
    StateInfo st;
    for (const Move m : Replies(h)) {
      if (KnownMated(h, m, st)) continue;
      bool answered = false;
      if (k > 0) {
        p.do_move(m, st);
        answered = OrWithin(h, slider, k - 1);
        p.undo_move(m);
      }
      if (answered) continue;
      if (ReplyMated(h, m, st)) continue;
      if (IsInterposition(p, m, slider) &&
          (FutileAtOnce(h, m, slider, 1) || (!chain_eval && ChainDrop(p, m, slider) && ChainFutile(h, m, slider))))
        continue;
      ok = false;
      break;
    }
    Bounds& b2 = bounds[Mix(Mix(p.key(), slider), chain_eval ? 12 : 2)];
    if (ok) b2.hi = std::min(b2.hi, k);
    else if (visits < kBudget) b2.lo = std::max(b2.lo, k);
    return ok;
  }

  bool Futile(SearchImpl& h, Move r, Square slider) {
    ++futility_depth;
    const bool f = FutileImpl(h, r, slider);
    --futility_depth;
    return f;
  }

  bool FutileImpl(SearchImpl& h, Move r, Square slider) {
    Position& p = h.pos;
    StateInfo s1, s2, s3;
    int after = -1;
    p.do_move(r, s1);
    for (const Move cap : Captures(p, slider, to_sq(r))) {
      p.do_move(cap, s2);
      for (int k = 0; k <= kMaxLength && after < 0; k += 2)
        if (AndWithin(h, to_sq(r), k)) after = k;
      p.undo_move(cap);
      if (after >= 0) break;
    }
    p.undo_move(r);
    if (after < 0) return false;  
    if (after == 0) return true;  
    for (const Move o : Replies(h)) {
      if (IsInterposition(p, o, slider) || ReplyMated(h, o, s3)) continue;
      p.do_move(o, s3);
      const bool shorter = OrWithin(h, slider, after - 2);
      p.undo_move(o);
      if (!shorter) return true;
    }
    return false;
  }

  bool ChainDrop(Position& p, Move r, Square slider) {
    if (!IsInterposition(p, r, slider)) return false;
    const Square ksq = p.king_square(~atk);
    if (ksq == SQ_NB) return false;
    if (!(between_bb(slider, ksq) & Bitboard(to_sq(r)))) return false;
    StateInfo st;
    p.do_move(r, st);
    const bool capturable = !Captures(p, slider, to_sq(r)).empty();
    p.undo_move(r);
    return capturable;
  }

  bool RealWithin(SearchImpl& h, Square slider, int k) {
    Position& p = h.pos;
    StateInfo st;
    for (const Move m : Replies(h)) {
      if (ChainDrop(p, m, slider)) continue;
      if (KnownMated(h, m, st)) continue;
      if (ReplyMated(h, m, st)) continue;
      if (k > 0) {
        p.do_move(m, st);
        const bool ok = OrWithin(h, slider, k - 1);
        p.undo_move(m);
        if (ok) continue;
      }
      if (IsInterposition(p, m, slider) && FutileAtOnce(h, m, slider, 1)) continue;
      return false;
    }
    return true;
  }

  static constexpr std::size_t kShortTableMb = 128;
  static constexpr std::uint64_t kShortCall = 3000000;    
  static constexpr std::uint64_t kShortBudget = 40000000;  
  static constexpr std::uint64_t kJudgeBudget = 6000000;
  int chain_depth = 0;           
  std::uint64_t judge_left = 0;  
  std::uint64_t last_bs_nodes = 0;
  std::unique_ptr<Solver> short_solver;
  std::unique_ptr<SearchImpl> short_search;
  std::uint64_t short_nodes = 0;
  int chain_eval = 0;          
  bool chain_restart = false;  
  std::unordered_map<Key, bool> chain_memo;     
  std::unordered_map<Key, int> real_memo;       
  std::unordered_map<Key, int> shortest_memo;   
  std::unordered_map<Key, bool> bound_memo;     

  SearchImpl& ShortSearcher(SearchImpl& h) {
    if (!short_search) {
      short_solver = std::make_unique<Solver>();
      short_solver->Resize(kShortTableMb);
      Options so = opt;
      so.cache_slots = 256;
      so.threads = 1;
      so.eps_percent = std::max(so.eps_percent, 50);  
      Limits sl = h.lim;  
      sl.nodes = 0;
      sl.pv_interval_ms = 0;
      short_search = std::make_unique<SearchImpl>(*short_solver, h.pos, so, sl, nullptr, 0, nullptr, atk);
    }
    return *short_search;
  }

  bool BoundedSearch(SearchImpl& h, int d, std::uint64_t cap = kShortCall) {
    last_bs_nodes = 0;
    std::uint64_t budget = std::min(cap, kShortBudget - std::min(short_nodes, kShortBudget));
    if (chain_depth) budget = std::min(budget, judge_left);
    if (d < 1 || budget == 0) return false;
    SearchImpl& s2 = ShortSearcher(h);
    short_solver->Clear();
    const std::uint64_t n0 = s2.nodes;
    const bool ok = s2.ShortSearch(d, budget);
    last_bs_nodes = s2.nodes - n0;
    short_nodes += last_bs_nodes;
    if (chain_depth) judge_left -= std::min(judge_left, last_bs_nodes);
    if (!ok) return false;
    CopyProof(s2, h, d);
    chain_restart = true;
    return true;
  }

  void CopyProof(SearchImpl& from, SearchImpl& to, int depth) {
    Position& p = to.pos;
    const Key board = BoardKey(p);
    const Hand hand = p.hand_of(atk);
    const Probe pr = from.Lookup(board, hand, kModeHisshi);
    if (pr.pn != 0) return;
    const bool or_node = p.side_to_move() == atk;
    const Move best = or_node ? p.to_move(Move16(pr.best)) : MOVE_NONE;
    to.Store(board, hand, kModeHisshi, 0, kInf, pr.len, best, false, 1000);
    if (depth <= 0) return;
    StateInfo st;
    if (or_node) {
      if (best == MOVE_NONE || !p.pseudo_legal_s<true>(best) || !p.legal(best)) return;
      p.do_move(best, st);
      CopyProof(from, to, depth - 1);
      p.undo_move(best);
      return;
    }
    for (const auto& em : MoveList<LEGAL_ALL>(p)) {
      p.do_move(em.move, st);
      CopyProof(from, to, depth - 1);
      p.undo_move(em.move);
    }
  }

  void Shortest(SearchImpl& h, int upper) {
    const Key key = Mix(h.pos.key(), 13);
    if (shortest_memo.count(key)) return;
    int best = upper;
    std::uint64_t cap = kShortCall;
    for (int d = upper - 2; d >= 1 && BoundedSearch(h, d, cap); d -= 2) {
      best = d;
      cap = std::min(kShortCall, std::max<std::uint64_t>(4 * last_bs_nodes, 300000));
    }
    shortest_memo[key] = best;
  }

  bool WithinByBoundedSearch(SearchImpl& h, int d) {
    const Key key = Mix(Mix(h.pos.key(), static_cast<std::uint64_t>(d)), 23);
    const auto it = bound_memo.find(key);
    if (it != bound_memo.end()) return it->second;
    return bound_memo[key] = BoundedSearch(h, d);
  }

  int RealLen(SearchImpl& h, Square slider) {
    const Key key = Mix(Mix(h.pos.key(), slider), 17);
    const auto it = real_memo.find(key);
    if (it != real_memo.end()) return it->second;
    Position& p = h.pos;
    int len = -1;
    for (int k = 0; k <= kMaxLengthChain && visits < kBudget; k += 2)
      if (RealWithin(h, slider, k)) { len = k; break; }
    if (len > 0) {
      StateInfo st;
      for (const Move m : Replies(h)) {
        if (ChainDrop(p, m, slider) || KnownMated(h, m, st)) continue;
        p.do_move(m, st);
        const bool longest = !OrWithin(h, slider, len - 3);
        if (longest) Shortest(h, len - 1);
        p.undo_move(m);
        if (longest) break;
      }
    }
    return real_memo[key] = len;
  }

  bool ChainFutile(SearchImpl& h, Move r, Square slider) {
    Position& p = h.pos;
    const Key key = Mix(Mix(BoardKey(p) ^ (p.side_to_move() == BLACK ? 0 : 0x5bd1e995ULL), to_sq(r)), slider);
    const auto it = chain_memo.find(key);
    if (it != chain_memo.end()) return it->second;
    if (chain_depth++ == 0) judge_left = kJudgeBudget;
    ++chain_eval;
    ++futility_depth;  
    const std::uint64_t saved_visits = visits;
    visits = 0;
    bool futile = false;
    const int real = RealLen(h, slider);
    if (real >= 0) {
      StateInfo s1, s2, s3;
      p.do_move(r, s1);
      for (const Move cap : Captures(p, slider, to_sq(r))) {
        p.do_move(cap, s2);
        const Square next = to_sq(r);
        bool within = RealWithin(h, next, real);
        if (!within) {
          StateInfo s4;
          for (const Move m : Replies(h)) {
            if (ChainDrop(p, m, next) || KnownMated(h, m, s4)) continue;
            p.do_move(m, s4);
            const bool longer = real < 1 || !OrWithin(h, next, real - 1);
            if (longer) WithinByBoundedSearch(h, real - 1);
            p.undo_move(m);
            if (longer) break;
          }
        }
        for (const Move r2 : Replies(h)) {
          if (!within) break;
          if (!ChainDrop(p, r2, next)) continue;
          --chain_eval;
          const bool f2 = ChainFutile(h, r2, next);
          ++chain_eval;
          if (f2) continue;
          p.do_move(r2, s3);
          within = real >= 1 && OrWithin(h, next, real - 1);
          p.undo_move(r2);
        }
        p.undo_move(cap);
        if (within) { futile = true; break; }
      }
      p.undo_move(r);
    }
    visits = saved_visits;
    --futility_depth;
    --chain_eval;
    --chain_depth;
    return chain_memo[key] = futile;
  }

  static constexpr int kShortenRounds = 16;
  static constexpr std::uint64_t kShortenBudget = 1500000;  
  static constexpr std::uint64_t kShortenChild = 10000;     
  static constexpr std::uint64_t kShortenThreat = 2000;     
  std::uint64_t shorten_nodes = 0;
  std::unordered_set<Key> shorten_tried;  

  std::vector<Move> Pv(SearchImpl& h) {
    std::vector<Move> pv, first;
    int total = -1;
    bool first_exact = false;
    bool last_exact = false;
    for (int round = 0;; ++round) {
      std::vector<Move> next = PvOnce(h, total);
      if (exact && next.size() % 2 == 0) exact = false;
      if (round > 0 && !exact && last_exact) {
        exact = true;
        break;
      }
      pv = std::move(next);
      last_exact = exact;
      if (round == 0) { first = pv; first_exact = exact; }
      bool more = exact && total > 1 && round < kShortenRounds && shorten_nodes < kShortenBudget &&
                  !limit_reached && !h.LimitReached();
      if (chain_restart && exact && round < kShortenRounds) {
        chain_restart = false;  
        more = true;
      } else if (more) {
        more = TryShorterAlong(h, pv, total);
      }
      if (!more) break;
      if (opt.futile) bounds.clear();
      else for (auto& [key, b] : bounds) b.lo = -1;
      proven_cache.clear();
      chain_memo.clear();
      real_memo.clear();
      visits = 0;
    }
    if (pv != first && !VerifyLine(h, pv)) {
      exact = first_exact;
      return first;
    }
    return pv;
  }

  bool VerifyLine(SearchImpl& h, const std::vector<Move>& pv) {
    if (pv.empty()) return true;
    Position& p = h.pos;
    std::vector<StateInfo> st(pv.size() + 1);
    Verifier v(h);
    v.own_log2 = 24;
    v.shared = verified;  
    for (std::size_t end = pv.size() % 2 ? pv.size() : pv.size() - 1;; end -= 2) {
      for (std::size_t i = 0; i < end; ++i) p.do_move(pv[i], st[i]);
      v.on_path.clear();
      v.raw_path.clear();
      v.error.clear();
      h.PushPath(p.key());
      const bool ok = v.Verify(0, kModeHisshi);
      h.PopPath();
      for (std::size_t i = end; i > 0; --i) p.undo_move(pv[i - 1]);
      if (!ok) return false;
      if (end < 2) break;
    }
    return true;
  }

  bool TryShorterAlong(SearchImpl& h, const std::vector<Move>& pv, int total) {
    Position& p = h.pos;
    std::vector<StateInfo> st(pv.size() + 1);
    std::vector<std::size_t> nodes;
    for (std::size_t ply = 0; ply < pv.size(); ply += 2)
      if (total - static_cast<int>(ply) >= 3) nodes.push_back(ply);
    for (auto it = nodes.rbegin(); it != nodes.rend() && shorten_nodes < kShortenBudget; ++it) {
      const std::size_t ply = *it;
      for (std::size_t i = 0; i < ply; ++i) p.do_move(pv[i], st[i]);
      const bool found = TryShorterHere(h);
      for (std::size_t i = ply; i > 0; --i) p.undo_move(pv[i - 1]);
      if (found) return true;  
    }
    return false;
  }

  bool TryShorterHere(SearchImpl& h) {
    Position& p = h.pos;
    if (pool) {
      std::vector<AnswerPool::Task> tasks;
      const std::string sfen = p.sfen();
      for (const auto& em : MoveList<LEGAL_ALL>(p)) {
        const Move m = em.move;
        if (SearchImpl::UselessNonPromotion(p, m)) continue;
        Child c{};
        c.move = m;
        if (h.ProbeChild(c, kModeHisshi).pn == 0) continue;  
        if (!shorten_tried.insert(Mix(p.key(), m)).second) continue;
        tasks.push_back(AnswerPool::Task{sfen, m, kShortenChild, kShortenThreat});
      }
      pool->Run(tasks);
      bool any = false;
      for (const auto& t : tasks) {
        shorten_nodes += t.nodes;
        if (t.result) any = true;
      }
      return any;
    }
    bool found = false;
    StateInfo s1, s2;
    for (const auto& em : MoveList<LEGAL_ALL>(p)) {
      if (shorten_nodes >= kShortenBudget) break;
      const Move m = em.move;
      if (SearchImpl::UselessNonPromotion(p, m)) continue;
      Child c{};
      c.move = m;
      if (h.ProbeChild(c, kModeHisshi).pn == 0) continue;  
      if (!shorten_tried.insert(Mix(p.key(), m)).second) continue;
      const bool check = p.gives_check(m);
      p.do_move(m, s1);
      const std::uint64_t n0 = h.nodes;
      bool threat = check;
      if (!check) {
        p.do_null_move(s2);
        threat = h.SubSearch(kModeMate, kShortenThreat).pn == 0;
        p.undo_null_move();
      }
      if (threat && h.SubSearch(kModeHisshi, kShortenChild).pn == 0) {
        found = true;
      }
      shorten_nodes += h.nodes - n0;
      p.undo_move(m);
    }
    return found;
  }

  std::vector<Move> PvOnce(SearchImpl& h, int& total) {
    Position& p = h.pos;
    total = -1;
    exact = false;
    for (int k = 1; k <= kMaxAnswer && visits < kBudget; k += 2)
      if (OrWithin(h, SQ_NB, k)) { total = k; break; }
    if (total < 0) return GreedyPv(h);
    exact = true;

    std::vector<StateInfo> st(kMaxAnswer + 2);
    std::vector<Move> pv;
    Square slider = SQ_NB;
    int k = total;  
    int ply = 0;
    while (k > 0) {
      if (p.side_to_move() == atk) {
        Move best = MOVE_NONE;
        for (const auto& lm : ProvenMoves(h)) {
          const Move m = lm.second;
          const Square next = IsSlider(type_of(p.moved_piece_after(m))) ? to_sq(m) : slider;
          p.do_move(m, st[ply]);
          const bool ok = AndWithin(h, next, k - 1);
          p.undo_move(m);
          if (ok) { best = m; slider = next; break; }
        }
        if (best == MOVE_NONE) break;  
        pv.push_back(best);
        p.do_move(best, st[ply++]);
        --k;
        continue;
      }
      if (AndWithin(h, slider, 0)) break;  
      Move chosen = MOVE_NONE;
      for (const Move m : Replies(h)) {
        if (KnownMated(h, m, st[ply])) continue;
        p.do_move(m, st[ply]);
        const bool shorter = OrWithin(h, slider, k - 3);
        p.undo_move(m);
        if (shorter) continue;
        if (ReplyMated(h, m, st[ply])) continue;
        if (IsInterposition(p, m, slider) && FutileAtOnce(h, m, slider, 1)) continue;
        if (ChainDrop(p, m, slider) && ChainFutile(h, m, slider)) continue;
        if (IsInterposition(p, m, slider) && Futile(h, m, slider)) continue;
        chosen = m;
        break;
      }
      if (chosen == MOVE_NONE) {
        int best_len = -1;
        for (const Move m : Replies(h)) {
          if (KnownMated(h, m, st[ply]) || ReplyMated(h, m, st[ply])) continue;
          if (IsInterposition(p, m, slider) && FutileAtOnce(h, m, slider, 1)) continue;
          if (IsInterposition(p, m, slider) && Futile(h, m, slider)) continue;
          p.do_move(m, st[ply]);
          int len = -1;
          for (int j = 1; j <= k - 3; j += 2)
            if (OrWithin(h, slider, j)) { len = j; break; }
          p.undo_move(m);
          if (len > best_len) { best_len = len; chosen = m; }
        }
        if (chosen == MOVE_NONE || best_len < 0) break;
        k = best_len + 1;
      }
      pv.push_back(chosen);
      p.do_move(chosen, st[ply++]);
      --k;
    }
    while (ply > 0) p.undo_move(pv[--ply]);
    return pv;
  }

  static constexpr int kMaxMateTail = 255;               
  static constexpr std::uint64_t kMateTailProbe = 1000000;  

  int MateLen(SearchImpl& h) {
    Position& p = h.pos;
    if (p.side_to_move() != atk && MoveList<LEGAL_ALL>(p).size() == 0) return p.in_check() ? 0 : -1;
    if (p.side_to_move() == atk && !p.in_check() && Mate::mate_1ply(p) != MOVE_NONE) return 1;
    Probe pr = h.Lookup(BoardKey(p), p.hand_of(atk), kModeMate);
    if (pr.pn != 0 && pr.dn != 0) {
      h.SubSearch(kModeMate, kMateTailProbe);
      pr = h.Lookup(BoardKey(p), p.hand_of(atk), kModeMate);
    }
    return pr.pn == 0 ? pr.len : -1;
  }

  static Move MoveFromTo(const Position& p, Square from, Square to) {
    Move found = MOVE_NONE;
    for (const auto& em : MoveList<LEGAL_ALL>(p))
      if (!is_drop(em.move) && from_sq(em.move) == from && to_sq(em.move) == to) {
        if (found == MOVE_NONE || is_promote(found)) found = em.move;
      }
    return found;
  }

  int AppendMateLine(SearchImpl& h, std::vector<Move>& pv) {
    Position& p = h.pos;
    std::vector<StateInfo> st(pv.size() + kMaxMateTail + 2);
    std::size_t ply = 0;
    for (; ply < pv.size(); ++ply) p.do_move(pv[ply], st[ply]);
    const std::size_t base = pv.size();
    bool ok = !pv.empty() && p.side_to_move() != atk && p.in_check();
    Square slider = SQ_NB;  
    if (ok && IsSlider(type_of(p.piece_on(to_sq(pv.back()))))) slider = to_sq(pv.back());
    bool mated = false;
    while (ok && pv.size() - base < static_cast<std::size_t>(kMaxMateTail)) {
      Move chosen = MOVE_NONE;
      if (p.side_to_move() != atk) {
        if (MoveList<LEGAL_ALL>(p).size() == 0) {
          mated = true;
          break;
        }
        int best = -1, best_other = -1;
        std::vector<std::pair<int, Move>> inters;
        for (const auto& em : MoveList<LEGAL_ALL>(p)) {
          p.do_move(em.move, st[ply]);
          const int len = MateLen(h);
          p.undo_move(em.move);
          if (len < 0) { ok = false; break; }
          if (IsInterposition(p, em.move, slider)) {
            inters.emplace_back(len, em.move);
            continue;
          }
          if (len > best_other) best_other = len;
          if (len > best) { best = len; chosen = em.move; }
        }
        if (!ok) break;
        for (const auto& [len, m] : inters) {
          if (best_other >= 0) {
            StateInfo s1, s2;
            p.do_move(m, s1);
            const Move cap = MoveFromTo(p, slider, to_sq(m));
            int after = -1;
            if (cap != MOVE_NONE) {
              p.do_move(cap, s2);
              after = MateLen(h);
              p.undo_move(cap);
            }
            p.undo_move(m);
            if (after >= 0 && after + 1 <= best_other) continue;  
          }
          if (len > best) { best = len; chosen = m; }
        }
      } else {
        int best = 0x7fffffff;
        for (const auto& em : MoveList<LEGAL_ALL>(p)) {
          if (!p.gives_check(em.move)) continue;
          p.do_move(em.move, st[ply]);
          const int len = MateLen(h);
          p.undo_move(em.move);
          if (len >= 0 && len < best) { best = len; chosen = em.move; }
        }
        if (chosen != MOVE_NONE && IsSlider(type_of(p.moved_piece_after(chosen)))) slider = to_sq(chosen);
        else if (chosen != MOVE_NONE) slider = SQ_NB;
      }
      if (chosen == MOVE_NONE) { ok = false; break; }
      pv.push_back(chosen);
      p.do_move(chosen, st[ply++]);
    }
    while (ply > 0) p.undo_move(pv[--ply]);
    if (!ok || !mated) {
      pv.resize(base);  
      return 0;
    }
    return static_cast<int>(pv.size() - base);
  }

  std::vector<Move> GreedyPv(SearchImpl& h) {
    struct IgnoreLimits {
      SearchImpl& s;
      explicit IgnoreLimits(SearchImpl& x) : s(x) { s.ignore_limits = true; }
      ~IgnoreLimits() { s.ignore_limits = false; }
    } ignore_limits(h);
    Position& p = h.pos;
    std::vector<StateInfo> st(1024);
    std::vector<Move> pv;
    std::unordered_set<Key> seen;
    Square slider = SQ_NB;  
    int ply = 0;
    while (ply < 1000) {
      if (seen.count(p.key())) break;
      seen.insert(p.key());
      if (p.side_to_move() == atk) {
        Move best = MOVE_NONE;
        if (!p.in_check()) best = Mate::mate_1ply(p);
        if (best == MOVE_NONE) {
          int bl = 0x7fffffff;
          for (const auto& em : MoveList<LEGAL_ALL>(p)) {
            Child c{};
            c.move = em.move;
            const Probe pr = h.ProbeChild(c, kModeHisshi);
            if (pr.pn == 0 && pr.len < bl) { bl = pr.len; best = em.move; }
          }
        }
        if (best == MOVE_NONE) break;
        if (IsSlider(type_of(p.moved_piece_after(best)))) slider = to_sq(best);
        pv.push_back(best);
        p.do_move(best, st[ply++]);
        continue;
      }
      Move chosen = MOVE_NONE;
      for (const Move m : Replies(h)) {
        if (ReplyMated(h, m, st[ply])) continue;
        if (ChainDrop(p, m, slider) && ChainFutile(h, m, slider)) continue;
        if (IsInterposition(p, m, slider) && Futile(h, m, slider)) continue;
        chosen = m;
        break;
      }
      if (chosen == MOVE_NONE) break;
      pv.push_back(chosen);
      p.do_move(chosen, st[ply++]);
    }
    while (ply > 0) p.undo_move(pv[--ply]);
    return pv;
  }
};

}  
}  

#endif  
