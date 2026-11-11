#include "../../config.h"

#if defined(EVAL_MATERIAL)

#include "../../types.h"
#include "../../position.h"
#include "../../evaluate.h"

#include "../../engine//yaneuraou-engine/yaneuraou-param-common.h"
#include <array>
#include <cmath>
#include <algorithm> 
#include <numeric>	 
#define SIZE_OF_ARRAY(array) (sizeof(array)/sizeof(array[0]))

namespace Eval
{
	void load_eval() {}
	void print_eval_stat(Position& pos) {}
	void evaluate_with_no_return(const Position& pos) {}
	Value evaluate(const Position& pos) { return compute_eval(pos); }

#if MATERIAL_LEVEL == 1 || !defined(MATERIAL_LEVEL)

	void init() {}
	Value compute_eval(const Position& pos) {
		auto score = pos.state()->materialValue;
		ASSERT_LV5(pos.state()->materialValue == Eval::material(pos));

		return pos.side_to_move() == BLACK ? score : -score;
	}

#elif MATERIAL_LEVEL == 2


	void init() {}
	Value compute_eval(const Position& pos) {
		auto score = pos.state()->materialValue;

		for (auto sq : SQ)
		{
			auto pc = pos.piece_on(sq);
			if (pc == NO_PIECE)
				continue;

			auto piece_value = PieceValue[pc];
			score -= piece_value * 104 / 1024;
		}

		return pos.side_to_move() == BLACK ? score : -score;
	}

#elif MATERIAL_LEVEL == 3



	int our_effect_value[9];
	int their_effect_value[9];

	void init() {
		for (int i = 0; i < 9; ++i)
		{
			our_effect_value[i]  = 68 * 1024 / (i + 1);
			their_effect_value[i] = 96 * 1024 / (i + 1);
		}
	}

	Value compute_eval(const Position& pos) {
		auto score = pos.state()->materialValue;

		for (auto sq : SQ)
		{
			int effects[2] = { pos.board_effect[BLACK].effect(sq) , pos.board_effect[WHITE].effect(sq) };

			for (auto color : COLOR)
			{
				auto king_sq = pos.king_square(color);

				int d = dist(sq, king_sq);

				int s1 = effects[ color] * our_effect_value [d] / 1024;
				int s2 = effects[~color] * their_effect_value[d] / 1024;

				score += color == BLACK ? (s1 - s2) : (s2 - s1);
			}

			auto pc = pos.piece_on(sq);
			if (pc == NO_PIECE)
				continue;

			auto piece_value = PieceValue[pc];
			score -= piece_value * 104 / 1024;
		}

		return pos.side_to_move() == BLACK ? score : -score;
	}

#elif MATERIAL_LEVEL == 4


	int our_effect_value  [9];
	int their_effect_value[9];

	int multi_effect_value[11];

	void init() {

		for (int d = 0; d < 9; ++d)
		{
			our_effect_value[d]   = 85 * 1024 / (d + 1);
			their_effect_value[d] = 98 * 1024 / (d + 1);
		}

		for (int m = 0; m < 11; ++m)
			multi_effect_value[m] = m == 0 ? 0 : int(6365 - std::pow(0.8525 , m - 1)*5341 );
	}

	Value compute_eval(const Position& pos) {
		auto score = pos.state()->materialValue;

		for (auto sq : SQ)
		{
			int effects[2] = { pos.board_effect[BLACK].effect(sq) , pos.board_effect[WHITE].effect(sq) };

			for (auto color : COLOR)
			{
				auto king_sq = pos.king_square(color);

				int d = dist(sq, king_sq);

				int s1 = multi_effect_value[effects[ color]] * our_effect_value  [d] / (1024 * 1024);
				int s2 = multi_effect_value[effects[~color]] * their_effect_value[d] / (1024 * 1024);

				score += color == BLACK ? (s1 - s2) : (s2 - s1);
			}

			auto pc = pos.piece_on(sq);
			if (pc == NO_PIECE)
				continue;

			auto piece_value = PieceValue[pc];
			score -= piece_value * 104 / 1024;
		}

		return pos.side_to_move() == BLACK ? score : -score;
	}

#elif MATERIAL_LEVEL == 5


