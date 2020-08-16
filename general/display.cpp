#include "display.h"
#include "board.h"

namespace sxako {

  using namespace std;

  info_t info_turn_last_move(string const &turn, LastMove const &last_move) {
    info_t info;
    info[1]="turn: "+turn;
    if (last_move.number)
      info[2]="last: "+to_text(last_move.number)+". "+last_move.move;
    return info;
  }

}
