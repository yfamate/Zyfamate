#ifndef _TYPES_H_INCLUDED
#define _TYPES_H_INCLUDED


#include "config.h"



#include "extra/bitop.h"

#include <iostream>     
#include <string>       
#include <algorithm>    
#include <climits>		


enum Color { BLACK=0,WHITE=1,COLOR_NB  , COLOR_ZERO = 0,};

constexpr Color operator ~(Color c) { return (Color)(c ^ 1);  }

constexpr bool is_ok(Color c) { return COLOR_ZERO <= c && c < COLOR_NB; }

std::ostream& operator<<(std::ostream& os, Color c);


enum File : int { FILE_1, FILE_2, FILE_3, FILE_4, FILE_5, FILE_6, FILE_7, FILE_8, FILE_9 , FILE_NB , FILE_ZERO=0 };

constexpr bool is_ok(File f) { return FILE_ZERO <= f && f < FILE_NB; }

constexpr File toFile(char c) { return (File)(c - '1'); }

std::string pretty(File f);

static std::ostream& operator<<(std::ostream& os, File f) { os << (char)('1' + f); return os; }


enum Rank : int { RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9 , RANK_NB , RANK_ZERO = 0};

constexpr bool is_ok(Rank r) { return RANK_ZERO <= r && r < RANK_NB; }

constexpr bool canPromote(const Color c, const Rank fromOrToRank) {
	return static_cast<bool>(0x1c00007u & (1u << ((c << 4) + fromOrToRank)));
}

constexpr Rank relative_rank(Color c, Rank r) { return c == BLACK ? r : (Rank)(8 - r); }

constexpr Rank toRank(char c) { return (Rank)(c - 'a'); }

std::string pretty(Rank r);

static std::ostream& operator<<(std::ostream& os, Rank r) { os << (char)('a' + r); return os; }


enum Square: int32_t
{
	SQ_11, SQ_12, SQ_13, SQ_14, SQ_15, SQ_16, SQ_17, SQ_18, SQ_19,
	SQ_21, SQ_22, SQ_23, SQ_24, SQ_25, SQ_26, SQ_27, SQ_28, SQ_29,
	SQ_31, SQ_32, SQ_33, SQ_34, SQ_35, SQ_36, SQ_37, SQ_38, SQ_39,
	SQ_41, SQ_42, SQ_43, SQ_44, SQ_45, SQ_46, SQ_47, SQ_48, SQ_49,
	SQ_51, SQ_52, SQ_53, SQ_54, SQ_55, SQ_56, SQ_57, SQ_58, SQ_59,
	SQ_61, SQ_62, SQ_63, SQ_64, SQ_65, SQ_66, SQ_67, SQ_68, SQ_69,
	SQ_71, SQ_72, SQ_73, SQ_74, SQ_75, SQ_76, SQ_77, SQ_78, SQ_79,
	SQ_81, SQ_82, SQ_83, SQ_84, SQ_85, SQ_86, SQ_87, SQ_88, SQ_89,
	SQ_91, SQ_92, SQ_93, SQ_94, SQ_95, SQ_96, SQ_97, SQ_98, SQ_99,

	SQ_ZERO = 0, SQ_NB = 81,
	SQ_NB_PLUS1 = SQ_NB + 1, 

	SQ_D = +1, 
	SQ_R = -9, 
	SQ_U = -1, 
	SQ_L = +9, 

	SQ_RU = int(SQ_U) + int(SQ_R), 
	SQ_RD = int(SQ_D) + int(SQ_R), 
	SQ_LU = int(SQ_U) + int(SQ_L), 
	SQ_LD = int(SQ_D) + int(SQ_L), 
	SQ_RUU = int(SQ_RU) + int(SQ_U), 
	SQ_LUU = int(SQ_LU) + int(SQ_U), 
	SQ_RDD = int(SQ_RD) + int(SQ_D), 
	SQ_LDD = int(SQ_LD) + int(SQ_D), 
};

constexpr bool is_ok(Square sq) { return SQ_ZERO <= sq && sq <= SQ_NB; }

constexpr bool is_ok_plus1(Square sq) { return SQ_ZERO <= sq && sq < SQ_NB_PLUS1; }

