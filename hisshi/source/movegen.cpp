
#include "types.h"
#include "position.h"

#include <iostream>
using namespace std;


template <PieceType Pt, Color Us, bool All> struct make_move_target {
	FORCE_INLINE ExtMove* operator()(const Position& pos, Square from, const Bitboard& target_, ExtMove* mlist)
	{
		Square to;
		Bitboard target = target_;
		Bitboard target2;

		switch (Pt)
		{
		case PAWN:
			if (target)
			{
				to = from + (Us == BLACK ? SQ_U : SQ_D); 
				if (canPromote(Us, to))
				{
					mlist++->move = make_move_promote(from, to , Us, Pt);
					if (All && rank_of(to) != (Us == BLACK ? RANK_1 : RANK_9))
						mlist++->move = make_move(from, to , Us, Pt);
				}
				else
					mlist++->move = make_move(from, to , Us, Pt);
			}
			break;

		case LANCE:
		{
			target2 = target & enemy_field(Us);
			target2.foreach([&](Square to) { mlist++->move = make_move_promote(from, to , Us , Pt); });

			target &= All ? (Us == BLACK ? ForwardRanksBB[WHITE][RANK_1] : ForwardRanksBB[BLACK][RANK_9]) :
							(Us == BLACK ? ForwardRanksBB[WHITE][RANK_2] : ForwardRanksBB[BLACK][RANK_8]);

			target.foreach([&](Square to) { mlist++->move = make_move(from,to , Us , Pt); });
		}
		break;

		case KNIGHT:
		{
			while (target)
			{
				to = target.pop();
				if (canPromote(Us, to))
					mlist++->move = make_move_promote(from, to  , Us, Pt);
				if ((Us == BLACK && rank_of(to) >= RANK_3) || (Us == WHITE && rank_of(to) <= RANK_7))
					mlist++->move = make_move(from, to , Us, Pt);
			}
		}
		break;

		case SILVER:
		{
			if (enemy_field(Us) & from) {
				target.foreach([&](Square to) {
					mlist++->move = make_move_promote(from, to , Us, Pt);
					mlist++->move = make_move(from, to  , Us, Pt);
				});
			}
			else
			{
				target2 = target & enemy_field(Us);
				target2.foreach([&](Square to) {
					mlist++->move = make_move_promote(from, to , Us, Pt);
					mlist++->move = make_move(from, to , Us, Pt);
				});
				target &= ~enemy_field(Us);
				target.foreach([&](Square to) { mlist++->move = make_move(from,to , Us,Pt); });
			}
		}
		break;

		case GOLD: case PRO_PAWN: case PRO_LANCE: case PRO_KNIGHT: case PRO_SILVER: case HORSE: case DRAGON: case KING:
			target.foreach([&](Square to) { mlist++->move = make_move(from,to , Us, Pt); });
			break;

		case GPM_GHDK:
			target.foreach([&](Square to) { mlist++->move = make_move(from, to, pos.piece_on(from)); });
			break;

		case BISHOP: case ROOK:
			if (canPromote(Us, from)) {
				target.foreach([&](Square to) {
					mlist++->move = make_move_promote(from,to , Us,Pt);
					if (All) mlist++->move = make_move(from,to , Us,Pt);
				});
			}
			else
			{
				target2 = target & enemy_field(Us);
				target2.foreach([&](Square to) {
					mlist++->move = make_move_promote(from,to , Us,Pt);
					if (All) mlist++->move = make_move(from,to , Us,Pt);
				});

				target &= ~enemy_field(Us);
				target.foreach([&](Square to) { mlist++->move = make_move(from,to , Us,Pt); });
			}
			break;

		case GPM_BR:
			if (canPromote(Us, from)) {
				target.foreach([&](Square to){
					mlist++->move = make_move_promote(from,to , pos.piece_on(from));
					if (All) mlist++->move = make_move(from,to , pos.piece_on(from))  ;
				});
			}
			else
			{
				target2 = target & enemy_field(Us);
				target2.foreach([&](Square to){
					mlist++->move = make_move_promote(from,to, pos.piece_on(from));
					if (All) mlist++->move = make_move(from,to, pos.piece_on(from));
				});

				target &= ~enemy_field(Us);
				target.foreach([&](Square to) { mlist++->move = make_move(from,to , pos.piece_on(from)); });
			}
			break;


		default: UNREACHABLE; break;
		}

		return mlist;
	}
};

