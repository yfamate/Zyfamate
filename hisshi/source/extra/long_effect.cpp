#include "long_effect.h"



#if defined(LONG_EFFECT_LIBRARY)

#include "../bitboard.h"
#include "../position.h"

#include <cstring>	

std::ostream& output_around_n(std::ostream& os, uint32_t d,int n)
{
  int c = n / 2;
  for (int j = -c; j <= +c; ++j)
  {
    for (int i = -c; i <= +c; ++i)
    {
      int bit = (j + c) + (-i + c) * n - (((i < 0) || (i == 0 && j >= 0)) ? 1 : 0);
      os << ((i == 0 && j == 0) ? '+' : (((1 << bit) & d) ? '*' : '.'));
    }
    os << std::endl;
  }
  return os;
}


namespace Effect8
{
  Directions board_mask_table[SQ_NB_PLUS1];
  Directions piece_effect_not_table[PIECE_NB][DIRECT_NB];
  Directions piece_check_around8_table[PIECE_NB];
  Directions cutoff_directions_table[DIRECT_NB_PLUS4][DIRECTIONS_NB];

  void init()
  {
    
    for (auto sq : SQ)
      board_mask_table[sq] = around8(kingEffect(sq), sq);


    for (auto pc : Piece())
      for (auto d : Direct())
      {
        auto effect = effects_from(pc, SQ_22 + DirectToDelta(d), ZERO_BB);
        auto effect8_not = ~around8(effect, SQ_22);

        piece_effect_not_table[pc][d] = effect8_not;
      }


    for (auto pc : Piece())
    {
      auto effect = effects_from(Piece(pc ^ PIECE_WHITE), SQ_22,ZERO_BB);
      auto effect8 = around8(effect, SQ_22);
      piece_check_around8_table[pc] = effect8;
    }


    for (int d1 = 0; d1 < DIRECT_NB_PLUS4; ++d1)
      for (uint16_t d2 = 0; d2 < 0x100; ++d2)
      {
        Bitboard bb = ZERO_BB;


        auto d2t = Directions(d2);
        while (d2t)
        {
          auto dir = pop_directions(d2t);

          auto sq = to_sqww(SQ_55 + DirectToDelta(Direct(d1)));
          auto delta = DirectToDeltaWW(dir);
          
          for (int i = 0; i < 3; ++i)
          {
            sq += delta;
            if (!is_ok(sq)) break;
            bb ^= sqww_to_sq(sq);
          }
        }
        
        auto effect8 = around8(bb, SQ_55);

        cutoff_directions_table[d1][d2] = effect8;
      }

  }

  Directions around8(const Bitboard& b, Square sq)
  {

    auto t = uint32_t((sq < SQ_32) ? (b.p[0] << int(SQ_32 - sq)) :
      ((b.p[0] >> int(sq - SQ_32)) | (b.p[1] << int(SQ_92 + SQ_L - sq)))); 

    return (Directions)PEXT32(t, 0b111000000101000000111000000000);
  }

  std::ostream& operator<<(std::ostream& os, Directions d) { return output_around_n(os, d, 3); }
}


namespace Effect24
{
  Directions board_mask_table[SQ_NB];
  ymm ymm_direct_to_around8[3];

  void init()
  {

    for (auto sq : SQ)
      board_mask_table[sq] = around24(around24_bb(sq), sq);

    {
      u16 d81[81];
      memset(d81, 0, sizeof(d81));
      auto sq33_around9 = kingEffect(SQ_33) ^ SQ_33;

      for (auto sq : SQ)
      {
        Effect8::Directions d = Effect8::DIRECTIONS_ZERO;
        for (int i = 0; i < 8; ++i)
        {
          auto dir = Effect8::Direct(i);
          auto delta = Effect8::DirectToDeltaWW(dir);

          auto sqww = to_sqww(sq) + delta;
          if (!is_ok(sqww))
            continue;
          if (sq33_around9 & sqww_to_sq(sqww))
            d |= Effect8::to_directions(dir);
        }
        d81[sq] = d;
      }
      for (int i = 0; i < 3; ++i)
        ymm_direct_to_around8[i] = ymm(&d81[i * 16]); 
    }

  }

