#ifndef _BITBOARD_H_
#define _BITBOARD_H_

#include "types.h"


namespace Bitboards { void init(); }

#if defined (__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif


struct alignas(16) Bitboard
{
#if defined (USE_SSE2)
	union
	{
		u64 p[2];

		__m128i m;
	};
#else 
	u64 p[2];
#endif

#if defined (USE_SSE2)

	Bitboard& operator = (const Bitboard& rhs) { _mm_store_si128(&this->m, rhs.m); return *this; }

	Bitboard(const Bitboard& bb) { _mm_store_si128(&this->m, bb.m); }
#endif


	Bitboard() {}

	Bitboard(u64 p0, u64 p1);
	
	Bitboard(Square sq);
  
	void set(u64 p0, u64 p1);


	operator bool() const;

  
	bool test(Bitboard rhs) const;
	bool test(Square sq) const { return test(Bitboard(sq)); }

	template <int n> u64 extract64() const;

	template <int n> Bitboard& insert64(u64 u);

	u64 merge() const { return extract64<0>() | extract64<1>(); }

	bool cross_over() const { return extract64<0>() & extract64<1>(); }

	constexpr static int part(Square sq) { return static_cast<int>(SQ_79 < sq); }


	FORCE_INLINE Square pop();

	FORCE_INLINE Square pop_c() const { u64 q0 = extract64<0>();  return (q0 != 0) ? Square(LSB64(q0)) : Square(LSB64(extract64<1>()) + 63); }

	FORCE_INLINE Square pop_from_p0() { u64 q0 = extract64<0>(); ASSERT_LV3(q0 != 0);  Square sq = Square(pop_lsb(q0)); insert64<0>(q0); return sq; }
	FORCE_INLINE Square pop_from_p1() { u64 q1 = extract64<1>(); ASSERT_LV3(q1 != 0);  Square sq = Square(pop_lsb(q1) + 63); insert64<1>(q1); return sq; }

	int pop_count() const { return (int)(POPCNT64(extract64<0>()) + POPCNT64(extract64<1>())); }


#if defined (USE_SSE2)
	Bitboard& operator |= (const Bitboard& b1) { this->m = _mm_or_si128( m, b1.m); return *this; }
	Bitboard& operator &= (const Bitboard& b1) { this->m = _mm_and_si128(m, b1.m); return *this; }
	Bitboard& operator ^= (const Bitboard& b1) { this->m = _mm_xor_si128(m, b1.m); return *this; }
	Bitboard& operator += (const Bitboard& b1) { this->m = _mm_add_epi64(m, b1.m); return *this; }
	Bitboard& operator -= (const Bitboard& b1) { this->m = _mm_sub_epi64(m, b1.m); return *this; }

	Bitboard& operator <<= (int shift) {  m = _mm_slli_epi64(m, shift); return *this; }

	Bitboard& operator >>= (int shift) {  m = _mm_srli_epi64(m, shift); return *this; }

#else
	Bitboard& operator |= (const Bitboard& b1) { this->p[0] |= b1.p[0]; this->p[1] |= b1.p[1]; return *this; }
	Bitboard& operator &= (const Bitboard& b1) { this->p[0] &= b1.p[0]; this->p[1] &= b1.p[1]; return *this; }
	Bitboard& operator ^= (const Bitboard& b1) { this->p[0] ^= b1.p[0]; this->p[1] ^= b1.p[1]; return *this; }
	Bitboard& operator += (const Bitboard& b1) { this->p[0] += b1.p[0]; this->p[1] += b1.p[1]; return *this; }
	Bitboard& operator -= (const Bitboard& b1) { this->p[0] -= b1.p[0]; this->p[1] -= b1.p[1]; return *this; }

	Bitboard& operator <<= (int shift) {  this->p[0] <<= shift; this->p[1] <<= shift; return *this; }
	Bitboard& operator >>= (int shift) {  this->p[0] >>= shift; this->p[1] >>= shift; return *this; }
#endif


	bool operator == (const Bitboard& rhs) const;
	bool operator != (const Bitboard& rhs) const { return !(*this == rhs); }


