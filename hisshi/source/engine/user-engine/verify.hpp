
#ifndef HISSHI_VERIFY_HPP_
#define HISSHI_VERIFY_HPP_

#include "search.hpp"
#include "tree_refute.hpp"

namespace hisshi {
namespace detail {

class KeySet {
 public:
  bool Contains(Key k) const {
    if (t_.empty()) return false;
    k = k ? k : 1;
    for (std::size_t i = Index(k);; i = (i + 1) & mask_) {
      if (t_[i] == k) return true;
      if (t_[i] == 0) return false;
    }
  }
  void Insert(Key k) {
    if ((count_ + 1) * 2 > t_.size()) Grow();
    k = k ? k : 1;
    for (std::size_t i = Index(k);; i = (i + 1) & mask_) {
      if (t_[i] == k) return;
      if (t_[i] == 0) { t_[i] = k; ++count_; return; }
    }
  }

 private:
  std::size_t Index(Key k) const { return static_cast<std::size_t>((k * 0x9E3779B97F4A7C15ULL) >> 20) & mask_; }
  void Grow() {
    std::vector<Key> old;
    old.swap(t_);
    t_.assign(old.empty() ? (1u << 16) : old.size() * 2, 0);
    mask_ = t_.size() - 1;
    count_ = 0;
    for (Key k : old)
      if (k) Insert(k);
  }
  std::vector<Key> t_;
  std::size_t mask_ = 0, count_ = 0;
};

class AtomicHandSet {
 public:
  explicit AtomicHandSet(int log2) : size_(std::size_t(1) << log2), t_(new std::atomic<std::uint64_t>[size_]) {
    for (std::size_t i = 0; i < size_; ++i) t_[i].store(0, std::memory_order_relaxed);
  }
  bool Contains(Key board, Hand hand) const {
    const std::uint64_t kb = KeyBits(board);
    for (std::size_t n = 0, i = Index(board); n < kProbe; ++n, i = (i + 1) & (size_ - 1)) {
      const std::uint64_t cur = t_[i].load(std::memory_order_acquire);
      if (cur == 0) return false;
      if ((cur >> kHandBits) == kb && hand_is_equal_or_superior(hand, Unpack(cur))) return true;
    }
    return false;
  }
  void Insert(Key board, Hand hand) {
    const std::uint64_t v = (KeyBits(board) << kHandBits) | Pack(hand);
    for (std::size_t n = 0, i = Index(board); n < kProbe; ++n, i = (i + 1) & (size_ - 1)) {
      std::uint64_t cur = t_[i].load(std::memory_order_acquire);
      if (cur == v) return;
      if (cur == 0) {
        if (t_[i].compare_exchange_strong(cur, v, std::memory_order_acq_rel)) return;
        if (cur == v) return;
      }
    }
    t_[(Index(board) + static_cast<std::size_t>(v & (kProbe - 1))) & (size_ - 1)].store(v, std::memory_order_release);
  }

 private:
  static constexpr std::size_t kProbe = 16;
  static constexpr int kHandBits = 21;
  static std::uint64_t KeyBits(Key k) {
    const std::uint64_t b = static_cast<std::uint64_t>(k) >> kHandBits;
    return b ? b : 1;
  }
  static std::uint64_t Pack(Hand h) {
    const std::uint32_t x = static_cast<std::uint32_t>(h);
    return (x & 31) | ((x >> 8 & 7) << 5) | ((x >> 12 & 7) << 8) | ((x >> 16 & 7) << 11) |
           ((x >> 20 & 3) << 14) | ((x >> 24 & 3) << 16) | ((x >> 28 & 7) << 18);
  }
  static Hand Unpack(std::uint64_t v) {
    const std::uint32_t x = static_cast<std::uint32_t>(v);
    return static_cast<Hand>((x & 31) | ((x >> 5 & 7) << 8) | ((x >> 8 & 7) << 12) | ((x >> 11 & 7) << 16) |
                             ((x >> 14 & 3) << 20) | ((x >> 16 & 3) << 24) | ((x >> 18 & 7) << 28));
  }
  std::size_t Index(Key k) const { return static_cast<std::size_t>((k * 0x9E3779B97F4A7C15ULL) >> 20) & (size_ - 1); }
  std::size_t size_;
  std::unique_ptr<std::atomic<std::uint64_t>[]> t_;
};

struct VerifyTask {
  std::vector<Move> moves;
  std::uint8_t mode;
  std::vector<Key> ancestors;      
  std::vector<Key> ancestors_raw;  
};

struct Verifier {
  SearchImpl& s;
  Position& pos;
  std::unique_ptr<AtomicHandSet> own;  
  int own_log2 = 22;                   
  AtomicHandSet* shared = nullptr;     
  std::vector<Key> on_path;    
  std::vector<Key> raw_path;   
  bool cycle_seen = false;     
  std::uint64_t visited = 0;
  std::string error;
  std::vector<StateInfo> st;
  int collect_depth = -1;
  std::vector<Move> path;
  std::vector<VerifyTask>* tasks = nullptr;

