#ifndef SXAKO_HUMAN_HEADER_
#define SXAKO_HUMAN_HEADER_

#include "think.h"

namespace sxako {

  class StreamPlayer
    : public Player {
  public:
    // the command handler gets "moves" starting with '/' or empty, and returns
    // "true" if the game goes on, or "false" if it's aborted
    using command_handler_t=
      std::function<bool (Game const &,
                          input_f input,
                          output_f board_output, output_f message_output,
                          std::string command)>;
    StreamPlayer(std::string name,
                 input_f input,
                 output_f board_output, output_f message_output,
                 command_handler_t command_handler);
    MoveScore get_move(Game const &g) override;
  private:
    input_f const input;
    output_f const board_output, message_output;
    command_handler_t const command_handler;
  };

  StreamPlayer::command_handler_t
  human_command_handler(Game &g, std::string display_style);

}

#endif
