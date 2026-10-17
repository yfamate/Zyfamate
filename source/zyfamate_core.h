#pragma once
// Zyfamate ― 自己完結の df-pn 詰将棋エンジン（盤面・指し手生成・make/unmake・df-pn+・証明駒・USI）。
// 単独では source/main.cpp から usi_loop() を呼ぶ。Cyfamate はこのファイルを source/zyfamate/ に借りて、
// 常駐の詰み探索として通常探索と並走させる。マクロは末尾で全て #undef し、SQ は push/pop で隔離する。
// ZYFAMATE_HOST_YANEURAOU: Cyfamate（やねうら王系）に組み込む時だけ定義する。定義すると、証明木を探索側の
// 局面鍵（やねうら王の Zobrist）と指し手形式で渡す経路が有効になる。単独版では無効（鍵は 0、渡し先も無い）。
#include <atomic>
#include <mutex>
#include <vector>
#include <utility>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#ifndef CYF_ZYF_TEST_STAGE
#define CYF_ZYF_TEST_STAGE(n) ((void)0)
#endif
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <assert.h>
#include <setjmp.h>
#include <pthread.h>
#pragma push_macro("SQ")
#undef SQ
/* [ZYF-O3 2026-10-05] The solver alone is built at -O3 under GCC; the engine keeps the build's -O2. On the avx512_vnni
 * bench (tests/zyf_bench.cpp, 332 positions x 100k nodes, -march=native) the same source ran ~176k nps at -O2 and
 * ~223k nps at -O3 with identical results. -mprefer-vector-width=512 added only ~3%. Clang ignores this pragma. */
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize("O3")
#endif
namespace Zyfamate {
struct _CMul{ size_t s; template<class T> operator T*() const { return (T*)std::malloc(s); } };
struct _CCal{ size_t n,s; template<class T> operator T*() const { return (T*)std::calloc(n,s); } };
static inline _CMul _zmalloc(size_t s){ return {s}; }
static inline _CCal _zcalloc(size_t n,size_t s){ return {n,s}; }
#define malloc(s) Zyfamate::_zmalloc(s)
#define calloc(n,s) Zyfamate::_zcalloc(n,s)
/* core.c -- 高速詰将棋エンジン(C). 盤面/指し手生成/make-unmake/df-pn+/証明駒.
 * 純Python実装(shogi.py,dfpn.py)を移植・高速化したもの。perftとPython差分で検証。 */

/* 指し手バッファの上限。将棋の合法手最大は593(成/不成別計上込み)。王手/合法手は
 * その部分集合なので1024で安全。デバッグビルド(assert有効)では各生成器の出口で超過を検出。 */
#define MAXMOVES 1024

enum { EMPTY=0, PAWN=1,LANCE,KNIGHT,SILVER,GOLD,BISHOP,ROOK,KING,
       PPAWN,PLANCE,PKNIGHT,PSILVER,HORSE,DRAGON };
#define WOFF 16
#define BLACK 0
#define WHITE 1
static inline int pcolor(int pc){return pc>=WOFF?WHITE:BLACK;}
static inline int ptype(int pc){return pc>=WOFF?pc-WOFF:pc;}
static inline int mkpc(int pt,int col){return pt+(col==WHITE?WOFF:0);}

static int PROMO[15];   /* base type -> promoted type (0 if none) */
static int DEMO[15];    /* any type -> base droppable type */
static int IS_PROMOTABLE[15];

/* move encoding */
#define MV(from,to,promo,drop) ((from)|((to)<<7)|((promo)<<14)|((drop)<<15))
#define M_FROM(m) ((m)&0x7F)
#define M_TO(m)   (((m)>>7)&0x7F)
#define M_PROMO(m) (((m)>>14)&1)
#define M_DROP(m) (((m)>>15)&0xF)
#define DROP_FROM 0x7F
#define IS_DROP(m) (M_FROM(m)==DROP_FROM)

/* geometry */
static inline int SQ(int r,int c){return r*9+c;}
static inline int RNK(int s){return s/9;}
static inline int COL(int s){return s%9;}
static inline int inb(int r,int c){return r>=0&&r<9&&c>=0&&c<9;}

/* black-oriented deltas */
static const int PAWN_D[1][2]={{-1,0}};
static const int KNIGHT_D[2][2]={{-2,-1},{-2,1}};
static const int SILVER_D[5][2]={{-1,-1},{-1,0},{-1,1},{1,-1},{1,1}};
static const int GOLD_D[6][2]={{-1,-1},{-1,0},{-1,1},{0,-1},{0,1},{1,0}};
static const int KING_D[8][2]={{-1,-1},{-1,0},{-1,1},{0,-1},{0,1},{1,-1},{1,0},{1,1}};
static const int BISHOP_D[4][2]={{-1,-1},{-1,1},{1,-1},{1,1}};
static const int ROOK_D[4][2]={{-1,0},{1,0},{0,-1},{0,1}};
/* 事前計算: 利きテーブル(is_attacked高速化) */
typedef struct { int q; uint64_t mask; } StepAtk;
static StepAtk STEP_ATK[2][81][16]; static int STEP_ATK_N[2][81];
static int RAY[81][8][9]; static int RAYN[81][8]; static int LANCE_INDIR[2];
/* [ZYF-OCC 2026-10-05] 81-square occupancy bitboards per colour (bit s of occc[c] = b[s] holds a c piece), kept in Pos
 * by every board write. A step attacker can only stand on STEPBB[by][sq], so most step scans end with one AND; a
 * slider ray finds its first blocker with one AND and a bit scan instead of walking square by square. RAYBB[s][d] holds
 * RAY[s][d]; directions 1,3,6,7 run toward larger square numbers (first blocker = lowest bit), 0,2,4,5 toward smaller
 * ones (highest bit). Every caller still decides on b[blocker] exactly as the square walk did. */
typedef unsigned __int128 ZBB;
static ZBB SQBB[81]; static ZBB RAYBB[81][8]; static ZBB STEPBB[2][81]; static ZBB MVSTBB[2][15][81]; static ZBB MVREACH[2][15][81];
static const int RAY_UP[8]={0,1,0,1,0,0,1,1};
static inline int bb_lsb(ZBB m){ uint64_t lo=(uint64_t)m; return lo?__builtin_ctzll(lo):64+__builtin_ctzll((uint64_t)(m>>64)); }
static inline int bb_msb(ZBB m){ uint64_t hi=(uint64_t)(m>>64); return hi?127-__builtin_clzll(hi):63-__builtin_clzll((uint64_t)m); }
static inline int bb_first(ZBB m,int d){ return RAY_UP[d]?bb_lsb(m):bb_msb(m); }
/* slo[c]/sld[c]: squares of c's rook/dragon/lance and bishop/horse. A ray whose squares hold none of them cannot carry a
 * slider attack, so the scan is skipped. SLOM/SLDM[pc] are all-ones for those piece codes. */
static ZBB SLOM[32], SLDM[32]; static ZBB ORTHBB[81], DIAGBB[81];
/* STEP_REL[by][q-sq+19]: the STEP_ATK mask of square q for target sq. It depends only on the offset (|dc|<=1, |dr|<=2),
 * so a step scan visits just the by pieces in STEPBB (ascending q, the STEP_ATK order) instead of every listed square. */
static uint64_t STEP_REL[2][39];
static int MV_ST[2][15][81][8]; static int MV_STN[2][15][81];
static int MV_SDIR[2][15][4]; static int MV_SDN[2][15];
static void build_attack_tables(void);

/* zobrist */
static uint64_t Z_PIECE[31][81], Z_HAND[2][8][40], Z_SIDE;
static uint64_t sm_state;
static uint64_t sm_next(){ uint64_t z=(sm_state+=0x9E3779B97F4A7C15ULL);
  z=(z^(z>>30))*0xBF58476D1CE4E5B9ULL; z=(z^(z>>27))*0x94D049BB133111EBULL; return z^(z>>31);}

/* [ZYF-TREE 2026-10-05] ybk/yhk: the same position's YaneuraOu key (pos.key() = board_key ^ hand_key), kept
 * incrementally next to Zyfamate's own zobrist so a Zyfamate node can be named in the search's key domain. */
typedef struct { int b[81]; int hand[2][8]; int side; int ksq[2]; uint64_t key; uint64_t bkey; uint64_t ybk, yhk; ZBB occc[2]; ZBB slo[2], sld[2]; } Pos;

typedef struct { int move; int captured; uint64_t prevkey; uint64_t prev_bkey; uint64_t prev_ybk, prev_yhk; } Undo;

/* YaneuraOu Zobrist in Zyfamate's piece/square numbering. Filled once after Position::init() (lazily, from the first
 * ykey_refresh); board_key XORs psq and side (WHITE to move), hand_key adds hand[c][pt] per piece in hand. */
static uint64_t Y_PSQ[31][81], Y_HAND[2][8], Y_SIDE;
static std::atomic<int> g_y_ready{0};
static void yo_tables_init(){
  static std::mutex mx; std::lock_guard<std::mutex> lk(mx);
  if(g_y_ready.load(std::memory_order_relaxed)) return;
#if !defined(ZYFAMATE_HOST_YANEURAOU)
  g_y_ready.store(1,std::memory_order_release); return;   /* standalone: no search-side key domain (keys stay 0) */
#else
  static_assert(sizeof(YaneuraOu::Zobrist::side)==sizeof(uint64_t), "64-bit YaneuraOu keys");
  if(!YaneuraOu::Zobrist::side) return;            /* Position::init() has not run yet */
  /* Zyfamate type -> YaneuraOu PieceType: gold/bishop/rook are ordered differently. */
  static const int YT[15]={0,1,2,3,4,7,5,6,8,9,10,11,12,13,14};
  for(int pc=0;pc<31;pc++) for(int s=0;s<81;s++){ Y_PSQ[pc][s]=0;
    int t=pc>=16?pc-16:pc; if(t<1||t>14) continue;
    int ypc=YT[t]+(pc>=16?16:0), ysq=(8-s%9)*9+s/9;
    Y_PSQ[pc][s]=(uint64_t)YaneuraOu::Zobrist::psq[ypc][ysq]; }
  for(int c=0;c<2;c++) for(int t=0;t<8;t++) Y_HAND[c][t]=(t>=1&&t<=7)?(uint64_t)YaneuraOu::Zobrist::hand[c][YT[t]]:0;
  Y_SIDE=(uint64_t)YaneuraOu::Zobrist::side;
  g_y_ready.store(1,std::memory_order_release);
#endif
}
static void ykey_refresh(Pos*p){
  if(!g_y_ready.load(std::memory_order_acquire)) yo_tables_init();
  uint64_t b=0,h=0;
  for(int s=0;s<81;s++) if(p->b[s]) b^=Y_PSQ[p->b[s]][s];
  for(int c=0;c<2;c++) for(int t=1;t<=7;t++) h+=Y_HAND[c][t]*(uint64_t)p->hand[c][t];
  if(p->side==WHITE) b^=Y_SIDE;
  p->ybk=b; p->yhk=h; }
static inline uint64_t yo_key(const Pos*p){ return p->ybk^p->yhk; }

static void init_tables(){
  for(int i=0;i<15;i++){PROMO[i]=0;DEMO[i]=i;IS_PROMOTABLE[i]=0;}
  PROMO[PAWN]=PPAWN;PROMO[LANCE]=PLANCE;PROMO[KNIGHT]=PKNIGHT;PROMO[SILVER]=PSILVER;
  PROMO[BISHOP]=HORSE;PROMO[ROOK]=DRAGON;
  IS_PROMOTABLE[PAWN]=IS_PROMOTABLE[LANCE]=IS_PROMOTABLE[KNIGHT]=IS_PROMOTABLE[SILVER]=
  IS_PROMOTABLE[BISHOP]=IS_PROMOTABLE[ROOK]=1;
  DEMO[PPAWN]=PAWN;DEMO[PLANCE]=LANCE;DEMO[PKNIGHT]=KNIGHT;DEMO[PSILVER]=SILVER;
  DEMO[HORSE]=BISHOP;DEMO[DRAGON]=ROOK;
  sm_state=0x1234567890ABCDEFULL;
  for(int p=0;p<31;p++)for(int s=0;s<81;s++)Z_PIECE[p][s]=sm_next();
  for(int c=0;c<2;c++)for(int t=0;t<8;t++)for(int n=0;n<40;n++)Z_HAND[c][t][n]=sm_next();
  Z_SIDE=sm_next();
  build_attack_tables();
}

static uint64_t compute_key(Pos*p){
  uint64_t k=0;
  for(int s=0;s<81;s++) if(p->b[s]) k^=Z_PIECE[p->b[s]][s];
  for(int c=0;c<2;c++)for(int t=1;t<=7;t++)for(int n=1;n<=p->hand[c][t];n++) k^=Z_HAND[c][t][n];
  if(p->side==WHITE) k^=Z_SIDE;
  return k;
}

static uint64_t compute_bkey(Pos*p){ uint64_t k=0; for(int s=0;s<81;s++) if(p->b[s]) k^=Z_PIECE[p->b[s]][s];
  if(p->side==WHITE)k^=Z_SIDE; return k; }
static void refresh_kings(Pos*p){   /* also rebuilds occ: every board loader ends here */
  p->occc[0]=p->occc[1]=p->slo[0]=p->slo[1]=p->sld[0]=p->sld[1]=0;
  for(int s=0;s<81;s++){ int pc=p->b[s]; if(!pc)continue; p->occc[pcolor(pc)]|=SQBB[s];
    p->slo[pcolor(pc)]|=SQBB[s]&SLOM[pc]; p->sld[pcolor(pc)]|=SQBB[s]&SLDM[pc]; }
  p->ksq[0]=p->ksq[1]=-1;
  for(int s=0;s<81;s++){int pc=p->b[s]; if(pc&&ptype(pc)==KING)p->ksq[pcolor(pc)]=s;}
}

/* ---- SFEN parse ---- */
static int char_type(char c){
  switch(c){case 'P':return PAWN;case 'L':return LANCE;case 'N':return KNIGHT;
   case 'S':return SILVER;case 'G':return GOLD;case 'B':return BISHOP;case 'R':return ROOK;
   case 'K':return KING;} return 0;
}
static int parse_sfen(Pos*p,const char*sfen){
  memset(p,0,sizeof(*p));
  const char*s=sfen; int r=0,c=0,promo=0;
  while(*s==' ')s++;
  /* [FIX] "sfen <board> ..." プレフィックス付き入力を受理。Pyfamate の
   * ShogiBoard.sfen() は必ず "sfen " で始まる文字列を返すため、旧実装は
   * 全呼び出しが parse error になり統合が無音で死んでいた。 */
  if(!strncmp(s,"sfen ",5)){ s+=5; while(*s==' ')s++; }
  while(*s&&*s!=' '){
    char ch=*s++;
    if(ch=='/'){r++;c=0;continue;}
    if(ch=='+'){promo=1;continue;}
    if(ch>='1'&&ch<='9'){c+=ch-'0';continue;}
    int col=(ch>='A'&&ch<='Z')?BLACK:WHITE;
    char up=(ch>='a'&&ch<='z')?ch-32:ch;
    int t=char_type(up); if(!t)return 0;
    if(promo){t=PROMO[t];promo=0;}
    if(r<0||r>8||c<0||c>8)return 0;
    p->b[SQ(r,c)]=mkpc(t,col); c++;
  }
  while(*s==' ')s++;
  p->side = (*s=='w')?WHITE:BLACK; if(*s)s++;
  while(*s==' ')s++;
  if(*s=='-'){s++;}
  else{
    int num=0;
    while(*s&&*s!=' '){
      char ch=*s++;
      if(ch>='0'&&ch<='9'){num=num*10+(ch-'0');continue;}
      int col=(ch>='A'&&ch<='Z')?BLACK:WHITE;
      char up=(ch>='a'&&ch<='z')?ch-32:ch;
      int t=char_type(up); if(!t||t==KING)return 0;
      p->hand[col][t]+=num?num:1; num=0;
    }
  }
  refresh_kings(p);
  p->key=compute_key(p); p->bkey=compute_bkey(p); ykey_refresh(p);
  return 1;
}

/* ---- attack detection ---- */
static void build_attack_tables(void){
  static const int RD[8][2]={{-1,0},{1,0},{0,-1},{0,1},{-1,-1},{-1,1},{1,-1},{1,1}};
  LANCE_INDIR[BLACK]=1; LANCE_INDIR[WHITE]=0;
  for(int s=0;s<81;s++) SQBB[s]=(ZBB)1<<s;
  for(int s=0;s<81;s++){ int r=s/9,c=s%9;
    for(int d=0;d<8;d++){ int len=0,rr=r+RD[d][0],cc=c+RD[d][1];
      while(rr>=0&&rr<9&&cc>=0&&cc<9){ RAY[s][d][len++]=rr*9+cc; rr+=RD[d][0]; cc+=RD[d][1]; }
      RAYN[s][d]=len; RAYBB[s][d]=0; for(int k=0;k<len;k++) RAYBB[s][d]|=(ZBB)1<<RAY[s][d][k]; }
    ORTHBB[s]=RAYBB[s][0]|RAYBB[s][1]|RAYBB[s][2]|RAYBB[s][3]; DIAGBB[s]=RAYBB[s][4]|RAYBB[s][5]|RAYBB[s][6]|RAYBB[s][7]; }
  for(int pc=0;pc<32;pc++){ int t=pc>=WOFF?pc-WOFF:pc; SLOM[pc]=(pc&&(t==ROOK||t==DRAGON||t==LANCE))?~(ZBB)0:0;
    SLDM[pc]=(pc&&(t==BISHOP||t==HORSE))?~(ZBB)0:0; }
  for(int by=0;by<2;by++) for(int s=0;s<81;s++){
    uint64_t tmp[81]; for(int i=0;i<81;i++)tmp[i]=0; int r=s/9,c=s%9,sgn=(by==BLACK)?1:-1;
    #define ADD(D,N,PT) for(int i=0;i<N;i++){int qr=r-sgn*D[i][0],qc=c-sgn*D[i][1]; \
        if(qr>=0&&qr<9&&qc>=0&&qc<9)tmp[qr*9+qc]|=(1ULL<<mkpc(PT,by));}
    ADD(PAWN_D,1,PAWN) ADD(KNIGHT_D,2,KNIGHT) ADD(SILVER_D,5,SILVER) ADD(KING_D,8,KING)
    ADD(ROOK_D,4,HORSE) ADD(BISHOP_D,4,DRAGON)
    #undef ADD
    for(int i=0;i<6;i++){int qr=r-sgn*GOLD_D[i][0],qc=c-sgn*GOLD_D[i][1];
      if(qr>=0&&qr<9&&qc>=0&&qc<9){int q=qr*9+qc;
        tmp[q]|=(1ULL<<mkpc(GOLD,by))|(1ULL<<mkpc(PPAWN,by))|(1ULL<<mkpc(PLANCE,by))
               |(1ULL<<mkpc(PKNIGHT,by))|(1ULL<<mkpc(PSILVER,by));}}
    int n=0; STEPBB[by][s]=0; for(int q=0;q<81;q++) if(tmp[q]){STEP_ATK[by][s][n].q=q;STEP_ATK[by][s][n].mask=tmp[q];n++;STEPBB[by][s]|=SQBB[q];}
    STEP_ATK_N[by][s]=n;
    for(int i=0;i<n;i++){ uint64_t*rel=&STEP_REL[by][STEP_ATK[by][s][i].q-s+19]; assert(*rel==0||*rel==STEP_ATK[by][s][i].mask); *rel=STEP_ATK[by][s][i].mask; }
  }
  for(int col=0;col<2;col++){ int sgn=(col==BLACK)?1:-1;
    #define FILL(PT,D,ND) for(int s=0;s<81;s++){int r=s/9,c=s%9,n=0; for(int i=0;i<ND;i++){ \
        int nr=r+sgn*D[i][0],nc=c+sgn*D[i][1]; if(nr>=0&&nr<9&&nc>=0&&nc<9)MV_ST[col][PT][s][n++]=nr*9+nc;} MV_STN[col][PT][s]=n;}
    FILL(PAWN,PAWN_D,1) FILL(KNIGHT,KNIGHT_D,2) FILL(SILVER,SILVER_D,5) FILL(KING,KING_D,8)
    FILL(GOLD,GOLD_D,6) FILL(PPAWN,GOLD_D,6) FILL(PLANCE,GOLD_D,6) FILL(PKNIGHT,GOLD_D,6)
    FILL(PSILVER,GOLD_D,6) FILL(HORSE,ROOK_D,4) FILL(DRAGON,BISHOP_D,4)
    #undef FILL
    for(int t=0;t<15;t++) for(int s=0;s<81;s++){ MVSTBB[col][t][s]=0; for(int k=0;k<MV_STN[col][t][s];k++) MVSTBB[col][t][s]|=SQBB[MV_ST[col][t][s][k]]; }
    MV_SDIR[col][LANCE][0]=(col==BLACK)?0:1; MV_SDN[col][LANCE]=1;
    for(int i=0;i<4;i++){MV_SDIR[col][BISHOP][i]=4+i; MV_SDIR[col][HORSE][i]=4+i;
      MV_SDIR[col][ROOK][i]=i; MV_SDIR[col][DRAGON][i]=i;}
    MV_SDN[col][BISHOP]=MV_SDN[col][HORSE]=MV_SDN[col][ROOK]=MV_SDN[col][DRAGON]=4;
    for(int t=0;t<15;t++) for(int s=0;s<81;s++){ MVREACH[col][t][s]=MVSTBB[col][t][s];
      for(int di=0;di<MV_SDN[col][t];di++) MVREACH[col][t][s]|=RAYBB[s][MV_SDIR[col][t][di]]; }
  }
}
/* occ = the occupancy the slider rays see (normally occc[0]|occc[1]; king_move_safe passes it without the king). */
static inline int is_attacked_occ(Pos*p,int sq,int by,const ZBB occ){
  int*b=p->b;
  const StepAtk*sa=STEP_ATK[by][sq]; int n=STEP_ATK_N[by][sq];
  (void)sa; (void)n;
  for(ZBB c=p->occc[by]&STEPBB[by][sq]; c; c&=c-1){ int q=bb_lsb(c); if((1ULL<<b[q])&STEP_REL[by][q-sq+19]) return 1; }
  if(p->slo[by]&ORTHBB[sq]) for(int d=0;d<4;d++){ if(!(p->slo[by]&RAYBB[sq][d]))continue; ZBB m=occ&RAYBB[sq][d]; int pc=b[bb_first(m,d)];
    if(pcolor(pc)==by){int t=ptype(pc); if(t==ROOK||t==DRAGON)return 1; if(t==LANCE&&d==LANCE_INDIR[by])return 1;} }
  if(p->sld[by]&DIAGBB[sq]) for(int d=4;d<8;d++){ if(!(p->sld[by]&RAYBB[sq][d]))continue; ZBB m=occ&RAYBB[sq][d]; int pc=b[bb_first(m,d)];
    if(pcolor(pc)==by){int t=ptype(pc); if(t==BISHOP||t==HORSE)return 1;} }
  return 0;
}
static int is_attacked(Pos*p,int sq,int by){ return is_attacked_occ(p,sq,by,p->occc[0]|p->occc[1]); }
/* [ZYF-OCC] Same answer as tb_apply(king from->to) + own_safe + tb_revert without moving anything: a step attacker of
 * `to` never stands on `to` or is the moving king, rays from `to` never contain `to`, so only `from` leaving the
 * occupancy matters (a captured piece on `to` is not on any ray from `to`). */
