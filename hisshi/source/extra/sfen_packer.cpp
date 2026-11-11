#include "../config.h"

#if defined (USE_SFEN_PACKER)

#include "../misc.h"
#include "../position.h"

#include <sstream>
#include <fstream>
#include <cstring>	

using namespace std;


struct BitStream
{
  void  set_data(u8* data_) { data = data_; reset(); }

  u8* get_data() const { return data; }

  int get_cursor() const { return bit_cursor; }

  void reset() { bit_cursor = 0; }

  FORCE_INLINE void write_one_bit(int b)
  {
    if (b)
      data[bit_cursor / 8] |= 1 << (bit_cursor & 7);

    ++bit_cursor;
  }

  FORCE_INLINE int read_one_bit()
  {
    int b = (data[bit_cursor / 8] >> (bit_cursor & 7)) & 1;
    ++bit_cursor;

    return b;
  }

  void write_n_bit(int d, int n)
  {
    for (int i = 0; i < n; ++i)
      write_one_bit(d & (1 << i));
  }

  int read_n_bit(int n)
  {
    int result = 0;
    for (int i = 0; i < n; ++i)
      result |= read_one_bit() ? (1 << i) : 0;

    return result;
  }

private:
  int bit_cursor;

  u8* data;
};



struct HuffmanedPiece
{
  int code; 
  int bits; 
};

HuffmanedPiece huffman_table[] =
{
  {0x00,1}, 
  {0x01,2}, 
  {0x03,4}, 
  {0x0b,4}, 
  {0x07,4}, 
  {0x1f,6}, 
  {0x3f,6}, 
  {0x0f,5}, 
};

struct SfenPacker
{
  void pack(const Position& pos)
  {

    memset(data, 0, 32 );
    stream.set_data(data);

    stream.write_one_bit((int)(pos.side_to_move()));

    for(auto c : COLOR)
      stream.write_n_bit(pos.king_square(c), 7);

    for (auto sq : SQ)
    {
      Piece pc = pos.piece_on(sq);
      if (type_of(pc) == KING)
        continue;
      write_board_piece_to_stream(pc);
    }

    for (auto c : COLOR)
      for (PieceType pr = PAWN; pr < KING; ++pr)
      {
        int n = hand_count(pos.hand_of(c), pr);

        for (int i = 0; i < n; ++i)
          write_hand_piece_to_stream(make_piece(c, pr));
      }


    ASSERT_LV3(stream.get_cursor() == 256);
  }

  string unpack()
  {
    stream.set_data(data);

    Piece board[81];
    memset(board, 0, sizeof(Piece)*81);

    Color turn = (Color)stream.read_one_bit();
    
    for (auto c : COLOR)
      board[stream.read_n_bit(7)] = make_piece(c, KING);

    for (auto sq : SQ)
    {
      if (type_of(board[sq]) == KING)
        continue;

      board[sq] = read_board_piece_from_stream();


      ASSERT_LV3(stream.get_cursor() <= 256);
    }

    Hand hand[2] = { HAND_ZERO,HAND_ZERO };
    while (stream.get_cursor() != 256)
    {
      auto pc = read_hand_piece_from_stream();
      add_hand(hand[(int)color_of(pc)], type_of(pc));
    }


    return Position::sfen_from_rawdata(board, hand, turn, 0);
  }

  u8 *data; 


  BitStream stream;

  void write_board_piece_to_stream(Piece pc)
  {
    PieceType pr = raw_type_of(pc);
    auto c = huffman_table[pr];
    stream.write_n_bit(c.code, c.bits);
 
    if (pc == NO_PIECE)
      return;

    if (pr!=GOLD)
      stream.write_one_bit((PIECE_PROMOTE & pc) ? 1 : 0);

    stream.write_one_bit(color_of(pc));
  }

  void write_hand_piece_to_stream(Piece pc)
  {
    ASSERT_LV3(pc != NO_PIECE);

    PieceType pr = raw_type_of(pc);
    auto c = huffman_table[pr];
    stream.write_n_bit(c.code >> 1, c.bits - 1);

    if (pr != GOLD)
      stream.write_one_bit(false);

    stream.write_one_bit(color_of(pc));
  }

  Piece read_board_piece_from_stream()
  {
    PieceType pr = NO_PIECE_TYPE;
    int code = 0, bits = 0;
    while (true)
    {
      code |= stream.read_one_bit() << bits;
      ++bits;

      ASSERT_LV3(bits <= 6);

      for (pr = NO_PIECE_TYPE; pr < KING; ++pr)
        if (huffman_table[pr].code == code
          && huffman_table[pr].bits == bits)
          goto Found;
    }
  Found:;
    if (pr == NO_PIECE_TYPE)
      return NO_PIECE;

    bool promote = (pr == GOLD) ? false : stream.read_one_bit();

    Color c = (Color)stream.read_one_bit();
    
    return make_piece(c, pr + (promote ? PIECE_TYPE_PROMOTE : NO_PIECE_TYPE));
  }