	Bitboard operator & (const Bitboard& rhs) const { return Bitboard(*this) &= rhs; }
	Bitboard operator | (const Bitboard& rhs) const { return Bitboard(*this) |= rhs; }
	Bitboard operator ^ (const Bitboard& rhs) const { return Bitboard(*this) ^= rhs; }
	Bitboard operator + (const Bitboard& rhs) const { return Bitboard(*this) += rhs; }
	Bitboard operator << (const int i) const { return Bitboard(*this) <<= i; }
	Bitboard operator >> (const int i) const { return Bitboard(*this) >>= i; }

	Square operator*() { return pop(); }
	void operator++() {}

	template <typename T> FORCE_INLINE void foreach(T t) const
	{
		u64 p0 = this->extract64<0>();
		while (p0) t( (Square)pop_lsb(p0));

		u64 p1 = this->extract64<1>();
		while (p1) t( (Square)(pop_lsb(p1) + 63));
	}

	template <typename T> FORCE_INLINE void foreach_part(T t) const
	{
		u64 p0 = this->extract64<0>();
		while (p0) { t( (Square)(pop_lsb(p0)     ),0); }

		u64 p1 = this->extract64<1>();
		while (p1) { t( (Square)(pop_lsb(p1) + 63),1); }
	}

};

#if defined (__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic warning "-Wmaybe-uninitialized"
#endif


inline Bitboard::Bitboard(u64 p0, u64 p1) :
#if defined(USE_SSE2)
	m( _mm_set_epi64x(p1,p0))
#else
	p { p0 , p1 }
#endif
{}

inline void Bitboard::set(u64 p0, u64 p1)
{
#if defined(USE_SSE2)
	m = _mm_set_epi64x(p1,p0);
#else
	p[0] = p0; p[1] = p1;
#endif
}



inline Bitboard::operator bool() const
{
#if defined(USE_SSE41)
	return !_mm_testz_si128(m, m);
#else
	return (this->merge() ? true : false);
#endif
}

inline bool Bitboard::test(Bitboard rhs) const
{
#if defined(USE_SSE41)
	return !_mm_testz_si128(m, rhs.m);
#else
	return (*this & rhs);
#endif
}

FORCE_INLINE Square Bitboard::pop()
{
	u64 q0 = extract64<0>();  Square sq;
	if (q0 != 0) { sq = Square(pop_lsb(q0)); insert64<0>(q0); }
	else { u64 q1 = extract64<1>();  sq = Square(pop_lsb(q1) + 63); insert64<1>(q1); }
	return sq;
}

template <int n>
inline u64 Bitboard::extract64() const
{
	static_assert(n == 0 || n == 1, "");
#if defined(USE_SSE41)
	return (u64)(_mm_extract_epi64(m, n));
#else
	return p[n];
#endif
}

template <int n>
inline Bitboard& Bitboard::insert64(u64 u)
{
	static_assert(n == 0 || n == 1, "");
#if defined(USE_SSE41)
	m = _mm_insert_epi64(m, u, n);
#else
	p[n] = u;
#endif
	return *this;
}

inline bool Bitboard::operator == (const Bitboard& rhs) const
{
#if defined (USE_SSE41)
	__m128i neq = _mm_xor_si128(this->m, rhs.m);
	return _mm_test_all_zeros(neq, neq) ? true : false;
#else
	return (this->p[0] == rhs.p[0]) && (this->p[1] == rhs.p[1]);
#endif
}


extern Bitboard SquareBB[SQ_NB_PLUS1];
inline Bitboard::Bitboard(Square sq) { *this = SquareBB[sq]; }

extern Bitboard ALL_BB;

extern Bitboard ZERO_BB;

inline Bitboard operator|(const Bitboard& b, Square s) { return b | SquareBB[s]; }
inline Bitboard operator&(const Bitboard& b, Square s) { return b & SquareBB[s]; }
inline Bitboard operator^(const Bitboard& b, Square s) { return b ^ SquareBB[s]; }

inline Bitboard operator ~ (const Bitboard& a) { return a ^ ALL_BB; }