static inline int king_move_safe(Pos*p,int from,int to,int side){ return !is_attacked_occ(p,to,side^1,(p->occc[0]|p->occc[1])&~SQBB[from]); }
static inline int in_check(Pos*p,int col){return p->ksq[col]>=0 && is_attacked(p,p->ksq[col],col^1);}

/* ---- move gen ---- */
static inline int in_zone(int s,int col){int r=RNK(s);return col==BLACK?r<=2:r>=6;}
static inline int last_rank(int s,int col){int r=RNK(s);return col==BLACK?r==0:r==8;}
static inline int last_two(int s,int col){int r=RNK(s);return col==BLACK?r<=1:r>=7;}

static int nifu(Pos*p,int col,int c){int tg=mkpc(PAWN,col);for(int r=0;r<9;r++)if(p->b[r*9+c]==tg)return 1;return 0;}

static void emit(int from,int to,int t,int col,int*out,int*n){
  int canp = IS_PROMOTABLE[t] && (in_zone(from,col)||in_zone(to,col));
  if(canp) out[(*n)++]=MV(from,to,1,0);
  int must = ((t==PAWN||t==LANCE)&&last_rank(to,col)) || (t==KNIGHT&&last_two(to,col));
  if(!must) out[(*n)++]=MV(from,to,0,0);
}

/* generate pseudo-legal moves for color; returns count */
/* [ZYF-OCC] Pieces are visited through occc[col] in ascending square order, as the old 0..80 scan did. With a zone,
 * a piece that is not a discoverer (disc[s]==0) and cannot reach the zone (MVREACH) is skipped: its moves would all be
 * dropped by the caller's zone/discovery filter, so the filtered list is unchanged. */
static inline int gen_board_z(Pos*p,int col,int*out,const ZBB zone,const int*disc){
  int n=0; int*b=p->b;
  for(ZBB c=p->occc[col]; c; c&=c-1){ int s=bb_lsb(c);
    int pc=b[s]; int t=ptype(pc);
    if(disc && !disc[s] && !(MVREACH[col][t][s]&zone)) continue;
    int*st=MV_ST[col][t][s]; int sn=MV_STN[col][t][s];
    for(int k=0;k<sn;k++){ int to=st[k]; int tp=b[to];
      if(tp){ if(pcolor(tp)==col||ptype(tp)==KING)continue; } emit(s,to,t,col,out,&n); }
    int sdn=MV_SDN[col][t];
    for(int di=0;di<sdn;di++){ int dir=MV_SDIR[col][t][di]; const int*ray=RAY[s][dir]; int len=RAYN[s][dir];
      for(int k=0;k<len;k++){ int to=ray[k]; int tp=b[to];
        if(!tp){ emit(s,to,t,col,out,&n); }
        else { if(pcolor(tp)!=col&&ptype(tp)!=KING)emit(s,to,t,col,out,&n); break; } } }
  }
  return n;
}
static int gen_board(Pos*p,int col,int*out){ return gen_board_z(p,col,out,~(ZBB)0,NULL); }   /* board moves only (no drops) */
static int droppable_at(Pos*p,int col,int t,int to){   /* 打てるか(空升前提) */
  if(t==PAWN){ if(last_rank(to,col)||nifu(p,col,COL(to)))return 0; }
  else if(t==LANCE){ if(last_rank(to,col))return 0; }
  else if(t==KNIGHT){ if(last_two(to,col))return 0; }
  return 1;
}
static int gen_pseudo(Pos*p,int col,int*out){
  int n=gen_board(p,col,out); int*b=p->b;
  static const int HT[7]={PAWN,LANCE,KNIGHT,SILVER,GOLD,BISHOP,ROOK};
  for(int hi=0;hi<7;hi++){int t=HT[hi]; if(p->hand[col][t]==0)continue;
    for(int to=0;to<81;to++){ if(b[to])continue; if(droppable_at(p,col,t,to)) out[n++]=MV(DROP_FROM,to,0,t); }
  }
  assert(n<=MAXMOVES); return n;
}

/* ---- make / unmake ---- */
static __thread Undo ustack[4096]; static __thread int usp=0;
static void make_move(Pos*p,int m){
  assert(usp<4095);   /* [FIX-DEPTH-GUARD] 無検査だった。ZYF_MAX_DEPTH が本番での上限。*/
  int col=p->side; int*b=p->b; uint64_t pk=p->key,pbk=p->bkey,pyb=p->ybk,pyh=p->yhk; int to=M_TO(m);
  if(IS_DROP(m)){int dt=M_DROP(m);int pc=mkpc(dt,col);int cnt=p->hand[col][dt];
    p->key^=Z_HAND[col][dt][cnt]; p->hand[col][dt]=cnt-1; b[to]=pc; p->yhk-=Y_HAND[col][dt]; p->occc[col]|=SQBB[to];
    p->slo[col]|=SQBB[to]&SLOM[pc]; p->sld[col]|=SQBB[to]&SLDM[pc];
    p->key^=Z_PIECE[pc][to]; p->bkey^=Z_PIECE[pc][to]; p->ybk^=Y_PSQ[pc][to];
    ustack[usp++]=(Undo){m,EMPTY,pk,pbk,pyb,pyh};
  } else {
    int from=M_FROM(m); int moved=b[from]; int cap=b[to];
    p->key^=Z_PIECE[moved][from]; p->bkey^=Z_PIECE[moved][from]; p->ybk^=Y_PSQ[moved][from]; b[from]=EMPTY; p->occc[col]=(p->occc[col]&~SQBB[from])|SQBB[to];
    p->slo[col]&=~SQBB[from]; p->sld[col]&=~SQBB[from];
    if(cap){ p->occc[col^1]&=~SQBB[to]; p->slo[col^1]&=~SQBB[to]; p->sld[col^1]&=~SQBB[to]; p->key^=Z_PIECE[cap][to]; p->bkey^=Z_PIECE[cap][to]; p->ybk^=Y_PSQ[cap][to];
      int base=DEMO[ptype(cap)]; int cnt=p->hand[col][base];
      p->key^=Z_HAND[col][base][cnt+1]; p->hand[col][base]=cnt+1; p->yhk+=Y_HAND[col][base]; }
    int mt=ptype(moved); int nt=M_PROMO(m)?PROMO[mt]:mt; int npc=mkpc(nt,col);
    b[to]=npc; p->key^=Z_PIECE[npc][to]; p->bkey^=Z_PIECE[npc][to]; p->ybk^=Y_PSQ[npc][to];
    p->slo[col]|=SQBB[to]&SLOM[npc]; p->sld[col]|=SQBB[to]&SLDM[npc];
    if(mt==KING)p->ksq[col]=to;
    ustack[usp++]=(Undo){m,cap,pk,pbk,pyb,pyh};
  }
  p->side^=1; p->key^=Z_SIDE; p->bkey^=Z_SIDE; p->ybk^=Y_SIDE;
}
static void unmake_move(Pos*p){
  Undo u=ustack[--usp]; p->side^=1; int col=p->side; int*b=p->b; int m=u.move; int to=M_TO(m);
  p->key=u.prevkey; p->bkey=u.prev_bkey; p->ybk=u.prev_ybk; p->yhk=u.prev_yhk;
  if(IS_DROP(m)){int dt=M_DROP(m); b[to]=EMPTY; p->hand[col][dt]++; p->occc[col]&=~SQBB[to]; p->slo[col]&=~SQBB[to]; p->sld[col]&=~SQBB[to]; }
  else{int from=M_FROM(m); int mt=ptype(b[to]); if(M_PROMO(m))mt=DEMO[mt];
    b[from]=mkpc(mt,col); b[to]=u.captured; p->occc[col]=(p->occc[col]&~SQBB[to])|SQBB[from]; if(u.captured)p->occc[col^1]|=SQBB[to];
    p->slo[col]=(p->slo[col]&~SQBB[to])|(SQBB[from]&SLOM[b[from]]); p->sld[col]=(p->sld[col]&~SQBB[to])|(SQBB[from]&SLDM[b[from]]);
    p->slo[col^1]|=SQBB[to]&SLOM[u.captured]; p->sld[col^1]|=SQBB[to]&SLDM[u.captured];
    if(u.captured){int base=DEMO[ptype(u.captured)];p->hand[col][base]--;}
    if(mt==KING)p->ksq[col]=from;
  }
}
/* Key after make_move(p,m), computed without moving (same Zobrist updates as make_move). Used to prefetch TT lines. */
static inline uint64_t child_key(const Pos*p,int m){
  int col=p->side; uint64_t k=p->key^Z_SIDE; int to=M_TO(m);
  if(IS_DROP(m)){ int dt=M_DROP(m); return k^Z_HAND[col][dt][p->hand[col][dt]]^Z_PIECE[mkpc(dt,col)][to]; }
  int from=M_FROM(m); int moved=p->b[from]; int cap=p->b[to];
  k^=Z_PIECE[moved][from];
  if(cap){ int base=DEMO[ptype(cap)]; k^=Z_PIECE[cap][to]^Z_HAND[col][base][p->hand[col][base]+1]; }
  int mt=ptype(moved); return k^Z_PIECE[mkpc(M_PROMO(m)?PROMO[mt]:mt,col)][to];
}
static inline int own_safe(Pos*p,int mover){int k=p->ksq[mover];return k<0||!is_attacked(p,k,mover^1);}

/* ---- perft ---- */
static long perft(Pos*p,int depth){
  if(depth==0)return 1;
  int mv[MAXMOVES]; int n=gen_pseudo(p,p->side,mv); long tot=0; int col=p->side;
  for(int i=0;i<n;i++){make_move(p,mv[i]); if(own_safe(p,col))tot+=perft(p,depth-1); unmake_move(p);}
  return tot;
}
/* [S-1 検証] is_mated(p) == (gen_legal(p)==0) を perft 木の全節点で突き合わせる。
 * is_mated は gen_legal の並べ替え + 早期脱出であり等価でなければならない。
 * 不一致は false mate / 詰み見逃しに直結するので即 abort する。
 * `Zyfamate.exe verify-ismated N` で起動 (テスト専用。探索経路からは呼ばれない)。 */
static int is_mated(Pos*p);
static int gen_legal(Pos*p,int*out);
#define IS_MATED_KEEPPINS(p, dst) do { \
    int _pin_save[81]; memcpy(_pin_save,g_pin,sizeof(_pin_save)); \
    (dst) = is_mated(p); \
    memcpy(g_pin,_pin_save,sizeof(_pin_save)); \
  } while(0)
static long g_vm_checked=0;
static long perft_verify_ismated(Pos*p,int depth){
  { int buf[MAXMOVES]; int nl=gen_legal(p,buf); int im=is_mated(p); g_vm_checked++;
    if((nl==0)!=(im!=0)){ fprintf(stderr,"IS_MATED MISMATCH: gen_legal=%d is_mated=%d side=%d\n",nl,im,p->side); abort(); } }
  if(depth==0)return 1;
  int mv[MAXMOVES]; int n=gen_pseudo(p,p->side,mv); long tot=0; int col=p->side;
  for(int i=0;i<n;i++){make_move(p,mv[i]); if(own_safe(p,col))tot+=perft_verify_ismated(p,depth-1); unmake_move(p);}
  return tot;
}


#define INF (1<<28)

/* 世代カウンタ(memset回避) */
/* [ZYF-CTX 2026-10-05] Solver state that a solve() owns (TT, superiority and twin tables, generation, attacker,
 * limits, mate-length memo) lives in a ZCtx. Each thread reads it through the thread_local pointer zc, which starts at
 * the shared context 0, so every existing caller (the service's first lane, MateWorker, MPON, the main thread while
 * the service is paused) keeps using the same tables as before. Cyf_Zyf_Defense's second service lane switches to
 * context 1 (use_context(1)) and so solves concurrently with its own tables. Read-only tables (attack masks, zobrist),
 * table sizes (hash_autosize) and options stay process-wide; g_stop stays shared so a pause stops every lane.
 * The names below are macros for zc->member, so the code and `Zyfamate::g_gen`-style references read as before. */
#define TWK 12
typedef struct { int pn, dn; uint32_t gen; } TTRec;
typedef struct { uint64_t ah, dh; } SHand;
typedef struct { uint64_t*bk; uint32_t*gen; SHand*h; } SupTab;
struct ZCtx {
  uint32_t g_gen_=0;
  uint64_t *tt_key_=NULL; TTRec *tt_rec_=NULL; int g_attacker_=0;
  SupTab proof_tab_s_{}, nomate_tab_s_{}; SupTab *proof_tab_=NULL, *nomate_tab_=NULL;
  long g_limit_=0; double g_t0_=0, g_tlimit_=0;
  uint64_t *tw_key_=NULL; uint64_t (*tw_cause_)[TWK]=NULL; uint8_t *tw_n_=NULL,*tw_used_=NULL; long tw_live_=0;
  uint64_t *m3_key_=NULL; uint32_t *m3_gen_=NULL;
  uint64_t *lm_key_=NULL; uint8_t *lm_val_=NULL,*lm_used_=NULL; long lm_nodes_=0,lm_limit_=0; jmp_buf lm_jmp_;
  int lm_honor_stop_=1; double lm_deadline_=0; double g_tes_deadline_=0;
};
static ZCtx g_ctx[2];
static thread_local ZCtx* zc=&g_ctx[0];
static inline void use_context(int i){ zc=&g_ctx[i==1?1:0]; }
#define g_gen zc->g_gen_
#define tt_key zc->tt_key_
#define tt_rec zc->tt_rec_
#define g_attacker zc->g_attacker_
#define proof_tab_s zc->proof_tab_s_
#define nomate_tab_s zc->nomate_tab_s_
#define proof_tab zc->proof_tab_
#define nomate_tab zc->nomate_tab_
#define g_limit zc->g_limit_
#define g_t0 zc->g_t0_
#define g_tlimit zc->g_tlimit_
#define tw_key zc->tw_key_
#define tw_cause zc->tw_cause_
#define tw_n zc->tw_n_
#define tw_used zc->tw_used_
#define tw_live zc->tw_live_
#define m3_key zc->m3_key_
#define m3_gen zc->m3_gen_
#define lm_key zc->lm_key_
#define lm_val zc->lm_val_
#define lm_used zc->lm_used_
#define lm_nodes zc->lm_nodes_
#define lm_limit zc->lm_limit_
#define lm_jmp zc->lm_jmp_
#define lm_honor_stop zc->lm_honor_stop_
#define lm_deadline zc->lm_deadline_
#define g_tes_deadline zc->g_tes_deadline_
/* ==== ハッシュ表の実行時自動サイズ (8GB〜2TB のどの機種でも適正化) ====
 * 物理RAMを起動時に検出し、総予算 = RAM/16 を [256MB, 8GB] にクランプ。
 * one-shot CLI なので過大確保は起動コスト増 — 上限は控えめにする。
 * 環境変数 ZYFAMATE_HASH_MB で明示指定可 (親プロセスから調整できる)。 */
#ifdef _WIN32
#else
#endif
static unsigned tt_bits, st_bits, tw_bits;
static size_t tt_size, st_size, tw_size;
static uint64_t tt_mask, st_mask, tw_mask;
static uint64_t phys_ram_bytes(void){
#ifdef _WIN32
    MEMORYSTATUSEX ms; ms.dwLength=sizeof(ms);
    if(GlobalMemoryStatusEx(&ms)) return (uint64_t)ms.ullTotalPhys;
#else
    long pg=sysconf(_SC_PHYS_PAGES), ps=sysconf(_SC_PAGE_SIZE);
    if(pg>0&&ps>0) return (uint64_t)pg*(uint64_t)ps;
#endif
    return 8ULL<<30; /* 検出不能時は 8GB 相当として控えめに */
}
static unsigned fit_bits(uint64_t budget,unsigned entry,unsigned lo,unsigned hi){
    unsigned b=lo; while(b<hi && ((uint64_t)entry<<(b+1))<=budget) b++; return b;
}
static void hash_autosize_once(void);
static void hash_autosize(void){ static std::once_flag once; std::call_once(once,hash_autosize_once); }   /* [ZYF-CTX] lanes may race */
static void hash_autosize_once(void){
    uint64_t budget; const char*ev=getenv("ZYFAMATE_HASH_MB");
    /* [FIX-HASH-CLAMP] env 経路は無制限だった。malloc は戻り値を検査していないので
     * (下記 zmalloc 参照) 過大指定は NULL 参照 = segfault になる。物理RAMの半分と
     * 16MB を上下限にクランプし、親プロセスの設定ミスを事故にしない。 */
    if(ev&&atoll(ev)>0){ budget=(uint64_t)atoll(ev)<<20;
           uint64_t cap=phys_ram_bytes()/2;
           if(budget<(16ULL<<20))budget=16ULL<<20;
           if(cap>(16ULL<<20) && budget>cap) budget=cap; }
    else { budget=phys_ram_bytes()/16;
           if(budget<(256ULL<<20))budget=256ULL<<20;
           if(budget>(8ULL<<30))budget=8ULL<<30; }
    /* エントリ実測: TT=20B, 優越表=2×sizeof(SEnt)=64B, twin≈106B */
    tt_bits=fit_bits(budget*55/100,20,20,30);
    st_bits=fit_bits(budget*35/100,64,18,28);
    tw_bits=fit_bits(budget*10/100,106,16,24);
    tt_size=(size_t)1<<tt_bits; tt_mask=tt_size-1;
    st_size=(size_t)1<<st_bits; st_mask=st_size-1;
    tw_size=(size_t)1<<tw_bits; tw_mask=tw_size-1;
    fprintf(stderr,"info hash tt=2^%u st=2^%u tw=2^%u (budget=%lluMB)\n",
            tt_bits,st_bits,tw_bits,(unsigned long long)(budget>>20));
}
#define TTSIZE tt_size
#define TTMASK tt_mask
#define STSIZE st_size
#define STMASK st_mask
#define STBITS st_bits
#define TWSIZE tw_size
#define TWMASK tw_mask
/* [FIX-ALLOC-CHECK] 全ての表の malloc/calloc は戻り値を検査していなかった。GB級の
 * 確保が失敗すると NULL のまま探索に入り segfault する (「表が確保できなかった」と
 * いう最も診断しやすい失敗が、最も診断しにくいクラッシュに化けていた)。ここで
 * 明示的に落とす。malloc/calloc をそのまま呼ぶので、埋め込み側の
 * `#define malloc(s) Zyfamate::_zmalloc(s)` プロキシは従来通り効く。 */
