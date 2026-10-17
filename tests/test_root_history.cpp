// Root-history guard tests: a first move that completes 連続王手の千日手 must be detected.
// build: clang++ -std=c++20 -O2 -static tests/test_root_history.cpp -o test_root_history.exe -lpthread
#include <cstdio>
#include <cstring>
#include "../source/zyfamate_core.h"
using namespace Zyfamate;

static int fails = 0;
static void check(const char* name, int ok) { std::printf("%s %s\n", ok ? "PASS" : "FAIL", name); if (!ok) fails++; }

// rook 2i <-> 1i gives check every other move while the bare king walks 1a <-> 2a.
static int build(Pos* p, uint64_t* keys, unsigned char* chk, int ncycles) {
  parse_sfen(p, "8k/9/9/9/9/9/9/9/4K2R1 b - 1");
  p->key = compute_key(p); p->bkey = compute_bkey(p); ykey_refresh(p);
  const char* cyc[4] = {"2i1i", "1a2a", "1i2i", "2a1a"};
  int n = 0; keys[0] = p->key;
  for (int c = 0; c < ncycles; c++) for (int i = 0; i < 4; i++) {
    int m = parse_usi_move(p, cyc[i]); make_move(p, m);
    p->key = compute_key(p); p->bkey = compute_bkey(p); ykey_refresh(p);
    chk[n] = (unsigned char)in_check(p, p->side); keys[++n] = p->key;
  }
  return n;
}

int main() {
  init_tables();
  Pos p; uint64_t keys[64]; unsigned char chk[64];
  int n = build(&p, keys, chk, 3);
  zyf_set_root_history(keys, chk, n);
  int mv = parse_usi_move(&p, "2i1i");
  check("perpetual_first_move_detected", root_move_perpetual(&p, mv) == 1);
  int quiet = parse_usi_move(&p, "2i3i");
  check("non_check_move_not_perpetual", root_move_perpetual(&p, quiet) == 0);
  chk[4] = 0;   // pretend one rook move inside the cycle was quiet
  zyf_set_root_history(keys, chk, n);
  check("cycle_with_quiet_move_not_perpetual", root_move_perpetual(&p, mv) == 0);
  Pos p2; int n2 = build(&p2, keys, chk, 2);   // only 3rd occurrence -> not 千日手 yet
  zyf_set_root_history(keys, chk, n2);
  check("third_occurrence_not_sennichite", root_move_perpetual(&p2, parse_usi_move(&p2, "2i1i")) == 0);
  zyf_clear_root_history();
  check("no_history_no_guard", root_move_perpetual(&p, mv) == 0);
  std::printf("%s\n", fails ? "FAILED" : "ALL PASS");
  return fails ? 1 : 0;
}