inline const Bitboard begin(const Bitboard& b) { return b; }
inline const Bitboard end(const Bitboard&) { return ZERO_BB; }

std::ostream& operator<<(std::ostream& os, const Bitboard& board);


extern Bitboard FILE1_BB;
extern Bitboard FILE2_BB;
extern Bitboard FILE3_BB;
extern Bitboard FILE4_BB;
extern Bitboard FILE5_BB;
extern Bitboard FILE6_BB;
extern Bitboard FILE7_BB;
extern Bitboard FILE8_BB;
extern Bitboard FILE9_BB;

extern Bitboard RANK1_BB;
extern Bitboard RANK2_BB;
extern Bitboard RANK3_BB;
extern Bitboard RANK4_BB;
extern Bitboard RANK5_BB;
extern Bitboard RANK6_BB;
extern Bitboard RANK7_BB;
extern Bitboard RANK8_BB;
extern Bitboard RANK9_BB;

extern Bitboard FILE_BB[FILE_NB];

extern Bitboard RANK_BB[RANK_NB];

extern Bitboard ForwardRanksBB[COLOR_NB][RANK_NB];

inline const Bitboard rank1_n_bb(Color US, const Rank r) { ASSERT_LV2(is_ok(r));  return ForwardRanksBB[US][(US == BLACK ? r + 1 : 7 - r)]; }

extern Bitboard EnemyField[COLOR_NB];
inline const Bitboard enemy_field(Color Us) { return EnemyField[Us]; }

extern u64 PAWN_DROP_MASKS[SQ_NB];

extern Bitboard BetweenBB[785];
extern u16 BetweenIndex[SQ_NB_PLUS1][SQ_NB_PLUS1];

inline const Bitboard between_bb(Square sq1, Square sq2) { return BetweenBB[BetweenIndex[sq1][sq2]]; }

extern Bitboard LineBB[SQ_NB][4];

inline const Bitboard line_bb(Square sq1, Square sq2)
{
	static_assert(Effect8::DIRECT_RU == 0 && Effect8::DIRECT_LD == 7 , "");
	auto directions = Effect8::directions_of(sq1, sq2);
	ASSERT_LV3(directions != 0);
	static const int a[8] = { 0 , 1 , 2 , 3 , 3 , 2 , 1 , 0 };
	return LineBB[sq1][a[(int)Effect8::pop_directions(directions)]];
}

#if 0
inline bool aligned(Square s1, Square s2, Square s3) {
	return LineBB[s1][s2] & s3;
}
#endif

extern Bitboard CheckCandidateBB[SQ_NB_PLUS1][KING-1][COLOR_NB];
extern Bitboard CheckCandidateKingBB[SQ_NB_PLUS1];

inline const Bitboard check_candidate_bb(Color us, PieceType pr, Square sq)
{
	ASSERT_LV3(PAWN<= pr && pr < KING && sq <= SQ_NB && is_ok(us));
	return CheckCandidateBB[sq][pr - 1][us];
}

inline const Bitboard around24_bb(Square sq)
{
	ASSERT_LV3(sq <= SQ_NB);
	return CheckCandidateKingBB[sq];
}





extern Bitboard KingEffectBB[SQ_NB_PLUS1];
extern Bitboard GoldEffectBB[SQ_NB_PLUS1][COLOR_NB];
extern Bitboard SilverEffectBB[SQ_NB_PLUS1][COLOR_NB];
extern Bitboard KnightEffectBB[SQ_NB_PLUS1][COLOR_NB];
extern Bitboard PawnEffectBB[SQ_NB_PLUS1][COLOR_NB];

extern Bitboard LanceStepEffectBB[SQ_NB_PLUS1][COLOR_NB];
extern Bitboard BishopStepEffectBB[SQ_NB_PLUS1];
extern Bitboard RookStepEffectBB[SQ_NB_PLUS1];


extern u8		Slide[SQ_NB_PLUS1];
extern u64      RookFileEffect[RANK_NB + 1][128];

#if defined(USE_OLD_YANEURAOU_EFFECT)