template <MOVE_GEN_TYPE GenType, PieceType Pt, Color Us, bool All> struct GeneratePieceMoves {
	FORCE_INLINE ExtMove* operator()(const Position&pos, ExtMove*mlist, const Bitboard& target) {
		auto pieces = pos.pieces(Us, Pt);
		const auto occ = pos.pieces();

		while (pieces)
		{
			auto from = pieces.pop();

			auto target2 =
				Pt == LANCE  ? lanceEffect(Us, from, occ) :
				Pt == KNIGHT ? knightEffect(Us, from) :
				Pt == SILVER ? silverEffect(Us, from) :
				ALL_BB; 

			target2 &= target;
			mlist = make_move_target<Pt, Us, All>()(pos, from, target2, mlist);
		}

		return mlist;
	}
};

template <MOVE_GEN_TYPE GenType, Color Us, bool All> struct GeneratePieceMoves<GenType, PAWN, Us, All> {
	FORCE_INLINE ExtMove* operator()(const Position&pos, ExtMove*mlist, const Bitboard& target)
	{
		auto pieces = pos.pieces(Us, PAWN);

		auto target2 = pawnEffect(Us,pieces) & target;

		const Rank T_RANK1 = (Us == BLACK) ? RANK_1 : RANK_9;

		while (target2)
		{
			auto to = target2.pop();
			auto from = to + (Us == BLACK ? SQ_D : SQ_U);
			if (canPromote(Us, to))
			{
				mlist++->move = make_move_promote(from, to , Us, PAWN);

				if (All && rank_of(to) != T_RANK1)
					mlist++->move = make_move(from, to, Us, PAWN);
			}
			else
				mlist++->move = make_move(from, to , Us, PAWN);
		}
		return mlist;
	}
};

template <MOVE_GEN_TYPE GenType, Color Us, bool All> struct GeneratePieceMoves<GenType, GPM_BR, Us, All> {
	FORCE_INLINE ExtMove* operator()(const Position&pos, ExtMove*mlist, const Bitboard& target)
	{
		auto pieces = pos.pieces(Us,BISHOP,ROOK);
		auto occ = pos.pieces();

		while (pieces)
		{
			auto from = pieces.pop();

			auto target2 = effects_from(pos.piece_on(from), from, occ) & target;

			mlist = make_move_target<GPM_BR, Us, All>()(pos, from, target2, mlist);
		}
		return mlist;
	}
};

template <MOVE_GEN_TYPE GenType, Color Us, bool All> struct GeneratePieceMoves<GenType, GPM_GHDK, Us, All> {
	FORCE_INLINE ExtMove* operator()(const Position&pos, ExtMove*mlist, const Bitboard& target)
	{
		auto pieces = pos.pieces(Us,GOLDS,HDK);
		auto occ = pos.pieces();

		while (pieces)
		{
			auto from = pieces.pop();
			auto target2 = effects_from(pos.piece_on(from), from, occ) & target;

			mlist = make_move_target<GPM_GHDK, Us, All>()(pos, from, target2, mlist);
		}
		return mlist;
	}
};

template <MOVE_GEN_TYPE GenType, Color Us, bool All> struct GeneratePieceMoves<GenType, GPM_GHD, Us, All> {
	FORCE_INLINE ExtMove* operator()(const Position&pos, ExtMove*mlist, const Bitboard& target)
	{
		auto pieces = pos.pieces(Us,GOLDS,HORSE,DRAGON);
		auto occ = pos.pieces();

		while (pieces)
		{
			auto from = pieces.pop();
			const auto pc = pos.piece_on(from);
			auto target2 = effects_from(pc, from, occ) & target;
			target2.foreach([&](Square to) { mlist++->move = make_move(from,to,pc); });
		}
		return mlist;
	}
};



