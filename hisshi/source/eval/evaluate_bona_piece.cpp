#include "../evaluate.h"
#include "../position.h"

using namespace std;

namespace Eval
{
#if defined (USE_PIECE_VALUE)

    int PieceValue[PIECE_NB] =
    {
        0, PawnValue, LanceValue, KnightValue, SilverValue, BishopValue, RookValue,GoldValue,
        KingValue, ProPawnValue, ProLanceValue, ProKnightValue, ProSilverValue, HorseValue, DragonValue,0,

        0, -PawnValue, -LanceValue, -KnightValue, -SilverValue, -BishopValue, -RookValue,-GoldValue,
        -KingValue, -ProPawnValue, -ProLanceValue, -ProKnightValue, -ProSilverValue, -HorseValue, -DragonValue,0,
    };

    int CapturePieceValue[PIECE_NB] =
    {
        VALUE_ZERO             , PawnValue * 2   , LanceValue * 2   , KnightValue * 2   , SilverValue * 2  ,
        BishopValue * 2, RookValue * 2, GoldValue * 2, 0  ,
        ProPawnValue + PawnValue, ProLanceValue + LanceValue, ProKnightValue + KnightValue, ProSilverValue + SilverValue,
        HorseValue + BishopValue, DragonValue + RookValue, VALUE_ZERO ,
 
        VALUE_ZERO             , PawnValue * 2   , LanceValue * 2   , KnightValue * 2   , SilverValue * 2  ,
        BishopValue * 2, RookValue * 2, GoldValue * 2, 0 ,
        ProPawnValue + PawnValue, ProLanceValue + LanceValue, ProKnightValue + KnightValue, ProSilverValue + SilverValue,
        HorseValue + BishopValue, DragonValue + RookValue, VALUE_ZERO ,
    };

    int ProDiffPieceValue[PIECE_NB] =
    {
        VALUE_ZERO, ProPawnValue - PawnValue, ProLanceValue - LanceValue, ProKnightValue - KnightValue, ProSilverValue - SilverValue, HorseValue - BishopValue, DragonValue - RookValue, VALUE_ZERO ,
        VALUE_ZERO, ProPawnValue - PawnValue, ProLanceValue - LanceValue, ProKnightValue - KnightValue, ProSilverValue - SilverValue, HorseValue - BishopValue, DragonValue - RookValue, VALUE_ZERO ,
        VALUE_ZERO, ProPawnValue - PawnValue, ProLanceValue - LanceValue, ProKnightValue - KnightValue, ProSilverValue - SilverValue, HorseValue - BishopValue, DragonValue - RookValue, VALUE_ZERO ,
        VALUE_ZERO, ProPawnValue - PawnValue, ProLanceValue - LanceValue, ProKnightValue - KnightValue, ProSilverValue - SilverValue, HorseValue - BishopValue, DragonValue - RookValue, VALUE_ZERO ,
    };
#endif


#if defined(USE_EVAL_LIST)
    ExtBonaPiece kpp_board_index[PIECE_NB] = {
        { BONA_PIECE_ZERO, BONA_PIECE_ZERO },
        { f_pawn, e_pawn },
        { f_lance, e_lance },
        { f_knight, e_knight },
        { f_silver, e_silver },
        { f_bishop, e_bishop },
        { f_rook, e_rook },
        { f_gold, e_gold },
        { f_king, e_king },

    #if !defined (DISTINGUISH_GOLDS)
        { f_gold, e_gold }, 
        { f_gold, e_gold }, 
        { f_gold, e_gold }, 
        { f_gold, e_gold }, 

    #else
        { f_pro_pawn, e_pro_pawn },     
        { f_pro_lance, e_pro_lance },   
        { f_pro_knight, e_pro_knight }, 
        { f_pro_silver, e_pro_silver }, 
    #endif

        { f_horse, e_horse }, 
        { f_dragon, e_dragon }, 
        { BONA_PIECE_ZERO, BONA_PIECE_ZERO }, 

        { BONA_PIECE_ZERO, BONA_PIECE_ZERO },
        { e_pawn, f_pawn },
        { e_lance, f_lance },
        { e_knight, f_knight },
        { e_silver, f_silver },
        { e_bishop, f_bishop },
        { e_rook, f_rook },
        { e_gold, f_gold },
        { e_king, f_king },

    #if !defined (DISTINGUISH_GOLDS)
        { e_gold, f_gold }, 
        { e_gold, f_gold }, 
        { e_gold, f_gold }, 
        { e_gold, f_gold }, 

    #else
        { e_pro_pawn, f_pro_pawn },     
        { e_pro_lance, f_pro_lance },   
        { e_pro_knight, f_pro_knight }, 
        { e_pro_silver, f_pro_silver }, 
    #endif

        { e_horse, f_horse }, 
        { e_dragon, f_dragon }, 
        { BONA_PIECE_ZERO, BONA_PIECE_ZERO }, 
    };

    ExtBonaPiece kpp_hand_index[COLOR_NB][KING] = {
    {
        { BONA_PIECE_ZERO, BONA_PIECE_ZERO },
        { f_hand_pawn, e_hand_pawn },
        { f_hand_lance, e_hand_lance },
        { f_hand_knight, e_hand_knight },
        { f_hand_silver, e_hand_silver },
        { f_hand_bishop, e_hand_bishop },
        { f_hand_rook, e_hand_rook },
        { f_hand_gold, e_hand_gold },
    },
    {
        { BONA_PIECE_ZERO, BONA_PIECE_ZERO },
        { e_hand_pawn, f_hand_pawn },
        { e_hand_lance, f_hand_lance },
        { e_hand_knight, f_hand_knight },
        { e_hand_silver, f_hand_silver },
        { e_hand_bishop, f_hand_bishop },
        { e_hand_rook, f_hand_rook },
        { e_hand_gold, f_hand_gold },
    },
    };

    std::ostream& operator<<(std::ostream& os, BonaPiece bp)
    {
        if (bp < fe_hand_end)
        {
            for (auto c : COLOR)
            for (PieceType pc = PAWN; pc < KING; ++pc)
            {
                int kind_num = kpp_hand_index[c][pc].fw - kpp_hand_index[c][pc].fb;
                int start = kpp_hand_index[c][pc].fb;
                if (start <= bp && bp < start+kind_num * 2)
                {
                bool is_black = bp < start + kind_num;
                if (!is_black) bp = (BonaPiece)(bp - kind_num);
        #if defined (PRETTY_JP)
                os << "手" << (is_black ? "先" : "後") << pretty(pc) << int(bp - start + 1); 
        #else
                os << "H" << (is_black ? "B" : "W") << pc << int(bp - kpp_hand_index[c][pc].fb + 1); 
        #endif
                break;
                }
            }
        } else {
            for (auto pc : Piece())
            if (kpp_board_index[pc].fb <= bp && bp < kpp_board_index[pc].fb + SQ_NB)
            {
        #if defined (PRETTY_JP)
                os << Square(bp - kpp_board_index[pc].fb) << pretty(pc); 
        #else
                os << Square(bp - kpp_board_index[pc].fb) << pc; 
        #endif
                break;
            }
        }

        return os;
    }

#endif 

} 