  Directions around24(const Bitboard& b, Square sq)
  {
    auto t = (sq < SQ_33) ? (b.p[0] << int(SQ_33 - sq)) :
      ((b.p[0] >> int(sq - SQ_33)) | (b.p[1] << int(SQ_93 + SQ_L - sq))); 

    return (Directions)PEXT64(t, 0b11111000011111000011011000011111000011111);
  }

  std::ostream& operator<<(std::ostream& os, Directions d) { return output_around_n(os, d, 5); }
}


namespace LongEffect
{

  std::ostream& operator<<(std::ostream& os, const ByteBoard& board)
  {
    for (auto r : Rank())
    {
      for (File f = FILE_9; f >= FILE_1; --f)
      {
        int e = uint8_t(board.e[f | r]);
        if (e < 16)
          os << "0123456789ABCDEF"[e];
        else
          os << "[" << e << "]"; 
      }
      os << std::endl;
    }
    return os;
  }

  void ByteBoard::clear() { memset(e, 0, sizeof(e)); }


  std::ostream& operator<<(std::ostream& os, const WordBoard& board)
  {
    for (auto r : Rank())
    {
      for (File f = FILE_9; f >= FILE_1; --f)
      {
        auto e = board.le16[f | r];
        os << '[';
        int i;
        for (i = 0; i < 4; ++i)
          if (e.u16)
            os << "01234567abcdefgh"[pop_lsb(e.u16)]; 
          else
            os << ' ';
        os << ']';
      }
      os << std::endl;
    }
    return os;
  }

  void WordBoard::clear() { memset(le16, 0, sizeof(le16)); }


  void calc_effect(Position& pos)
  {
    using namespace Effect8;

    auto& board_effect = pos.board_effect;
    auto& long_effect = pos.long_effect;

    for (auto c : COLOR) board_effect[c].clear();
    long_effect.clear();

    for (auto sq : pos.pieces())
    {
      Piece pc = pos.piece_on(sq);
      auto effect = effects_from(pc, sq, pos.pieces());
      Color c = color_of(pc);
      for (auto to : effect)
        ADD_BOARD_EFFECT(c, to, 1);
      if (has_long_effect(pc))
      {
        if (type_of(pc) != LANCE)
          effect = effects_from(Piece(pc & ~PIECE_PROMOTE), sq, pos.pieces());

        for (auto to : effect)
        {
          auto dir = directions_of(sq, to);
          long_effect.le16[to].dirs[c] ^= dir;
        }
      }
    }


  }


  using namespace Effect8;
 