extern Bitboard BishopEffect[2][1856+1];
extern Bitboard BishopEffectMask[2][SQ_NB_PLUS1];
extern int		BishopEffectIndex[2][SQ_NB_PLUS1];


extern Bitboard RookRankEffect[FILE_NB + 1][128];

#else 





extern Bitboard BishopAttack[20224 + 1 ];
extern int BishopAttackIndex[SQ_NB_PLUS1];
extern Bitboard BishopBlockMask[SQ_NB_PLUS1];

extern const int BishopBlockBits[SQ_NB_PLUS1];
extern const int BishopShiftBits[SQ_NB_PLUS1];


extern const int RookBlockBits[SQ_NB_PLUS1];
extern const int RookShiftBits[SQ_NB_PLUS1];

#if defined (USE_BMI2)
extern Bitboard RookAttack[495616 + 1 ];
#else
extern Bitboard RookAttack[512000 + 1 ];
#endif

extern int RookAttackIndex[SQ_NB_PLUS1];
extern Bitboard RookBlockMask[SQ_NB_PLUS1];


#if defined (USE_BMI2)
#else
extern const u64 RookMagic[SQ_NB_PLUS1];
extern const u64 BishopMagic[SQ_NB_PLUS1];
#endif

#endif 



inline Bitboard kingEffect(const Square sq) { ASSERT_LV3(sq <= SQ_NB); return KingEffectBB[sq]; }

inline Bitboard pawnEffect(const Color c, const Square sq)
{
	ASSERT_LV3(is_ok(c) && sq <= SQ_NB);
	return PawnEffectBB[sq][c];
}

inline Bitboard pawnEffect(const Color c, const Bitboard bb)
{
	ASSERT_LV3(is_ok(c));
	return
		c == BLACK ? (bb >> 1) :
		c == WHITE ? (bb << 1) :
		ZERO_BB;
}

inline Bitboard knightEffect(const Color c, const Square sq)
{
	ASSERT_LV3(is_ok(c) && sq <= SQ_NB);
	return KnightEffectBB[sq][c];
}

inline Bitboard silverEffect(const Color c, const Square sq) { ASSERT_LV3(is_ok(c) && sq <= SQ_NB); return SilverEffectBB[sq][c]; }

inline Bitboard goldEffect(const Color c, const Square sq) { ASSERT_LV3(is_ok(c) && sq <= SQ_NB); return GoldEffectBB[sq][c]; }


inline Bitboard bishopStepEffect(Square sq) { ASSERT_LV3(sq <= SQ_NB); return BishopStepEffectBB[sq]; }

inline Bitboard rookStepEffect(Square sq) { ASSERT_LV3(sq <= SQ_NB); return RookStepEffectBB[sq]; }

inline Bitboard lanceStepEffect(Color c, Square sq) { ASSERT_LV3(is_ok(c) && sq <= SQ_NB); return LanceStepEffectBB[sq][c]; }

inline Bitboard queenStepEffect(Square sq) { ASSERT_LV3(sq <= SQ_NB); return rookStepEffect(sq) | bishopStepEffect(sq); }

inline Bitboard cross00StepEffect(Square sq) { ASSERT_LV3(sq <= SQ_NB); return rookStepEffect(sq) & kingEffect(sq); }

inline Bitboard cross45StepEffect(Square sq) { ASSERT_LV3(sq <= SQ_NB); return bishopStepEffect(sq) & kingEffect(sq); }


inline Bitboard rookFileEffect(Square sq, const Bitboard& occupied)
{
	ASSERT_LV3(sq <= SQ_NB);
	const int index = (occupied.p[Bitboard::part(sq)] >> Slide[sq]) & 0x7f;
	File f = file_of(sq);
	return (f <= FILE_7) ?
		Bitboard(RookFileEffect[rank_of(sq)][index] << int(f | RANK_1), 0) :
		Bitboard(0, RookFileEffect[rank_of(sq)][index] << int((File)(f - FILE_8) | RANK_1));
}

inline Bitboard lanceEffect(Color c, Square sq, const Bitboard& occupied)
{
	return rookFileEffect(sq, occupied) & lanceStepEffect(c, sq);
}