static void zchk(const void*q,const char*what,uint64_t bytes){
  if(!q){ fprintf(stderr,"info string FATAL: Zyfamate could not allocate %s (%llu MB). "
                  "Lower ZYFAMATE_HASH_MB.\n",what,(unsigned long long)(bytes>>20));
          fflush(stderr); exit(1); }
}
/* [埋め込み互換] 確保は必ず**代入式のその場**で行い、検査だけを別に回す。
 * cyfamate.cpp は `#define malloc(s) Zyfamate::_zmalloc(s)` で確保をプロキシ型
 * `_CMul{ template<class T> operator T*() const }` に差し替えており、代入先の型 T へ
 * **暗黙変換**することで動いている。ここで `p = zchk(malloc(n))` のように void* を
 * 返す関数を挟むと、C では通っても **C++ では void*→T* の暗黙変換が無いためコンパイル
 * エラー**になり、埋め込みビルドが 12 箇所で壊れる (実機 g++ 16.1.0 で確認済み)。
 * 代入をマクロ内に閉じ込めれば、プロキシの変換演算子が従来どおり働く。 */
#define ZMALLOC(lhs,n,what)     do{ (lhs)=malloc(n);        zchk((lhs),(what),(uint64_t)(n)); }while(0)
#define ZCALLOC(lhs,c,s,what)   do{ (lhs)=calloc((c),(s));  zchk((lhs),(what),(uint64_t)(c)*(uint64_t)(s)); }while(0)
/* TT */
/* [ZYF-TT-LAYOUT 2026-10-05] Keys stay in their own array (8 keys = one 64B line, the probe window) and pn/dn/gen
 * share one 12B record, so a probe reads the key line(s) and touches the record line only on a key match; the old
 * layout (key, pn, dn, gen in four arrays, gen tested first) touched up to four lines per probe. Probe order, slot
 * choice and replacement are unchanged, so the search (nodes, verdicts) is identical. 8+12B = the old 20B/entry. */
/* TTRec, tt_key, tt_rec: ZCtx */
static void tt_init(){ hash_autosize(); if(!tt_key){ ZMALLOC(tt_key,sizeof(uint64_t)*TTSIZE,"tt_key"); ZCALLOC(tt_rec,TTSIZE,sizeof(TTRec),"tt_rec");} }
static inline void tt_prefetch(uint64_t k){ __builtin_prefetch(&tt_key[k&TTMASK]); __builtin_prefetch(&tt_key[(k+7)&TTMASK]);
  __builtin_prefetch(&tt_rec[k&TTMASK]); }
static inline void tt_clear_gen(){ for(size_t i=0;i<TTSIZE;i++) tt_rec[i].gen=0; }
/* g_attacker: ZCtx */
/* [ZY01-KEEP-PROOFS 2026-10-02] Opt-in proof retention across solve() calls (cf. Suisho-BM KeepOnlyProofs).
 * Off (default): the code paths below are the original ones. On: a proof (pn==0, dn>=INF) is tagged PROOF_GEN
 * and stays readable by later solves; every other entry still dies with g_gen, and twins are still cleared per
 * solve, so path-dependent disproofs are never carried over. Proofs are already unconditional in finalize_ghi.
 * pn/dn mean "for g_attacker", so keys are salted by the attacker colour while keeping proofs. */
#define PROOF_GEN 0xFFFFFFFFu
static int g_keep_proofs=0; static std::atomic<long> g_proof_hits{0};
static inline uint64_t tt_salt(){ return g_attacker ? 0x9E3779B97F4A7C15ULL : 0x5851F42D4C957F2DULL; }
static int tt_find_keep(uint64_t k){int i=(int)(k&TTMASK); for(int j=0;j<8;j++){int x=(i+j)&TTMASK;
  if(tt_key[x]==k){ if(tt_rec[x].gen==g_gen)return x; if(tt_rec[x].gen==PROOF_GEN){++g_proof_hits;return x;} } } return -1;}
static void tt_store_keep(uint64_t k,int pn,int dn){int i=(int)(k&TTMASK); int slot=-1,fr=-1;
  for(int j=0;j<8;j++){int x=(i+j)&TTMASK; if(tt_rec[x].gen==g_gen||tt_rec[x].gen==PROOF_GEN){ if(tt_key[x]==k){slot=x;break;} } else if(fr<0)fr=x; }
  if(slot<0)slot=(fr>=0?fr:i);
  tt_rec[slot].gen=(pn==0&&dn>=INF)?PROOF_GEN:g_gen;tt_key[slot]=k;tt_rec[slot].pn=pn;tt_rec[slot].dn=dn;}
static int tt_find(uint64_t k){ if(g_keep_proofs) return tt_find_keep(k^tt_salt());
  int i=(int)(k&TTMASK); for(int j=0;j<8;j++){int x=(i+j)&TTMASK;
  if(tt_key[x]==k && tt_rec[x].gen==g_gen)return x;} return -1;}
static void tt_read(uint64_t k,int*pn,int*dn){int x=tt_find(k); if(x<0){*pn=1;*dn=1;}else{*pn=tt_rec[x].pn;*dn=tt_rec[x].dn;}}
static void tt_store(uint64_t k,int pn,int dn){ if(g_keep_proofs){ tt_store_keep(k^tt_salt(),pn,dn); return; }
  int i=(int)(k&TTMASK); int slot=-1,fr=-1;
  for(int j=0;j<8;j++){int x=(i+j)&TTMASK; if(tt_rec[x].gen==g_gen){ if(tt_key[x]==k){slot=x;break;} } else if(fr<0)fr=x; }
  if(slot<0)slot=(fr>=0?fr:i);
  tt_rec[slot].gen=g_gen;tt_key[slot]=k;tt_rec[slot].pn=pn;tt_rec[slot].dn=dn;}

static unsigned g_order_variant=0; // Independent-process portfolio, identical legal move sets.
/* ---- 優越関係(証明駒/反証駒) ---- */
#define STPROBE 16
/* [ZYF-SUP-SOA 2026-10-05] Each superiority table is three arrays: board keys, generations and packed hands (byte t-1 =
 * count of hand type t, byte 7 = 0). A probe of the 16-slot window reads 128B of keys and 64B of generations and
 * touches a hand record only on a key match; the old 32B entries spread the window over 512B. Window, order, match
 * and replacement rules are those of the old loops, so the search is unchanged. 8+4+16=28B/entry (old: 32B).
 * An AVX-512 window compare (16 gens in one compare, 16 keys in two) kept the search identical but was ~15% slower
 * than this scalar loop on the avx512_vnni bench (AVX2: no gain), so it is not used. */
/* SHand, SupTab, proof_tab(_s), nomate_tab(_s): ZCtx */
static int use_superior=1;
static void sup_alloc(SupTab*t,const char*what){ ZCALLOC(t->bk,STSIZE,sizeof(uint64_t),what); ZCALLOC(t->gen,STSIZE,sizeof(uint32_t),what);
  ZCALLOC(t->h,STSIZE,sizeof(SHand),what); }
static void sup_clear(SupTab*t){ memset(t->bk,0,STSIZE*sizeof(uint64_t)); memset(t->gen,0,STSIZE*sizeof(uint32_t)); memset(t->h,0,STSIZE*sizeof(SHand)); }
static void sup_init(){ hash_autosize(); if(!proof_tab){ sup_alloc(&proof_tab_s,"proof_tab"); sup_alloc(&nomate_tab_s,"nomate_tab");
  proof_tab=&proof_tab_s; nomate_tab=&nomate_tab_s; } }
static inline int sup_idx(uint64_t bk){ return (int)((bk*0x9E3779B97F4A7C15ULL)>>(64-STBITS)); }
static inline uint64_t hand_pack(Pos*p,int col){ uint64_t h=0; for(int t=1;t<=7;t++) h|=(uint64_t)(uint8_t)p->hand[col][t]<<(8*(t-1)); return h; }
/* a[i]<=b[i] for every byte: counts are <128, so (b|0x80)-a keeps bit 7 of a byte exactly when a<=b, with no borrow. */
static inline int subseteq(uint64_t a,uint64_t b){ const uint64_t H=0x8080808080808080ULL; return (((b|H)-a)&H)==H; }
/* Bits j (0..15) of the window i0..i0+15: *live = gen is current, *hit = gen current and key == bk. */
static inline void sup_masks(const SupTab*t,int i0,uint64_t bk,unsigned*live,unsigned*hit){
  unsigned g=0,k=0; for(int j=0;j<STPROBE;j++){ int x=(i0+j)&STMASK; if(t->gen[x]==g_gen){ g|=1u<<j; if(t->bk[x]==bk) k|=1u<<j; } }
  *live=g; *hit=k;
}
/* è©°ã¿ä¸€èˆ¬åŒ–: e.ahâŠ†A ã‹ã¤ e.dhâŠ‡D ã®è¨¼æ˜Žæ¸ˆã¿å±€é¢ãŒã‚ã‚Œã°ç¾å±€é¢ã‚‚è©°ã¿ */
static int sup_mate(uint64_t bk,uint64_t A,uint64_t D){ int i0=sup_idx(bk); unsigned live,hit; sup_masks(proof_tab,i0,bk,&live,&hit);
  for(;hit;hit&=hit-1){ const SHand*e=&proof_tab->h[(i0+__builtin_ctz(hit))&STMASK]; if(subseteq(e->ah,A)&&subseteq(D,e->dh))return 1; } return 0; }
/* ä¸è©°ä¸€èˆ¬åŒ–(åƒæ—¥æ‰‹éžä¾å­˜ã®ã¿è¨˜éŒ²): e.ahâŠ‡A ã‹ã¤ e.dhâŠ†D ã®ä¸è©°å±€é¢ãŒã‚ã‚Œã°ç¾å±€é¢ã‚‚ä¸è©° */
static int sup_nomate(uint64_t bk,uint64_t A,uint64_t D){ int i0=sup_idx(bk); unsigned live,hit; sup_masks(nomate_tab,i0,bk,&live,&hit);
  for(;hit;hit&=hit-1){ const SHand*e=&nomate_tab->h[(i0+__builtin_ctz(hit))&STMASK]; if(subseteq(A,e->ah)&&subseteq(e->dh,D))return 1; } return 0; }
/* The old loop: the first live slot with key bk is refreshed; otherwise the first stale slot, else slot i0, is taken. */
static void sup_rec(SupTab*tab,uint64_t bk,uint64_t A,uint64_t D){ int i0=sup_idx(bk); unsigned live,hit; sup_masks(tab,i0,bk,&live,&hit);
  if(hit){ SHand*e=&tab->h[(i0+__builtin_ctz(hit))&STMASK]; e->ah=A; e->dh=D; return; }
  unsigned fr=~live&((1u<<STPROBE)-1); int s=fr?(i0+__builtin_ctz(fr))&STMASK:(i0&STMASK);
  tab->gen[s]=g_gen; tab->bk[s]=bk; tab->h[s].ah=A; tab->h[s].dh=D; }

static __thread long g_nodes; static __thread jmp_buf g_jmp;   /* g_limit, g_t0, g_tlimit: ZCtx */
/* [ZY01-STOP-REASON] why the last solve() on this thread returned unknown: 0 none, 1 node limit, 2 time limit,
 * 3 external g_stop, 4 depth guard. Lets a caller keep "budget ran out" apart from "cancelled". */
static __thread int g_stop_reason=0;
// Short native jobs must observe their clock and cancellation before a long node batch.
static __thread unsigned g_poll_mask=8191;
static std::atomic<int> g_stop{0}; static int g_parallel=0;
static volatile int g_usi_mode=0; static double g_usi_last_info=0;
static __thread uint64_t g_path[4096]; static __thread int g_pathlen;
static int on_path(uint64_t k){for(int i=0;i<g_pathlen;i++)if(g_path[i]==k)return 1;return 0;}

/* ===== GHI対策: base/twin法 (Kishimoto & Müller 2004, 簡略版) ===== */
/* TWK, tw_key, tw_cause, tw_n, tw_used: ZCtx */
/* [ZYF-TW-LIVE 2026-10-05] Number of slots with tw_used==1. twin_find can only return a slot with tw_used==1, so with
 * none live it returns -1 without touching the table (most solves have few or no GHI twins). */
/* tw_live: ZCtx */
static void twin_init(){ hash_autosize(); if(!tw_key){ ZMALLOC(tw_key,sizeof(uint64_t)*TWSIZE,"tw_key"); ZMALLOC(tw_cause,sizeof(uint64_t)*TWK*TWSIZE,"tw_cause");
    ZMALLOC(tw_n,TWSIZE,"tw_n"); ZMALLOC(tw_used,TWSIZE,"tw_used");} memset(tw_used,0,TWSIZE); tw_live=0; }
/* [FIX-TWIN-NULL] solve_parallel は tt_init/wk_init しか呼ばず twin_init を呼ばないため、
 * tw_used が NULL のまま can_mate (dn==0&&pn>=INF 枝) から到達し得る。実測では発火を
 * 再現できなかった (「並列が証明できる易しい詰み」×「can_mate が (INF,0) 節点に降りる」
 * の2条件がほぼ排他) が、mate_length 入口で tw_used=0x0 を実測しており潜在的な NULL
 * 参照であることは確認済み。ここで塞げば全呼び元 (can_mate/first_mate_move) が同時に守られる。*/
static int twin_find(uint64_t k){ if(!tw_used || tw_live==0) return -1;
  int i=(int)(k&TWMASK); for(int j=0;j<8;j++){int x=(i+j)&TWMASK;
  if(tw_used[x]==0)return -1; if(tw_used[x]==1&&tw_key[x]==k)return x;} return -1; }
static void twin_remove(uint64_t k){ int x=twin_find(k); if(x>=0){ tw_used[x]=2; --tw_live; } }
static void twin_put(uint64_t k,const uint64_t*causes,int n){ if(n>TWK)n=TWK;
  int i=(int)(k&TWMASK),slot=-1; for(int j=0;j<8;j++){int x=(i+j)&TWMASK;
    if(tw_used[x]==1&&tw_key[x]==k){slot=x;break;} if(tw_used[x]!=1&&slot<0)slot=x; if(tw_used[x]==0)break;}
  if(slot<0)slot=i&TWMASK;
  /* [FIX-TWIN-VICTIM] 8スロット全部が別キーで live のとき、ここは他人の live twin を
   * 無言で潰していた。twin は「この不詰は千日手に依存する」という条件を保持しており、
   * TT 側には finalize_ghi が書いた生の (INF,0) が残る。twin だけ消えると
   * tt_read_ghi が twin 枝を通らず (INF,0) をそのまま返す = 経路依存の不詰が
   * 無条件 nomate に昇格 (false nomate)。さらに sup_rec(nomate_tab) 経由で
   * 持駒の上位集合すべてに一般化されて伝播する。犠牲者の TT を「未知」(1,1) に
   * 戻し、再探索を強制することで条件を復元する (別ノードなので無限ループしない)。
   * 注: tt_store 側の同型の盲目上書き (下記 tt_store) は TT が純キャッシュなので
   * 健全性は損なわない。健全性を持つのは twin だけ。 */
  if(tw_used[slot]==1 && tw_key[slot]!=k) tt_store(tw_key[slot],1,1);
  if(tw_used[slot]!=1) ++tw_live; tw_used[slot]=1; tw_key[slot]=k; tw_n[slot]=n;
  for(int t=0;t<n;t++)tw_cause[slot][t]=causes[t]; }
static int twin_cause_on_path(int x){ for(int t=0;t<tw_n[x];t++) if(on_path(tw_cause[x][t]))return 1; return 0; }
static void tt_read_ghi(uint64_t k,int*pn,int*dn){ int x=tt_find(k); if(x<0){*pn=1;*dn=1;return;}
  int tw=twin_find(k); if(tw>=0){ if(twin_cause_on_path(tw)){*pn=INF;*dn=0;}else{*pn=1;*dn=1;} return; }
  *pn=tt_rec[x].pn; *dn=tt_rec[x].dn; }
static int finalize_ghi(uint64_t key,int pn,int dn,const uint64_t*rt,int rtn,uint64_t*out){
  if(pn==0){ tt_store(key,0,INF); twin_remove(key); return 0; }
  if(dn==0){ int no=0;
    for(int i=0;i<rtn;i++){ uint64_t k=rt[i]; if(k!=key && on_path(k)){ int dup=0;
      for(int j=0;j<no;j++)if(out[j]==k){dup=1;break;} if(!dup&&no<TWK)out[no++]=k; } }
    tt_store(key,INF,0);
    if(no>0){ twin_put(key,out,no); return no; }
    twin_remove(key); return 0; }
  tt_store(key,pn,dn); twin_remove(key); return 0; }

static double now(){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts); return ts.tv_sec+ts.tv_nsec*1e-9; }

static const int RD8[8][2]={{-1,0},{1,0},{0,-1},{0,1},{-1,-1},{-1,1},{1,-1},{1,1}};
static __thread int g_pin[81];
/* [FIX-VERIFY-DEAD] g_verify は宣言と 2 箇所の read しか存在せず、**書き込みが無かった**
 * ため到達不能 (-O3 では常に 0 と証明されて両ブロックごと削除される) = 生成器の
 * クロスチェック機構が存在しないのと同じだった。VERIFY 環境変数で有効化する。
 * 埋め込み側からは同一TU内なので `Zyfamate::g_verify=1;` で直接叩ける。非static関数を
 * 生やすとファイル唯一の外部シンボルになり埋め込み時の衝突面になるので置かない。 */
static int g_verify=0;
static int isPinner(int Qpc,int d){ int t=ptype(Qpc);
  if(d<4){ if(t==ROOK||t==DRAGON)return 1;
    if(t==LANCE){ int fr=(pcolor(Qpc)==BLACK)?-1:1; return RD8[d][1]==0 && fr==-RD8[d][0]; }
    return 0; }
  return t==BISHOP||t==HORSE; }
static void compute_pins(Pos*p,int me){
  for(int i=0;i<81;i++)g_pin[i]=0;
  int K=p->ksq[me]; if(K<0)return; int*b=p->b; int en=me^1; const ZBB occ=p->occc[0]|p->occc[1];
  for(int d=0;d<8;d++){ ZBB m=occ&RAYBB[K][d]; if(!m)continue; int cand=bb_first(m,d);
    if(pcolor(b[cand])!=me)continue; m&=~SQBB[cand]; if(!m)continue; int pc=b[bb_first(m,d)];
    if(pcolor(pc)==en && isPinner(pc,d))g_pin[cand]=d+1; } }
static inline int colinearK(Pos*p,int K,int to,int pinidx){
  (void)p; int d=pinidx-1; int tr=RNK(to)-RNK(K), tc=COL(to)-COL(K);
  return tr*RD8[d][1]==tc*RD8[d][0]; }