template <Color Us> struct GenerateDropMoves {
	ExtMove* operator()(const Position&pos, ExtMove*mlist, const Bitboard& target) {

		const Hand hand = pos.hand_of(Us);
		if (hand == 0)
			return mlist;

		if (hand_exists(hand, PAWN))
		{


			Bitboard target2 = rank1_n_bb(~Us, RANK_8) & target;

			Bitboard pawnBB = pos.pieces(Us, PAWN);
			pawnBB.foreach_part([&](Square pawnSq,int part) {
				target2.p[part] &= PAWN_DROP_MASKS[pawnSq];
			});

			Bitboard pe = pawnEffect(~Us, pos.king_square(~Us));
			if (pe & target2)
			{
				Square to = pe.pop_c();
				if (!pos.legal_drop(to))
					target2 ^= pe;
			}

			target2.foreach([&](Square sq) {
				mlist++->move = make_move_drop(PAWN , sq , Us );
			});

		}


		if (hand_except_pawn_exists(hand))
		{
			Move drops[6];


			int num = 0;
			if (hand_exists(hand, KNIGHT)) drops[num++] = make_move_drop(KNIGHT, SQ_ZERO , Us);

			int nextToKnight = num; 
			if (hand_exists(hand, LANCE )) drops[num++] = make_move_drop(LANCE, SQ_ZERO  , Us);

			int nextToLance = num; 

			if (hand_exists(hand, SILVER)) drops[num++] = make_move_drop(SILVER, SQ_ZERO , Us);
			if (hand_exists(hand, GOLD)  ) drops[num++] = make_move_drop(GOLD  , SQ_ZERO , Us);
			if (hand_exists(hand, BISHOP)) drops[num++] = make_move_drop(BISHOP, SQ_ZERO , Us);
			if (hand_exists(hand, ROOK)  ) drops[num++] = make_move_drop(ROOK  , SQ_ZERO , Us);



			if (nextToLance == 0)
			{
				Bitboard target2 = target;

				switch (num)
				{
				case 1: target2.foreach([&](Square sq) { Unroller<1>()([&](int i){ mlist++->move = (Move)(drops[i] + sq); }); }); break;
				case 2: target2.foreach([&](Square sq) { Unroller<2>()([&](int i){ mlist++->move = (Move)(drops[i] + sq); }); }); break;
				case 3: target2.foreach([&](Square sq) { Unroller<3>()([&](int i){ mlist++->move = (Move)(drops[i] + sq); }); }); break;
				case 4: target2.foreach([&](Square sq) { Unroller<4>()([&](int i){ mlist++->move = (Move)(drops[i] + sq); }); }); break;
				default: UNREACHABLE;
				}
			}
			else
			{

				Bitboard target1 = target & rank1_n_bb(Us, RANK_1); 
				Bitboard target2 = target & (Us == BLACK ? RANK2_BB : RANK8_BB); 
				Bitboard target3 = target & rank1_n_bb(~Us, RANK_7); 

				switch (num - nextToLance) 
				{
				case 0: break; 
				case 1: target1.foreach([&](Square sq) { Unroller<1>()([&](int i){ mlist++->move = (Move)(drops[i + nextToLance] + sq); }); }); break;
				case 2: target1.foreach([&](Square sq) { Unroller<2>()([&](int i){ mlist++->move = (Move)(drops[i + nextToLance] + sq); }); }); break;
				case 3: target1.foreach([&](Square sq) { Unroller<3>()([&](int i){ mlist++->move = (Move)(drops[i + nextToLance] + sq); }); }); break;
				case 4: target1.foreach([&](Square sq) { Unroller<4>()([&](int i){ mlist++->move = (Move)(drops[i + nextToLance] + sq); }); }); break;
				default: UNREACHABLE;
				}

				switch (num - nextToKnight) 
				{
				case 0: break; 
				case 1: target2.foreach([&](Square sq) { Unroller<1>()([&](int i){ mlist++->move = (Move)(drops[i + nextToKnight] + sq); }); }); break;
				case 2: target2.foreach([&](Square sq) { Unroller<2>()([&](int i){ mlist++->move = (Move)(drops[i + nextToKnight] + sq); }); }); break;
				case 3: target2.foreach([&](Square sq) { Unroller<3>()([&](int i){ mlist++->move = (Move)(drops[i + nextToKnight] + sq); }); }); break;
				case 4: target2.foreach([&](Square sq) { Unroller<4>()([&](int i){ mlist++->move = (Move)(drops[i + nextToKnight] + sq); }); }); break;
				case 5: target2.foreach([&](Square sq) { Unroller<5>()([&](int i){ mlist++->move = (Move)(drops[i + nextToKnight] + sq); }); }); break;
				default: UNREACHABLE;
				}

				switch (num) 
				{
				case 1: target3.foreach([&](Square sq) { Unroller<1>()([&](int i){ mlist++->move = (Move)(drops[i] + sq); }); }); break;
				case 2: target3.foreach([&](Square sq) { Unroller<2>()([&](int i){ mlist++->move = (Move)(drops[i] + sq); }); }); break;
				case 3: target3.foreach([&](Square sq) { Unroller<3>()([&](int i){ mlist++->move = (Move)(drops[i] + sq); }); }); break;
				case 4: target3.foreach([&](Square sq) { Unroller<4>()([&](int i){ mlist++->move = (Move)(drops[i] + sq); }); }); break;
				case 5: target3.foreach([&](Square sq) { Unroller<5>()([&](int i){ mlist++->move = (Move)(drops[i] + sq); }); }); break;
				case 6: target3.foreach([&](Square sq) { Unroller<6>()([&](int i){ mlist++->move = (Move)(drops[i] + sq); }); }); break;
				default: UNREACHABLE;
				}
			}

		}

		return mlist;
	}
};