	int16_t effect_table[SQ_NB][SQ_NB][SQ_NB][11][11];

	void init() {


		int our_effect_value[9];
		int their_effect_value[9];

		for (int d = 0; d < 9; ++d)
		{
			our_effect_value[d]   = 83 * 1024  / (d + 1);
			their_effect_value[d] = 92 * 1024  / (d + 1);
		}


		int multi_effect_value[11];

		for (int m = 0; m < 11; ++m)
			multi_effect_value[m] = m == 0 ? 0 : int(6365 - std::pow(0.8525, m - 1) * 5341);

		int our_effect_table  [SQ_NB][SQ_NB][11];
		int their_effect_table[SQ_NB][SQ_NB][11];

		for(auto king_sq : SQ)
			for (auto sq : SQ)
				for(int m = 0 ; m < 11 ; ++m) 
				{
					int d = dist(sq, king_sq);

					our_effect_table  [king_sq][sq][m] = multi_effect_value[m] * our_effect_value  [d] / (1024 * 1024);
					their_effect_table[king_sq][sq][m] = multi_effect_value[m] * their_effect_value[d] / (1024 * 1024);
				}

		for (auto king_black : SQ)
			for (auto king_white : SQ)
				for (auto sq : SQ)
					for(int m1 = 0; m1<11;++m1) 
						for (int m2 = 0; m2 < 11; ++m2) 
						{
							int score = 0;
							score += our_effect_table  [    king_black ][    sq ][m1];
							score -= their_effect_table[    king_black ][    sq ][m2];
							score -= our_effect_table  [Inv(king_white)][Inv(sq)][m2];
							score += their_effect_table[Inv(king_white)][Inv(sq)][m1];
							effect_table[king_black][king_white][sq][m1][m2] = int16_t(score);
						}

	}

	Value compute_eval(const Position& pos) {

		auto score = pos.state()->materialValue;

		for (auto sq : SQ)
		{
			score += effect_table[pos.king_square(BLACK)][pos.king_square(WHITE)][sq][pos.board_effect[BLACK].effect(sq)][pos.board_effect[WHITE].effect(sq)];

			auto pc = pos.piece_on(sq);
			if (pc == NO_PIECE)
				continue;

			auto piece_value = PieceValue[pc];
			score -= piece_value * 104 / 1024;
		}

		return pos.side_to_move() == BLACK ? score : -score;
	}

#elif MATERIAL_LEVEL == 6


	int16_t KKPEE[SQ_NB][SQ_NB][SQ_NB][3][3][PIECE_NB];

	void init() {


		int our_effect_value[9];
		int their_effect_value[9];

		for (int d = 0; d < 9; ++d)
		{
			our_effect_value[d]   = 83 * 1024  / (d + 1);
			their_effect_value[d] = 92 * 1024  / (d + 1);
		}


		int multi_effect_value[11];

		for (int m = 0; m < 11; ++m)
			multi_effect_value[m] = m == 0 ? 0 : int(6365 - std::pow(0.8525, m - 1) * 5341);

		double our_effect_table  [SQ_NB][SQ_NB][11];
		double their_effect_table[SQ_NB][SQ_NB][11];

		for(auto king_sq : SQ)
			for (auto sq : SQ)
				for(int m = 0 ; m < 3 ; ++m) 
				{
					int d = dist(sq, king_sq);

					our_effect_table  [king_sq][sq][m] = double(multi_effect_value[m] * our_effect_value  [d]) / (1024 * 1024);
					their_effect_table[king_sq][sq][m] = double(multi_effect_value[m] * their_effect_value[d]) / (1024 * 1024);
				}

		for (auto king_black : SQ)
			for (auto king_white : SQ)
				for (auto sq : SQ)
					for(int m1 = 0; m1<3;++m1) 
						for (int m2 = 0; m2 <3; ++m2) 
							for(Piece pc = NO_PIECE;pc < PIECE_NB ; ++pc) 
							{
								double score = 0;

								score += our_effect_table  [    king_black ][    sq ][m1];
								score -= their_effect_table[    king_black ][    sq ][m2];
								score -= our_effect_table  [Inv(king_white)][Inv(sq)][m2];
								score += their_effect_table[Inv(king_white)][Inv(sq)][m1];

								if (pc != NO_PIECE)
								{
									auto piece_value = PieceValue[pc];
									score -= piece_value * 104 / 1024;
								}

								KKPEE[king_black][king_white][sq][m1][m2][pc] = int16_t(score);
							}

	}