constexpr File SquareToFile[SQ_NB_PLUS1] =
{
	FILE_1, FILE_1, FILE_1, FILE_1, FILE_1, FILE_1, FILE_1, FILE_1, FILE_1,
	FILE_2, FILE_2, FILE_2, FILE_2, FILE_2, FILE_2, FILE_2, FILE_2, FILE_2,
	FILE_3, FILE_3, FILE_3, FILE_3, FILE_3, FILE_3, FILE_3, FILE_3, FILE_3,
	FILE_4, FILE_4, FILE_4, FILE_4, FILE_4, FILE_4, FILE_4, FILE_4, FILE_4,
	FILE_5, FILE_5, FILE_5, FILE_5, FILE_5, FILE_5, FILE_5, FILE_5, FILE_5,
	FILE_6, FILE_6, FILE_6, FILE_6, FILE_6, FILE_6, FILE_6, FILE_6, FILE_6,
	FILE_7, FILE_7, FILE_7, FILE_7, FILE_7, FILE_7, FILE_7, FILE_7, FILE_7,
	FILE_8, FILE_8, FILE_8, FILE_8, FILE_8, FILE_8, FILE_8, FILE_8, FILE_8,
	FILE_9, FILE_9, FILE_9, FILE_9, FILE_9, FILE_9, FILE_9, FILE_9, FILE_9,
	FILE_NB, 
};

constexpr File file_of(Square sq) {   return SquareToFile[sq]; }

constexpr Rank SquareToRank[SQ_NB_PLUS1] =
{
	RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9,
	RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9,
	RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9,
	RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9,
	RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9,
	RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9,
	RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9,
	RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9,
	RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9,
	RANK_NB, 
};

constexpr Rank rank_of(Square sq) {   return SquareToRank[sq]; }

constexpr Square operator | (File f, Rank r) { Square sq = (Square)(f * 9 + r);  return sq; }

constexpr int dist(Square sq1, Square sq2) { return (!is_ok(sq1) || !is_ok(sq2)) ? INT_MAX : std::max(abs(file_of(sq1) - file_of(sq2)), abs(rank_of(sq1) - rank_of(sq2))); }

constexpr bool canPromote(const Color c, const Square fromOrTo) {
	ASSERT_LV2(is_ok(fromOrTo));
	return canPromote(c, rank_of(fromOrTo));
}

constexpr bool canPromote(const Color c, const Square from, const Square to)
{
  return canPromote(c, from) || canPromote(c, to);
}

constexpr Square Inv(Square sq) { return (Square)((SQ_NB - 1) - sq); }

constexpr Square Mir(Square sq) { return File(8-(int)file_of(sq)) | rank_of(sq); }

constexpr Square Flip(Square sq) { return (Square)(SQ_NB - sq - 1); }

static std::string pretty(Square sq) { return pretty(file_of(sq)) + pretty(rank_of(sq)); }


static std::ostream& operator<<(std::ostream& os, Square sq) { os << file_of(sq) << rank_of(sq); return os; }




enum SquareWithWall: int32_t {
	SQWW_R = SQ_R - (1 << 9) + (1 << 24), SQWW_U = SQ_U - (1 << 14) + (1 << 19), SQWW_D = -int(SQWW_U), SQWW_L = -int(SQWW_R),
	SQWW_RU = int(SQWW_R) + int(SQWW_U), SQWW_RD = int(SQWW_R) + int(SQWW_D), SQWW_LU = int(SQWW_L) + int(SQWW_U), SQWW_LD = int(SQWW_L) + int(SQWW_D),

	SQWW_11 = SQ_11 | (1 << 8)  | (0 << 9)  | (0 << 14)  | (8 << 19)  | (8 << 24) ,

	SQWW_BORROW_MASK = (1 << 13) | (1 << 18) | (1 << 23) | (1 << 28),
};

constexpr Square sqww_to_sq(SquareWithWall sqww) { return Square(sqww & 0xff); }

extern SquareWithWall sqww_table[SQ_NB_PLUS1];

static SquareWithWall to_sqww(Square sq) { return sqww_table[sq]; }

constexpr bool is_ok(SquareWithWall sqww) { return (sqww & SQWW_BORROW_MASK) == 0; }