template<Color Us, bool All>
ExtMove* generate_evasions(const Position& pos, ExtMove* mlist)
{

	ASSERT_LV2(pos.in_check());

	Bitboard sliderAttacks = ZERO_BB;

	Bitboard checkers = pos.checkers();

	int checkersCnt = 0;

	Square ksq = pos.king_square(Us);
	Bitboard occ = pos.pieces() ^ Bitboard(ksq);

	Square checksq;

	do
	{
		++checkersCnt;

		checksq = checkers.pop();

		ASSERT_LV3(color_of(pos.piece_on(checksq)) == ~Us);

		sliderAttacks |= effects_from(pos.piece_on(checksq), checksq, occ);

	} while (checkers);


	Bitboard bb = kingEffect(ksq) & ~(pos.pieces(Us) | sliderAttacks);
	while (bb) { Square to = bb.pop(); mlist++->move = make_move(ksq, to , Us, KING); }

	if (checkersCnt > 1)
		return mlist;




	const Bitboard target1 = between_bb(checksq, ksq);
	const Bitboard target2 = target1 | checksq;

	mlist = GeneratePieceMoves<NON_EVASIONS, PAWN   , Us, All>()(pos, mlist, target2);
	mlist = GeneratePieceMoves<NON_EVASIONS, LANCE  , Us, All>()(pos, mlist, target2);
	mlist = GeneratePieceMoves<NON_EVASIONS, KNIGHT , Us, All>()(pos, mlist, target2);
	mlist = GeneratePieceMoves<NON_EVASIONS, SILVER , Us, All>()(pos, mlist, target2);
	mlist = GeneratePieceMoves<NON_EVASIONS, GPM_BR , Us, All>()(pos, mlist, target2);
	mlist = GeneratePieceMoves<NON_EVASIONS, GPM_GHD, Us, All>()(pos, mlist, target2); 
	mlist = GenerateDropMoves<Us>()(pos, mlist, target1);

	return mlist;
}



template<MOVE_GEN_TYPE GenType, Color Us, bool All>
ExtMove* generate_general(const Position& pos, ExtMove* mlist, Square recapSq = SQ_NB)
{


	static_assert(GenType != EVASIONS_ALL && GenType != NON_EVASIONS_ALL && GenType != RECAPTURES_ALL, "*_ALL is not allowed.");

	const Bitboard target =
		(GenType == NON_CAPTURES)      ? pos.empties()      : 
		(GenType == CAPTURES)          ? pos.pieces(~Us)    : 
		(GenType == NON_CAPTURES_PRO_MINUS) ? pos.empties() : 
		(GenType == CAPTURES_PRO_PLUS) ? pos.pieces(~Us)    : 
		(GenType == NON_EVASIONS)      ? ~pos.pieces(Us)    : 
		(GenType == RECAPTURES)        ? Bitboard(recapSq)  : 
		ALL_BB; 

	const Bitboard targetPawn =
		(GenType == NON_CAPTURES_PRO_MINUS) ? (pos.empties() & ~enemy_field(Us)) : 
		(GenType == CAPTURES_PRO_PLUS)      ? (pos.pieces(~Us) | (~pos.pieces(Us) & enemy_field(Us))) : 
		target;

	mlist = GeneratePieceMoves<GenType, PAWN    , Us, All>()(pos, mlist, targetPawn);

	mlist = GeneratePieceMoves<GenType, LANCE   , Us, All>()(pos, mlist, target);
	mlist = GeneratePieceMoves<GenType, KNIGHT  , Us, All>()(pos, mlist, target);
	mlist = GeneratePieceMoves<GenType, SILVER  , Us, All>()(pos, mlist, target);

	mlist = GeneratePieceMoves<GenType, GPM_BR  , Us, All>()(pos, mlist, target);

	mlist = GeneratePieceMoves<GenType, GPM_GHDK, Us, All>()(pos, mlist, target);

	if (GenType == NON_CAPTURES || GenType == NON_CAPTURES_PRO_MINUS || GenType == NON_EVASIONS)
		mlist = GenerateDropMoves<Us>()(pos, mlist, pos.empties());

	return mlist;
}