#if defined(USE_OLD_YANEURAOU_EFFECT)


inline uint64_t occupiedToIndex(const Bitboard& occupied, const Bitboard& mask) { return PEXT64(occupied.merge(), mask.merge()); }

inline Bitboard bishopEffect0(Square sq, const Bitboard& occupied)
{
	ASSERT_LV3(sq <= SQ_NB);
	const Bitboard block0(occupied & BishopEffectMask[0][sq]);
	return BishopEffect[0][BishopEffectIndex[0][sq] + occupiedToIndex(block0, BishopEffectMask[0][sq])];
}

inline Bitboard bishopEffect1(Square sq, const Bitboard& occupied)
{
	ASSERT_LV3(sq <= SQ_NB);
	const Bitboard block1(occupied & BishopEffectMask[1][sq]);
	return BishopEffect[1][BishopEffectIndex[1][sq] + occupiedToIndex(block1, BishopEffectMask[1][sq])];
}

inline Bitboard bishopEffect(Square sq, const Bitboard& occupied)
{
	return bishopEffect0(sq, occupied) | bishopEffect1(sq, occupied);
}

inline Bitboard rookRankEffect(Square sq, const Bitboard& occupied)
{
	ASSERT_LV3(sq <= SQ_NB);
	int r = rank_of(sq);
	u64 u = (occupied.extract64<1>() << 6 * 9) + (occupied.extract64<0>() >> 9);
	u64 index = PEXT64(u, 0b1000000001000000001000000001000000001000000001000000001 << r);
	return RookRankEffect[file_of(sq)][index] << r;
}

inline Bitboard rookEffect(Square sq, const Bitboard& occupied)
{
	return rookFileEffect(sq, occupied) | rookRankEffect(sq, occupied);
}

#else 


#if defined (USE_BMI2)

inline u64 occupiedToIndex(const Bitboard& block, const Bitboard& mask) {
	return PEXT64(block.merge(), mask.merge());
}

inline Bitboard rookEffect(const Square sq, const Bitboard& occupied) {
	const Bitboard block(occupied & RookBlockMask[sq]);
	return RookAttack[RookAttackIndex[sq] + occupiedToIndex(block, RookBlockMask[sq])];
}
inline Bitboard bishopEffect(const Square sq, const Bitboard& occupied) {
	const Bitboard block(occupied & BishopBlockMask[sq]);
	return BishopAttack[BishopAttackIndex[sq] + occupiedToIndex(block, BishopBlockMask[sq])];
}
#else


inline u64 occupiedToIndex(const Bitboard& block, const u64 magic, const int shiftBits) {
	return (block.merge() * magic) >> shiftBits;
}

inline Bitboard rookEffect(const Square sq, const Bitboard& occupied) {
	const Bitboard block(occupied & RookBlockMask[sq]);
	return RookAttack[RookAttackIndex[sq] + occupiedToIndex(block, RookMagic[sq], RookShiftBits[sq])];
}

inline Bitboard bishopEffect(const Square sq, const Bitboard& occupied) {
	const Bitboard block(occupied & BishopBlockMask[sq]);
	return BishopAttack[BishopAttackIndex[sq] + occupiedToIndex(block, BishopMagic[sq], BishopShiftBits[sq])];
}

#endif 

inline Bitboard rookRankEffect(Square sq, const Bitboard& occupied)
{
	return rookEffect(sq, occupied) & RANK_BB[rank_of(sq)];
}


#endif 


inline Bitboard horseEffect(Square sq, const Bitboard& occupied)
{
	return bishopEffect(sq, occupied) | kingEffect(sq);
}

inline Bitboard dragonEffect(Square sq, const Bitboard& occupied)
{
	return rookEffect(sq, occupied) | kingEffect(sq);
}


extern Bitboard effects_from(Piece pc, Square sq, const Bitboard& occ);


inline bool more_than_one(const Bitboard& bb) { ASSERT_LV2(!bb.cross_over()); return POPCNT64(bb.merge()) > 1; }



#endif 