  static constexpr std::uint64_t kMemoMinVisits = 3;  

  explicit Verifier(SearchImpl& si) : s(si), pos(si.pos) { st.resize(4096); }

  AtomicHandSet& Done() {
    if (shared) return *shared;
    if (!own) own = std::make_unique<AtomicHandSet>(own_log2);
    return *own;
  }
  bool IsDone(Key board, Hand hand) { return Done().Contains(board, hand); }
  void MarkDone(Key board, Hand hand) { Done().Insert(board, hand); }

  static Key Salt(Key k, std::uint8_t mode) { return mode ? k ^ 0x9e3779b97f4a7c15ULL : k; }

  bool ProveHere(std::uint8_t mode) {
    const Probe p = s.Lookup(BoardKey(pos), pos.hand_of(s.atk), mode);
    if (p.pn == 0) return true;
    return s.SubSearch(mode, 5000000).pn == 0;
  }

  bool Verify(int depth, std::uint8_t mode) {
    if (depth >= 4000) { error = "too deep"; return false; }
    const Key k = Salt(pos.key(), mode);
    const Key board = Salt(BoardKey(pos), mode);
    const Hand hand = pos.hand_of(s.atk);
    if (IsDone(board, hand)) return true;
    if (std::find(on_path.begin(), on_path.end(), k) != on_path.end()) {
      error = "cycle in proof";
      cycle_seen = true;
      return false;
    }
    if (depth == collect_depth) {
      tasks->push_back(VerifyTask{path, mode, on_path, raw_path});
      MarkDone(board, hand);  
      return true;
    }
    const std::uint64_t visited_before = visited++;
    on_path.push_back(k);
    raw_path.push_back(s.PathEntry(pos.key(), mode));
    bool ok = true;
    if (pos.side_to_move() == s.atk) {
      ok = VerifyOr(depth, mode);
    } else {
      ok = VerifyAnd(depth, mode);
    }
    on_path.pop_back();
    raw_path.pop_back();
    if (ok && visited - visited_before >= kMemoMinVisits) MarkDone(board, hand);
    return ok;
  }