static std::ostream& operator<<(std::ostream& os, SquareWithWall sqww) { os << sqww_to_sq(sqww); return os; }


namespace Effect8
{
	enum Directions: uint8_t {
		DIRECTIONS_ZERO  = 0, DIRECTIONS_RU = 1, DIRECTIONS_R = 2, DIRECTIONS_RD = 4,
		DIRECTIONS_U     = 8, DIRECTIONS_D = 16, DIRECTIONS_LU = 32, DIRECTIONS_L = 64, DIRECTIONS_LD = 128,
		DIRECTIONS_CROSS = DIRECTIONS_U  | DIRECTIONS_D  | DIRECTIONS_R  | DIRECTIONS_L,
		DIRECTIONS_DIAG  = DIRECTIONS_RU | DIRECTIONS_RD | DIRECTIONS_LU | DIRECTIONS_LD,
	};

	extern Directions direc_table[SQ_NB_PLUS1][SQ_NB_PLUS1];
	static Directions directions_of(Square sq1, Square sq2) { return direc_table[sq1][sq2]; }

	enum Direct {
		DIRECT_RU, DIRECT_R, DIRECT_RD, DIRECT_U, DIRECT_D, DIRECT_LU, DIRECT_L, DIRECT_LD,
		DIRECT_NB, DIRECT_ZERO = 0, DIRECT_RUU = 8, DIRECT_LUU, DIRECT_RDD, DIRECT_LDD, DIRECT_NB_PLUS4
	};

	static Direct pop_directions(Directions& d) { return (Direct)pop_lsb(d); }

	constexpr Direct operator~(Direct d) {
		static_assert(Effect8::DIRECT_R == 1, "");
		static_assert(Effect8::DIRECT_L == 6, "");
		return Direct(7 - d);
	}

	constexpr Directions to_directions(Direct d) { return Directions(1 << d); }

	constexpr bool is_ok(Direct d) { return DIRECT_ZERO <= d && d < DIRECT_NB_PLUS4; }

	constexpr SquareWithWall DirectToDeltaWW_[DIRECT_NB] = { SQWW_RU,SQWW_R,SQWW_RD,SQWW_U,SQWW_D,SQWW_LU,SQWW_L,SQWW_LD, };
	constexpr SquareWithWall DirectToDeltaWW(Direct d) {  return DirectToDeltaWW_[d]; }
}

static bool aligned(Square sq1, Square sq2, Square sq3)
{
	auto d1 = Effect8::directions_of(sq1, sq3);
	return d1 ? d1 == Effect8::directions_of(sq2, sq3) : false;
}


constexpr int MAX_PLY = MAX_PLY_NUM;

typedef int Depth;

enum : int {

	DEPTH_QS_CHECKS = 0,

	DEPTH_QS_NO_CHECKS = -1,

	DEPTH_QS_RECAPTURES = -5,

	DEPTH_NONE = -6,

	DEPTH_OFFSET = -7
};


enum Bound {
	BOUND_NONE,  
	BOUND_UPPER, 
	BOUND_LOWER, 
	BOUND_EXACT = BOUND_UPPER | BOUND_LOWER 
};


enum Value: int32_t
{
	VALUE_ZERO = 0,

	VALUE_MATE = 32000,

	VALUE_INFINITE = 32001,

	VALUE_NONE = 32002,

	VALUE_TB_WIN_IN_MAX_PLY  =  VALUE_MATE -  1 * MAX_PLY,
	VALUE_TB_LOSS_IN_MAX_PLY = -VALUE_TB_WIN_IN_MAX_PLY, 

	VALUE_MATE_IN_MAX_PLY  =   VALUE_MATE - MAX_PLY , 
	VALUE_MATED_IN_MAX_PLY = -VALUE_MATE_IN_MAX_PLY , 


	VALUE_KNOWN_WIN = int(VALUE_MATE_IN_MAX_PLY) - 1000,

	VALUE_SUPERIOR = 28000,

	VALUE_MAX_EVAL = 27000,

	VALUE_NOT_EVALUATED = INT32_MAX,
};

constexpr Value mate_in(int ply) {  return (Value)(VALUE_MATE - ply);}

constexpr Value mated_in(int ply) {  return (Value)(-VALUE_MATE + ply);}