	Value compute_eval(const Position& pos) {

		auto score = pos.state()->materialValue;

		for (auto sq : SQ)
			score += KKPEE[pos.king_square(BLACK)][pos.king_square(WHITE)][sq]
				[std::min(int(pos.board_effect[BLACK].effect(sq)),2)][std::min(int(pos.board_effect[WHITE].effect(sq)),2)][pos.piece_on(sq)];

		return pos.side_to_move() == BLACK ? score : -score;
	}

#elif MATERIAL_LEVEL == 7



	int16_t KKPEE[SQ_NB][SQ_NB][SQ_NB][3][3][PIECE_NB];

	constexpr int FV_SCALE = 32;

	void init() {


		int our_effect_value[9];
		int their_effect_value[9];

		for (int d = 0; d < 9; ++d)
		{
			our_effect_value[d] = 83 * 1024 / (d + 1);
			their_effect_value[d] = 92 * 1024 / (d + 1);
		}


		int multi_effect_value[11];

		for (int m = 0; m < 11; ++m)
			multi_effect_value[m] = m == 0 ? 0 : int(6365 - std::pow(0.8525, m - 1) * 5341);

		double our_effect_table  [SQ_NB][SQ_NB][11];
		double their_effect_table[SQ_NB][SQ_NB][11];

		for(auto king_sq : SQ)
			for (auto sq : SQ)
				for(int m = 0 ; m < 3 ; ++m) 
				{
					int d = dist(sq, king_sq);

					our_effect_table  [king_sq][sq][m] = double(multi_effect_value[m] * our_effect_value  [d]) / (1024 * 1024);
					their_effect_table[king_sq][sq][m] = double(multi_effect_value[m] * their_effect_value[d]) / (1024 * 1024);
				}

		int our_effect_to_our_piece[3] = { 0,  33 ,  43 };
		int their_effect_to_our_piece[3] = { 0, 113 , 122 };

		for (Piece pc = NO_PIECE; pc < PIECE_NB; ++pc) 
			for (auto king_black : SQ)
				for (auto king_white : SQ)
					for (auto sq : SQ)
						for(int m1 = 0; m1<3;++m1) 
							for (int m2 = 0; m2 <3; ++m2) 
							{
								double score = 0;


								score += our_effect_table  [    king_black ][    sq ][m1];
								score -= their_effect_table[    king_black ][    sq ][m2];
								score -= our_effect_table  [Inv(king_white)][Inv(sq)][m2];
								score += their_effect_table[Inv(king_white)][Inv(sq)][m1];

								if (pc == NO_PIECE)
								{

								} else if (type_of(pc) == KING) {


								} else  {



									double piece_value = PieceValue[pc];
									score -= piece_value * 104 / 1024;


									int effect_us = color_of(pc) == BLACK ? m1 : m2;
									int effect_them = color_of(pc) == BLACK ? m2 : m1;
									score += piece_value * our_effect_to_our_piece  [effect_us]   / 4096;
									score -= piece_value * their_effect_to_our_piece[effect_them] / 4096;
								}

								KKPEE[king_black][king_white][sq][m1][m2][pc] = int16_t(score * FV_SCALE);
							}
	}