template <Color Us, bool All> struct make_move_target_general {
	ExtMove* operator()(const Position& pos, Piece pc, Square from, const Bitboard& target, ExtMove* mlist)
	{
		ASSERT_LV2(pc != NO_PIECE);
		auto effect = effects_from(pc, from, pos.pieces());
		switch (type_of(pc))
		{
		case PAWN      : mlist = make_move_target<PAWN      , Us, All>()(pos, from, effect & target, mlist); break;
		case LANCE     : mlist = make_move_target<LANCE     , Us, All>()(pos, from, effect & target, mlist); break;
		case KNIGHT    : mlist = make_move_target<KNIGHT    , Us, All>()(pos, from, effect & target, mlist); break;
		case SILVER    : mlist = make_move_target<SILVER    , Us, All>()(pos, from, effect & target, mlist); break;
		case GOLD      : mlist = make_move_target<GOLD      , Us, All>()(pos, from, effect & target, mlist); break;
		case BISHOP    : mlist = make_move_target<BISHOP    , Us, All>()(pos, from, effect & target, mlist); break;
		case ROOK      : mlist = make_move_target<ROOK      , Us, All>()(pos, from, effect & target, mlist); break;
		case KING      : mlist = make_move_target<KING      , Us, All>()(pos, from, effect & target, mlist); break;
		case PRO_PAWN  : mlist = make_move_target<PRO_PAWN  , Us, All>()(pos, from, effect & target, mlist); break;
		case PRO_LANCE : mlist = make_move_target<PRO_LANCE , Us, All>()(pos, from, effect & target, mlist); break;
		case PRO_KNIGHT: mlist = make_move_target<PRO_KNIGHT, Us, All>()(pos, from, effect & target, mlist); break;
		case PRO_SILVER: mlist = make_move_target<PRO_SILVER, Us, All>()(pos, from, effect & target, mlist); break;
		case HORSE     : mlist = make_move_target<HORSE     , Us, All>()(pos, from, effect & target, mlist); break;
		case DRAGON    : mlist = make_move_target<DRAGON    , Us, All>()(pos, from, effect & target, mlist); break;
		default: UNREACHABLE;
		}
		return mlist;
	}
};

template <PieceType Pt, Color Us, bool All, bool Promote>
ExtMove* make_move_target_pro(Square from, const Bitboard& target, ExtMove* mlist)
{
	auto bb = target;
	while (bb)
	{
		auto to = bb.pop();
		if (Promote)
			mlist++->move = make_move_promote(from, to , Us, Pt);
		else
		{
			if (((Pt == PAWN) &&
				((!All && !canPromote(Us, to)) ||
				(All && rank_of(to) != (Us == BLACK ? RANK_1 : RANK_9))))
				|| ((Pt == LANCE) &&
				((!All && ((Us == BLACK && rank_of(to) >= RANK_3) || (Us == WHITE && rank_of(to) <= RANK_7))) ||
					(All && rank_of(to) != (Us == BLACK ? RANK_1 : RANK_9))))
				|| (Pt == KNIGHT && ((Us == BLACK && rank_of(to) >= RANK_3) || (Us == WHITE && rank_of(to) <= RANK_7)))
				|| (Pt == SILVER)
				|| ((Pt == BISHOP || Pt == ROOK) && (!(canPromote(Us, from) || canPromote(Us, to)) || All))
				)

				mlist++->move = make_move(from, to , Us, Pt);
		}
	}
	return mlist;
}

template <Color Us, bool All>
ExtMove* make_move_check(const Position& pos, Piece pc, Square from, Square ksq, const Bitboard& target, ExtMove* mlist)
{
#define GEN_MOVE_NONPRO_CHECK(X,X_Effect,Y_Effect) {                   \
    dst = X_Effect(Us, from) & Y_Effect(~Us, ksq) & target;            \
    if (!(enemy_field(Us) & from))                                     \
      dst &= enemy_field(Us);                                          \
    mlist = make_move_target_pro<X, Us, All, true>(from, dst, mlist);  \
    dst = X_Effect(Us, from) & X_Effect(~Us, ksq) & target;            \
    mlist = make_move_target_pro<X, Us, All, false>(from, dst, mlist); }