extern const char* USI_PIECE;

enum PieceType : uint32_t
{
	NO_PIECE_TYPE, PAWN, LANCE, KNIGHT, SILVER, BISHOP, ROOK, GOLD,
	KING = 8, PRO_PAWN , PRO_LANCE , PRO_KNIGHT , PRO_SILVER , HORSE, DRAGON, QUEEN,

	PIECE_HAND_ZERO = PAWN, 
	PIECE_HAND_NB = KING,   

	PIECE_TYPE_PROMOTE = 8, 
	PIECE_TYPE_NB = 16, 


	GPM_BR = 16,     
	GPM_GBR = 17,     
	GPM_GHD = 18,     
	GPM_GHDK = 19,     

	ALL_PIECES = 0,			
	GOLDS = QUEEN,			
	HDK,				    
	BISHOP_HORSE,			
	ROOK_DRAGON,			
	SILVER_HDK,				
	GOLDS_HDK,				
	PIECE_BB_NB,			
};

enum Piece : uint32_t
{
	NO_PIECE = 0,

	B_PAWN = 1, B_LANCE, B_KNIGHT, B_SILVER, B_BISHOP, B_ROOK, B_GOLD, B_KING, B_PRO_PAWN, B_PRO_LANCE, B_PRO_KNIGHT, B_PRO_SILVER, B_HORSE, B_DRAGON, B_QUEEN,
	W_PAWN = 17, W_LANCE, W_KNIGHT, W_SILVER, W_BISHOP, W_ROOK, W_GOLD, W_KING, W_PRO_PAWN, W_PRO_LANCE, W_PRO_KNIGHT, W_PRO_SILVER, W_HORSE, W_DRAGON, W_QUEEN,
	PIECE_NB, 
	PIECE_ZERO = 0,


	PIECE_PROMOTE = 8, 
	PIECE_WHITE = 16,  
	PIECE_RAW_NB = 8,  
};

static std::string usi_piece(Piece pc) { return std::string((pc & 32) ? "D":"")
		  + std::string(USI_PIECE).substr((pc & 31) * 2, 2); }

constexpr Color color_of(Piece pc)
{
	static_assert(PIECE_WHITE == 16 && WHITE == 1 && BLACK == 0, "");
	return (Color)((pc & PIECE_WHITE) >> 4);
}

constexpr PieceType type_of(Piece pc) { return (PieceType)(pc & 15); }

constexpr PieceType raw_type_of(Piece pc) { return (PieceType)(pc & 7); }

constexpr Piece raw_of(Piece pc) { return (Piece)(pc & ~8); }

constexpr Piece make_piece(Color c, PieceType pt) {  return (Piece)((c << 4) + pt); }

constexpr bool has_long_effect(Piece pc) { return (type_of(pc) == LANCE) || (((pc+1) & 6)==6); }

constexpr bool is_ok(Piece pc) { return NO_PIECE <= pc && pc < PIECE_NB; }

std::string pretty(Piece pc);
static std::string pretty(PieceType pt) { return pretty((Piece)pt); }

static std::string pretty2(Piece pc) { ASSERT_LV1(color_of(pc) == BLACK); auto s = pretty(pc); return s.substr(1, s.length() - 1); }
static std::string pretty2(PieceType pt) { return pretty2((Piece)pt); }

extern std::string PieceToCharBW;

std::ostream& operator<<(std::ostream& os, Piece pc);
static std::ostream& operator<<(std::ostream& os, PieceType pc) { return os << (Piece)pc; }


enum PieceNumber : u8
{
  PIECE_NUMBER_PAWN = 0, PIECE_NUMBER_LANCE = 18, PIECE_NUMBER_KNIGHT = 22, PIECE_NUMBER_SILVER = 26,
  PIECE_NUMBER_GOLD = 30, PIECE_NUMBER_BISHOP = 34, PIECE_NUMBER_ROOK = 36, PIECE_NUMBER_KING = 38,
  PIECE_NUMBER_BKING = 38, PIECE_NUMBER_WKING = 39, 
  PIECE_NUMBER_ZERO = 0, PIECE_NUMBER_NB = 40,
};

constexpr bool is_ok(PieceNumber pn) { return pn < PIECE_NUMBER_NB; }