	Value compute_eval(const Position& pos) {

		Value score = VALUE_ZERO;
		for (auto sq : SQ)
			score += KKPEE[pos.king_square(BLACK)][pos.king_square(WHITE)][sq]
				[std::min(int(pos.board_effect[BLACK].effect(sq)),2)][std::min(int(pos.board_effect[WHITE].effect(sq)),2)][pos.piece_on(sq)];

		score = score / FV_SCALE + pos.state()->materialValue;

		return pos.side_to_move() == BLACK ? score : -score;
	}

#elif MATERIAL_LEVEL == 8



	int16_t KKPEE[SQ_NB][SQ_NB][SQ_NB][3][3][PIECE_NB];

	constexpr int FV_SCALE = 32;

	void init() {


		int our_effect_value[9];
		int their_effect_value[9];

		for (int d = 0; d < 9; ++d)
		{
			our_effect_value[d] = 83 * 1024 / (d + 1);
			their_effect_value[d] = 92 * 1024 / (d + 1);
		}


		int multi_effect_value[11];

		for (int m = 0; m < 11; ++m)
			multi_effect_value[m] = m == 0 ? 0 : int(6365 - std::pow(0.8525, m - 1) * 5341);

		double our_effect_table  [SQ_NB][SQ_NB][11];
		double their_effect_table[SQ_NB][SQ_NB][11];

		for(auto king_sq : SQ)
			for (auto sq : SQ)
				for(int m = 0 ; m < 3 ; ++m) 
				{
					int d = dist(sq, king_sq);

					our_effect_table  [king_sq][sq][m] = double(multi_effect_value[m] * our_effect_value  [d]) / (1024 * 1024);
					their_effect_table[king_sq][sq][m] = double(multi_effect_value[m] * their_effect_value[d]) / (1024 * 1024);
				}

		int our_effect_to_our_piece[3] = { 0,  33 ,  43 };
		int their_effect_to_our_piece[3] = { 0, 113 , 122 };


		int king_pos_bonus[] = {
			875, 655, 830, 680, 770, 815, 720, 945, 755,
			605, 455, 610, 595, 730, 610, 600, 590, 615,
			565, 640, 555, 525, 635, 565, 440, 600, 575,
			520, 515, 580, 420, 640, 535, 565, 500, 510,
			220, 355, 240, 375, 340, 335, 305, 275, 320,
			500, 530, 560, 445, 510, 395, 455, 490, 410,
			345, 275, 250, 355, 295, 280, 420, 235, 135,
			335, 370, 385, 255, 295, 200, 265, 305, 305,
			255, 225, 245, 295, 200, 320, 275,  70, 200
		};

		for (Piece pc = NO_PIECE; pc < PIECE_NB; ++pc) 
			for (auto king_black : SQ)
				for (auto king_white : SQ)
					for (auto sq : SQ)
						for(int m1 = 0; m1<3;++m1) 
							for (int m2 = 0; m2 <3; ++m2) 
							{
								double score = 0;


								score += our_effect_table  [    king_black ][    sq ][m1];
								score -= their_effect_table[    king_black ][    sq ][m2];
								score -= our_effect_table  [Inv(king_white)][Inv(sq)][m2];
								score += their_effect_table[Inv(king_white)][Inv(sq)][m1];

								for (auto color : COLOR) 
								{
									auto king_sq = color == BLACK ? king_black : king_white;

									if (dist(sq, king_sq) == 1)
									{
										int effect_us = color == BLACK ? m1 : m2;
										if (effect_us <= 1)
											score -= double((pc == NO_PIECE || color_of(pc) != color) ? 11 : -20) * (color == BLACK ? 1 : -1);
										else
											score -= double((pc == NO_PIECE || color_of(pc) != color) ? 0 : - 11 ) * (color == BLACK ? 1 : -1);
									}
								}

								if (pc == NO_PIECE)
								{

								} else if (type_of(pc) == KING) {



									score += color_of(pc) == BLACK ? king_pos_bonus[(FILE_9-file_of(    sq )) + int(rank_of(    sq )*9)] :
																	-king_pos_bonus[(FILE_9-file_of(Inv(sq))) + int(rank_of(Inv(sq))*9)];

								} else  {

									

									double piece_value = PieceValue[pc];
									score -= piece_value * 104 / 1024;


									int effect_us = color_of(pc) == BLACK ? m1 : m2;
									int effect_them = color_of(pc) == BLACK ? m2 : m1;
									score += piece_value * our_effect_to_our_piece  [effect_us]   / 4096;
									score -= piece_value * their_effect_to_our_piece[effect_them] / 4096;
								}

								KKPEE[king_black][king_white][sq][m1][m2][pc] = int16_t(score * FV_SCALE);
							}
	}