static __thread int g_disc[81];
static void compute_disc(Pos*p,int att){      /* 開き王手の元(攻方駒が dk と攻方走り駒の間) */
  for(int i=0;i<81;i++)g_disc[i]=0;
  int dk=p->ksq[att^1]; if(dk<0)return; int*b=p->b; const ZBB occ=p->occc[0]|p->occc[1];
  for(int d=0;d<8;d++){ ZBB m=occ&RAYBB[dk][d]; if(!m)continue; int cand=bb_first(m,d);
    if(pcolor(b[cand])!=att)continue; m&=~SQBB[cand]; if(!m)continue; int pc=b[bb_first(m,d)];
    if(pcolor(pc)==att && isPinner(pc,d))g_disc[cand]=d+1; }
}
static int attacks_sq(Pos*p,int to,int T,int att,int dk,int from){ /* 型Tの攻方駒がtoからdkを攻めるか */
  /* dk holds the defending king, so "dk comes before any other piece on the ray, from treated as empty" is
   * "dk is the first blocker of occ minus from" (same answer as the square walk). */
  if(MVSTBB[att][T][to]&SQBB[dk]) return 1;
  int sdn=MV_SDN[att][T]; if(!sdn) return 0;
  ZBB occ=p->occc[0]|p->occc[1]; if(from<81) occ&=~SQBB[from];
  for(int di=0;di<sdn;di++){ int dir=MV_SDIR[att][T][di]; ZBB m=occ&RAYBB[to][dir]; if(m && bb_first(m,dir)==dk) return 1; }
  return 0;
}
static int gives_check_fast(Pos*p,int m,int att,int dk){
  int to=M_TO(m), from=M_FROM(m), T;
  if(from==DROP_FROM)T=M_DROP(m); else {int mt=ptype(p->b[from]); T=M_PROMO(m)?PROMO[mt]:mt;}
  if(attacks_sq(p,to,T,att,dk,from)) return 1;                 /* 直接王手 */
  if(from!=DROP_FROM && g_disc[from] && !colinearK(p,dk,to,g_disc[from])) return 1; /* 開き王手 */
  return 0;
}
/* 利き判定専用の軽量着手(盤面とking_sqのみ更新, 置換/持駒/keyは触らない) */
typedef struct { int from,to,moved,cap,kfrom; } TB;
static inline TB tb_apply(Pos*p,int m){
  int*b=p->b; int col=p->side; int to=M_TO(m); TB t; t.to=to; t.kfrom=-1;
  if(M_FROM(m)==DROP_FROM){ t.from=-1; t.moved=0; t.cap=0; b[to]=mkpc(M_DROP(m),col); p->occc[col]|=SQBB[to];
    p->slo[col]|=SQBB[to]&SLOM[b[to]]; p->sld[col]|=SQBB[to]&SLDM[b[to]]; return t; }
  int from=M_FROM(m); int moved=b[from]; int cap=b[to];
  t.from=from; t.moved=moved; t.cap=cap; b[from]=EMPTY; p->occc[col]=(p->occc[col]&~SQBB[from])|SQBB[to]; if(cap)p->occc[col^1]&=~SQBB[to];
  int mt=ptype(moved); b[to]=mkpc(M_PROMO(m)?PROMO[mt]:mt,col);
  p->slo[col]=(p->slo[col]&~SQBB[from])|(SQBB[to]&SLOM[b[to]]); p->sld[col]=(p->sld[col]&~SQBB[from])|(SQBB[to]&SLDM[b[to]]);
  p->slo[col^1]&=~SQBB[to]; p->sld[col^1]&=~SQBB[to];
  if(mt==KING){ t.kfrom=from; p->ksq[col]=to; }
  return t;
}
static inline void tb_revert(Pos*p,TB t){
  int*b=p->b; if(t.from==-1){ b[t.to]=EMPTY; p->occc[p->side]&=~SQBB[t.to]; p->slo[p->side]&=~SQBB[t.to]; p->sld[p->side]&=~SQBB[t.to]; return; }
  b[t.from]=t.moved; b[t.to]=t.cap; if(t.kfrom>=0)p->ksq[p->side]=t.kfrom;
  p->occc[p->side]=(p->occc[p->side]&~SQBB[t.to])|SQBB[t.from]; if(t.cap)p->occc[p->side^1]|=SQBB[t.to];
  { int c=p->side; p->slo[c]=(p->slo[c]&~SQBB[t.to])|(SQBB[t.from]&SLOM[t.moved]); p->sld[c]=(p->sld[c]&~SQBB[t.to])|(SQBB[t.from]&SLDM[t.moved]);
    p->slo[c^1]|=SQBB[t.to]&SLOM[t.cap]; p->sld[c^1]|=SQBB[t.to]&SLDM[t.cap]; }
}
/* 攻方の王手生成(打ち歩詰を除外) */
static int gen_checks(Pos*p,int*out){
  int att=p->side, def=att^1; int K=p->ksq[att];
  int incheck=(K>=0 && is_attacked(p,K,def));
  compute_pins(p,att); compute_disc(p,att); int dk=p->ksq[def];
  int*b=p->b; int m=0;
  if(incheck || dk<0){                       /* 稀: 攻方が王手されている等は旧法 */
    int pm[MAXMOVES]; int n=gen_pseudo(p,att,pm);
    for(int i=0;i<n;i++){ int mv=pm[i],from=M_FROM(mv); TB t=tb_apply(p,mv); int legal=own_safe(p,att); tb_revert(p,t);
      if(!legal)continue; if(!(dk>=0&&gives_check_fast(p,mv,att,dk)))continue;
      /* [S-1] 打歩詰判定: 旧実装は gen_pseudo で受け方の全 pseudo-legal 手 (81升走査 +
       * 全打ち先) を作ってから逃れ手を探していた。is_mated は王手回避に特化した
       * 生成 (玉の逃げ→取り→合駒) を早期脱出付きで行うので同一判定をずっと安く出す。
       * make_move を tb_apply に落としてはならない — tb_apply は p->side で駒を着色し
       * ksq も p->side で戻すため、side を反転せずに受け方の逃げを組み立てられない。*/
      if(from==DROP_FROM&&M_DROP(mv)==PAWN){ make_move(p,mv); int mated; IS_MATED_KEEPPINS(p,mated); unmake_move(p); if(mated)continue; }
      out[m++]=mv; }
  } else {
    ZBB zone=STEPBB[att][dk]; for(int d=0;d<8;d++) zone|=RAYBB[dk][d];   /* [ZYF-OCC] old inzone[]: 直接王手になり得る升(健全な上位集合) */
    int bm[MAXMOVES]; int bn=gen_board_z(p,att,bm,zone,g_disc);                 /* 盤上の王手手 */
    for(int i=0;i<bn;i++){ int mv=bm[i],from=M_FROM(mv),to=M_TO(mv);
      if(!(zone&SQBB[to]) && !g_disc[from]) continue;              /* 直接王手の可能性なし且つ開き王手でない→非王手 */
      int mt=ptype(b[from]); int legal;
      if(mt==KING){ legal=king_move_safe(p,from,to,att); }
      else legal=!g_pin[from]||colinearK(p,K,M_TO(mv),g_pin[from]);
      if(!legal)continue; if(!gives_check_fast(p,mv,att,dk))continue; out[m++]=mv; }
    /* 王手になる打ちだけ直接生成(玉に利く升のみ) */
    const StepAtk*sa=STEP_ATK[att][dk]; int sn=STEP_ATK_N[att][dk];
    for(int i=0;i<sn;i++){ int q=sa[i].q; if(b[q])continue; uint64_t mask=sa[i].mask;
      if((mask&(1ULL<<mkpc(PAWN,att)))&&p->hand[att][PAWN]&&droppable_at(p,att,PAWN,q)){
        /* [S-1] 打歩詰判定を is_mated に (上記 in-check 枝と同じ理由)。 */
        int mv=MV(DROP_FROM,q,0,PAWN); make_move(p,mv); int mated; IS_MATED_KEEPPINS(p,mated); unmake_move(p);
        if(!mated) out[m++]=mv; }
      if((mask&(1ULL<<mkpc(KNIGHT,att)))&&p->hand[att][KNIGHT]&&droppable_at(p,att,KNIGHT,q)) out[m++]=MV(DROP_FROM,q,0,KNIGHT);
      if((mask&(1ULL<<mkpc(SILVER,att)))&&p->hand[att][SILVER]) out[m++]=MV(DROP_FROM,q,0,SILVER);
      if((mask&(1ULL<<mkpc(GOLD,att)))&&p->hand[att][GOLD]) out[m++]=MV(DROP_FROM,q,0,GOLD);
    }
    if(p->hand[att][LANCE]){ int d=LANCE_INDIR[att]; const int*ray=RAY[dk][d]; int len=RAYN[dk][d];
      for(int k=0;k<len;k++){ int q=ray[k]; if(b[q])break; if(droppable_at(p,att,LANCE,q)) out[m++]=MV(DROP_FROM,q,0,LANCE); } }
    if(p->hand[att][ROOK]){ for(int d=0;d<4;d++){ const int*ray=RAY[dk][d]; int len=RAYN[dk][d];
      for(int k=0;k<len;k++){ int q=ray[k]; if(b[q])break; out[m++]=MV(DROP_FROM,q,0,ROOK); } } }
    if(p->hand[att][BISHOP]){ for(int d=4;d<8;d++){ const int*ray=RAY[dk][d]; int len=RAYN[dk][d];
      for(int k=0;k<len;k++){ int q=ray[k]; if(b[q])break; out[m++]=MV(DROP_FROM,q,0,BISHOP); } } }
  }
  if(g_verify){ int pm[MAXMOVES]; int n=gen_pseudo(p,att,pm); int ref[MAXMOVES],rn=0;
    for(int i=0;i<n;i++){ int mv=pm[i],from=M_FROM(mv); TB t=tb_apply(p,mv); int legal=own_safe(p,att); tb_revert(p,t);
      if(!legal)continue; if(!(dk>=0&&gives_check_fast(p,mv,att,dk)))continue;
      if(from==DROP_FROM&&M_DROP(mv)==PAWN){ make_move(p,mv); int any=0;int dm[MAXMOVES];int dn=gen_pseudo(p,def,dm);
        for(int j=0;j<dn;j++){TB t2=tb_apply(p,dm[j]);if(own_safe(p,def))any=1;tb_revert(p,t2);if(any)break;} unmake_move(p); if(!any)continue; }
      ref[rn++]=mv; }
    if(rn!=m){fprintf(stderr,"CHECK COUNT MISMATCH new=%d old=%d incheck=%d\n",m,rn,incheck);abort();}
    for(int i=0;i<rn;i++){int f=0;for(int j=0;j<m;j++)if(out[j]==ref[i]){f=1;break;}if(!f){fprintf(stderr,"CHECK SET MISMATCH mv=%d\n",ref[i]);abort();}} }
  if(g_order_variant && m>1){
    const unsigned offset=unsigned((p->key ^ (uint64_t(g_order_variant)*0x9e3779b97f4a7c15ULL))%m);
    std::rotate(out,out+offset,out+m);
  }
  assert(m<=MAXMOVES); return m;
}
/* 王手している敵駒の升を chk[] に集める(最大2)。is_attacked と同一構造で照合 */
static int find_checkers(Pos*p,int side,int K,int*chk){
  int by=side^1,nc=0;
  const StepAtk*sa=STEP_ATK[by][K]; int n=STEP_ATK_N[by][K];
  (void)sa; (void)n;
  for(ZBB c=p->occc[by]&STEPBB[by][K]; c; c&=c-1){ int q=bb_lsb(c); if((1ULL<<p->b[q])&STEP_REL[by][q-K+19]){ if(nc<2)chk[nc]=q; nc++; } }
  const ZBB occ=p->occc[0]|p->occc[1];
  for(int d=0;d<8;d++){ if(!((d<4?p->slo[by]:p->sld[by])&RAYBB[K][d]))continue; ZBB m=occ&RAYBB[K][d]; int sq=bb_first(m,d); int pc=p->b[sq];
    if(pcolor(pc)==by){ int t=ptype(pc),hit=0;
      if(d<4){ if(t==ROOK||t==DRAGON)hit=1; else if(t==LANCE&&d==LANCE_INDIR[by])hit=1; }
      else { if(t==BISHOP||t==HORSE)hit=1; }
      if(hit){ if(nc<2)chk[nc]=sq; nc++; } } }
  return nc;
}
/* 単独王手駒 C への取り升と、K-C 間の合駒升を tgt[] にマーク */
static void build_targets(int K,int C,char*tgt,int*btw,int*nb){
  tgt[C]=1; *nb=0; int kr=K/9,kc=K%9,cr=C/9,cc=C%9; int dr=cr-kr,dc=cc-kc;
  if(dr==0||dc==0||dr==dc||dr==-dc){ int sr=(dr>0)-(dr<0),sc=(dc>0)-(dc<0);
    int r=kr+sr,c=kc+sc; while(r!=cr||c!=cc){ int s=r*9+c; tgt[s]=1; btw[(*nb)++]=s; r+=sr; c+=sc; } }
}
/* 合法手生成: 王手時は回避手(玉逃げ/取り/合駒)だけ利き判定し is_attacked を激減 */
/* 目標升 T へ動ける side の駒(非玉)を生成(逆引き)。is_attacked と同じ分割で重複なし。
   王手回避の取り/合駒に用い、合法性はピンで判定(利き判定を回避)。 */
static void gen_moves_to(Pos*p,int side,int T,int*out,int*pm,int K){
  int tp=p->b[T]; if(tp && (pcolor(tp)==side||ptype(tp)==KING)) return;
  for(ZBB c=p->occc[side]&STEPBB[side][T]; c; c&=c-1){ int s=bb_lsb(c); int pc=p->b[s];
    int t=ptype(pc); if(t==KING)continue; if(!(STEP_REL[side][s-T+19]&(1ULL<<pc)))continue;
    if(g_pin[s] && !colinearK(p,K,T,g_pin[s]))continue; emit(s,T,t,side,out,pm); }
  const ZBB occ=p->occc[0]|p->occc[1];
  for(int d=0;d<8;d++){ ZBB m=occ&RAYBB[T][d]; if(!m)continue; int s=bb_first(m,d); int pc=p->b[s];
    if(pcolor(pc)==side){ int t=ptype(pc),hit=0;
      if(d<4){ if(t==ROOK||t==DRAGON)hit=1; else if(t==LANCE&&d==LANCE_INDIR[side])hit=1; }
      else { if(t==BISHOP||t==HORSE)hit=1; }
      if(hit && !(g_pin[s] && !colinearK(p,K,T,g_pin[s]))) emit(s,T,t,side,out,pm); } }
}
static int gen_legal(Pos*p,int*out){
  int side=p->side, K=p->ksq[side]; int m=0;
  /* [ZYF-OCC] find_checkers applies exactly is_attacked's tests, so nc>0 <=> in check; one scan serves both. */
  int chk[2]; int nc=K>=0?find_checkers(p,side,K,chk):0; int incheck=(nc>0);
  if(!incheck){
    compute_pins(p,side);
    int pm[MAXMOVES]; int n=gen_pseudo(p,side,pm);
    for(int i=0;i<n;i++){ int mv=pm[i],from=M_FROM(mv),legal;
      if(from==DROP_FROM) legal=1;
      else { int mt=ptype(p->b[from]);
        if(mt==KING){ legal=king_move_safe(p,from,M_TO(mv),side); }
        else legal=!g_pin[from]||colinearK(p,K,M_TO(mv),g_pin[from]); }
      if(legal) out[m++]=mv; }
  } else {
    if(nc==0){ int pm[MAXMOVES]; int n=gen_pseudo(p,side,pm);
      for(int i=0;i<n;i++){ TB t=tb_apply(p,pm[i]); if(own_safe(p,side))out[m++]=pm[i]; tb_revert(p,t);} }
    else { char tgt[81]; memset(tgt,0,81); int btw[16],nb=0; if(nc==1) build_targets(K,chk[0],tgt,btw,&nb);
      compute_pins(p,side);
      { int*st=MV_ST[side][KING][K]; int sn=MV_STN[side][KING][K];          /* 玉の逃げ */
        for(int k=0;k<sn;k++){ int to=st[k]; int tp=p->b[to]; if(tp&&(pcolor(tp)==side||ptype(tp)==KING))continue;
          int mv=MV(K,to,0,0); if(king_move_safe(p,K,to,side))out[m++]=mv; } }
      if(nc==1){ gen_moves_to(p,side,chk[0],out,&m,K);                       /* 王手駒を取る(非玉) */
        for(int j=0;j<nb;j++) gen_moves_to(p,side,btw[j],out,&m,K);          /* 合駒(盤上, 非玉) */
        static const int HT[7]={PAWN,LANCE,KNIGHT,SILVER,GOLD,BISHOP,ROOK};  /* 合駒(打ち, 常に合法) */
        for(int j=0;j<nb;j++){ int s=btw[j];
          for(int hi=0;hi<7;hi++){ int t=HT[hi]; if(p->hand[side][t]==0)continue;
            if(droppable_at(p,side,t,s)) out[m++]=MV(DROP_FROM,s,0,t); } } }
    }
  }
  if(g_verify){ int pm[MAXMOVES]; int n=gen_pseudo(p,side,pm); int ref[MAXMOVES],rn=0;
    for(int i=0;i<n;i++){ TB t=tb_apply(p,pm[i]); if(own_safe(p,side))ref[rn++]=pm[i]; tb_revert(p,t); }
    if(rn!=m){ fprintf(stderr,"LEGAL COUNT MISMATCH new=%d old=%d incheck=%d\n",m,rn,incheck); abort(); }
    for(int i=0;i<rn;i++){ int f=0; for(int j=0;j<m;j++)if(out[j]==ref[i]){f=1;break;} if(!f){ fprintf(stderr,"LEGAL SET MISMATCH mv=%d\n",ref[i]); abort(); } } }
  assert(m<=MAXMOVES); return m;
}
static int has_legal(Pos*p,int side){int pm[MAXMOVES];int n=gen_pseudo(p,side,pm);
  for(int i=0;i<n;i++){ TB t=tb_apply(p,pm[i]); int ok=own_safe(p,side); tb_revert(p,t); if(ok)return 1;} return 0;}
/* [S-1] 「手番側に合法手が無い(=詰んでいる)か」の述語。gen_legal の構造を逐語で保ちつつ、
 * 最初の合法手で即 return 0 する。gen_legal との差は「リストを作らない」ことだけで、
 * 判定は同一。実測で mate_search(g_mfd=3) が runtime の ~70% を占めており、その葉は
 * 例外なく「gen_legal をフル生成して nd==0 だけ見る」形だった (dm[] はどの depth でも
 * 一度も読まれない = 純粋な存在判定にフル生成の代金を払っていた)。
 *
 * 早期脱出で省けるもの (詰みでない局面 = 圧倒的多数):
 *   - 王手駒を取る手 / 合駒 (gen_moves_to) の全生成
 *   - 打ち合駒の 7*nb 二重ループ
 *   - build_targets と compute_pins (その第1文は g_pin の 81 要素クリア)
 * 真の詰み局面 (逃れ無し) では全部歩くので高速化はゼロ — それでよい (数が少ない)。
 *
 * 順序に関する重要事項:
 *  (a) 玉の逃げを compute_pins より**前**に置いてある。玉の逃げの合法性は tb_apply +
 *      own_safe で判定され g_pin を使わないので安全であり、逃げが1つでもあれば
 *      compute_pins/build_targets を丸ごと払わずに済む (これが本命の節約)。
 *      gen_moves_to は g_pin を使うので、その前には必ず compute_pins を通す。
 *  (b) 玉の逃げは**必ず tb_apply して**判定する。玉自身が王手線を塞いでいるため、
 *      玉を置いたまま is_attacked(逃げ先) を見ると飛/香の王手線が隠れて安全と誤判定する。
 *  (c) nc==0 (is_attacked と find_checkers の不整合) の防御的 fallback を逐語で残す。
 *      ここを落とすと false mate になり得る。
 *  (d) nc>=2 (両王手) は玉の逃げのみ。build_targets は単独王手を前提としている。 */
/* [S-1 HAZARD — 必読] is_mated は内部で compute_pins(p,side) を呼び得るため、
 * **thread-local の g_pin を手番側 (= 受け方) 視点で上書きする副作用を持つ**。
 * g_disc は触らない (compute_disc を呼ばない)。
 *
 * 現状の呼び元と、なぜ安全か:
 *  - mate_search_with: g_pin を読まないので無関係。
 *  - gen_checks: 自分の g_pin (攻方視点) を `legal=!g_pin[from]||colinearK(p,K,...)` で
 *    読む。**読みは盤上手ループで完了し、is_mated を呼ぶ打ちループはその後**なので
 *    今日は順序で救われているが、その順序を強制するものは何も無い。壊れた場合の症状は
 *    「非合法手を王手として生成 = **false mate**」= 最悪。よって gen_checks 側の
 *    2 箇所の呼び出しを IS_MATED_KEEPPINS() で包み、順序への依存を断つ。
 *
 * 保存・復元を is_mated 内部でなく呼び元に置く理由: 受け方が王手されている通常ケース
 * (mate_search_with の全呼び出し) では、is_mated は玉の逃げが1つ見つかった時点で
 * compute_pins に到達せず return する。内部に置くと、その最ホットパス
 * (実測 runtime の大半) に無駄な 324B memcpy 2 回が乗り、実測で 1.4-2.7% 遅くなった。
 * 保護が要るのは gen_checks の希少な打歩詰パスだけなので、そこにだけ置く。 */
