#ifndef _LONG_EFFECT_H_
#define _LONG_EFFECT_H_


#include "../types.h"

#if defined (LONG_EFFECT_LIBRARY)

struct Bitboard;

namespace Effect8
{




  const uint32_t DIRECTIONS_NB = 256; 

  Directions around8(const Bitboard& b, Square sq);

  extern Directions board_mask_table[SQ_NB_PLUS1];
  inline Directions board_mask(Square sq) { return board_mask_table[sq]; }



  template <Color c> Direct direct_knight_of(Square sq1, Square sq2) {
    return Direct(DIRECT_RUU + (( sq1 < sq2 ) ? 1 : 0) + ((c == WHITE) ? 0 : 2));
  }

  std::ostream& operator<<(std::ostream& os, Directions d);




  static_assert(DIRECT_RU == 0 && DIRECT_RD == 2 && DIRECT_LU == 5 && DIRECT_LD == 7, "ERROR , DIRECT_DIAG");
  inline bool is_diag(Direct d) { return ((d & 1) - (d >> 2))==0; }

  const Square DirectToDelta_[DIRECT_NB_PLUS4] = { SQ_RU,SQ_R,SQ_RD,SQ_U,SQ_D,SQ_LU,SQ_L,SQ_LD,SQ_RUU,SQ_LUU,SQ_RDD,SQ_LDD, };
  inline Square DirectToDelta(Direct d) { ASSERT_LV3(is_ok(d));  return DirectToDelta_[d]; }




  ENABLE_BIT_OPERATORS_ON(Directions)
  ENABLE_RANGE_OPERATORS_ON(Direct, DIRECT_ZERO, DIRECT_NB)


  extern Directions piece_effect_not_table[PIECE_NB][DIRECT_NB];
  inline Directions piece_effect_not(Piece pc, Direct drop_sq) { return piece_effect_not_table[pc][drop_sq]; }

  extern Directions piece_check_around8_table[PIECE_NB];
  inline Directions piece_check_around8(Piece pc) { return piece_check_around8_table[pc]; }

  extern Directions cutoff_directions_table[DIRECT_NB_PLUS4][DIRECTIONS_NB];
  inline Directions cutoff_directions(Direct d1, Directions d2) { return cutoff_directions_table[d1][d2]; }


  void init();
}


namespace Effect24
{

  enum Directions : uint32_t { DIRECTIONS_NB = 1 << 24 };
  
  inline Directions operator |(const Directions d1, const Directions d2) { return Directions(int(d1) + int(d2)); }

  Directions around24(const Bitboard& b, Square sq);

  extern Directions board_mask_table[SQ_NB];
  inline Directions board_mask(Square sq) { return board_mask_table[sq]; }

  std::ostream& operator<<(std::ostream& os, Directions d);


  enum Direct { DIRECT_ZERO = 0, DIRECT_NB = 24 };

  inline bool is_ok(Direct d) { return DIRECT_ZERO <= d && d < DIRECT_NB; }

  inline Direct pop_directions(uint32_t& d) { return (Direct)pop_lsb(d); }

  inline Directions to_directions(Direct d) { return Directions(1 << d); }

  const SquareWithWall DirectToDeltaWW_[DIRECT_NB] = {
    SQWW_RU + SQWW_RU   , SQWW_R  + SQWW_RU   , SQWW_R + SQWW_R , SQWW_R + SQWW_RD  , SQWW_RD   + SQWW_RD    ,
    SQWW_RU + SQWW_U    , SQWW_RU             , SQWW_R          , SQWW_RD           , SQWW_RD   + SQWW_D     ,
    SQWW_U  + SQWW_U    , SQWW_U                                , SQWW_D            , SQWW_D    + SQWW_D     ,
    SQWW_LU + SQWW_U    , SQWW_LU             , SQWW_L          , SQWW_LD           , SQWW_LD   + SQWW_D     ,
    SQWW_LU + SQWW_LU   , SQWW_L  + SQWW_LU   , SQWW_L + SQWW_L , SQWW_L  + SQWW_LD , SQWW_LD   + SQWW_LD    ,
  };
  inline SquareWithWall DirectToDeltaWW(Direct d) { ASSERT_LV3(is_ok(d));  return DirectToDeltaWW_[d]; }

  const Square DirectToDelta_[DIRECT_NB] = {
    SQ_RU + SQ_RU   , SQ_R + SQ_RU   , SQ_R + SQ_R , SQ_R  + SQ_RD  , SQ_RD + SQ_RD  ,
    SQ_RU + SQ_U    , SQ_RU          , SQ_R        , SQ_RD          , SQ_RD + SQ_D   ,
    SQ_U  + SQ_U    , SQ_U                         , SQ_D           , SQ_D  + SQ_D   ,
    SQ_LU + SQ_U    , SQ_LU          , SQ_L        , SQ_LD          , SQ_LD + SQ_D   ,
    SQ_LU + SQ_LU   , SQ_L + SQ_LU   , SQ_L + SQ_L , SQ_L  + SQ_LD  , SQ_LD + SQ_LD  ,
  };
  inline Square DirectToDelta(Direct d) { ASSERT_LV3(is_ok(d));  return DirectToDelta_[d]; }