	Value compute_eval(const Position& pos) {

		Value score = VALUE_ZERO;
		for (auto sq : SQ)
			score += KKPEE[pos.king_square(BLACK)][pos.king_square(WHITE)][sq]
				[std::min(int(pos.board_effect[BLACK].effect(sq)),2)][std::min(int(pos.board_effect[WHITE].effect(sq)),2)][pos.piece_on(sq)];

		score = score / FV_SCALE + pos.state()->materialValue;

		return pos.side_to_move() == BLACK ? score : -score;
	}

#elif MATERIAL_LEVEL == 9



	int16_t KKPEE[SQ_NB][SQ_NB][SQ_NB][3][3][PIECE_NB];

	constexpr int FV_SCALE = 32;

	int direction_of(Square king, Square sq)
	{
		int df = int(file_of(sq) - file_of(king));
		int dr = int(rank_of(sq) - rank_of(king));

		if (df > 0) df = -df;

		if (df ==  0 && dr == 0) return 9;

		if (df ==  0 && dr < 0 ) return 0;
		if (df >  dr && dr < 0 ) return 1;
		if (df == dr && dr < 0 ) return 2;
		if (df <  dr && dr < 0 ) return 3;
		if (df <   0 && dr == 0) return 4;
		if (df < -dr && dr > 0 ) return 5;
		if (df ==-dr && dr > 0 ) return 6;
		if (df ==  0 && dr > 0 ) return 8; 
		if (df > -dr && dr > 0 ) return 7;

		ASSERT(false);
		return -1;
								}