static int is_mated(Pos*p){
  int side=p->side, K=p->ksq[side];
  /* [ZYF-OCC] find_checkers applies exactly is_attacked's tests, so nc>0 <=> in check; one scan serves both. */
  int chk[2]; int nc=K>=0?find_checkers(p,side,K,chk):0; int incheck=(nc>0);
  if(!incheck){
    compute_pins(p,side);
    int pm[MAXMOVES]; int n=gen_pseudo(p,side,pm);
    for(int i=0;i<n;i++){ int mv=pm[i],from=M_FROM(mv),legal;
      if(from==DROP_FROM) legal=1;
      else { int mt=ptype(p->b[from]);
        if(mt==KING){ legal=king_move_safe(p,from,M_TO(mv),side); }
        else legal=!g_pin[from]||colinearK(p,K,M_TO(mv),g_pin[from]); }
      if(legal) return 0; }
    return 1;
  }
  if(nc==0){ int pm[MAXMOVES]; int n=gen_pseudo(p,side,pm);          /* (c) 防御的 fallback */
    for(int i=0;i<n;i++){ TB t=tb_apply(p,pm[i]); int ok=own_safe(p,side); tb_revert(p,t); if(ok)return 0; }
    return 1; }
  { int*st=MV_ST[side][KING][K]; int sn=MV_STN[side][KING][K];        /* 玉の逃げ (a)(b) */
    for(int k=0;k<sn;k++){ int to=st[k]; int tp=p->b[to]; if(tp&&(pcolor(tp)==side||ptype(tp)==KING))continue;
      if(king_move_safe(p,K,to,side))return 0; } }
  if(nc>=2) return 1;                                                 /* (d) 両王手 */
  { char tgt[81]; memset(tgt,0,81); int btw[16],nb=0; build_targets(K,chk[0],tgt,btw,&nb);
    compute_pins(p,side);
    int tmp[MAXMOVES]; int m;
    m=0; gen_moves_to(p,side,chk[0],tmp,&m,K); if(m) return 0;        /* 王手駒を取る(非玉) */
    for(int j=0;j<nb;j++){ m=0; gen_moves_to(p,side,btw[j],tmp,&m,K); if(m) return 0; }  /* 合駒(盤上) */
    static const int HT[7]={PAWN,LANCE,KNIGHT,SILVER,GOLD,BISHOP,ROOK};
    for(int j=0;j<nb;j++){ int s=btw[j];                              /* 合駒(打ち, 常に合法) */
      for(int hi=0;hi<7;hi++){ int t=HT[hi]; if(p->hand[side][t]==0)continue;
        if(droppable_at(p,side,t,s)) return 0; } }
  }
  return 1;
}
static int use_plus=1;
/* [S-5] move ordering ゲート。**既定 0 = 無効。実測に基づく判断であり、未実装だからではない。**
 *
 * 経緯: 従来 g_order は宣言と NOORDER 代入の 2 箇所しか無く **read がゼロの死にコード**で、
 * NOORDER は無言の no-op だった = 「ordering は効かない」と結論した過去の実験は何も測って
 * いなかった。そこで実配線し、floodgate 実戦詰み 13 局面 @30s で A/B を取った (2026-07-17):
 *
 *   ordering OFF (既定)                        : **12/13** solved, 157.0s, 111K nps
 *   VARIANT C (旧レンジ[1,9]維持・特徴は±1のみ) : 11/13 solved, 156.7s, 115K nps
 *   VARIANT D (特徴なし・純粋にスケールを拡大)  :  5/13 solved, 260.8s, 139K nps
 *   full (スケール拡大 + 特徴)                  :  3/13 solved, 300.7s, 141K nps
 *
 * 結論: **壊しているのは「特徴」ではなく「seed スケール」**。特徴を一切足さず libs*6 に
 * 広げただけの VARIANT D が 12/13 → 5/13 に崩れる。機構は報告書のハザード 2(a) の通りで、
 * 呼び元は TT hit の子を skip し tt_read_ghi の miss 既定は (1,1) なので、**未 seed / 追い出された
 * 子は常に pn=1 = 最良に見える**。seed を広げるほど「実際に評価した子」が「まだ見ていない子」に
 * 常に負け、選択が壊れて thrash する。nps は上がる (141K > 111K) のに解数は 1/4 — nps を
 * 主指標にしてはならないという実測がここでも再現している。
 * 旧レンジを守った VARIANT C ですら OFF に届かない (11/13 < 12/13) ため、この特徴集合に価値は無い。
 *
 * よって既定 OFF。配線と env は残す — 次に誰かが ordering を試すとき、
 * (i) 土台は既にあり (ii) スケールを広げてはならないことが測定済みだと分かるように。
 * `ORDER=1` で有効化 (実験用)。`NOORDER=1` は互換のため受理 (既定と同じ = 無効)。
 *
 * [S-5b 追試 2026-07-18] TT seed を (1,1) のまま保ち、tie-breaking のみで ordering する
 * 手法も試した。seed の非対称 (= ORDER=1 の根因) は解消したが結果は **11/13** (既定 12/13):
 *   ORDER=2 sort (is_attacked 込み)        : 11/13, 181s, 92K nps
 *   ORDER=2 sort (M_TO-玉 距離のみ)        : 11/13, 179s, 157K nps
 *   ORDER=2 tie-break (距離のみ, ソートなし) : 11/13, 178s, 160K nps (同一探索木)
 * 3 手法とも位置 #9 が 1.9M→13M ノードに退化し timeout に転落。距離ヒューリスティックは
 * 一部の位置で改善 (#6, #10, #13) し別の位置で悪化 (#4, #5, #9, #11) する = 位置依存。
 * **df-pn の tie-breaking は静的ヒューリスティックで安定改善できない**。
 * gen_checks の生成順は (偶然を含め) 既に概ね最善。 */
static int g_order=0;
static int g_sort=0;    /* ORDER=2: selection tie-break by distance (既定 OFF, 実測により) */
/* [S-4] df-pn+ の初期推定。**呼び出し時 p は make_move 済みの子局面**で、mv がその手。
 *
 * 誤解しやすい点 (報告書の訂正): 「ordering が無いので argmin は生成順に退化する」は**半分誤り**。
 * estimate は make_move の**後**に呼ばれるので seed は既に事実上 move-dependent であり、
 * OR ノードの argmin は「玉の自由度を最も削る王手」を先に展開している。生成順が効くのは
 * **pn が完全に同値の子の間だけ**。よって真の欠陥は「識別が皆無」ではなく
 *   (i) 分解能が粗い (玉の逃げ場という 1 特徴・0..8 の 9 段階しかない)
 *   (ii) 特徴軸が足りない (支え/取り/近さの情報が無い)
 * の 2 点。ここでは軸を足して分解能を上げる。
 *
 * 数値ハザード (どれか一つ破ると壊れる):
 *  1. **0 を書いてはならない**。first_mate_move は「pn==0 は証明済み詰みのみ (estimate は
 *     0 を書かない)」に依存しており、破ると未証明手を「証明済み」として指す = false mate。
 *  2. **スケールを大きくしてはならない**。(a) 呼び元は TT hit の子を skip し、tt_read_ghi の
 *     miss 既定は (1,1) なので、広いスケールでは「追い出された子」が常に最良に見えて選択が壊れる。
 *     (b) 1+eps 歩行 `thpn<best2+1` は単位粒度を前提としており、粗いスケールでは +1 が no-op 化する。
 *     よって [1, ZY_SEED_MAX=64] にハードクランプする (旧 1..9 から適度に上げるだけ)。
 *  3. g_snda は子の値 > INF/16 を無限扱いするので、その帯に触れてはならない (64 なら遠い)。
 *  4. 反転規約は pn/dn 側で行う (旧実装と同じ)。スコア側で反転すると二重適用になる。
 *
 * g_order=0 (NOORDER=1) で旧実装に**厳密復元**する。これが無いと ordering の A/B が測れない
 * (従来 g_order は read ゼロの死にコードで、NOORDER は無言の no-op だった)。 */
#define ZY_SEED_MAX 64
static const int ZY_PVAL[15]={0,1,2,2,3,4,5,6,0,3,3,3,3,7,8};  /* 駒種の粗い価値。ptype() 経由で引くこと */
static void estimate_mv(Pos*p,int mv,int*epn,int*edn){
  int def=g_attacker^1; int ks=p->ksq[def]; if(ks<0){*epn=1;*edn=1;return;}
  int r=RNK(ks),c=COL(ks),libs=0;
  for(int dr=-1;dr<=1;dr++)for(int dc=-1;dc<=1;dc++){ if(!dr&&!dc)continue; int rr=r+dr,cc=c+dc;
    if(!inb(rr,cc))continue; int t=SQ(rr,cc); int pc=p->b[t];
    if(pc==0||pcolor(pc)==g_attacker){ if(!is_attacked(p,t,g_attacker))libs++; } }
  int e=libs+1;                                   /* 旧実装の値 (1..9) */
  if(g_order && mv>=0){
    /* 玉の自由度を主軸に保ちつつ (支配的), 副次軸で同値を割る。小さいほど「詰みに近い」。*/
    int s=libs*6;                                 /* 0..48 : 主軸 */
    int to=M_TO(mv);
    /* (a) 王手駒が攻方に支えられているか。支えの無い王手は玉に取られて終わりやすい。*/
    if(!is_attacked(p,to,g_attacker)) s+=3;
    /* (b) 取った駒。持駒が増えるほど後続の王手が増えるので詰みに近い。
     *     captured は make_move が積んだ Undo からタダで読める。*/
    { int cap=ustack[usp-1].captured;
      if(cap) { int v=ZY_PVAL[ptype(cap)]; s-= (v>2?2:v); } }
    /* (c) 玉からの距離 (Chebyshev)。遠い王手は緩みやすい。*/
    { int dr2=RNK(to)-RNK(ks), dc2=COL(to)-COL(ks);
      if(dr2<0)dr2=-dr2; if(dc2<0)dc2=-dc2; int d=dr2>dc2?dr2:dc2;
      s += (d>3?3:d); }
    if(s<0)s=0;
    e=s+1;                                        /* >=1 を構造的に保証 (ハザード1) */
    if(e>ZY_SEED_MAX)e=ZY_SEED_MAX;               /* ハザード2 */
  }
  if(p->side==g_attacker){*epn=1;*edn=e;} else {*epn=e;*edn=1;}
}
/* [S-5b] Sort-only quality score. Same features as estimate_mv but:
 * (1) always computes features (independent of g_order),
 * (2) returns a sort key instead of writing pn/dn seeds to TT.
 * Lower = more promising for attacker. Called after make_move. */
static int move_quality(Pos*p,int mv){
  int def=g_attacker^1; int ks=p->ksq[def];
  if(ks<0) return 0;
  int r=RNK(ks),c=COL(ks),libs=0;
  for(int dr=-1;dr<=1;dr++)for(int dc=-1;dc<=1;dc++){ if(!dr&&!dc)continue; int rr=r+dr,cc=c+dc;
    if(!inb(rr,cc))continue; int t=SQ(rr,cc); int pc=p->b[t];
    if(pc==0||pcolor(pc)==g_attacker){ if(!is_attacked(p,t,g_attacker))libs++; } }
  int s=libs*6;
  int to=M_TO(mv);
  if(!is_attacked(p,to,g_attacker)) s+=3;
  { int cap=ustack[usp-1].captured;
    if(cap){ int v=ZY_PVAL[ptype(cap)]; s-=(v>2?2:v); } }
  { int dr2=RNK(to)-RNK(ks),dc2=COL(to)-COL(ks);
    if(dr2<0)dr2=-dr2; if(dc2<0)dc2=-dc2; int d=dr2>dc2?dr2:dc2;
    s+=(d>3?3:d); }
  return s;
}

/* df-pn MID. 返り値は使わない(置換表に格納)。*/
/* ===== 固定深さ mate_search (dlshogi着想, mate3を一般化) =====
   ORノード(攻方手番)から depth 手以内に詰むか。千日手は on_path、打歩詰は gen_checks で整合。 */
#ifndef MFD
#define MFD 3
#endif
static int g_mfd=MFD; static int g_nodelay=0; static int g_snda=1;   /* 固定深さmate探索の深さ(実行時可変, 既定3) */
static int mate_and(Pos*p,int depth);
/* [S-1] 王手リストを呼び元から借りる版。mid は自分で gen_checks(p,moves) した直後に
 * これを呼ぶので、同一局面での二重生成 (旧 mate_search の第1文) を避けられる。
 * cm は値のコピー配列なので、内部の make_move/unmake_move の影響を受けない。 */
static int mate_search_with(Pos*p,int depth,const int*cm,int n){   /* OR: depth は奇数 */
  for(int i=0;i<n;i++){ make_move(p,cm[i]); int r;
    if(on_path(p->key)) r=0;                   /* 攻方が千日手に入る手は不可 */
    else { if(is_mated(p)) r=1;                 /* [S-1] 旧: gen_legal をフル生成して nd==0 だけ見ていた */
      else if(depth<=1) r=0;
      else { g_path[g_pathlen++]=p->key; r=mate_and(p,depth-1); g_pathlen--; } }
    /* [S-2] 証明できた子 (AND節点) を TT に残す。旧実装は「その節点1つ」だけを
     * mid 側で書き、歩いた木を丸ごと捨てていた。mid は mate_search が成功すると
     * 子ループ (estimate/ck[] を埋める箇所) より**前に return する**ため、
     * 3手詰 probe で証明された root の子は誰も TT エントリを持たず、
     * first_mate_move (子に tt_read して pn==0 を探す) が miss 既定の (1,1) を見て
     * **-1 を返す** = 初手が取れない。中盤の短い詰み = MateWorker の主用途で
     * 構造的に壊れていた。ここで子を書けば穴が塞がる。
     * 健全性: r==1 は「is_mated (受け方に合法手なし)」か「mate_and が全逃れ手を
     * 潰した」のいずれかで、どちらも有限深さの強制詰みの証明。on_path は
     * mate_search/mate_and では**棄却にしか使われない** (攻方の千日手手を捨て、
     * 受け方の千日手逃れを「逃れ」に倒す) ので判定は経路非依存であり、
     * mid が同じ値を root について既に tt_store しているのと同じ資格で安全。 */
    if(r){ tt_store(p->key,0,INF); twin_remove(p->key); }   /* [S-2] 証明は無条件: 既存 twin を除去し finalize_ghi と揃える */
    unmake_move(p); if(r) return 1; }
  return 0; }
/* 呼び元が王手リストを持たない場合 (mate_and からの再帰) 用のラッパ。
 * 注: 報告書は「mate_search の唯一の呼び元は mid」としていたが誤りで、
 * mate_and も mate_search を再帰呼び出しする。よってこのラッパは必須。 */
static int mate_search(Pos*p,int depth){
  int cm[MAXMOVES]; int n=gen_checks(p,cm); return mate_search_with(p,depth,cm,n); }
static int mate_and(Pos*p,int depth){          /* AND: p=受け方手番 */
  int dm[MAXMOVES]; int nd=gen_legal(p,dm);
  /* [S-2] ここも証明済み終端 (受け方に合法手なし=詰み) なので TT に残す。mid の
   * `else { nm=gen_legal(...); if(nm==0){tt_store(key,0,INF);...} }` と同一の資格。
   * 注: mate_and の**失敗** (return 0) は深さ制限に依存するので (INF,0) として
   * 書いてはならない — 「深さ3で詰まない」は「詰まない」ではない。書くのは勝ち側だけ。*/
  if(nd==0){ tt_store(p->key,0,INF); twin_remove(p->key); return 1; }  /* [S-2] 既に詰み。twin も除去 */
  if(depth<=0) return 0;
  for(int j=0;j<nd;j++){ make_move(p,dm[j]); int r;
    if(on_path(p->key)) r=0;                    /* 受け方が千日手で逃げた */
    else { g_path[g_pathlen++]=p->key; r=mate_search(p,depth-1); g_pathlen--; }
    /* [S-2] 証明できた OR 節点も残す (mate_search_with の AND 側の store と対称)。
     * これが無いと probe 由来の証明木は AND→OR で鎖が切れる: mid が root(OR) を、
     * mate_search_with が子 C(AND) を、その先の mate_search_with が E(AND) を書く一方で、
     * 間の OR 節点 D だけが未格納になり、TT だけを辿る証明木の走査が成立しない。
     * 健全性は mate_search_with 側と同じ (r==1 は有限深さの強制詰みの証明で経路非依存)。*/
    if(r){ tt_store(p->key,0,INF); twin_remove(p->key); }   /* [S-2] 証明は無条件: twin 除去 */
    unmake_move(p); if(!r) return 0; }          /* 一つでも逃れがあれば不詰 */
  return 1; }
/* 固定深さ探索を試した節点の記録(初回のみ実行) */
/* 3手詰を試した節点の記録(初回のみ実行) */
#define M3BITS 22
#define M3SIZE (1<<M3BITS)
#define M3MASK (M3SIZE-1)
/* m3_key, m3_gen: ZCtx */
static void m3_init(){ if(!m3_key){ ZMALLOC(m3_key,sizeof(uint64_t)*M3SIZE,"m3_key"); ZCALLOC(m3_gen,M3SIZE,sizeof(uint32_t),"m3_gen");} }
static int m3_seen(uint64_t k){ int i=k&M3MASK; for(int j=0;j<6;j++){int x=(i+j)&M3MASK; if(m3_gen[x]==g_gen&&m3_key[x]==k)return 1;} return 0; }
static void m3_add(uint64_t k){ int i=k&M3MASK,slot=-1,fr=-1; for(int j=0;j<6;j++){int x=(i+j)&M3MASK; if(m3_gen[x]==g_gen){if(m3_key[x]==k){slot=x;break;}} else if(fr<0)fr=x;} if(slot<0)slot=(fr>=0?fr:i&M3MASK); m3_gen[slot]=g_gen;m3_key[slot]=k; }
/* [FIX-DEPTH-GUARD] 深さの上限が存在しなかった。g_path[4096] (:373) は :694 で、
 * ustack[4096] (:240) は make_move で、どちらも**境界検査なし**に書かれるため、
 * 十分深い df-pn 降下は静かにメモリを壊す。加えて mid() の 1 フレームは実測 19,074B
 * (moves/ck/repf/dly/gnx/rep_targets) で、2MB スタック予約なら ~108 手で踏み抜く。
 * ここで打ち切れば、残余のオーバーフローは「クラッシュ/沈黙の破壊」から「unknown を
 * 返す」に降格する (longjmp 先は solve の setjmp = 予算切れと同じ扱い)。
 * g_pathlen は再帰深さそのもの (solve が root を push し、各段が 1 個 push する)
 * なので depth 引数は不要で、usp も同深度で動くため 1 個のガードで両方カバーできる。
 * 3900 は g_path[4096] を守る値。スタック側は build.bat の -Wl,--stack で
 * 3900*19KB=74MB を賄う (下記 ZYF_MAX_DEPTH のコメント参照)。 */
#define ZYF_MAX_DEPTH 3900
/* [ZYF-TREE 2026-10-05] Optional per-thread proof export (Cyf_Zyf_Tree_Share): each OR node mid() proves, with the
 * proving move, in the search's key/move domain. Off (null) costs one pointer test per finished OR node. Proofs are
 * unconditional here (finalize_ghi), so a proven subtree of an unknown or disproved root is still a proof. */
static __thread std::vector<std::pair<uint64_t,uint16_t>>* g_yexport=nullptr; static __thread size_t g_yexport_cap=0;
/* [ZYF-TREE-LIVE 2026-10-05] Optional live publication: called with each proven OR node as mid() proves it, so the
 * search can use a proof tree while Zyfamate is still solving (Cyf_Zyf_Tree_Live). */
