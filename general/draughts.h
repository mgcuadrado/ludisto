#ifndef SXAKO_GENERAL_DRAUGHTS_HEADER_
#define SXAKO_GENERAL_DRAUGHTS_HEADER_

#include "piece_game.h"
#include "think.h"

namespace sxako::draughts {

  using namespace piece_2d_game;

  series<Displacement> const
    w_pawn_steps={{-1, +1}, {+1, +1}},
    b_pawn_steps={{-1, -1}, {+1, -1}},
    king_steps=
      {{-1, +1}, {+1, +1},
       {-1, -1}, {+1, -1}};

}

#endif