  bool VerifyOr(int depth, std::uint8_t mode) {
    if (!pos.in_check()) {
      const Move mate = Mate::mate_1ply(pos);
      if (mate != MOVE_NONE && TryOrMove(depth, mode, mate)) return true;
    }
    if (s.Lookup(BoardKey(pos), pos.hand_of(s.atk), mode).pn != 0) {
      for (const auto& em : MoveList<LEGAL_ALL>(pos)) {
        const Move m = em.move;
        if (mode == kModeMate && !pos.gives_check(m)) continue;
        Child c{};
        c.move = m;
        Key cb, cf;
        Hand ch;
        std::uint8_t cm;
        s.ChildKey(c, mode, cb, ch, cm, cf);
        if (IsDone(Salt(cb, cm), ch)) return true;
      }
    }
    if (!ProveHere(mode)) { error = "OR node not proven"; return false; }
    {
      const Probe p = s.Lookup(BoardKey(pos), pos.hand_of(s.atk), mode);
      const Move best = p.pn == 0 ? pos.to_move(Move16(p.best)) : MOVE_NONE;
      if (best != MOVE_NONE && pos.pseudo_legal_s<true>(best) && pos.legal(best) &&
          (mode != kModeMate || pos.gives_check(best)) && TryOrMove(depth, mode, best))
        return true;
    }
    if (cycle_seen && ReproveAndRetry(depth, mode)) return true;
    std::vector<std::pair<int, Move>> cands;
    for (const auto& em : MoveList<LEGAL_ALL>(pos)) {
      const Move m = em.move;
      if (mode == kModeMate && !pos.gives_check(m)) continue;
      Child c{};
      c.move = m;
      const Probe p = s.ProbeChild(c, mode);
      if (p.pn == 0) cands.emplace_back(p.len, m);
    }
    std::sort(cands.begin(), cands.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    for (const auto& [len, m] : cands) {
      if (TryOrMove(depth, mode, m)) return true;
    }
    const Probe p = s.Lookup(BoardKey(pos), pos.hand_of(s.atk), mode);
    const Move best = pos.to_move(Move16(p.best));
    if (best != MOVE_NONE && pos.pseudo_legal_s<true>(best) && pos.legal(best) &&
        (mode != kModeMate || pos.gives_check(best)))
      if (TryOrMove(depth, mode, best)) return true;
    if (cycle_seen && ReproveAndRetry(depth, mode)) return true;
    if (error.empty()) error = "no verified attack";
    return false;
  }

  bool ReproveAndRetry(int depth, std::uint8_t mode) {
    if (!ReproveAvoidingPath(mode)) return false;
    const Probe q = s.Lookup(BoardKey(pos), pos.hand_of(s.atk), mode);
    const Move b = pos.to_move(Move16(q.best));
    return q.pn == 0 && b != MOVE_NONE && pos.pseudo_legal_s<true>(b) && pos.legal(b) &&
           (mode != kModeMate || pos.gives_check(b)) && TryOrMove(depth, mode, b);
  }

  bool ReproveAvoidingPath(std::uint8_t mode) {
    cycle_seen = false;
    s.EraseProofs(BoardKey(pos), pos.hand_of(s.atk), mode);
    for (std::size_t i = 0; i + 1 < raw_path.size(); ++i) s.PushPath(raw_path[i]);
    const NodeResult r = s.SubSearch(mode, 5000000);
    for (std::size_t i = 0; i + 1 < raw_path.size(); ++i) s.PopPath();
    return r.pn == 0;
  }

  bool TryOrMove(int depth, std::uint8_t mode, Move m) {
    pos.do_move(m, st[depth]);
    path.push_back(m);
    const bool ok = Verify(depth + 1, mode);
    path.pop_back();
    pos.undo_move(m);
    if (!ok) return false;
    error.clear();
    return true;
  }

  bool VerifyAnd(int depth, std::uint8_t mode) {
    const bool check = pos.in_check();
    if (mode == kModeMate && !check) { error = "mate-mode node without check"; return false; }
    if (!check) {
      pos.do_null_move(st[depth]);
      path.push_back(MOVE_NULL);
      const bool ok = Verify(depth + 1, kModeMate);
      path.pop_back();
      pos.undo_null_move();
      if (!ok) { if (error.empty()) error = "pass not mated"; return false; }
    }
    for (const auto& em : MoveList<LEGAL_ALL>(pos)) {
      const Move m = em.move;
      pos.do_move(m, st[depth]);
      path.push_back(m);
      const bool ok = Verify(depth + 1, mode);
      path.pop_back();
      pos.undo_move(m);
      if (!ok) return false;
    }
    return true;
  }
};

struct DisproofVerifier {
  static constexpr std::uint64_t kBudget = 2000000;  
  static constexpr int kTries = 4;                   
  static constexpr int kRounds = 3;                  
  static constexpr std::size_t kTreeNodes = std::size_t(1) << 22;     
  static constexpr std::uint64_t kTreeTotal = std::uint64_t(1) << 26;  
  static constexpr int kNoDep = 0x7fffffff;  

  SearchImpl& s;
  Position& pos;
  KeySet done;              
  std::vector<Key> on_path;
  std::vector<StateInfo> st;
  std::string error;
  std::uint64_t visited = 0, searches = 0;
  std::uint64_t trees = 0, trees_disproven = 0, tree_nodes = 0;
  std::uint64_t last_tree_nodes = 0;
  bool last_tree_full = false;

  explicit DisproofVerifier(SearchImpl& si) : s(si), pos(si.pos) { st.resize(4096); }

  static Key Salt(Key k, std::uint8_t mode) { return mode ? k ^ 0x9e3779b97f4a7c15ULL : k; }

  bool Refuted(int depth, std::uint8_t mode, int& dep) {
    const Key k = Salt(pos.key(), mode);
    if (done.Contains(k)) return true;
    const auto it = std::find(on_path.begin(), on_path.end(), k);
    if (it != on_path.end()) {
      dep = std::min(dep, static_cast<int>(it - on_path.begin()));
      return true;
    }
    if (depth >= 4000) { error = "too deep"; return false; }
    ++visited;
    const int index = static_cast<int>(on_path.size());
    on_path.push_back(k);
    int my_dep = kNoDep;
    const bool ok = pos.side_to_move() == s.atk ? OrRefuted(depth, mode, my_dep) : AndRefuted(depth, mode, my_dep);
    on_path.pop_back();
    if (ok) {
      if (my_dep >= index) done.Insert(k);
      else dep = std::min(dep, my_dep);
    }
    return ok;
  }

  bool OrRefuted(int depth, std::uint8_t mode, int& dep) {
    for (const auto& em : MoveList<LEGAL_ALL>(pos)) {
      const Move m = em.move;
      if (mode == kModeMate && !pos.gives_check(m)) continue;
      if (!Refutes(depth, mode, m, dep)) {
        if (error.empty()) error = "attack " + to_usi_string(m) + " not refuted";
        return false;
      }
    }
    return true;
  }