static __thread void (*g_ylive)(uint64_t,uint16_t)=nullptr;
static void move_usi(int m,char*buf);
#if defined(ZYFAMATE_HOST_YANEURAOU)
static inline uint16_t yo_move16(int m){ char buf[8]={0}; move_usi(m,buf); return YaneuraOu::USIEngine::to_move16(buf).to_u16(); }
#else
static inline uint16_t yo_move16(int){ return 0; }   /* standalone: proof-tree export has no consumer */
#endif
static int mid(Pos*p,int thpn,int thdn,uint64_t*rtout){
  /* [EMBED-PARITY] g_stop を直列 mid() でも honor する。従来これは cyfamate.cpp の
   * 埋め込みコピー側にだけ手作業で当てられていた差分で、Zyfamate.c 本体には無かった
   * (= 逐語コピーではなかった)。埋め込みエンジンは MateWorker::stop_join() が g_stop=1 を
   * 立てて worker を止めるため、これが無いと直列探索を中断できない。
   * SEAL-PARALLEL により Cyf_Mate_Threads>1 も mid_par ではなく mid を通るようになったので、
   * 本体側に取り込んで差分を消す。standalone では g_stop を 1 にするのは pworker の完了時のみで、
   * その後 mid() を呼ぶ経路は無いため無害 (solve_parallel は自前で g_stop=0 してから起動する)。*/
  if((++g_nodes & g_poll_mask)==0){ if(g_stop||g_nodes>g_limit||now()-g_t0>g_tlimit){ g_stop_reason=g_stop?3:(g_nodes>g_limit?1:2); longjmp(g_jmp,1); }
    if(g_usi_mode){ double t=now()-g_t0; if(t-g_usi_last_info>=1.0){
      g_usi_last_info=t; long nps=t>0?(long)(g_nodes/t):0;
      printf("info nodes %ld nps %ld\n",g_nodes,nps); fflush(stdout); } } }
  if(g_pathlen>=ZYF_MAX_DEPTH){ g_stop_reason=4; longjmp(g_jmp,1); }
  uint64_t key=p->key;
  uint64_t A=0,D=0; if(use_superior){ A=hand_pack(p,g_attacker); D=hand_pack(p,g_attacker^1);
    if(sup_mate(p->bkey,A,D)){ tt_store(key,0,INF); twin_remove(key); return 0; }
    if(sup_nomate(p->bkey,A,D)){ tt_store(key,INF,0); twin_remove(key); return 0; } }
  int is_or=(p->side==g_attacker);
  int moves[MAXMOVES]; int nm;
  if(is_or){ nm=gen_checks(p,moves); if(nm==0){tt_store(key,INF,0); twin_remove(key); if(use_superior)sup_rec(nomate_tab,p->bkey,A,D); return 0;}
    /* [S-1] 直前に生成した moves/nm をそのまま渡し、mate_search 内の重複 gen_checks を省く。*/
    if(!m3_seen(key)){ m3_add(key); if(mate_search_with(p,g_mfd,moves,nm)){ tt_store(key,0,INF); twin_remove(key); if(use_superior)sup_rec(proof_tab,p->bkey,A,D); return 0; } } }
  else { nm=gen_legal(p,moves); if(nm==0){tt_store(key,0,INF); twin_remove(key); if(use_superior)sup_rec(proof_tab,p->bkey,A,D); return 0;} }

  uint64_t ck[MAXMOVES]; uint8_t repf[MAXMOVES];
  /* [ZYF-TT-PREFETCH] Issue every child's TT probe before the first one is read so the misses overlap. */
  for(int i=0;i<nm;i++){ ck[i]=child_key(p,moves[i]); tt_prefetch(ck[i]); }
  for(int i=0;i<nm;i++){ make_move(p,moves[i]); assert(ck[i]==p->key); ck[i]=p->key;
    if(use_plus && tt_find(p->key)<0 && twin_find(p->key)<0){int ep,ed;estimate_mv(p,moves[i],&ep,&ed);tt_store(p->key,ep,ed);}
    repf[i]=0; unmake_move(p); }

  uint8_t dly[MAXMOVES]; int gnx[MAXMOVES];                 /* 合駒の遅延展開(KomoringHeights着想) */
  for(int i=0;i<nm;i++){ dly[i]=0; gnx[i]=-1; }
  if(!is_or && !g_nodelay){                          /* 同升合駒は連続生成なのでO(nm)で群化 */
    int head=-1,tail=-1;
    for(int i=0;i<nm;i++){
      if(IS_DROP(moves[i])){
        if(head>=0 && M_TO(moves[i])==M_TO(moves[head])){ gnx[tail]=i; dly[i]=1; tail=i; }
        else { head=i; tail=i; }
      } else head=-1; } }
  uint64_t rep_targets[64]; int rtn=0;   /* この節点の不詰が依存する祖先key集合 */

  for(;;){
    int pn,dn,best=-1,best1=INF,best2=INF;
    /* [FIX-LLP64] 累算は int64_t で行う。Windows(LLP64) の long は 32bit であり
     * (本機 gcc 16.1.0 ucrt64 で sizeof(long)=4 を実測)、`dd>INF?INF:(int)dd` の
     * 飽和ガードは「正の大値」しか捕まえられない — 符号付きオーバーフローは負に
     * 折り返るため、ガードをすり抜けて pn/dn が負になる。負の pn は pn>=thpn を
     * 偽にし、repf 子由来の dn==0 が dn>=thdn も偽にするため脱出条件が発火せず、
     * make_move→on_path→repf[best]=1→continue のループは g_nodes も時計も
     * 触らない (それらの検査は mid() 入口にしかない) = タイムアウト脱出不能の
     * 無限ループになる。既定 g_snda=1 は各項を <=INF/16 に抑えるため 128 子以上
     * 必要で実質到達不能だが (実測の最大和は 2.68e8 で INT32_MAX の 1/8)、
     * NOSNDA=1 では上限が消え INF の子 9 個で溢れる。int64_t なら構造的に不可能。*/
    if(is_or){ pn=INF; int64_t dnsum=0; int dnmax=0; int best_d=99;
      int kr=-1,kc=-1; if(g_sort){int ks=p->ksq[g_attacker^1]; if(ks>=0){kr=RNK(ks);kc=COL(ks);}}
      for(int i=0;i<nm;i++){int cpn,cdn; if(repf[i]){cpn=INF;cdn=0;}else tt_read_ghi(ck[i],&cpn,&cdn);
        if(cpn<pn)pn=cpn;
        if(g_snda && cdn>(INF/16)){ if(cdn>dnmax)dnmax=cdn; } else dnsum+=cdn;
        if(cpn<best1){best2=best1;best1=cpn;best=i;
          if(kr>=0){int to=M_TO(moves[i]),dr=RNK(to)-kr,dc=COL(to)-kc;if(dr<0)dr=-dr;if(dc<0)dc=-dc;best_d=dr>dc?dr:dc;}}
        else if(cpn==best1&&kr>=0){int to=M_TO(moves[i]),dr=RNK(to)-kr,dc=COL(to)-kc;if(dr<0)dr=-dr;if(dc<0)dc=-dc;int d=dr>dc?dr:dc;
          if(d<best_d){best=i;best_d=d;} if(cpn<best2)best2=cpn;}
        else if(cpn<best2)best2=cpn; }
      { int64_t dd=dnsum+dnmax; dn=dd>INF?INF:(int)dd; }
    } else { int act=1;
      int64_t pnsum=0; int pnmax=0;
      while(act){ act=0; pnsum=0;pnmax=0;dn=INF;best=-1;best1=INF;best2=INF;
        for(int i=0;i<nm;i++){ if(dly[i])continue; int cpn,cdn; if(repf[i]){cpn=INF;cdn=0;}else tt_read_ghi(ck[i],&cpn,&cdn);
          if(g_snda && cpn>(INF/16)){ if(cpn>pnmax)pnmax=cpn; } else pnsum+=cpn;
          if(cdn<dn)dn=cdn;
          if(cdn<best1){best2=best1;best1=cdn;best=i;} else if(cdn<best2)best2=cdn;
          if(gnx[i]>=0 && cpn==0 && dly[gnx[i]]){ dly[gnx[i]]=0; act=1; } } }
      { int64_t pp=pnsum+pnmax; pn=pp>INF?INF:(int)pp; }
      int anyd=0; for(int i=0;i<nm;i++) if(dly[i]){anyd=1;break;}
      if(anyd && pn<INF) pn+=1;                       /* 残遅延あり→半無限ループ回避 */
    }
    if(pn==0 && is_or && g_yexport && best>=0 && g_yexport->size()<g_yexport_cap){
      g_yexport->emplace_back(yo_key(p),yo_move16(moves[best]));
      if(g_ylive) g_ylive(g_yexport->back().first,g_yexport->back().second); }
    if(pn>=thpn||dn>=thdn){ uint64_t out[TWK]; int no=finalize_ghi(key,pn,dn,rep_targets,rtn,out);
      if(use_superior){ if(pn==0)sup_rec(proof_tab,p->bkey,A,D);
        else if(dn==0&&no==0)sup_rec(nomate_tab,p->bkey,A,D); }
      for(int i=0;i<no;i++)rtout[i]=out[i]; return no; }
    int ctp,ctd,cpn,cdn;
    if(repf[best]){cpn=INF;cdn=0;}else tt_read_ghi(ck[best],&cpn,&cdn);
    if(is_or){ ctp=(thpn<best2+1?thpn:best2+1); int64_t v=(int64_t)thdn-(dn-cdn); ctd=v>INF?INF:(int)v; }
    else { ctd=(thdn<best2+1?thdn:best2+1); int64_t v=(int64_t)thpn-(pn-cpn); ctp=v>INF?INF:(int)v; }
    make_move(p,moves[best]);
    if(on_path(p->key)){ repf[best]=1; uint64_t a=p->key; int dup=0;
      for(int j=0;j<rtn;j++)if(rep_targets[j]==a){dup=1;break;} if(!dup&&rtn<64)rep_targets[rtn++]=a;
      unmake_move(p); continue; }
    g_path[g_pathlen++]=p->key; uint64_t crt[TWK]; int crn=mid(p,ctp,ctd,crt); g_pathlen--; unmake_move(p);
    for(int i=0;i<crn;i++){ uint64_t a=crt[i]; int dup=0;
      for(int j=0;j<rtn;j++)if(rep_targets[j]==a){dup=1;break;} if(!dup&&rtn<64)rep_targets[rtn++]=a; }
  }
}

/* ===== 並列df-pn(共有TT + virtual proof/disproof number, GPS金子2010の正攻法) ===== */
static pthread_mutex_t g_mtx=PTHREAD_MUTEX_INITIALIZER;
static void ttp_read(uint64_t k,int*pn,int*dn){ pthread_mutex_lock(&g_mtx); tt_read(k,pn,dn); pthread_mutex_unlock(&g_mtx); }
static void ttp_store(uint64_t k,int pn,int dn){ pthread_mutex_lock(&g_mtx); tt_store(k,pn,dn); pthread_mutex_unlock(&g_mtx); }
static int  ttp_has(uint64_t k){ pthread_mutex_lock(&g_mtx); int r=tt_find(k)>=0; pthread_mutex_unlock(&g_mtx); return r; }
/* working-count テーブル(virtual loss): key -> いまその部分木を探索中のスレッド数 */
#define WKSIZE (1<<21)
#define WKMASK (WKSIZE-1)
static uint64_t *wk_key=NULL; static int *wk_cnt; static uint8_t *wk_used;
static void wk_init(){ if(!wk_key){ ZMALLOC(wk_key,8*WKSIZE,"wk_key"); ZMALLOC(wk_cnt,4*WKSIZE,"wk_cnt"); ZMALLOC(wk_used,WKSIZE,"wk_used");} memset(wk_used,0,WKSIZE); }
static void wk_inc(uint64_t k){ pthread_mutex_lock(&g_mtx); int i=k&WKMASK; for(int j=0;j<16;j++){int x=(i+j)&WKMASK;
  if(!wk_used[x]){wk_used[x]=1;wk_key[x]=k;wk_cnt[x]=1;break;} if(wk_key[x]==k){wk_cnt[x]++;break;}} pthread_mutex_unlock(&g_mtx); }
static void wk_dec(uint64_t k){ pthread_mutex_lock(&g_mtx); int i=k&WKMASK; for(int j=0;j<16;j++){int x=(i+j)&WKMASK;
  if(!wk_used[x])break; if(wk_key[x]==k){ if(wk_cnt[x]>0)wk_cnt[x]--; break;}} pthread_mutex_unlock(&g_mtx); }
static int  wk_get(uint64_t k){ pthread_mutex_lock(&g_mtx); int i=k&WKMASK; int r=0; for(int j=0;j<16;j++){int x=(i+j)&WKMASK;
  if(!wk_used[x])break; if(wk_key[x]==k){r=wk_cnt[x];break;}} pthread_mutex_unlock(&g_mtx); return r; }

/* virtual値は子選択の誘導のみに使用。終端/勝敗/閾値判定は実値(real)で行う=健全 */
static void mid_par(Pos*p,int thpn,int thdn,int*outrep){
  if((++g_nodes & std::min(g_poll_mask,4095u))==0){ if(g_stop||g_nodes>g_limit||now()-g_t0>g_tlimit){ g_stop_reason=g_stop?3:(g_nodes>g_limit?1:2); longjmp(g_jmp,1); } }
  if(g_pathlen>=ZYF_MAX_DEPTH){ g_stop_reason=4; longjmp(g_jmp,1); }   /* [FIX-DEPTH-GUARD] mid と同じ */
  uint64_t key=p->key; int rep_here=0;
  int is_or=(p->side==g_attacker);
  int moves[MAXMOVES]; int nm;
  if(is_or){ nm=gen_checks(p,moves); if(nm==0){ttp_store(key,INF,0); *outrep=0; return;} }
  else { nm=gen_legal(p,moves); if(nm==0){ttp_store(key,0,INF); *outrep=0; return;} }
  wk_inc(key);
  uint64_t ck[MAXMOVES]; uint8_t repf[MAXMOVES];
  for(int i=0;i<nm;i++){ make_move(p,moves[i]); ck[i]=p->key;
    if(use_plus && !ttp_has(p->key)){int ep,ed;estimate_mv(p,moves[i],&ep,&ed);ttp_store(p->key,ep,ed);}
    repf[i]=0; unmake_move(p); }
  for(;;){
    int best=-1;
    if(is_or){
      int real_pn=INF,real_dn=0,rp1=INF,rp2=INF,argmin=-1,effbest=INF,bestdn=0;
      for(int i=0;i<nm;i++){ int cpn,cdn,w; if(repf[i]){cpn=INF;cdn=0;w=0;} else {ttp_read(ck[i],&cpn,&cdn); w=wk_get(ck[i]);}
        real_dn+=cdn; if(real_dn>INF)real_dn=INF;
        if(cpn<rp1){rp2=rp1;rp1=cpn;argmin=i;} else if(cpn<rp2)rp2=cpn;
        int eff=cpn+w; if(eff>INF)eff=INF; if(eff<effbest){effbest=eff;best=i;bestdn=cdn;} }
      real_pn=rp1;
      if(real_pn>=thpn||real_dn>=thdn){ ttp_store(key,real_pn,real_dn); wk_dec(key); *outrep=rep_here; return; }
      int om=(best==argmin)?rp2:rp1; int ctp=thpn<om+1?thpn:om+1;
      long v=(long)thdn-(real_dn-bestdn); int ctd=v>INF?INF:(int)v;
      make_move(p,moves[best]);
      if(on_path(p->key)){ repf[best]=1; rep_here=1; unmake_move(p); continue; }
      g_path[g_pathlen++]=p->key; int cr=0; mid_par(p,ctp,ctd,&cr); g_pathlen--; unmake_move(p); rep_here|=cr;
    } else {
      int real_pn=0,real_dn=INF,rd1=INF,rd2=INF,argmin=-1,effbest=INF,bestpn=0;
      for(int i=0;i<nm;i++){ int cpn,cdn,w; if(repf[i]){cpn=INF;cdn=0;w=0;} else {ttp_read(ck[i],&cpn,&cdn); w=wk_get(ck[i]);}
        real_pn+=cpn; if(real_pn>INF)real_pn=INF;
        if(cdn<rd1){rd2=rd1;rd1=cdn;argmin=i;} else if(cdn<rd2)rd2=cdn;
        int eff=cdn+w; if(eff>INF)eff=INF; if(eff<effbest){effbest=eff;best=i;bestpn=cpn;} }
      real_dn=rd1;
      if(real_pn>=thpn||real_dn>=thdn){ ttp_store(key,real_pn,real_dn); wk_dec(key); *outrep=rep_here; return; }
      int om=(best==argmin)?rd2:rd1; int ctd=thdn<om+1?thdn:om+1;
      long v=(long)thpn-(real_pn-bestpn); int ctp=v>INF?INF:(int)v;
      make_move(p,moves[best]);
      if(on_path(p->key)){ repf[best]=1; rep_here=1; unmake_move(p); continue; }
      g_path[g_pathlen++]=p->key; int cr=0; mid_par(p,ctp,ctd,&cr); g_pathlen--; unmake_move(p); rep_here|=cr;
    }
  }
}

static int solve(Pos*p,long nodelimit,double tlimit,long*nodes_out){
  g_attacker=p->side; tt_init(); twin_init(); m3_init(); if(use_superior)sup_init(); g_stop_reason=0;
  g_poll_mask = tlimit <= 0.02 ? 63u : tlimit <= 0.05 ? 255u : tlimit <= 0.25 ? 1023u : 8191u;   /* [TM-FAST 2026-10-08] a <=20 ms solve polls the clock every 64 nodes */
  if(++g_gen==0 || g_gen==PROOF_GEN){ tt_clear_gen(); memset(m3_gen,0,sizeof(uint32_t)*M3SIZE);
    if(proof_tab){sup_clear(proof_tab);sup_clear(nomate_tab);} g_gen=1; } g_nodes=0; g_limit=nodelimit; g_t0=now(); g_tlimit=tlimit; g_pathlen=0;
  int usp_save=usp;
  if(setjmp(g_jmp)){ usp=usp_save; if(nodes_out)*nodes_out=g_nodes; return 2; } /* unknown */
  g_path[g_pathlen++]=p->key;
  { uint64_t rt[16]; mid(p,INF,INF,rt); }
  g_pathlen=0;                 /* 経路をクリア(以降のon_path利用を汚さない) */
  if(nodes_out)*nodes_out=g_nodes;
  int pn,dn; tt_read_ghi(p->key,&pn,&dn);
  if(pn==0)return 1; if(dn==0)return 0; return 2;
}

typedef struct { const char*sfen; long nodes; } PArg;
/* キャストは C++ 埋め込み互換のため (C では不要だが有害でもない)。cyfamate.cpp の
 * 埋め込みコピーは既に手作業で (PArg*) を足しており、ここで合わせると逐語コピーに近づく。*/
static void* pworker(void*arg){ PArg*a=(PArg*)arg; Pos p; parse_sfen(&p,a->sfen); g_nodes=0;
  if(setjmp(g_jmp)){ a->nodes=g_nodes; return NULL; }
  g_pathlen=0; g_path[g_pathlen++]=p.key; int rep=0;
  mid_par(&p,INF,INF,&rep);
  g_stop=1; a->nodes=g_nodes; return NULL; }