struct Move16;

enum Move: uint32_t {

	MOVE_NONE    = 0,             

	MOVE_NULL    = (1 << 7) + 1,  
	MOVE_RESIGN  = (2 << 7) + 2,  
	MOVE_WIN     = (3 << 7) + 3,  

	MOVE_DROP    = 1 << 14,       
	MOVE_PROMOTE = 1 << 15,       
};

std::string to_usi_string(Move m);
std::string to_usi_string(Move16 m);

struct Move16
{
	constexpr Move16():move(0){}
	constexpr Move16(u16 m): move(m) {}

	constexpr Move16(Move m) :move((u16)m){}

	constexpr uint16_t to_u16() const { return (u16)move; }

	constexpr bool operator == (const Move16 rhs) const { return move == rhs.move; }
	constexpr bool operator != (const Move16 rhs) const { return !(*this == rhs); }
	constexpr bool operator == (const Move rhs) const { return move == (u16)rhs; }
	constexpr bool operator != (const Move rhs) const { return !(*this == rhs); }

	std::string to_usi_string() const { return ::to_usi_string(move); }

	constexpr bool is_ok() const { return (move >> 7) != (move & 0x7f); }

private:
	uint16_t move;
};

static std::ostream& operator<<(std::ostream& os, Move m)   { os << to_usi_string(m); return os; }
static std::ostream& operator<<(std::ostream& os, Move16 m) { os << to_usi_string(m); return os; }

constexpr Square from_sq(Move   m) { return Square((m          >> 7) & 0x7f); }
static    Square from_sq(Move16 m) { return Square((m.to_u16() >> 7) & 0x7f); }

constexpr Square to_sq(Move   m) { return Square(m          & 0x7f); }
static    Square to_sq(Move16 m) { return Square(m.to_u16() & 0x7f); }

constexpr bool is_drop(Move   m){ return (m          & MOVE_DROP)!=0; }
static    bool is_drop(Move16 m){ return (m.to_u16() & MOVE_DROP)!=0; }

constexpr int from_to(Move   m) { return (int)(from_sq(m) + (is_drop(m) ? (SQ_NB - 1) : 0)) * (int)SQ_NB + (int)to_sq(m); }
static    int from_to(Move16 m) { return (int)(from_sq(m) + (is_drop(m) ? (SQ_NB - 1) : 0)) * (int)SQ_NB + (int)to_sq(m); }

constexpr bool is_promote(Move   m) { return (m          & MOVE_PROMOTE)!=0; }
static    bool is_promote(Move16 m) { return (m.to_u16() & MOVE_PROMOTE)!=0; }

constexpr PieceType move_dropped_piece(Move   m) { return (PieceType)((m          >> 7) & 0x7f); }
static    PieceType move_dropped_piece(Move16 m) { return (PieceType)((m.to_u16() >> 7) & 0x7f); }

static Move16 make_move16(Square from, Square to) { return Move16(to + (from << 7)); }
constexpr Move make_move(Square from, Square to, Piece pc ) { return Move(to + (from << 7) + (pc << 16)) ; }
constexpr Move make_move(Square from, Square to, Color us , PieceType pt ) { return Move(to + (from << 7) + (((us ? u32(PIECE_WHITE) : 0) + (pt)) << 16)) ; }

static Move16 make_move_promote16(Square from, Square to) { return (Move16)(to + (from << 7) + MOVE_PROMOTE); }
constexpr Move make_move_promote(Square from, Square to , Piece pc) { return (Move)(to + (from << 7) + MOVE_PROMOTE + ((pc | PIECE_PROMOTE) << 16)); }
constexpr Move make_move_promote(Square from, Square to , Color us , PieceType pt) { return (Move)(to + (from << 7) + MOVE_PROMOTE + (((us ? u32(PIECE_WHITE) : 0) + (pt | PIECE_PROMOTE)) << 16)); }

static Move16 make_move_drop16(PieceType pt, Square to) { return (Move16)(to + (pt << 7) + MOVE_DROP); }
constexpr Move make_move_drop(PieceType pt, Square to , Color us ) { return (Move)(to + (pt << 7) + MOVE_DROP + (((us ? u32(PIECE_WHITE) : 0) + (pt)) << 16)); }