#define GEN_MOVE_LANCE_CHECK(X,X_Effect,Y_Effect) {                    \
    occ = pos.pieces();                                                \
    dst = X_Effect(Us, from,occ) & Y_Effect(~Us, ksq) & target;        \
    if (!(enemy_field(Us) & from))                                     \
      dst &= enemy_field(Us);                                          \
    mlist = make_move_target_pro<X, Us, All, true>(from, dst, mlist);  \
    if (file_of(from) == file_of(ksq) && !more_than_one(between_bb(from, ksq) & occ)){ \
      dst = pos.pieces(~Us) & between_bb(from, ksq) & target;            \
      mlist = make_move_target_pro<X, Us, All, false>(from, dst, mlist); \
    }}

#define GEN_MOVE_NONPRO_PRO_CHECK_BR(X,X_Effect,Y_Effect) {            \
  occ = pos.pieces();                                                  \
  dst = X_Effect(from,occ) & Y_Effect(ksq,occ) & target;               \
    if (!(enemy_field(Us) & from))                                     \
      dst &= enemy_field(Us);                                          \
    mlist = make_move_target_pro<X, Us, All, true>(from, dst, mlist);  \
    dst = X_Effect(from,occ) & X_Effect(ksq,occ) & target;             \
    mlist = make_move_target_pro<X, Us, All, false>(from, dst, mlist); }

#define GEN_MOVE_GOLD_CHECK(X,X_Effect) {                              \
  dst = X_Effect(Us, from) & X_Effect(~Us, ksq) & target;              \
  mlist = make_move_target<X, Us, All>()(pos,from, dst, mlist); }

#define GEN_MOVE_HD_CHECK(X,X_Effect) {                                \
  occ = pos.pieces();                                                  \
  dst = X_Effect(from,occ) & X_Effect(ksq,occ) & target;               \
  mlist = make_move_target<X, Us, All>()(pos,from, dst, mlist); }

	Bitboard dst, occ;
	switch (type_of(pc))
	{
	case PAWN      : GEN_MOVE_NONPRO_CHECK(PAWN, pawnEffect, goldEffect); break;
	case LANCE     : GEN_MOVE_LANCE_CHECK(LANCE, lanceEffect, goldEffect); break;
	case KNIGHT    : GEN_MOVE_NONPRO_CHECK(KNIGHT, knightEffect, goldEffect); break;
	case SILVER    : GEN_MOVE_NONPRO_CHECK(SILVER, silverEffect, goldEffect); break;
	case BISHOP    : GEN_MOVE_NONPRO_PRO_CHECK_BR(BISHOP, bishopEffect, horseEffect); break;
	case ROOK      : GEN_MOVE_NONPRO_PRO_CHECK_BR(ROOK, rookEffect, dragonEffect); break;

	case PRO_PAWN  : GEN_MOVE_GOLD_CHECK(PRO_PAWN, goldEffect); break;
	case PRO_LANCE : GEN_MOVE_GOLD_CHECK(PRO_LANCE, goldEffect); break;
	case PRO_KNIGHT: GEN_MOVE_GOLD_CHECK(PRO_KNIGHT, goldEffect); break;
	case PRO_SILVER: GEN_MOVE_GOLD_CHECK(PRO_SILVER, goldEffect); break;
	case GOLD      : GEN_MOVE_GOLD_CHECK(GOLD, goldEffect); break;
	case HORSE     : GEN_MOVE_HD_CHECK(HORSE, horseEffect); break;
	case DRAGON    : GEN_MOVE_HD_CHECK(DRAGON, dragonEffect); break;

	default:UNREACHABLE;
	}
	return mlist;
}


template <Color Us, PieceType Pt> struct GenerateCheckDropMoves {
	ExtMove* operator()(const Position& , const Bitboard& target, ExtMove* mlist)
	{
		auto bb = target;
		while (bb)
		{
			auto to = bb.pop();
			mlist++->move = make_move_drop(Pt, to , Us);
		}
		return mlist;
	}
};

template <Color Us> struct GenerateCheckDropMoves<Us, PAWN> {
	ExtMove* operator()(const Position& pos, const Bitboard& target, ExtMove* mlist)
	{
		auto bb = target;
		if (bb) 
		{
			auto to = bb.pop_c();

			if (pos.legal_pawn_drop(Us, to))
				mlist++->move = make_move_drop(PAWN, to , Us);
		}
		return mlist;
	}
};