static int solve_parallel(const char*sfen,int nthreads,long nl,double tl,long*nodes_out){
  Pos p; if(!parse_sfen(&p,sfen)){ if(nodes_out)*nodes_out=0; return 2; }
  /* [SEAL-PARALLEL] 既定では直列 solve() に委譲する。**この関数の中で封じるのが要点**で、
   * cyfamate.cpp:33496 が `if (nthreads > 1) r = Zyfamate::solve_parallel(...)` と
   * ここを直接呼ぶため (Cyf_Mate_Threads は USI option で 1..64)、main() 側の CLI 引数を
   * 潰すだけでは実機エンジンを保護できない。
   * 根拠 (実測 2026-07-17, 16コア, 合計ノード数/5秒):
   *   threads: 1=1,388,544 / 2=827,392 / 4=827,392 / 8=647,168 / 16=970,752
   *   → 2スレッドで 0.60x、16スレッドでも 0.70x。「スケールしない」ではなく**純損**。
   * 機構: g_mtx が単一グローバルで、ttp_read と wk_get を子選択ループの毎周・子ごとに
   * 2 回取る。クリティカルセクションは数 ns の配列 probe なので mutex 往復に埋没する。
   * さらに mid_par は mid の陳腐化したフォークで、固定深さ mate_search (実測で 12/13 の
   * 主役)・優越関係・GHI/twin を**どれも持たない** → 経路依存の不詰を無条件 nomate として
   * 共有 TT に書く (false nomate)。つまり並列は速度でも健全性でも直列に劣る。
   * よって委譲先は「nthreads=1 の mid_par」ではなく **solve() (= mid)** にする。
   * これにより Cyf_Mate_Threads>1 は「遅くて不健全な並列」から「速くて健全な直列」に化ける。*/
  if(nthreads<=1 || !getenv("ZYFAMATE_ALLOW_PARALLEL")){
    if(nthreads>1) fprintf(stderr,"info string Zyfamate: parallel df-pn is a measured net loss "
        "(0.60x at 2T) and mid_par lacks mate_search/superiority/GHI; using the serial solver. "
        "Set ZYFAMATE_ALLOW_PARALLEL=1 to force the old behaviour.\n");
    return solve(&p,nl,tl,nodes_out);
  }
  g_attacker=p.side; g_limit=nl; g_t0=now(); g_tlimit=tl; g_stop=0;
  /* [FIX-TWIN-NULL] twin_init() を追加。solve_parallel 単独経路では tw_used が NULL の
   * まま can_mate/mate_length が twin_find に到達し得た (twin_find 側でも塞いだが、
   * 表そのものを用意しておく方が将来 mid_par に GHI を移植するときの前提になる)。 */
  int save_sup=use_superior; use_superior=0; g_parallel=1; tt_init(); twin_init(); wk_init();
  if(++g_gen==0 || g_gen==PROOF_GEN){ tt_clear_gen(); g_gen=1; }
  if(nthreads<1)nthreads=1; if(nthreads>64)nthreads=64;
  pthread_t th[64]; PArg a[64];
  for(int t=0;t<nthreads;t++){ a[t].sfen=sfen; a[t].nodes=0; pthread_create(&th[t],NULL,pworker,&a[t]); }
  long tot=0; for(int t=0;t<nthreads;t++){ pthread_join(th[t],NULL); tot+=a[t].nodes; }
  g_parallel=0; use_superior=save_sup; if(nodes_out)*nodes_out=tot;
  int pn,dn; tt_read(p.key,&pn,&dn); return pn==0?1:(dn==0?0:2); }

/* ---- exact mate length (iterative deepening + (key,remaining) memo) ---- */
#define LMBITS 22
#define LMSIZE (1<<LMBITS)
#define LMMASK (LMSIZE-1)
/* lm_key, lm_val, lm_used, lm_nodes, lm_limit, lm_jmp: ZCtx */
/* [FIX-MATELEN-STOP] 1=mate_length (長い/中断可) / 0=print_pv (短い/予算内で完走)。
 * can_mate は両方から使われるので、どちらの文脈かをこれで区別する。 */
/* lm_honor_stop: ZCtx */
/* lm_deadline: ZCtx (0 = no wall-clock limit for mate length / PV formatting) */
static void lm_init(){ if(!lm_key){ ZMALLOC(lm_key,sizeof(uint64_t)*LMSIZE,"lm_key"); ZMALLOC(lm_val,LMSIZE,"lm_val"); ZMALLOC(lm_used,LMSIZE,"lm_used");} memset(lm_used,0,LMSIZE); }
static int lm_get(uint64_t h,int*v){int i=h&LMMASK;for(int j=0;j<6;j++){int x=(i+j)&LMMASK;
  if(!lm_used[x])return 0; if(lm_key[x]==h){*v=lm_val[x];return 1;}}return 0;}
static void lm_put(uint64_t h,int v){int i=h&LMMASK;int slot=-1;for(int j=0;j<6;j++){int x=(i+j)&LMMASK;
  if(!lm_used[x]||lm_key[x]==h){slot=x;break;}} if(slot<0)slot=i; lm_used[slot]=1;lm_key[slot]=h;lm_val[slot]=v;}

/* ===== [PHASE5] TT proof-walk による手数取得 =====
 * 動機 (実測): mate_length は d=1,3,5,... の反復深化で**ゼロから再探索**するため実戦局面では
 * 10 秒でも終わらず、MFD=3 の 12 解のうち 9 件が "mate ?" になっていた。埋め込み側は
 * 予算 0.5 秒しか与えられないので事実上**必ず**失敗し、L=-1 → len=0 → cyfamate が
 * 「score mate 1」と誤報していた (floodgate #4 は実際 ~20手)。
 *
 * 一方 df-pn は既に証明を TT に持っている。[S-2] で mate_search/mate_and が証明した連鎖も
 * 書き戻すようにしたので、**証明済み節点は TT を辿るだけで列挙できる**。再探索は要らない。
 *
 * 意味論 (重要): 返すのは「実際に到達可能な詰み手順の長さ」= **上界**であって、
 * can_mate が返す**厳密最小ではない**。OR 節点で pn==0 の子を 1 つ選んで進むため。
 * 手数を過大申告するのは安全側 (過小申告は「もっと早く詰む」と嘘をつくことになり危険)。
 * 厳密最小が要る用途 (tsume CLI) は従来どおり mate_length を使う。
 *
 * 健全性: 参照するのは pn==0 の節点のみ = 証明済みのみ。estimate は 0 を書かない
 * (first_mate_move が依存している不変条件と同じ)。証明は経路非依存なので memo は key のみで良い。
 * 証明が TT に無い節点 (sup_mate ヒットで子を残していない等) に当たったら -1 を返して
 * 呼び元が mate_length にフォールバックする。 */
#define PW_UNKNOWN 255
static int mate_len_tt(Pos*p,int cap){
  CYF_ZYF_TEST_STAGE(2);
  // Unwind through ordinary returns so each made move is restored on cancellation.
  if(g_stop || (lm_deadline>0 && now()>=lm_deadline)) return -1;
  if(cap<0) return -1;
  /* memo: can_mate の h=key^(K*(rem+1)) と衝突しない別定数を使う (rem>=0 なので K*0 は未使用)。*/
  uint64_t h=p->key ^ 0xD6E8FEB86659FD93ULL;
  int v; if(lm_get(h,&v)) return v==PW_UNKNOWN?-1:v;
  int is_or=(p->side==g_attacker); int res;
  if(!is_or){
    int mv[MAXMOVES]; int n=gen_legal(p,mv);
    if(n==0){ lm_put(h,0); return 0; }              /* 受け方に合法手なし = 既に詰み */
    if(cap==0) return -1;   /* [PHASE5] cap 依存の打ち切りは memo しない (memo は position 鍵のみ) */
    int worst=-1;
    for(int i=0;i<n;i++){ make_move(p,mv[i]); int pn,dn; tt_read(p->key,&pn,&dn);
      int r=(pn==0)?mate_len_tt(p,cap-1):-1; unmake_move(p);
      if(r<0){ lm_put(h,PW_UNKNOWN); return -1; }   /* 一つでも証明が欠けたら辿れない */
      if(r>worst)worst=r; }                          /* 受け方は最長を選ぶ */
    res=1+worst;
  } else {
    if(cap==0) return -1;   /* [PHASE5] cap 依存の打ち切りは memo しない (memo は position 鍵のみ) */
    int mv[MAXMOVES]; int n=gen_checks(p,mv); res=-1;
    for(int i=0;i<n;i++){ make_move(p,mv[i]); int pn,dn; tt_read(p->key,&pn,&dn);
      int r=(pn==0)?mate_len_tt(p,cap-1):-1; unmake_move(p);
      if(r>=0){ res=1+r; break; } }                  /* 攻方は証明済みの子を 1 つ選べば良い */
    if(res<0){ lm_put(h,PW_UNKNOWN); return -1; }
  }
  if(res>=0 && res<PW_UNKNOWN) lm_put(h,res);
  return res;
}
/* 証明済み局面の手数を返す。まず TT proof-walk (O(証明木), 再探索なし) を試し、
 * 証明が TT に残っていなければ従来の反復深化 mate_length にフォールバックする。 */
static int mate_length(Pos*p,int maxd,double budget_s);
static int mate_len_fast(Pos*p,int maxd,double budget_s){
  lm_init(); lm_nodes=0; lm_limit=200000000; lm_honor_stop=1;
  lm_deadline = budget_s>0 ? now()+budget_s : 0;
  int saved=g_pathlen;
  if(!setjmp(lm_jmp)){
    int L=mate_len_tt(p, maxd<99?maxd:99);
    g_pathlen=saved;
    if(L>0) return L;
  } else g_pathlen=saved;
  return mate_length(p,maxd,budget_s);   /* 証明が辿れなかった: 従来法へ */
}
/* [PHASE5] 埋め込み (MateWorker) 用の再探索なし・即答版。
 * TT proof-walk のみ。証明が辿れれば厳密手数、辿れなければ (sup_mate/mate_search 由来の
 * 行き止まり) -1 を返す。呼び元 (cyfamate) は -1 を「詰みだが手数不明/長い」として
 * mate_in(MAX_PLY-1) で扱う — 手数の**過小申告は危険** (より緊急に見える) だが
 * 過大申告は安全なので、不明時は大きい手数に倒すのが正しい。
 * mate_length に落ちないので stop_join のブロック時間を気にせず呼べる (常に O(証明木))。
 * static でよい: 埋め込みでは同一 TU なので Zyfamate::mate_len_proof で参照できる
 * (mate_length 等と同じ。非 static シンボルを増やさない = 埋め込みガイドラインの不変条件)。 */
static int mate_len_proof(Pos*p,int cap,double budget_s=0){
  lm_init(); lm_nodes=0; lm_limit=20000000; lm_honor_stop=1;
  lm_deadline=budget_s>0 ? now()+budget_s : 0;
  int saved=g_pathlen;
  if(setjmp(lm_jmp)){ g_pathlen=saved; return -1; }
  int L=mate_len_tt(p, cap<99?cap:99);
  g_pathlen=saved;
  return L>0?L:-1;
}

/* rem手以内に攻方が詰ませられるか。探索本体(mate_search/mate_and)と同じ千日手
 * セマンティクスで判定: 攻方が経路上の局面に戻る手は進展なし(不可)、受方が経路上に
 * 戻る手は逃れ。*rep_out には「この結果が千日手判定に依存したか」を返す。
 * memoは千日手非依存の結果のみ格納(GHIによる誤キャッシュを回避する保守的方針)。 */
static int can_mate(Pos*p,int rem,int*rep_out){
  CYF_ZYF_TEST_STAGE(4);
  if(lm_honor_stop && (g_stop || (lm_deadline>0 && now()>=lm_deadline))) longjmp(lm_jmp,1);
  *rep_out=0;
  /* [FIX-MATELEN-STOP] 手数測定 (mate_length) だけ g_stop を honor する。
   * 動機: mate_length は中断不能だったため、埋め込み側の stop_join() のブロック時間が
   * そのまま mate_length の予算になっていた。time loss を恐れて MateWorker は予算を
   * 0.5 秒に絞り、その結果**実戦局面ではほぼ必ず手数測定が失敗**して L=-1 → len=0 に潰れ、
   * cyfamate 側が「score mate 1」と誤報していた (実測: floodgate #4 は実際 ~20手)。
   * g_stop を見るようにすれば長い予算を与えても stop_join は即座に戻れる。
   *
   * ただし print_pv には honor させない (lm_honor_stop=0)。stop_join は g_stop=1 を
   * 立ててから join するので、print_pv まで g_stop を見ると「証明済みなのに初手を
   * 再構築できず override が発火しない」= 証明を捨てることになる。print_pv は元々
   * 0.3 秒の予算で回っており、その分の stop 遅延は変更前から存在したもの。 */
  if((++lm_nodes & 16383)==0){
    if((lm_honor_stop&&g_stop)||lm_nodes>lm_limit||(lm_deadline>0&&now()>lm_deadline)) longjmp(lm_jmp,1); }
  uint64_t h=p->key ^ (0x9E3779B97F4A7C15ULL*(uint64_t)(rem+1));
  int v; if(lm_get(h,&v))return v;              /* memoは千日手非依存の結果のみ */
  int is_or=(p->side==g_attacker); int res, subrep=0;
  if(is_or){
    if(rem<=0){lm_put(h,0);return 0;}
    int pn,dn; tt_read(p->key,&pn,&dn); if(dn==0&&pn>=INF&&twin_find(p->key)<0){lm_put(h,0);return 0;}
    int mv[MAXMOVES]; int n=gen_checks(p,mv); res=0;
    for(int i=0;i<n;i++){ make_move(p,mv[i]); int r;
      if(on_path(p->key)){ r=0; *rep_out=1; }    /* 攻方が千日手に入る手は進展なし */
      else { int cr=0; g_path[g_pathlen++]=p->key; r=can_mate(p,rem-1,&cr); g_pathlen--; subrep|=cr; }
      unmake_move(p); if(r){res=1;break;} }
  } else {
    int mv[MAXMOVES]; int n=gen_legal(p,mv); if(n==0){lm_put(h,1);return 1;}
    if(rem<=0){lm_put(h,0);return 0;}
    int pn,dn; tt_read(p->key,&pn,&dn); if(dn==0&&pn>=INF&&twin_find(p->key)<0){lm_put(h,0);return 0;}
    res=1;
    for(int i=0;i<n;i++){ make_move(p,mv[i]); int r;
      if(on_path(p->key)){ r=0; *rep_out=1; }    /* 受方が千日手で逃れる */
      else { int cr=0; g_path[g_pathlen++]=p->key; r=can_mate(p,rem-1,&cr); g_pathlen--; subrep|=cr; }
      unmake_move(p); if(!r){res=0;break;} }
  }
  int rep_involved = (*rep_out) | subrep; *rep_out = rep_involved;
  if(!rep_involved) lm_put(h,res);              /* 千日手非依存のみキャッシュ */
  return res;
}
/* budget_s>0 なら壁時計でも打ち切る (詰み結果を握ったままプロセスが親のtimeoutで
 * 殺され「発見済みの詰みが無音で失われる」事故を構造的に排除するため)。 */
static int mate_length(Pos*p,int maxd,double budget_s){
  lm_init(); lm_nodes=0; lm_limit=200000000; lm_honor_stop=1;
  lm_deadline = budget_s>0 ? now()+budget_s : 0;
  int saved=g_pathlen, savedu=usp; const Pos original=*p;
  if(setjmp(lm_jmp)){ g_pathlen=saved; usp=savedu; *p=original; return -1; }
  for(int d=1;d<=maxd;d+=2){ g_pathlen=0; g_path[g_pathlen++]=p->key; int rep=0;
    if(can_mate(p,d,&rep)){ g_pathlen=saved; return d; } }
  g_pathlen=saved; return -1;
}
static int node_dist(Pos*p,int cap){
  int saved=g_pathlen, r=INF;
  for(int d=0;d<=cap;d++){ g_pathlen=0; g_path[g_pathlen++]=p->key; int rep=0;
    if(can_mate(p,d,&rep)){ r=d; break; } }
  g_pathlen=saved; return r;
}

/* USI move string */
static const char* CSAN[15]={"","FU","KY","KE","GI","KI","KA","HI","OU","TO","NY","NK","NG","UM","RY"};
static void move_csa(Pos*p,int m,int col,char*buf){
  int to=M_TO(m),tf=9-COL(to),tr=RNK(to)+1; char sg=col==BLACK?'+':'-';
  if(IS_DROP(m)) sprintf(buf,"%c00%d%d%s",sg,tf,tr,CSAN[M_DROP(m)]);
  else{ int f=M_FROM(m),ff=9-COL(f),fr=RNK(f)+1,mt=ptype(p->b[f]),nt=M_PROMO(m)?PROMO[mt]:mt;
    sprintf(buf,"%c%d%d%d%d%s",sg,ff,fr,tf,tr,CSAN[nt]); } }
/* USI format: "7g7f" (board move), "7a7b+" (promote), "G*5b" (drop) */
static void move_usi(int m,char*buf){
  int to=M_TO(m); char tf='1'+(8-COL(to)),tr='a'+RNK(to);
  if(IS_DROP(m)){ static const char DP[]="?PLNSGBR";
    int dt=M_DROP(m); buf[0]=(dt>=1&&dt<=7)?DP[dt]:'?'; buf[1]='*'; buf[2]=tf; buf[3]=tr; buf[4]=0; }
  else{ int f=M_FROM(m); buf[0]='1'+(8-COL(f)); buf[1]='a'+RNK(f);
    buf[2]=tf; buf[3]=tr; buf[4]=M_PROMO(m)?'+':0; buf[5]=0; } }
static int parse_usi_move(Pos*p,const char*s){
  if(!s||strlen(s)<4) return -1;
  int mv[MAXMOVES],nm=gen_legal(p,mv);
  for(int i=0;i<nm;i++){ char buf[8]; move_usi(mv[i],buf); if(!strcmp(buf,s)) return mv[i]; }
  return -1; }

/* PV(攻方最短/玉方最長) を CSA表記で out に書く。
 * [FIX] 旧実装は mate_length が返った後も can_mate が失効済み lm_jmp へ longjmp し得る
 * 未定義動作 (lm_nodes が累積で 2 億超 → クラッシュ/沈黙) を抱えていた。print_pv 自身が
 * setjmp を張り、budget_s の壁時計予算内で途中まで構築した部分PVを返す。 */
static void print_pv(Pos*p,int D,char*out,double budget_s,int honor_stop=0){
  CYF_ZYF_TEST_STAGE(3);
  out[0]=0; if(honor_stop && g_stop) return; int cur=D,guard=0; char mb[16];
  lm_init(); lm_nodes=0; lm_limit=200000000; lm_honor_stop=honor_stop; // native callers honor stop; CLI default remains separate
  lm_deadline = budget_s>0 ? now()+budget_s : 0;
  int savedp=g_pathlen, savedu=usp; const Pos original=*p;
  if(setjmp(lm_jmp)){ g_pathlen=savedp; usp=savedu; *p=original; return; }   /* 予算切れ: 部分PVのまま返す */
  while(cur>0 && guard++<2000){
    if((honor_stop && g_stop) || (lm_deadline>0 && now()>=lm_deadline)) break;
    int best=-1,bd;
    if(p->side==g_attacker){ int mv[MAXMOVES];int n=gen_checks(p,mv); bd=INF;
      for(int i=0;i<n;i++){make_move(p,mv[i]);int d=node_dist(p,cur-1);unmake_move(p);if(d<bd){bd=d;best=i;}}
      if(best<0)break; move_csa(p,mv[best],p->side,mb); if(out[0])strcat(out," "); strcat(out,mb); make_move(p,mv[best]); cur=bd;
    } else { int mv[MAXMOVES];int n=gen_legal(p,mv); bd=-1;
      for(int i=0;i<n;i++){make_move(p,mv[i]);int d=node_dist(p,cur-1);unmake_move(p);if(d<INF&&d>bd){bd=d;best=i;}}
      if(best<0)break; move_csa(p,mv[best],p->side,mb); if(out[0])strcat(out," "); strcat(out,mb); make_move(p,mv[best]); cur=bd;
    }
  }
  g_pathlen=savedp; usp=savedu; *p=original;
}

static int csa_type(const char*s){
  static const char*N[15]={"","FU","KY","KE","GI","KI","KA","HI","OU","TO","NY","NK","NG","UM","RY"};
  for(int t=1;t<15;t++) if(s[0]==N[t][0]&&s[1]==N[t][1]) return t; return 0; }
