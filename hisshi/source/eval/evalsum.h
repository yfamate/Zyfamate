#ifndef _EVAL_SUM_H_
#define _EVAL_SUM_H_

#include "../types.h"
#include <array>


namespace Eval {

	template <typename Tl, typename Tr>
	FORCE_INLINE std::array<Tl, 2> operator += (std::array<Tl, 2> & lhs, const std::array<Tr, 2> & rhs) {
		lhs[0] += (Tl)rhs[0];
		lhs[1] += (Tl)rhs[1];
		return lhs;
	}
	template <typename Tl, typename Tr>
	FORCE_INLINE std::array<Tl, 2> operator -= (std::array<Tl, 2> & lhs, const std::array<Tr, 2> & rhs) {
		lhs[0] -= (Tl)rhs[0];
		lhs[1] -= (Tl)rhs[1];
		return lhs;
	}
	template <typename Tl, typename Tr>
	FORCE_INLINE bool operator == (std::array<Tl, 2> & lhs, const std::array<Tr, 2> & rhs) {
		return lhs[0] == rhs[0] && lhs[1] == rhs[1];
	}
	template <typename Tl, typename Tr>
	FORCE_INLINE bool operator != (std::array<Tl, 2> & lhs, const std::array<Tr, 2> & rhs) {
		return !(lhs == rhs);
	}
	template <typename Tl>
	FORCE_INLINE std::array<Tl, 2> operator - (const std::array<Tl, 2> & rhs) {
		std::array<Tl, 2> a;
		a[0] = -rhs[0];
		a[1] = -rhs[1];
		return a;
	}
	template <typename Tl>
	FORCE_INLINE std::array<Tl, 2> operator + (const std::array<Tl, 2> & lhs, const std::array<Tl, 2> & rhs) {
		std::array<Tl, 2> tmp = lhs;
		tmp += rhs;
		return tmp;
	}
	template <typename Tl>
	FORCE_INLINE std::array<Tl, 2> operator - (const std::array<Tl, 2> & lhs, const std::array<Tl, 2> & rhs) {
		std::array<Tl, 2> tmp = lhs;
		tmp -= rhs;
		return tmp;
	}
	template <typename Tl>
	FORCE_INLINE std::array<Tl, 2> operator * (const std::array<Tl, 2> & rhs, int n) {
		std::array<Tl, 2> a;
		a[0] = rhs[0] * n;
		a[1] = rhs[1] * n;
		return a;
	}
	template <typename Tl>
	FORCE_INLINE std::array<Tl, 2> operator / (const std::array<Tl, 2> & rhs, int n) {
		std::array<Tl, 2> a;
		a[0] = rhs[0] / n;
		a[1] = rhs[1] / n;
		return a;
	}

	template <typename T>
	bool is_zero(const std::array<T, 2> v)
	{
		return v[0] == T(0) && v[1] == T(0);
	}

	template <typename T>
	std::ostream& operator<<(std::ostream& os, const std::array<T, 2> v)
	{
		os << "{ " << v[0] << "," << v[1] << " } ";
		return os;
	}



#if defined (__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

	struct alignas(32) EvalSum {

#if defined(USE_AVX2)
		EvalSum(const EvalSum & es) {
			_mm256_store_si256(&mm, es.mm);
		}
		EvalSum& operator = (const EvalSum & rhs) {
			_mm256_store_si256(&mm, rhs.mm);
			return *this;
		}
#elif defined(USE_SSE2)
		EvalSum(const EvalSum & es) {
			_mm_store_si128(&m[0], es.m[0]);
			_mm_store_si128(&m[1], es.m[1]);
		}
		EvalSum& operator = (const EvalSum & rhs) {
			_mm_store_si128(&m[0], rhs.m[0]);
			_mm_store_si128(&m[1], rhs.m[1]);
			return *this;
		}
#endif

		EvalSum() {}

		int32_t sum(const Color c) const
		{

#if defined(EVAL_KPP_KKPT)
			
			const int32_t scoreBoard = p[0][0] - p[1][0] + p[2][0];
			const int32_t scoreTurn = p[2][1];

#else 

			const int32_t scoreBoard = p[0][0] - p[1][0] + p[2][0];
			const int32_t scoreTurn = p[0][1] + p[1][1] + p[2][1];
#endif


			return (c == BLACK ? scoreBoard : -scoreBoard) + scoreTurn;
		}

		EvalSum& operator += (const EvalSum & rhs)
		{
#if defined(USE_AVX2)
			mm = _mm256_add_epi32(mm, rhs.mm);
#elif defined(USE_SSE2)
			m[0] = _mm_add_epi32(m[0], rhs.m[0]);
			m[1] = _mm_add_epi32(m[1], rhs.m[1]);
#else
			p[0][0] += rhs.p[0][0];
			p[0][1] += rhs.p[0][1];
			p[1][0] += rhs.p[1][0];
			p[1][1] += rhs.p[1][1];
			p[2][0] += rhs.p[2][0];
			p[2][1] += rhs.p[2][1];
#endif
			return *this;
		}
		EvalSum& operator -= (const EvalSum & rhs)
		{
#if defined(USE_AVX2)
			mm = _mm256_sub_epi32(mm, rhs.mm);
#elif defined(USE_SSE2)
			m[0] = _mm_sub_epi32(m[0], rhs.m[0]);
			m[1] = _mm_sub_epi32(m[1], rhs.m[1]);
#else
			p[0][0] -= rhs.p[0][0];
			p[0][1] -= rhs.p[0][1];
			p[1][0] -= rhs.p[1][0];
			p[1][1] -= rhs.p[1][1];
			p[2][0] -= rhs.p[2][0];
			p[2][1] -= rhs.p[2][1];
#endif
			return *this;
		}
		EvalSum operator + (const EvalSum & rhs) const { return EvalSum(*this) += rhs; }
		EvalSum operator - (const EvalSum & rhs) const { return EvalSum(*this) -= rhs; }

		void encode()
		{
#if defined USE_AVX2
#else
			key ^= data[0] ^ data[1] ^ data[2];
#endif
		}
		void decode() { encode(); }

		bool evaluated() const { return p[0][0] != VALUE_NOT_EVALUATED; }

		union {
			std::array<std::array<int32_t, 2>, 4> p;

			struct {
				u64 data[3];
				u64 key; 
			};
#if defined(USE_AVX2)
			__m256i mm;
			__m128i m[2];
#elif defined(USE_SSE2)
			__m128i m[2];
#endif
		};
	};

#if defined (__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic warning "-Wmaybe-uninitialized"
#endif

	static std::ostream& operator<<(std::ostream& os, const EvalSum& sum)
	{
		os << "sum BKPP   = " << sum.p[0][0] << " + " << sum.p[0][1] << std::endl;
		os << "sum WKPP   = " << sum.p[1][0] << " + " << sum.p[1][1] << std::endl;
		os << "sum KK,KKP = " << sum.p[2][0] << " + " << sum.p[2][1] << std::endl;
		return os;
	}


	static bool operator == (const EvalSum& lhs, const EvalSum rhs) { return lhs.p[0] == rhs.p[0] && lhs.p[1] == rhs.p[1] && lhs.p[2] == rhs.p[2]; }
	static bool operator != (const EvalSum& lhs, const EvalSum rhs) { return !(lhs == rhs); }

} 

#endif 
