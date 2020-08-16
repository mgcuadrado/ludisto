#ifndef SXAKO_UTILS_HEADER_
#define SXAKO_UTILS_HEADER_

#include <string>
#include <functional>
#include <memory>
#include "base.h"

namespace sxako {

  class Game;
  class Player;

  struct GameSetup {
    std::shared_ptr<Game> game;
    std::shared_ptr<Player> player_a, player_b;
    std::string display_style;
    output_f board_output, message_output;
    bool help_move, stats;
  };

  using go_on_t=std::function<bool ()>;
  go_on_t const always_go_on=[]() { return true; };

  std::string match_up(GameSetup gs,
                       go_on_t go_on=always_go_on);

  GameSetup game_setup(int argc, char *argv[],
                       input_f human_input,
                       output_f board_output,
                       output_f message_output,
                       std::string style_override="");

  int play_game(int argc, char *argv[],
                input_f human_input,
                output_f board_output,
                output_f message_output,
                go_on_t go=always_go_on,
                std::string style_override="");

}

#endif