  Piece read_hand_piece_from_stream()
  {
    PieceType pr = NO_PIECE_TYPE;
    int code = 0, bits = 0;
    while (true)
    {
      code |= stream.read_one_bit() << bits;
      ++bits;

      ASSERT_LV3(bits <= 6);

      for (pr = PAWN; pr < KING; ++pr)
        if ((huffman_table[pr].code >> 1) == code
          && (huffman_table[pr].bits -1) == bits)
          goto Found;
    }
  Found:;
    ASSERT_LV3(pr != NO_PIECE_TYPE);

    if (pr != GOLD)
      stream.read_one_bit();

    Color c = (Color)stream.read_one_bit();

    return make_piece(c, pr);
  }
};



Tools::Result Position::set_from_packed_sfen(const PackedSfen& sfen , StateInfo * si, Thread* th, bool mirror , int gamePly_ )
{
	SfenPacker packer;
	auto& stream = packer.stream;
	stream.set_data((u8*)&sfen);

	std::memset(this, 0, sizeof(Position));
	std::memset(si, 0, sizeof(StateInfo));
	st = si;

	sideToMove = (Color)stream.read_one_bit();

#if defined(USE_EVAL_LIST)

	evalList.clear();

	PieceNumber piece_no_count[KING] = { PIECE_NUMBER_ZERO,PIECE_NUMBER_PAWN,PIECE_NUMBER_LANCE,PIECE_NUMBER_KNIGHT,
		PIECE_NUMBER_SILVER, PIECE_NUMBER_BISHOP, PIECE_NUMBER_ROOK,PIECE_NUMBER_GOLD };
#endif

	kingSquare[BLACK] = kingSquare[WHITE] = SQ_NB;

	if (mirror)
	{
		for (auto c : COLOR)
			board[Mir((Square)stream.read_n_bit(7))] = make_piece(c, KING);
	}
	else
	{
		for (auto c : COLOR)
			board[stream.read_n_bit(7)] = make_piece(c, KING);
	}

	for (auto sq : SQ)
	{
		if (mirror)
			sq = Mir(sq);

		Piece pc;
		if (type_of(board[sq]) != KING)
		{
			ASSERT_LV3(board[sq] == NO_PIECE);
			pc = packer.read_board_piece_from_stream();
		}
		else
		{
			pc = board[sq];
			board[sq] = NO_PIECE; 
		}

		if (pc == NO_PIECE)
			continue;

		put_piece(sq, Piece(pc));

#if defined(USE_EVAL_LIST)
		PieceNumber piece_no =
			(pc == B_KING) ? PIECE_NUMBER_BKING : 
			(pc == W_KING) ? PIECE_NUMBER_WKING : 
			piece_no_count[raw_type_of(pc)]++; 

		evalList.put_piece(piece_no, sq, pc); 
#endif


		if (stream.get_cursor() > 256)
			return Tools::Result(Tools::ResultCode::SomeError);
	}

	hand[BLACK] = hand[WHITE] = (Hand)0;

	int i = 0;
	Piece lastPc = NO_PIECE;

	while (stream.get_cursor() < 256)
	{
		auto pc = packer.read_hand_piece_from_stream();
		add_hand(hand[(int)color_of(pc)], type_of(pc));

#if defined(USE_EVAL_LIST)
		if (lastPc != pc)
			i = 0;
		lastPc = pc;

		PieceType rpc = raw_type_of(pc);

        PieceNumber piece_no = piece_no_count[rpc]++;
		ASSERT_LV1(is_ok(piece_no));
		evalList.put_piece(piece_no, color_of(pc), rpc, i++);
#endif
	}

	if (stream.get_cursor() != 256)
	{
		return Tools::Result(Tools::ResultCode::SomeError);
	}

	gamePly = gamePly_;

	update_bitboards();
	update_kingSquare();

	set_state(st);


#if defined (LONG_EFFECT_LIBRARY)
	LongEffect::calc_effect(*this);
#endif


	st->materialValue = Eval::material(*this);
	Eval::compute_eval(*this);



	thisThread = th;

	return Tools::Result::Ok();
}

std::string Position::sfen_from_rawdata(Piece board[81], Hand hands[2], Color turn, int gamePly_)
{
  Position pos;

  memcpy(pos.board, board, sizeof(Piece) * 81);
  memcpy(pos.hand, hands, sizeof(Hand) * 2);
  pos.sideToMove = turn;
  pos.gamePly = gamePly_;

  return pos.sfen();

}

void Position::sfen_pack(PackedSfen& sfen)
{
  SfenPacker sp;
  sp.data = (u8*)&sfen;
  sp.pack(*this);
}

std::string Position::sfen_unpack(const PackedSfen& sfen)
{
  SfenPacker sp;
  sp.data = (u8*)&sfen;
  return sp.unpack();
}


#endif 