  bool Refutes(int depth, std::uint8_t mode, Move m, int& dep) {
    const bool pass = m == MOVE_NULL;
    const std::uint8_t cmode = pass ? kModeMate : mode;
    if (pass) pos.do_null_move(st[depth]);
    else pos.do_move(m, st[depth]);
    s.PushPath(s.PathEntry(pos.key(), cmode));
    const bool ok = Refuted(depth + 1, cmode, dep);
    s.PopPath();
    if (pass) pos.undo_null_move();
    else pos.undo_move(m);
    return ok;
  }

  void ReplyKeys(Move r, std::uint8_t mode, Key& board, Hand& hand, std::uint8_t& cmode, Key& full) {
    Child c{};
    c.move = r;
    c.pass = r == MOVE_NULL;
    s.ChildKey(c, mode, board, hand, cmode, full);
  }

  Probe ProbeReply(Move r, std::uint8_t mode) {
    Key board, full;
    Hand hand;
    std::uint8_t cmode;
    ReplyKeys(r, mode, board, hand, cmode, full);
    if (s.RepAt(s.ChildPathKey(full))) {
      Probe p;
      p.pn = kInf;
      p.dn = 0;
      p.found = true;
      p.rep = true;
      return p;
    }
    return s.Lookup(board, hand, cmode, s.ChildPathKey(full));
  }

  bool ReplyOnPath(Move r, std::uint8_t mode) {
    Key board, full;
    Hand hand;
    std::uint8_t cmode;
    ReplyKeys(r, mode, board, hand, cmode, full);
    return s.InPath(full);
  }

  bool AndRefuted(int depth, std::uint8_t mode, int& dep) {
    const bool check = pos.in_check();
    std::vector<Move> replies;
    if (!check) replies.push_back(MOVE_NULL);
    for (const auto& em : MoveList<LEGAL_ALL>(pos)) replies.push_back(em.move);
    if (check && replies.empty()) { error = "mated"; return false; }
    std::vector<Move> tried;
    std::uint64_t budget = kBudget;
    for (int round = 0; round < kRounds; ++round) {
      std::vector<std::pair<int, Move>> cands;
      for (Move r : replies) {
        if (std::find(tried.begin(), tried.end(), r) != tried.end()) continue;
        const Probe p = ProbeReply(r, mode);
        if (p.dn == 0) cands.emplace_back(p.rep ? 1 : 0, r);
        else if (ReplyOnPath(r, mode)) cands.emplace_back(1, r);
      }
      std::stable_sort(cands.begin(), cands.end(),
                       [](const auto& a, const auto& b) { return a.first < b.first; });
      for (const auto& [rep, r] : cands) {
        if (static_cast<int>(tried.size()) >= kTries * (round + 1)) break;
        tried.push_back(r);
        const std::string saved = error;
        int c = kNoDep;
        if (Refutes(depth, mode, r, c)) {
          dep = std::min(dep, c);
          error.clear();
          return true;
        }
        if (!saved.empty()) error = saved;  
      }
      if (round == kRounds - 1 || s.LimitReached()) break;
      ++searches;
      const Key self = s.path.back();
      s.PopPath();
      s.rederive_root = true;
      s.rederive_ply = static_cast<int>(s.path.size()) + 1;  
      s.force_full_width = round > 0;
      const NodeResult nr = s.SubSearch(mode, budget);
      s.force_full_width = false;
      s.rederive_root = false;
      s.PushPath(self);
      budget *= 8;
      if (nr.pn == 0) break;  
    }
    if (TreeRefutes(mode, dep)) {
      error.clear();
      return true;
    }
    if (error.empty())
      error = "no refuting reply at " + pos.sfen() + " (tree " + std::to_string(last_tree_nodes) +
              (last_tree_full ? " full)" : ")");
    return false;
  }

  bool TreeRefutes(std::uint8_t mode, int& dep) {
    if (tree_nodes >= kTreeTotal) return false;
    const std::vector<Key> above(s.path.begin(), s.path.end() - 1);
    TreeRefuter tree(pos, s.atk, mode, above, &s, kTreeNodes, [this]() { return s.LimitReached(); });
    const TreeRefuter::Result tr = tree.Run();
    ++trees;
    tree_nodes += tr.nodes;
    last_tree_nodes = tr.nodes;
    last_tree_full = tr.full;
    if (!tr.disproven) return false;
    ++trees_disproven;
    dep = std::min(dep, tr.rep_index);  
    return true;
  }
};

}  
}  

#endif  