  inline Bitboard short_effects_from(Piece pc,Square sq)
  {
    switch (pc)
    {
    case B_PAWN: return pawnEffect(BLACK, sq);
    case W_PAWN: return pawnEffect(WHITE, sq);

    case B_KNIGHT: return knightEffect(BLACK, sq);
    case W_KNIGHT: return knightEffect(WHITE, sq);

    case B_SILVER: return silverEffect(BLACK, sq);
    case W_SILVER: return silverEffect(WHITE, sq);

    case B_GOLD: case B_PRO_PAWN: case B_PRO_LANCE: case B_PRO_KNIGHT: case B_PRO_SILVER: return goldEffect(BLACK, sq);
    case W_GOLD: case W_PRO_PAWN: case W_PRO_LANCE: case W_PRO_KNIGHT: case W_PRO_SILVER: return goldEffect(WHITE, sq);
      
    case B_HORSE : case W_HORSE:
      return cross00StepEffect(sq);

    case B_DRAGON : case W_DRAGON:
      return cross45StepEffect(sq);

    case B_KING : case W_KING:
      return kingEffect(sq);

    case B_LANCE: case B_BISHOP: case B_ROOK:
    case W_LANCE: case W_BISHOP: case W_ROOK:
      return ZERO_BB;

    default:
      UNREACHABLE; return ZERO_BB;
    }
  }



#define UPDATE_LONG_EFFECT_FROM_(EFFECT_FUNC,to,dir_bw_us,dir_bw_others,p) {  \
    Square sq;                                                                                       \
    uint16_t dir_bw = dir_bw_us ^ dir_bw_others;                                       \
    auto toww = to_sqww(to);                                                                         \
    while (dir_bw)                                                                                   \
    {                                                                                                \
                                                                                \
      int dir = LSB32(dir_bw) & 7;                                             \
                                                \
      uint16_t value = uint16_t((1 << dir) | (1 << (dir + 8)));                                      \
                                                \
      value &= dir_bw;                                                                               \
      dir_bw &= ~value;                                     \
      auto delta = DirectToDeltaWW((Direct)dir);                                                     \
                                                                       \
      bool the_same_color = (Us == BLACK && (value & 0xff)) || ((Us == WHITE) && (value & 0xff00));  \
      int8_t e1 = (dir_bw_us & value) ? (+(p)) : (the_same_color ? (-(p)) : 0);                      \
      bool not_the_same = (Us == BLACK && (value & 0xff00)) || ((Us == WHITE) && (value & 0xff));    \
      int8_t e2 = not_the_same ? (-(p)) : 0;                                                         \
      auto toww2 = toww;                                                                             \
      do {                                                                                           \
        toww2 += delta;                                                                              \
        if (!is_ok(toww2)) break;                              \
        sq = sqww_to_sq(toww2);                                                                      \
                                                    \
        long_effect.le16[sq].u16 ^= value;                                                           \
        EFFECT_FUNC(Us,sq,e1,e2);                                                                    \
      } while (pos.piece_on(sq) == NO_PIECE);                                                        \
    }}

#define UPDATE_LONG_EFFECT_FROM(to,dir_bw_us,dir_bw_others,p) { UPDATE_LONG_EFFECT_FROM_(ADD_BOARD_EFFECT_BOTH,to,dir_bw_us,dir_bw_others,p); }