  extern ymm ymm_direct_to_around8[3];


  void init();
} 


namespace LongEffect
{
  using namespace Effect8;


  struct ByteBoard
  {
    uint8_t effect(Square sq) const { return e[sq]; }

    Directions around8(Square sq) const {


      return (Directions)PEXT32(ymm(&e[sq - SQ_22]).cmp(ymm_zero).to_uint32(), 0b111000000101000000111);
    }

    Directions around8_greater_than_one(Square sq) const
    {
      return (Directions)PEXT32(ymm(&e[sq - SQ_22]).cmp(ymm_one).to_uint32(), 0b111000000101000000111);
    }

	void clear();

    uint8_t padding[SQ_22];

    uint8_t e[SQ_NB_PLUS1];

    uint8_t padding2[32-SQ_22-1];
  };

  std::ostream& operator<<(std::ostream& os, const ByteBoard& board);


  union LongEffect16 {
    Directions dirs[COLOR_NB]; 
    uint16_t u16;              
  };

  const Directions BISHOP_DIR = DIRECTIONS_LU | DIRECTIONS_LD | DIRECTIONS_RU | DIRECTIONS_RD;
  const Directions ROOK_DIR = DIRECTIONS_R | DIRECTIONS_U | DIRECTIONS_D | DIRECTIONS_L;

  const uint16_t long_effect16_table[PIECE_NB] = {
    0,0,DIRECTIONS_U,0,0,BISHOP_DIR,ROOK_DIR,0,0,0,0,0,0,BISHOP_DIR,ROOK_DIR,0,                                          
    0,0,uint16_t(DIRECTIONS_D<<8),0,0,uint16_t(BISHOP_DIR<<8),uint16_t(ROOK_DIR<<8),0,0,0,0,0,0,uint16_t(BISHOP_DIR<<8),uint16_t(ROOK_DIR<<8),0, 
  };
  inline uint16_t long_effect16_of(Piece pc) { return long_effect16_table[pc]; }

  const uint8_t long_effect8_table[PIECE_NB] = {
    0,0,DIRECTIONS_U,0,0,BISHOP_DIR,ROOK_DIR,0,0,0,0,0,0,BISHOP_DIR,ROOK_DIR,0,                                          
    0,0,DIRECTIONS_D,0,0,BISHOP_DIR,ROOK_DIR,0,0,0,0,0,0,BISHOP_DIR,ROOK_DIR,0, 
  };
  inline Directions long_effect_of(Piece pc) { return (Directions)long_effect8_table[pc]; }

  struct WordBoard
  {
	void clear();

    Directions directions_of(Color us , Square sq) const { return le16[sq].dirs[us]; }

    uint16_t long_effect16(Square sq) const { return le16[sq].u16; }

    template <Color Us> Effect24::Directions long_effect24_to_around9(Square sq) const
    {


      const int offset = Us == BLACK ? 0 : 1;
      u32 bits0 = PEXT32((ymm((u8*)&le16[sq - SQ_33 +  0] + offset) & Effect24::ymm_direct_to_around8[0]).eq(ymm_zero).to_uint32(), 0b00000101010101000000000101010101); 
      u32 bits1 = PEXT32((ymm((u8*)&le16[sq - SQ_33 + 16] + offset) & Effect24::ymm_direct_to_around8[1]).eq(ymm_zero).to_uint32(), 0b01010101010000000001010001010000); 
      u32 bits2 = PEXT32((ymm((u8*)&le16[sq - SQ_33 + 32] + offset) & Effect24::ymm_direct_to_around8[2]).eq(ymm_zero).to_uint32(), 0b00000000000000010101010100000000); 
      return Effect24::Directions(~(bits0 + (bits1 << 10) + (bits2 << 19)));
    }

    LongEffect16 padding[SQ_33];

    LongEffect16 le16[SQ_NB_PLUS1];

    LongEffect16 padding2[48 - SQ_33 - 1];

  };

  std::ostream& operator<<(std::ostream& os, const WordBoard& board);


  void calc_effect(Position& pos);


  template <Color Us> void update_by_dropping_piece(Position& pos, Square to, Piece moved_pc);

  template <Color Us> void update_by_capturing_piece(Position& pos, Square from , Square to, Piece moved_pc, Piece moved_after_pc,Piece captured_pc);

  template <Color Us> void update_by_no_capturing_piece(Position& pos, Square from, Square to, Piece moved_pc, Piece moved_after_pc);



  template <Color Us> void rewind_by_dropping_piece(Position& pos, Square to, Piece moved_pc);
  template <Color Us> void rewind_by_capturing_piece(Position& pos, Square from, Square to, Piece moved_pc, Piece moved_after_pc, Piece captured_pc);
  template <Color Us> void rewind_by_no_capturing_piece(Position& pos, Square from, Square to, Piece moved_pc, Piece moved_after_pc);

  void init();
}

#endif 

#endif 