static Move16 reverse_move(Move m) { return make_move16(to_sq(m), from_sq(m)); }

constexpr bool is_ok(Move m) {
  return (m >> 7) != (m & 0x7f);
}

static bool is_ok(Move16 m) { return m.is_ok(); }

std::string pretty(Move m);

std::string pretty(Move m, Piece movedPieceType);
static std::string pretty(Move m, PieceType movedPieceType) { return pretty(m, (Piece)movedPieceType); }

enum MoveType {
	NORMAL,
	PROMOTION = MOVE_PROMOTE,
	DROP = MOVE_DROP,
};

constexpr MoveType type_of(Move m) { return MoveType(m & (MOVE_PROMOTE | MOVE_DROP)); }


struct ExtMove {

	Move move;  
	int value;	


	operator Move() const { return move; }
	void operator=(Move m) { move = m; }

	operator float() const = delete;
};

constexpr bool operator<(const ExtMove& first, const ExtMove& second) {
	return first.value < second.value;
}

static std::ostream& operator<<(std::ostream& os, ExtMove m) { os << m.move << '(' << m.value << ')'; return os; }


enum Hand : uint32_t { HAND_ZERO = 0, };

constexpr int PIECE_BITS[PIECE_HAND_NB] = { 0, 0 , 8 , 12 , 16 , 20 , 24  , 28  };

constexpr Hand PIECE_TO_HAND[PIECE_HAND_NB] = {
	(Hand)0,
	(Hand)(1 << PIECE_BITS[PAWN])   ,
	(Hand)(1 << PIECE_BITS[LANCE])  ,
	(Hand)(1 << PIECE_BITS[KNIGHT]) ,
	(Hand)(1 << PIECE_BITS[SILVER]) ,
	(Hand)(1 << PIECE_BITS[BISHOP]) ,
	(Hand)(1 << PIECE_BITS[ROOK])   ,
	(Hand)(1 << PIECE_BITS[GOLD])   
};

constexpr int PIECE_BIT_MASK[PIECE_HAND_NB] = { 0,31,7,7,7,3,3,7 };

constexpr u32 PIECE_BIT_MASK2[PIECE_HAND_NB] = {
	0,
	PIECE_BIT_MASK[PAWN]   << PIECE_BITS[PAWN]  ,
	PIECE_BIT_MASK[LANCE]  << PIECE_BITS[LANCE] ,
	PIECE_BIT_MASK[KNIGHT] << PIECE_BITS[KNIGHT],
	PIECE_BIT_MASK[SILVER] << PIECE_BITS[SILVER],
	PIECE_BIT_MASK[BISHOP] << PIECE_BITS[BISHOP],
	PIECE_BIT_MASK[ROOK]   << PIECE_BITS[ROOK]  ,
	PIECE_BIT_MASK[GOLD]   << PIECE_BITS[GOLD]
};

constexpr u32 HAND_BIT_MASK =
	PIECE_BIT_MASK2[PAWN]   |
	PIECE_BIT_MASK2[LANCE]  |
	PIECE_BIT_MASK2[KNIGHT] |
	PIECE_BIT_MASK2[SILVER] |
	PIECE_BIT_MASK2[BISHOP] |
	PIECE_BIT_MASK2[ROOK]   |
	PIECE_BIT_MASK2[GOLD];

constexpr u32 HAND_BORROW_MASK = (HAND_BIT_MASK << 1) & ~HAND_BIT_MASK;


constexpr int hand_count(Hand hand, PieceType pr) {  return (int)(hand >> PIECE_BITS[pr]) & PIECE_BIT_MASK[pr]; }

constexpr u32 hand_exists(Hand hand, PieceType pr) {  return hand & PIECE_BIT_MASK2[pr]; }

constexpr u32 hand_except_pawn_exists(Hand hand) { return hand & (HAND_BIT_MASK ^ PIECE_BIT_MASK2[PAWN]); }

constexpr void add_hand(Hand &hand, PieceType pr, int c = 1) { hand = (Hand)(hand + PIECE_TO_HAND[pr] * c); }

constexpr void sub_hand(Hand &hand, PieceType pr, int c = 1) { hand = (Hand)(hand - PIECE_TO_HAND[pr] * c); }