#define UPDATE_LONG_EFFECT_FROM_REWIND(to,dir_bw_us,dir_bw_others,p) { UPDATE_LONG_EFFECT_FROM_(ADD_BOARD_EFFECT_BOTH_REWIND,to,dir_bw_us,dir_bw_others,p); }


  template <Color Us> void update_by_dropping_piece(Position& pos, Square to, Piece dropped_pc)
  {
    auto& board_effect = pos.board_effect;

    auto inc_target = short_effects_from(dropped_pc, to);
    while (inc_target)
    {
      auto sq = inc_target.pop();
      ADD_BOARD_EFFECT(Us, sq, +1);
    }






    auto& long_effect = pos.long_effect;

    auto dir_bw_us = LongEffect::long_effect16_of(dropped_pc); 
    auto dir_bw_others = pos.long_effect.long_effect16(to); 
    UPDATE_LONG_EFFECT_FROM(to , dir_bw_us, dir_bw_others, +1);
  }

  template <Color Us> void update_by_capturing_piece(Position& pos, Square from, Square to, Piece moved_pc, Piece moved_after_pc, Piece captured_pc)
  {
    auto& board_effect = pos.board_effect;
    auto& long_effect = pos.long_effect;


    auto dec_target = short_effects_from(moved_pc, from);

    auto inc_target = short_effects_from(moved_after_pc, to);

    auto and_target = inc_target & dec_target;
    inc_target ^= and_target;
    dec_target ^= and_target;

    while (inc_target) { auto sq = inc_target.pop(); ADD_BOARD_EFFECT( Us, sq , +1); }
    while (dec_target) { auto sq = dec_target.pop(); ADD_BOARD_EFFECT( Us, sq , -1); }

    dec_target = short_effects_from(captured_pc, to);
    while (dec_target) { auto sq = dec_target.pop(); ADD_BOARD_EFFECT(~Us, sq , -1); }



    auto dir = directions_of(from, to);
    uint16_t dir_mask;
    if (dir != 0)
    {
      auto dir_cont = uint16_t(1 << (7 - LSB32(dir)));
      dir_mask = (uint16_t)~(dir_cont | (dir_cont << 8));
    } else {
      dir_mask = 0xffff;
    }

    auto dir_bw_us = LongEffect::long_effect16_of(moved_pc) & dir_mask;  
    auto dir_bw_others = pos.long_effect.long_effect16(from) & dir_mask; 
    UPDATE_LONG_EFFECT_FROM(from, dir_bw_us, dir_bw_others, -1);


    dir_bw_us = LongEffect::long_effect16_of(moved_after_pc);
    dir_bw_others = LongEffect::long_effect16_of(captured_pc);
    UPDATE_LONG_EFFECT_FROM(to, dir_bw_us , dir_bw_others , +1);
  }

  template <Color Us>void update_by_no_capturing_piece(Position& pos, Square from, Square to, Piece moved_pc, Piece moved_after_pc)
  {
    auto& board_effect = pos.board_effect;
    auto& long_effect = pos.long_effect;


    auto dec_target = short_effects_from(moved_pc, from);
    auto inc_target = short_effects_from(moved_after_pc, to);

    auto and_target = inc_target & dec_target;
    inc_target ^= and_target;
    dec_target ^= and_target;

    while (inc_target) { auto sq = inc_target.pop(); ADD_BOARD_EFFECT(Us, sq , +1); }
    while (dec_target) { auto sq = dec_target.pop(); ADD_BOARD_EFFECT(Us, sq , -1); }


    auto dir = directions_of(from, to);
    uint16_t dir_mask;
    if (dir != 0)
    {
      auto dir_cont = uint16_t(1 << (7 - LSB32(dir)));
      dir_mask = (uint16_t)~(dir_cont | (dir_cont << 8));
    } else {
      dir_mask = 0xffff;
    }

    auto dir_bw_us = LongEffect::long_effect16_of(moved_pc) & dir_mask;
    auto dir_bw_others = pos.long_effect.long_effect16(from) & dir_mask;
    UPDATE_LONG_EFFECT_FROM(from, dir_bw_us, dir_bw_others, -1);


    dir_bw_us = LongEffect::long_effect16_of(moved_after_pc);
    dir_bw_others = pos.long_effect.long_effect16(to);
    
    UPDATE_LONG_EFFECT_FROM(to, dir_bw_us, dir_bw_others, +1);
  }



  template <Color Us> void rewind_by_dropping_piece(Position& pos, Square to, Piece dropped_pc)
  {
    auto& board_effect = pos.board_effect;

    auto inc_target = short_effects_from(dropped_pc, to);
    while (inc_target)
    {
      auto sq = inc_target.pop();
      ADD_BOARD_EFFECT_REWIND(Us, sq, -1); 
    }

    auto& long_effect = pos.long_effect;

    auto dir_bw_us = LongEffect::long_effect16_of(dropped_pc);
    auto dir_bw_others = pos.long_effect.long_effect16(to);
    UPDATE_LONG_EFFECT_FROM_REWIND(to, dir_bw_us, dir_bw_others, -1); 
  }

  template <Color Us> void rewind_by_capturing_piece(Position& pos, Square from, Square to, Piece moved_pc, Piece moved_after_pc, Piece captured_pc)
  {
    auto& board_effect = pos.board_effect;
    auto& long_effect = pos.long_effect;

    auto inc_target = short_effects_from(moved_pc, from);
    auto dec_target = short_effects_from(moved_after_pc, to);

    auto and_target = inc_target & dec_target;
    inc_target ^= and_target;
    dec_target ^= and_target;

    while (inc_target) { auto sq = inc_target.pop(); ADD_BOARD_EFFECT_REWIND(Us, sq, +1); }
    while (dec_target) { auto sq = dec_target.pop(); ADD_BOARD_EFFECT_REWIND(Us, sq, -1); }

    inc_target = short_effects_from(captured_pc, to);
    while (inc_target) { auto sq = inc_target.pop(); ADD_BOARD_EFFECT_REWIND(~Us, sq, +1); }


    auto dir_bw_us = LongEffect::long_effect16_of(moved_after_pc);
    auto dir_bw_others = LongEffect::long_effect16_of(captured_pc);
    UPDATE_LONG_EFFECT_FROM_REWIND(to, dir_bw_us, dir_bw_others, -1); 
                                                                    

    auto dir = directions_of(from, to);
    uint16_t dir_mask;
    if (dir != 0)
    {
      auto dir_cont = uint16_t(1 << (7 - LSB32(dir)));
      dir_mask = (uint16_t)~(dir_cont | (dir_cont << 8));
    } else {
      dir_mask = 0xffff;
    }

    dir_bw_us = LongEffect::long_effect16_of(moved_pc) & dir_mask;
    dir_bw_others = pos.long_effect.long_effect16(from) & dir_mask;
    UPDATE_LONG_EFFECT_FROM_REWIND(from, dir_bw_us, dir_bw_others, +1); 
  }

  template <Color Us> void rewind_by_no_capturing_piece(Position& pos, Square from, Square to, Piece moved_pc, Piece moved_after_pc)
  {
    auto& board_effect = pos.board_effect;
    auto& long_effect = pos.long_effect;

    auto inc_target = short_effects_from(moved_pc, from);
    auto dec_target = short_effects_from(moved_after_pc, to);

    auto and_target = inc_target & dec_target;
    inc_target ^= and_target;
    dec_target ^= and_target;

    while (inc_target) { auto sq = inc_target.pop(); ADD_BOARD_EFFECT_REWIND(Us, sq, +1); }
    while (dec_target) { auto sq = dec_target.pop(); ADD_BOARD_EFFECT_REWIND(Us, sq, -1); }


    auto dir_bw_us = LongEffect::long_effect16_of(moved_after_pc);
    auto dir_bw_others = pos.long_effect.long_effect16(to);

    UPDATE_LONG_EFFECT_FROM_REWIND(to, dir_bw_us, dir_bw_others, -1); 
                                                                    

    auto dir = directions_of(from, to);
    uint16_t dir_mask;
    if (dir != 0)
    {
      auto dir_cont = uint16_t(1 << (7 - LSB32(dir)));
      dir_mask = (uint16_t)~(dir_cont | (dir_cont << 8));
    } else {
      dir_mask = 0xffff;
    }

    dir_bw_us = LongEffect::long_effect16_of(moved_pc) & dir_mask;
    dir_bw_others = pos.long_effect.long_effect16(from) & dir_mask;
    UPDATE_LONG_EFFECT_FROM_REWIND(from, dir_bw_us, dir_bw_others, +1); 
  }

  
  template void update_by_dropping_piece<BLACK>(Position& pos, Square to, Piece pc);
  template void update_by_dropping_piece<WHITE>(Position& pos, Square to, Piece pc);
  template void update_by_capturing_piece<BLACK>(Position& pos, Square from, Square to, Piece moved_pc, Piece moved_after_pc, Piece captured_pc);
  template void update_by_capturing_piece<WHITE>(Position& pos, Square from, Square to, Piece moved_pc, Piece moved_after_pc, Piece captured_pc);
  template void update_by_no_capturing_piece<BLACK>(Position& pos, Square from, Square to, Piece moved_pc, Piece moved_after_pc);
  template void update_by_no_capturing_piece<WHITE>(Position& pos, Square from, Square to, Piece moved_pc, Piece moved_after_pc);
  template void rewind_by_dropping_piece<BLACK>(Position& pos, Square to, Piece pc);
  template void rewind_by_dropping_piece<WHITE>(Position& pos, Square to, Piece pc);
  template void rewind_by_capturing_piece<BLACK>(Position& pos, Square from, Square to, Piece moved_pc, Piece moved_after_pc, Piece captured_pc);
  template void rewind_by_capturing_piece<WHITE>(Position& pos, Square from, Square to, Piece moved_pc, Piece moved_after_pc, Piece captured_pc);
  template void rewind_by_no_capturing_piece<BLACK>(Position& pos, Square from, Square to, Piece moved_pc, Piece moved_after_pc);
  template void rewind_by_no_capturing_piece<WHITE>(Position& pos, Square from, Square to, Piece moved_pc, Piece moved_after_pc);


  void init()
  {
    Effect8::init();
    Effect24::init();
  }

}

#endif 