template<MOVE_GEN_TYPE GenType, Color Us, bool All>
ExtMove* generate_checks(const Position& pos, ExtMove* mlist)
{




	const Square themKing = pos.king_square(~Us);

	const Bitboard x =
		(
			(pos.pieces(PAWN)   & check_candidate_bb(Us, PAWN  , themKing)) |
			(pos.pieces(LANCE)  & check_candidate_bb(Us, LANCE , themKing)) |
			(pos.pieces(KNIGHT) & check_candidate_bb(Us, KNIGHT, themKing)) |
			(pos.pieces(SILVER) & check_candidate_bb(Us, SILVER, themKing)) |
			(pos.pieces(GOLDS)  & check_candidate_bb(Us, GOLD  , themKing)) |
			(pos.pieces(BISHOP) & check_candidate_bb(Us, BISHOP, themKing)) |
			(pos.pieces(ROOK_DRAGON)) | 
			(pos.pieces(HORSE)  & check_candidate_bb(Us, ROOK  , themKing)) 
		) & pos.pieces(Us);


	const Bitboard y = pos.blockers_for_king(~Us) & pos.pieces(Us);

	const Bitboard target =
		(GenType == CHECKS || GenType == CHECKS_ALL) ? ~pos.pieces(Us) :                     
		(GenType == QUIET_CHECKS || GenType == QUIET_CHECKS_ALL) ? pos.empties() :           
		ALL_BB; 

	auto src = y;
	while (src)
	{
		auto from = src.pop();


		auto pin_line = line_bb(themKing, from);
		
		mlist = make_move_target_general<Us, All>()(pos, pos.piece_on(from), from, target & ~pin_line, mlist);

		if (x & from)
			mlist = make_move_check<Us, All>(pos, pos.piece_on(from), from, themKing, target & pin_line, mlist);
	}

	src = (x | y) ^ y;
	while (src)
	{
		auto from = src.pop();

		mlist = make_move_check<Us, All>(pos, pos.piece_on(from), from, themKing, target, mlist);
	}


	const Bitboard& empties = pos.empties();

	Hand h = pos.hand_of(Us);
	if (hand_exists(h, PAWN))
		mlist = GenerateCheckDropMoves<Us, PAWN>()(pos, pos.check_squares(PAWN) & empties, mlist);
	if (hand_exists(h, LANCE))
		mlist = GenerateCheckDropMoves<Us, LANCE>()(pos, pos.check_squares(LANCE) & empties, mlist);
	if (hand_exists(h, KNIGHT))
		mlist = GenerateCheckDropMoves<Us, KNIGHT>()(pos, pos.check_squares(KNIGHT) & empties, mlist);
	if (hand_exists(h, SILVER))
		mlist = GenerateCheckDropMoves<Us, SILVER>()(pos, pos.check_squares(SILVER) & empties, mlist);
	if (hand_exists(h, GOLD))
		mlist = GenerateCheckDropMoves<Us, GOLD>()(pos, pos.check_squares(GOLD) & empties, mlist);
	if (hand_exists(h, BISHOP))
		mlist = GenerateCheckDropMoves<Us, BISHOP>()(pos, pos.check_squares(BISHOP) & empties, mlist);
	if (hand_exists(h, ROOK))
		mlist = GenerateCheckDropMoves<Us, ROOK>()(pos, pos.check_squares(ROOK) & empties, mlist);

	return mlist;
}



template<MOVE_GEN_TYPE GenType, bool All>
ExtMove* generateMoves(const Position& pos, ExtMove* mlist, Square sq = SQ_NB)
{
	return pos.side_to_move() == BLACK ? generate_general<GenType, BLACK, All>(pos, mlist, sq) : generate_general<GenType, WHITE, All>(pos, mlist, sq);
}

template<bool All>
ExtMove* generateEvasionMoves(const Position& pos, ExtMove* mlist)
{
	return pos.side_to_move() == BLACK ? generate_evasions<BLACK, All>(pos, mlist) : generate_evasions<WHITE, All>(pos, mlist);
}

template<MOVE_GEN_TYPE GenType, bool All>
ExtMove* generateChecksMoves(const Position& pos, ExtMove* mlist)
{
	return pos.side_to_move() == BLACK ? generate_checks<GenType, BLACK, All>(pos, mlist) : generate_checks<GenType, WHITE, All>(pos, mlist);
}