constexpr bool hand_is_equal_or_superior(Hand h1, Hand h2) { return ((h1-h2) & HAND_BORROW_MASK) == 0; }

std::ostream& operator<<(std::ostream& os, Hand hand);


#if defined(LONG_EFFECT_LIBRARY)


enum HandKind : uint32_t {
	HAND_KIND_PAWN = 1 << (PAWN - 1), HAND_KIND_LANCE = 1 << (LANCE - 1), HAND_KIND_KNIGHT = 1 << (KNIGHT - 1),
	HAND_KIND_SILVER = 1 << (SILVER - 1), HAND_KIND_BISHOP = 1 << (BISHOP - 1), HAND_KIND_ROOK = 1 << (ROOK - 1), HAND_KIND_GOLD = 1 << (GOLD - 1),
	HAND_KIND_KING = 1 << (KING - 1), HAND_KIND_ZERO = 0,
};

static HandKind toHandKind(Hand h) { return (HandKind)PEXT32(h + HAND_BIT_MASK, HAND_BORROW_MASK); }

constexpr bool hand_exists(HandKind hk, Piece pt) {  return static_cast<bool>(hk & (1 << (pt - 1))); }

constexpr bool hand_exceptPawnExists(HandKind hk) { return hk & ~HAND_KIND_PAWN; }


#endif


constexpr int MAX_MOVES = 600;

enum MOVE_GEN_TYPE
{

	NON_CAPTURES,           
	CAPTURES,               

	CAPTURES_PRO_PLUS,      
	NON_CAPTURES_PRO_MINUS, 



	CAPTURES_PRO_PLUS_ALL,      
	NON_CAPTURES_PRO_MINUS_ALL, 
	
	EVASIONS,              
	EVASIONS_ALL,          

	NON_EVASIONS,          
	NON_EVASIONS_ALL,      

	LEGAL,                 
	LEGAL_ALL,             

	CHECKS,                
	CHECKS_ALL,            

	QUIET_CHECKS,          
	QUIET_CHECKS_ALL,      


	RECAPTURES,            
	RECAPTURES_ALL,        

	QUIETS = NON_CAPTURES, 
};

class Position; 

struct CheckInfo;
template <MOVE_GEN_TYPE gen_type> ExtMove* generateMoves(const Position& pos, ExtMove* mlist);
template <MOVE_GEN_TYPE gen_type> ExtMove* generateMoves(const Position& pos, ExtMove* mlist,Square recapSq); 

template<MOVE_GEN_TYPE GenType>
struct MoveList {

	explicit MoveList(const Position& pos) : last(generateMoves<GenType>(pos, mlist)) {}

	const ExtMove* begin() const { return mlist; }

	const ExtMove* end() const { return last; }

	bool contains(Move move) const {
		return std::find(begin(), end(), move) != end();
	}

	size_t size() const { return last - mlist; }

	const ExtMove at(size_t i) const { ASSERT_LV3(i < size()); return begin()[i]; }

private:
	ExtMove mlist[MAX_MOVES], *last;
};


typedef uint64_t Key;

constexpr Key make_key(uint64_t seed) {
	return (seed * 6364136223846793005ULL + 1442695040888963407ULL) & ~1ULL;
}


enum EnteringKingRule
{
	EKR_NONE,            
	EKR_24_POINT,        
	EKR_27_POINT,        
	EKR_TRY_RULE,        
};

enum RepetitionState
{
	REPETITION_NONE,     
	REPETITION_WIN,      
	REPETITION_LOSE,     
	REPETITION_DRAW,     
	REPETITION_SUPERIOR, 
	REPETITION_INFERIOR, 
	REPETITION_NB,
};

constexpr bool is_ok(RepetitionState rs) { return REPETITION_NONE <= rs && rs < REPETITION_NB; }

extern std::string to_usi_string(RepetitionState rs);

std::ostream& operator<<(std::ostream& os, RepetitionState rs);

extern Value drawValueTable[REPETITION_NB][COLOR_NB];
static Value draw_value(RepetitionState rs, Color c) {  return drawValueTable[rs][c]; }


namespace Eval
{
	enum BonaPiece: int32_t;

	Value evaluate(const Position& pos);
}


#include "extra/macros.h"


#endif 
