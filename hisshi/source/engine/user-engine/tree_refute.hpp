
#ifndef HISSHI_TREE_REFUTE_HPP_
#define HISSHI_TREE_REFUTE_HPP_

#include <functional>
#include <unordered_map>

#include "search.hpp"

namespace hisshi {
namespace detail {

class TreeRefuter {
 public:
  struct Result {
    bool disproven = false;  
    bool stopped = false;    
    bool full = false;       
    int rep_index = 0;
    std::uint64_t nodes = 0;
  };

  TreeRefuter(Position& p, Color attacker, std::uint8_t root_mode, const std::vector<Key>& above,
              SearchImpl* hint, std::size_t max_nodes, std::function<bool()> limit)
      : pos(p), atk(attacker), root_mode_(root_mode), above_(above), hint_(hint), max_nodes_(max_nodes),
        limit_(std::move(limit)) {}

  Result Run() {
    nodes_.clear();
    nodes_.reserve(std::min<std::size_t>(max_nodes_, 1 << 20));
    nodes_.push_back(Node{});
    nodes_[0].mode = root_mode_;
    states_.resize(kMaxPly + 2);
    path_ = above_;
    memo_.clear();
    Result res;
    while (!stopped_) {
      Search(0, 0, kInf, kInf);
      const Node& root = nodes_[0];
      if (root.pn == 0 || root.dn == 0) break;
      if (root.pn >= kInf || root.dn >= kInf) break;  
    }
    res.disproven = nodes_[0].dn == 0;
    res.rep_index = nodes_[0].rep;
    res.stopped = stopped_;
    res.full = full_;
    res.nodes = nodes_.size();
    return res;
  }

 private:
  static constexpr int kMaxPly = 400;
  static constexpr Key kMateSalt = 0x5D588B656C078965ULL;  
  static constexpr std::int16_t kNoRep = 0x7fff;          

  struct Node {
    Move move = MOVE_NONE;
    PnDn pn = 1, dn = 1;
    std::uint32_t child = 0;   
    std::uint16_t nchild = 0;
    std::int16_t rep = kNoRep;  
    std::uint8_t mode = kModeHisshi;
    std::uint8_t expanded = 0;
    std::uint8_t pass = 0;
  };

  Position& pos;
  Color atk;
  std::uint8_t root_mode_;
  std::vector<Key> above_;
  SearchImpl* hint_;
  std::size_t max_nodes_;
  std::function<bool()> limit_;
  std::vector<Node> nodes_;
  std::vector<StateInfo> states_;
  std::vector<Key> path_;
  std::unordered_map<Key, std::uint8_t> memo_;  
  bool stopped_ = false;
  bool full_ = false;  
  std::uint64_t checks_ = 0;

  static Key PathKey(Key k, std::uint8_t mode) { return mode == kModeMate ? k ^ kMateSalt : k; }

  static void SetWin(Node& n) { n.pn = 0; n.dn = kInf; n.expanded = 1; n.rep = kNoRep; }
  static void SetLoss(Node& n, std::int16_t rep) { n.pn = kInf; n.dn = 0; n.expanded = 1; n.rep = rep; }

  void Expand(std::uint32_t id, int ply) {
    const std::uint8_t mode = nodes_[id].mode;
    const bool or_node = pos.side_to_move() == atk;
    if (ply >= kMaxPly || full_) { SetWin(nodes_[id]); return; }  
    if (or_node && !pos.in_check() && Mate::mate_1ply(pos) != MOVE_NONE) { SetWin(nodes_[id]); return; }
    std::vector<Node> kids;
    if (or_node) {
      for (const auto& em : MoveList<LEGAL_ALL>(pos)) {
        if (mode == kModeMate && !pos.gives_check(em.move)) continue;
        Node c;
        c.move = em.move;
        c.mode = mode;
        kids.push_back(c);
      }
      if (kids.empty()) { SetLoss(nodes_[id], kNoRep); return; }
    } else {
      const bool check = pos.in_check();
      if (mode == kModeMate && !check) { SetLoss(nodes_[id], kNoRep); return; }  
      if (!check) {
        Node c;
        c.move = MOVE_NULL;
        c.mode = kModeMate;
        c.pass = 1;
        kids.push_back(c);
      }
      for (const auto& em : MoveList<LEGAL_ALL>(pos)) {
        Node c;
        c.move = em.move;
        c.mode = mode;
        kids.push_back(c);
      }
      if (check && kids.empty()) { SetWin(nodes_[id]); return; }  
    }
    {
      StateInfo st;
      for (Node& c : kids) {
        if (c.pass) pos.do_null_move(st);
        else pos.do_move(c.move, st);
        const auto it = memo_.find(PathKey(pos.key(), c.mode));
        if (it != memo_.end()) {
          if (it->second == 1) SetWin(c);
          else SetLoss(c, kNoRep);
        } else if (hint_) {
          const Probe p = hint_->Lookup(BoardKey(pos), pos.hand_of(atk), c.mode);
          if (p.pn == 0) {
            SetWin(c);
          } else if (p.dn == 0) {
            c.pn = 100;
            c.dn = 1;
          } else {
            c.pn = 2;
            c.dn = 2;
          }
        }
        if (c.pass) pos.undo_null_move();
        else pos.undo_move(c.move);
      }
    }
    if (nodes_.size() + kids.size() > max_nodes_) {
      full_ = true;
      SetWin(nodes_[id]);
      return;
    }
    Node& n = nodes_[id];
    n.child = static_cast<std::uint32_t>(nodes_.size());
    n.nchild = static_cast<std::uint16_t>(kids.size());
    n.expanded = 1;
    nodes_.insert(nodes_.end(), kids.begin(), kids.end());
  }