	void init() {


		int our_effect_value[9];
		int their_effect_value[9];

		for (int d = 0; d < 9; ++d)
		{
			our_effect_value[d]   = 83 * 1024  / (d + 1);
			their_effect_value[d] = 92 * 1024  / (d + 1);
		}


		int multi_effect_value[11];

		for (int m = 0; m < 11; ++m)
			multi_effect_value[m] = m == 0 ? 0 : int(6365 - std::pow(0.8525, m - 1) * 5341);

		double our_effect_table  [SQ_NB][SQ_NB][11];
		double their_effect_table[SQ_NB][SQ_NB][11];

		for(auto king_sq : SQ)
			for (auto sq : SQ)
				for(int m = 0 ; m < 3 ; ++m) 
				{
					int d = dist(sq, king_sq);

					our_effect_table  [king_sq][sq][m] = double(multi_effect_value[m] * our_effect_value  [d]) / (1024 * 1024);
					their_effect_table[king_sq][sq][m] = double(multi_effect_value[m] * their_effect_value[d]) / (1024 * 1024);
				}

		int our_effect_to_our_piece[3]   = { 0,  33 ,  43 };
		int their_effect_to_our_piece[3] = { 0, 113 , 122 };

		
		int our_effect_rate[10]   = { 1120, 1872,  112,  760, 744, 880, 1320, 600,  904 , 1024 };
		int their_effect_rate[10] = { 1056, 1714, 1688, 1208, 248, 240,  496, 816,  928 , 1024 };

		int king_pos_bonus[] = {
			875, 655, 830, 680, 770, 815, 720, 945, 755,
			605, 455, 610, 595, 730, 610, 600, 590, 615,
			565, 640, 555, 525, 635, 565, 440, 600, 575,
			520, 515, 580, 420, 640, 535, 565, 500, 510,
			220, 355, 240, 375, 340, 335, 305, 275, 320,
			500, 530, 560, 445, 510, 395, 455, 490, 410,
			345, 275, 250, 355, 295, 280, 420, 235, 135,
			335, 370, 385, 255, 295, 200, 265, 305, 305,
			255, 225, 245, 295, 200, 320, 275,  70, 200
		};

		for (Piece pc = NO_PIECE; pc < PIECE_NB; ++pc) 
			for (auto king_black : SQ)
				for (auto king_white : SQ)
					for (auto sq : SQ)
						for(int m1 = 0; m1<3;++m1) 
							for (int m2 = 0; m2 <3; ++m2) 
							{
								double score = 0;


								score += our_effect_table  [    king_black ][    sq ][m1] * our_effect_rate  [direction_of(    king_black ,    sq )] / 1024;
								score -= their_effect_table[    king_black ][    sq ][m2] * their_effect_rate[direction_of(    king_black ,    sq )] / 1024;
								score -= our_effect_table  [Inv(king_white)][Inv(sq)][m2] * our_effect_rate  [direction_of(Inv(king_white),Inv(sq))] / 1024;
								score += their_effect_table[Inv(king_white)][Inv(sq)][m1] * their_effect_rate[direction_of(Inv(king_white),Inv(sq))] / 1024;


								for (auto color : COLOR) 
								{
									auto king_sq = color == BLACK ? king_black : king_white;

									if (dist(sq, king_sq) == 1)
									{
										int effect_us = color == BLACK ? m1 : m2;
										if (effect_us <= 1)
											score -= double((pc == NO_PIECE || color_of(pc) != color) ? 11 : -20) * (color == BLACK ? 1 : -1);
										else
											score -= double((pc == NO_PIECE || color_of(pc) != color) ? 0 : - 11 ) * (color == BLACK ? 1 : -1);
									}
								}

								if (pc == NO_PIECE)
								{

								} else if (type_of(pc) == KING) {

									
									score += color_of(pc) == BLACK ? king_pos_bonus[(FILE_9-file_of(    sq )) + int(rank_of(    sq )*9)] :
																	-king_pos_bonus[(FILE_9-file_of(Inv(sq))) + int(rank_of(Inv(sq))*9)];

								} else  {



									double piece_value = PieceValue[pc];
									score -= piece_value * 104 / 1024;


									int effect_us = color_of(pc) == BLACK ? m1 : m2;
									int effect_them = color_of(pc) == BLACK ? m2 : m1;
									score += piece_value * our_effect_to_our_piece  [effect_us]   / 4096;
									score -= piece_value * their_effect_to_our_piece[effect_them] / 4096;
								}

								KKPEE[king_black][king_white][sq][m1][m2][pc] = int16_t(score * FV_SCALE);
							}
	}

	Value compute_eval(const Position& pos) {

		Value score = VALUE_ZERO;
		for (auto sq : SQ)
			score += KKPEE[pos.king_square(BLACK)][pos.king_square(WHITE)][sq]
				[std::min(int(pos.board_effect[BLACK].effect(sq)),2)][std::min(int(pos.board_effect[WHITE].effect(sq)),2)][pos.piece_on(sq)];

		score = score / FV_SCALE + pos.state()->materialValue;

		return pos.side_to_move() == BLACK ? score : -score;
	}

#endif 

}

#endif 