static int parse_csa(Pos*p,const char*text){
  memset(p,0,sizeof *p); p->ksq[0]=p->ksq[1]=-1; p->side=BLACK; int al=-1;
  const char*s=text; char line[256];
  while(*s){ int n=0; while(*s&&*s!='\n'&&n<255)line[n++]=*s++; line[n]=0; if(*s=='\n')s++;
    while(n>0&&line[n-1]=='\r')line[--n]=0;
    if(n>=2&&line[0]=='P'&&line[1]>='1'&&line[1]<='9'){ int r=line[1]-'1';
      for(int i=0;i<9;i++){ int b=2+3*i; if(b+2>=n)break; char c0=line[b];
        if(c0=='+'||c0=='-'){ int col=c0=='+'?BLACK:WHITE; int t=csa_type(&line[b+1]); if(t)p->b[r*9+i]=mkpc(t,col); } } }
    else if(n>=2&&line[0]=='P'&&(line[1]=='+'||line[1]=='-')){ int col=line[1]=='+'?BLACK:WHITE; int b=2;
      while(b+3<n){ if(line[b+2]=='A'&&line[b+3]=='L'){al=col;break;} int t=csa_type(&line[b+2]); if(t>=1&&t<=7)p->hand[col][t]++; b+=4; } }
    else if(n==1&&(line[0]=='+'||line[0]=='-')) p->side=line[0]=='+'?BLACK:WHITE;
  }
  if(al>=0){ int full[8]={0,18,4,4,4,4,2,2},used[8]={0};
    for(int sq=0;sq<81;sq++){int pc=p->b[sq]; if(pc){int t=ptype(pc),base=DEMO[t]; if(base>=1&&base<=7)used[base]++;}}
    for(int c=0;c<2;c++)for(int t=1;t<=7;t++)used[t]+=p->hand[c][t];
    for(int t=1;t<=7;t++){int rest=full[t]-used[t]; if(rest>0)p->hand[al][t]+=rest;} }
  refresh_kings(p); p->key=compute_key(p); p->bkey=compute_bkey(p); ykey_refresh(p); return 1; }
/* 直前の solve() が証明した詰みの初手を置換表から即答で引く (手数測定より遥かに軽い)。
 * 手数測定が予算切れでも初手だけは返せるようにする最終フォールバック。 */
static int first_mate_move(Pos*p,int honor_stop=0){
  if(honor_stop && g_stop) return -1;
  int mv[MAXMOVES]; int n=gen_checks(p,mv); int first=-1;
  for(int i=0;i<n;i++){ if(honor_stop && g_stop) return -1; int pn,dn; make_move(p,mv[i]); tt_read(p->key,&pn,&dn);
    /* A check that leaves no legal reply is mate in 1 (gen_checks already drops pawn-drop mates), so prefer it to
     * the first proven child in generation order, which may be a long mate. */
    int reply[MAXMOVES]; const int mated=gen_legal(p,reply)==0; unmake_move(p);
    if(mated) return mv[i];
    if(pn==0 && first<0) first=mv[i]; }
  if(first>=0) return first;   /* pn==0 は証明済み詰みのみ (estimate は 0 を書かない) */
  return -1; }
/* [予算規律] 全体の壁時計上限 ≒ tl を厳守する。旧実装は solve 後の mate_length
 * (node上限2億・時間無制限) と print_pv が tl を大幅超過し、親プロセス側 timeout で
 * kill され「詰みは証明済みなのに一行も出力されない」最悪の失敗モードがあった。 */
static void report_solve(Pos p,long nl,double tl,int wantpv){
  Pos p0=p; long nodes; double t=now(),deadline=t+tl;
  int r=solve(&p,nl,tl,&nodes); double dt=now()-t;
  if(r==1){
    double rem=deadline-now(); if(rem<0.05)rem=0.05;
    Pos q=p0; g_attacker=q.side; int L=mate_len_fast(&q,399,rem*0.7);
    if(L>0){ printf("mate %d nodes %ld time %.3f\n",L,nodes,dt);
      if(wantpv){ char pv[8192]; Pos q2=p0; g_attacker=q2.side;
        double rem2=deadline-now(); if(rem2<0.05)rem2=0.05;
        print_pv(&q2,L,pv,rem2); printf("%s\n",pv); } }
    else { printf("mate ? nodes %ld time %.3f\n",nodes,dt);   /* 手数不明でも詰み確定 */
      if(wantpv){ Pos q3=p0; g_attacker=q3.side; int fm=first_mate_move(&q3);
        if(fm>=0){ char mb[16]; move_csa(&q3,fm,q3.side,mb); printf("%s\n",mb); } } } }
  else if(r==0) printf("nomate nodes %ld time %.3f\n",nodes,dt);
  else printf("unknown nodes %ld time %.3f\n",nodes,dt); }
static void report_tsumero(Pos p,long nl,double tl){
  int def=p.side;
  if(p.ksq[def]>=0 && is_attacked(&p,p.ksq[def],def^1)){ printf("False (oute/王手)\n"); return; }
  Pos n=p; n.side^=1; n.key^=Z_SIDE; n.bkey^=Z_SIDE; n.ybk^=Y_SIDE;
  double t0=now(),deadline=t0+tl;
  Pos work=n; long nd; int r=solve(&work,nl,tl,&nd);
  if(r==1){ double rem=deadline-now(); if(rem<0.05)rem=0.05;
    Pos q=n; g_attacker=q.side; int L=mate_len_fast(&q,399,rem*0.7);
    if(L>0){ char pv[8192]; Pos q2=n; g_attacker=q2.side;
      double rem2=deadline-now(); if(rem2<0.05)rem2=0.05;
      print_pv(&q2,L,pv,rem2); printf("True (threat %d)\n%s\n",L,pv); }
    else printf("True (threat ?)\n"); }
  else if(r==0) printf("False\n"); else printf("Unknown\n"); }
/* n手すき(詰めろの深さ): 受け方がパスを重ねて攻方が詰みを強制できるまでの最小手数。
   1=詰めろ, 2=パス→攻方が一手で詰めろを作れる, ... cap(maxn)と時間予算で打ち切り。*/
/* g_tes_deadline: ZCtx */
/* [FIX-TSUMERO-COPY] 生の *p を solve() に渡してはならない。solve() の中断ハンドラ
 * (setjmp 先) は usp しか巻き戻さず、mid() が make_move で破壊した *p を復元しない。
 * report_tesuki は tes_each()=overall/4 を各 probe に与えるため予算切れ(r==2)は
 * 例外でなく設計上の通常ケースで、その時 *p は df-pn 木の任意の内部節点のまま残る。
 * 呼び元 tesuki() はその壊れた盤で gen_legal/make_move を続け、usp は釣り合うので
 * 何もトラップしない = tesuki の答えが静かにゴミになる。捨てコピーで探索する。
 * (report_solve は Pos 値渡し、report_tsumero は Pos work=n のコピーで元々安全。) */
static int is_tsumero_p(Pos*p,long nl,double tl){ int d=p->side;
  if(p->ksq[d]>=0 && is_attacked(p,p->ksq[d],d^1)) return 0;
  Pos w=*p; w.side^=1; w.key^=Z_SIDE; w.bkey^=Z_SIDE; w.ybk^=Y_SIDE;
  { double left=g_tes_deadline-now(); if(tl>left) tl=left<0.001?0.001:left; }   /* [TM-FAST 2026-10-08] tes_each() floors a solve at 50 ms, above the whole budget; the deadline was only checked between solves */
  long nd; int r=solve(&w,nl,tl,&nd); return r==1; }
static int defmated(Pos*p){ int d=p->side; return p->ksq[d]>=0 && is_attacked(p,p->ksq[d],d^1) && !has_legal(p,d); }
static int tesuki(Pos*p,int maxn,long nl,double tl){
  if(now()>g_tes_deadline) return -2;
  if(defmated(p)) return 0;
  if(is_tsumero_p(p,nl,tl)) return 1;
  if(maxn<=1) return -1;
  p->side^=1;p->key^=Z_SIDE;p->bkey^=Z_SIDE;p->ybk^=Y_SIDE;
  int mv[MAXMOVES]; int nm=gen_legal(p,mv); int best=-1,unk=0;
  for(int i=0;i<nm;i++){ if(now()>g_tes_deadline){unk=1;break;}   /* 全体予算をループ内でも確認 */
    make_move(p,mv[i]); int t=tesuki(p,maxn-1,nl,tl); unmake_move(p);
    if(t==-2){unk=1;continue;} if(t>=1){int v=1+t; if(best<0||v<best)best=v; if(best==2)break;} }
  p->side^=1;p->key^=Z_SIDE;p->bkey^=Z_SIDE;p->ybk^=Y_SIDE;
  return best>=1?best:(unk?-2:-1); }
static void report_tesuki(Pos p,int maxn,long nl,double tl,double overall){
  int d=p.side;
  if(p.ksq[d]>=0 && is_attacked(&p,p.ksq[d],d^1)){
    if(!has_legal(&p,d)) printf("tesuki 0 (mated)\n"); else printf("tesuki - (in check)\n"); return; }
  g_tes_deadline=now()+overall; int r=tesuki(&p,maxn,nl,tl);
  if(r==0) printf("tesuki 0 (mated)\n");
  else if(r>=1) printf("tesuki %d\n",r);
  else if(r==-1) printf("tesuki >%d (no threat within cap)\n",maxn);
  else printf("tesuki unknown (budget)\n"); }
/* tesuki の全体予算 overall から 1 solve あたりの時間を導出 */
static double tes_each(double overall){ double e=overall/4.0; if(e<0.05)e=0.05; return e; }

/* ===== serve: 常駐モード =====
 * one-shot 起動は「プロセス生成 + 数百MB〜GB級ハッシュ表の確保/ページウォーム」を
 * 1 リクエスト毎に支払う。serve は 1 プロセスで stdin から行単位のリクエストを受け、
 * 表を確保済み・ページ温存のまま連続処理する (呼び出しレイテンシが桁で縮む)。
 * プロトコル (1行=1リクエスト, 応答は従来 CLI と同一書式 + 終端行 "=done"):
 *   solve   <nodes> <time> <sfen...>
 *   tsumero <nodes> <time> <sfen...>
 *   tesuki  <maxn> <nodes> <time> <sfen...>   (time は全体予算)
 *   ping    → pong
 *   quit / EOF → 終了
 * 検索状態は従来通りリクエスト毎に世代更新 (solve() 内 g_gen++) で論理クリアされる。 */
static void serve_loop(void){
  char line[16384];
  while(fgets(line,sizeof line,stdin)){
    size_t L=strlen(line); while(L&&(line[L-1]=='\n'||line[L-1]=='\r'))line[--L]=0;
    if(!L)continue;
    if(!strcmp(line,"quit"))break;
    usp=0; g_pathlen=0;      /* print_pv の make_move は unmake しない — リクエスト毎に必ず巻き戻す */
    if(!strcmp(line,"ping")){ printf("pong\n=done\n"); fflush(stdout); continue; }
    long nl; double tl; int off=-1, maxn;
    if(sscanf(line,"solve %ld %lf %n",&nl,&tl,&off)==2&&off>0&&line[off]){
      Pos p; if(!parse_sfen(&p,line+off))printf("parse error\n"); else report_solve(p,nl,tl,1); }
    else if(sscanf(line,"tsumero %ld %lf %n",&nl,&tl,&off)==2&&off>0&&line[off]){
      Pos p; if(!parse_sfen(&p,line+off))printf("parse error\n"); else report_tsumero(p,nl,tl); }
    else if(sscanf(line,"tesuki %d %ld %lf %n",&maxn,&nl,&tl,&off)==3&&off>0&&line[off]){
      Pos p; if(!parse_sfen(&p,line+off))printf("parse error\n"); else report_tesuki(p,maxn,nl,tes_each(tl),tl); }
    else printf("error unknown command\n");
    printf("=done\n"); fflush(stdout);
  }
}

/* ===== USI protocol (ShogiGUI / ShogiHome 登録用) =====
 * 詰将棋専用エンジン。go infinite / go mate infinite で詰み探索を開始し、
 * stop で bestmove を返す。通常の go (depth/movetime 等) も go infinite 扱い。 */
static Pos g_usi_pos;
static int g_usi_has_pos=0;
static volatile int g_usi_searching=0;
static volatile int g_usi_done=0;
static int g_usi_gomate=0;
static pthread_t g_usi_thread;

typedef struct { long nodes; int result; int mate_len; int bestmove; } UsiResult;
static UsiResult g_usi_result;

/* print_pv のUSI版: proof tree を辿って USI 形式の手順を stdout に出力 */
static void print_pv_usi(Pos*p,int D,char*out,double budget_s){
  out[0]=0; int cur=D,guard=0; char mb[8];
  lm_init(); lm_nodes=0; lm_limit=200000000; lm_honor_stop=0;
  lm_deadline = budget_s>0 ? now()+budget_s : 0;
  int savedp=g_pathlen;
  if(setjmp(lm_jmp)){ g_pathlen=savedp; return; }
  while(cur>0 && guard++<2000){ int best=-1,bd;
    if(p->side==g_attacker){ int mv[MAXMOVES];int n=gen_checks(p,mv); bd=INF;
      for(int i=0;i<n;i++){make_move(p,mv[i]);int d=node_dist(p,cur-1);unmake_move(p);if(d<bd){bd=d;best=i;}}
      if(best<0)break; move_usi(mv[best],mb); if(out[0])strcat(out," "); strcat(out,mb); make_move(p,mv[best]); cur=bd;
    } else { int mv[MAXMOVES];int n=gen_legal(p,mv); bd=-1;
      for(int i=0;i<n;i++){make_move(p,mv[i]);int d=node_dist(p,cur-1);unmake_move(p);if(d<INF&&d>bd){bd=d;best=i;}}
      if(best<0)break; move_usi(mv[best],mb); if(out[0])strcat(out," "); strcat(out,mb); make_move(p,mv[best]); cur=bd;
    }
  }
  g_pathlen=savedp;
}

static void usi_emit_checkmate(void){
  if(g_usi_result.result==1 && g_usi_result.bestmove>=0){
    int L=g_usi_result.mate_len;
    if(L>0){ char pv[8192]; Pos q=g_usi_pos; g_attacker=q.side;
      print_pv_usi(&q,L,pv,5.0);
      if(pv[0]) printf("checkmate %s\n",pv);
      else { char mb[8]; move_usi(g_usi_result.bestmove,mb); printf("checkmate %s\n",mb); }
    } else { char mb[8]; move_usi(g_usi_result.bestmove,mb); printf("checkmate %s\n",mb); }
  } else if(g_usi_result.result==0){
    printf("checkmate nomate\n");
  } else {
    printf("checkmate timeout\n");
  }
  fflush(stdout);
}

static double g_usi_tlimit=1e9;
static void* usi_solver(void*arg){
  (void)arg;
  Pos p=g_usi_pos;
  long nodes; double tl=g_usi_tlimit;
  long nl=1000000000L;
  g_usi_result.result=solve(&p,nl,tl,&nodes);
  g_usi_result.nodes=nodes;
  if(g_usi_result.result==1){
    Pos q=g_usi_pos; g_attacker=q.side;
    g_usi_result.mate_len=mate_len_fast(&q,399,5.0);
    Pos q2=g_usi_pos; g_attacker=q2.side;
    g_usi_result.bestmove=first_mate_move(&q2);
  } else { g_usi_result.mate_len=-1; g_usi_result.bestmove=-1; }
  g_usi_done=1;
  if(g_usi_gomate) usi_emit_checkmate();
  g_usi_searching=0;
  return NULL;
}

static void usi_loop(void){
  g_usi_mode=1;
  char line[16384];
  while(fgets(line,sizeof line,stdin)){
    size_t L=strlen(line); while(L&&(line[L-1]=='\n'||line[L-1]=='\r'))line[--L]=0;
    if(!L)continue;

    if(!strcmp(line,"usi")){
      printf("id name Zyfamate\n");
      printf("id author yfamate\n");
      printf("option name USI_Hash type spin default 256 min 16 max 4096\n");
      printf("usiok\n"); fflush(stdout); continue; }

    if(!strcmp(line,"isready")){
      printf("readyok\n"); fflush(stdout); continue; }

    if(!strcmp(line,"usinewgame")){ continue; }

    if(!strncmp(line,"setoption ",10)){
      char name[256]="",val[256]="";
      if(sscanf(line,"setoption name %255s value %255s",name,val)==2){
        if(!strcmp(name,"USI_Hash")){
          int mb=atoi(val); if(mb>=16){
#ifdef _WIN32
            char ev[64]; snprintf(ev,sizeof ev,"ZYFAMATE_HASH_MB=%s",val); _putenv(ev);
#else
            setenv("ZYFAMATE_HASH_MB",val,1);
#endif
          }
        }
      }
      continue; }

    if(!strncmp(line,"position",8)){
      const char*s=line+8; while(*s==' ')s++;
      if(!strncmp(s,"startpos",8)){
        parse_sfen(&g_usi_pos,"lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
        s+=8; while(*s==' ')s++;
      } else if(!strncmp(s,"sfen ",5)){
        s+=5;
        char sfen_buf[1024]; int si=0;
        int fields=0;
        while(*s && fields<4){ if(*s==' '){ fields++; if(fields<4){sfen_buf[si++]=*s++;continue;} else break; }
          sfen_buf[si++]=*s++; }
        sfen_buf[si]=0;
        if(!parse_sfen(&g_usi_pos,sfen_buf)){ g_usi_has_pos=0; continue; }
        while(*s==' ')s++;
      }
      if(!strncmp(s,"moves",5)){
        s+=5; while(*s==' ')s++;
        char tok[16]; int ti;
        while(*s){
          ti=0; while(*s&&*s!=' '&&ti<15)tok[ti++]=*s++; tok[ti]=0;
          while(*s==' ')s++;
          if(!ti)continue;
          int m=parse_usi_move(&g_usi_pos,tok);
          if(m<0)break;
          make_move(&g_usi_pos,m);
          g_usi_pos.key=compute_key(&g_usi_pos);
          g_usi_pos.bkey=compute_bkey(&g_usi_pos); ykey_refresh(&g_usi_pos);
        }
      }
      g_usi_has_pos=1;
      continue; }

    if(!strncmp(line,"go",2)){
      if(!g_usi_has_pos){ printf("bestmove resign\n"); fflush(stdout); continue; }
      g_usi_gomate=!!strstr(line,"mate");
      /* `go mate <ms>`: answer within <ms> (USI). `go mate infinite` / no number: until stop.
       * Earlier the limit was ignored (tl=1e9), so a caller that trusted the protocol and waited
       * for the answer instead of sending stop blocked until the proof finished. */
      g_usi_tlimit=1e9;
      if(g_usi_gomate){ const char*mp=strstr(line,"mate")+4; while(*mp==' ')mp++;
        if(*mp>='0'&&*mp<='9'){ double ms=atof(mp); if(ms>0) g_usi_tlimit=ms/1000.0; } }
      g_stop=0; g_usi_searching=1; g_usi_done=0;
      usp=0; g_pathlen=0;
      pthread_create(&g_usi_thread,NULL,usi_solver,NULL);
      continue; }

    if(!strcmp(line,"stop")){
      if(g_usi_searching){ g_stop=1; pthread_join(g_usi_thread,NULL); g_usi_searching=0; }
      if(g_usi_gomate){
        if(!g_usi_done) usi_emit_checkmate();
      } else {
        if(g_usi_result.result==1 && g_usi_result.bestmove>=0){
          char mb[8]; move_usi(g_usi_result.bestmove,mb);
          if(g_usi_result.mate_len>0)
            printf("info score mate %d\n",g_usi_result.mate_len);
          printf("bestmove %s\n",mb);
        } else { printf("bestmove resign\n"); }
      }
      fflush(stdout); continue; }

    if(!strcmp(line,"quit")){
      if(g_usi_searching){ g_stop=1; pthread_join(g_usi_thread,NULL); }
      break; }

    if(!strcmp(line,"gameover")||!strncmp(line,"gameover ",9)){ continue; }
  }
}
#undef malloc
#undef calloc
} // namespace Zyfamate

// Zyfamate が定義したマクロを全て解除 (自動生成: 38 個)。
// BLACK/WHITE/MV 等は cyfamate の Color/Move を破壊するため取りこぼし厳禁。
#undef ADD
#undef BLACK
#undef DROP_FROM
#undef FILL
#undef INF
#undef IS_DROP
#undef IS_MATED_KEEPPINS
#undef LMBITS
#undef LMMASK
#undef LMSIZE
#undef M3BITS
#undef M3MASK
#undef M3SIZE
#undef MAXMOVES
#undef MFD
#undef MV
#undef M_DROP
#undef M_FROM
#undef M_PROMO
#undef M_TO
#undef PW_UNKNOWN
#undef STBITS
#undef STMASK
#undef STPROBE
#undef STSIZE
#undef TTMASK
#undef TTSIZE
#undef TWK
#undef TWMASK
#undef TWSIZE
#undef WHITE
#undef WKMASK
#undef WKSIZE
#undef WOFF
#undef ZCALLOC
#undef ZMALLOC
#undef ZYF_MAX_DEPTH
#undef ZY_SEED_MAX
#pragma pop_macro("SQ")
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC pop_options
#endif