  void Aggregate(std::uint32_t id, bool or_node, PnDn& pn, PnDn& dn, int& best, PnDn& second, std::int16_t& rep) {
    const Node& n = nodes_[id];
    best = -1;
    second = kInf;
    rep = kNoRep;
    if (or_node) {
      pn = kInf;
      dn = 0;
      for (int i = 0; i < n.nchild; ++i) {
        const Node& c = nodes_[n.child + i];
        dn = Add(dn, c.dn);
        if (c.dn == 0) rep = std::min(rep, c.rep);  
        if (best < 0 || c.pn < pn) {
          if (best >= 0) second = std::min(second, pn);
          pn = c.pn;
          best = i;
        } else {
          second = std::min(second, c.pn);
        }
      }
    } else {
      pn = 0;
      dn = kInf;
      std::int16_t best_rep = -1;
      for (int i = 0; i < n.nchild; ++i) {
        const Node& c = nodes_[n.child + i];
        pn = Add(pn, c.pn);
        if (c.dn == 0) best_rep = std::max(best_rep, c.rep);  
        if (best < 0 || c.dn < dn) {
          if (best >= 0) second = std::min(second, dn);
          dn = c.dn;
          best = i;
        } else {
          second = std::min(second, c.dn);
        }
      }
      if (best_rep >= 0) rep = best_rep;
    }
  }

  void Search(std::uint32_t id, int ply, PnDn thpn, PnDn thdn) {
    if ((++checks_ & 4095) == 0 && limit_ && limit_()) stopped_ = true;
    if (stopped_) return;
    const std::int16_t index = static_cast<std::int16_t>(path_.size());  
    const Key key = PathKey(pos.key(), nodes_[id].mode);
    if (!nodes_[id].expanded) {
      for (std::size_t q = 0; q < path_.size(); ++q)
        if (path_[q] == key) { SetLoss(nodes_[id], static_cast<std::int16_t>(q)); return; }
      const auto it = memo_.find(key);
      if (it != memo_.end()) {
        if (it->second == 1) SetWin(nodes_[id]);
        else SetLoss(nodes_[id], kNoRep);
        return;
      }
      Expand(id, ply);
      if (nodes_[id].pn == 0 || nodes_[id].dn == 0) {
        if (nodes_[id].pn == 0 && !full_) memo_[key] = 1;
        else if (nodes_[id].dn == 0) memo_[key] = 2;
        return;
      }
    }
    const bool or_node = pos.side_to_move() == atk;
    path_.push_back(key);
    for (;;) {
      PnDn pn, dn, second;
      int best;
      std::int16_t rep;
      Aggregate(id, or_node, pn, dn, best, second, rep);
      nodes_[id].pn = pn;
      nodes_[id].dn = dn;
      if (dn == 0) {
        nodes_[id].rep = rep;
        if (rep >= index) memo_[key] = 2;
        break;
      }
      if (pn == 0) {
        memo_[key] = 1;
        break;
      }
      if (pn >= thpn || dn >= thdn || stopped_ || best < 0) break;
      const std::uint32_t cid = nodes_[id].child + static_cast<std::uint32_t>(best);
      const Node c = nodes_[cid];
      PnDn cthpn, cthdn;
      if (or_node) {
        cthpn = std::min(thpn, NextThreshold(second));
        cthdn = (thdn >= kInf || dn >= kInf) ? thdn : thdn - (dn - c.dn);
      } else {
        cthdn = std::min(thdn, NextThreshold(second));
        cthpn = (thpn >= kInf || pn >= kInf) ? thpn : thpn - (pn - c.pn);
      }
      StateInfo& st = states_[ply];
      if (c.pass) pos.do_null_move(st);
      else pos.do_move(c.move, st);
      Search(cid, ply + 1, cthpn, cthdn);
      if (c.pass) pos.undo_null_move();
      else pos.undo_move(c.move);
    }
    path_.pop_back();
  }
};

}  
}  

#endif  