template<MOVE_GEN_TYPE GenType>
ExtMove* generateMoves(const Position& pos, ExtMove* mlist, Square recapSq)
{
	const bool All = (GenType == EVASIONS_ALL) || (GenType == CHECKS_ALL)     || (GenType == LEGAL_ALL)
		|| (GenType == NON_EVASIONS_ALL)       || (GenType == RECAPTURES_ALL) || (GenType == QUIET_CHECKS_ALL)
		|| (GenType == CAPTURES_PRO_PLUS_ALL)  || (GenType == NON_CAPTURES_PRO_MINUS_ALL);

	if (GenType == LEGAL || GenType == LEGAL_ALL)
	{

		auto last = pos.in_check() ? generateEvasionMoves<All>(pos, mlist) : generateMoves<NON_EVASIONS, All>(pos, mlist);

		while (mlist != last)
		{
			if (!pos.legal(*mlist))
				mlist->move = (--last)->move;
			else
				++mlist;
		}
		return last;
	}

	if (GenType == CHECKS || GenType == CHECKS_ALL || GenType == QUIET_CHECKS || GenType == QUIET_CHECKS_ALL)
	{
		auto last = generateChecksMoves<GenType, All>(pos, mlist);

		if (pos.in_check())
			while (mlist != last)
			{
				if (!pos.pseudo_legal(*mlist))
					mlist->move = (--last)->move;
				else
					++mlist;
			}
		return last;
	}

	if (GenType == EVASIONS || GenType == EVASIONS_ALL)
		return generateEvasionMoves<All>(pos, mlist);

	const auto GenType2 =
		GenType == NON_EVASIONS_ALL ? NON_EVASIONS :
		GenType == RECAPTURES_ALL   ? RECAPTURES :
		GenType == EVASIONS_ALL     ? EVASIONS :
		GenType == CAPTURES_PRO_PLUS_ALL ? CAPTURES_PRO_PLUS :
		GenType == NON_CAPTURES_PRO_MINUS_ALL ? NON_CAPTURES_PRO_MINUS :
		GenType; 
	return generateMoves<GenType2, All>(pos, mlist, recapSq);
}

template<MOVE_GEN_TYPE GenType>
ExtMove* generateMoves(const Position& pos, ExtMove* mlist)
{
	static_assert(GenType != RECAPTURES && GenType != RECAPTURES_ALL, "RECAPTURES , not allowed.");
	return generateMoves<GenType>(pos, mlist, SQ_NB);
}



template ExtMove* generateMoves<NON_CAPTURES          >(const Position& pos, ExtMove* mlist);
template ExtMove* generateMoves<CAPTURES              >(const Position& pos, ExtMove* mlist);

template ExtMove* generateMoves<NON_CAPTURES_PRO_MINUS    >(const Position& pos, ExtMove* mlist);
template ExtMove* generateMoves<NON_CAPTURES_PRO_MINUS_ALL>(const Position& pos, ExtMove* mlist);

template ExtMove* generateMoves<CAPTURES_PRO_PLUS         >(const Position& pos, ExtMove* mlist);
template ExtMove* generateMoves<CAPTURES_PRO_PLUS_ALL     >(const Position& pos, ExtMove* mlist);

template ExtMove* generateMoves<EVASIONS              >(const Position& pos, ExtMove* mlist);
template ExtMove* generateMoves<EVASIONS_ALL          >(const Position& pos, ExtMove* mlist);

template ExtMove* generateMoves<NON_EVASIONS          >(const Position& pos, ExtMove* mlist);
template ExtMove* generateMoves<NON_EVASIONS_ALL      >(const Position& pos, ExtMove* mlist);

template ExtMove* generateMoves<LEGAL                 >(const Position& pos, ExtMove* mlist);
template ExtMove* generateMoves<LEGAL_ALL             >(const Position& pos, ExtMove* mlist);

template ExtMove* generateMoves<CHECKS                >(const Position& pos, ExtMove* mlist);
template ExtMove* generateMoves<CHECKS_ALL            >(const Position& pos, ExtMove* mlist);

template ExtMove* generateMoves<QUIET_CHECKS          >(const Position& pos, ExtMove* mlist);
template ExtMove* generateMoves<QUIET_CHECKS_ALL      >(const Position& pos, ExtMove* mlist);

template ExtMove* generateMoves<RECAPTURES            >(const Position& pos, ExtMove* mlist, Square recapSq);
template ExtMove* generateMoves<RECAPTURES_ALL        >(const Position& pos, ExtMove* mlist, Square recapSq);
