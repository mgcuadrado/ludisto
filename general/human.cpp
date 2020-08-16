#include "human.h"
#include <limits>

using namespace std;

namespace sxako {

  namespace {

    score_t nan_score=numeric_limits<score_t>::signaling_NaN();

  }

  StreamPlayer::StreamPlayer(string name,
                             input_f input,
                             output_f board_output, output_f message_output,
                             command_handler_t command_handler)
    : Player(name),
      input(input),
      board_output(board_output), message_output(message_output),
      command_handler(command_handler) { }

  MoveScore StreamPlayer::get_move(Game const &g) {
    while (true) {
      string move_s=input();
      if (move_s.empty() or move_s[0]=='/') {
        if (not command_handler(g,
                                input,
                                board_output, message_output,
                                move_s)) // "false" aborts
          return {Move(), nan_score};
      }
      else {
        Move move;
        bool valid=true;
        try {
          move=g.parse_move(move_s);
        }
        catch (invalid_argument const &error) {
          message_output("error: "+move_s+": "+error.what()+"\n");
          valid=false;
        }
        if (not valid)
          continue;
        return {move, nan_score};
      }
    }
    return {Move(), nan_score};
  }

  StreamPlayer::command_handler_t
  human_command_handler(Game &ncg, string display_style) {
    return
      [&ncg, display_style](Game const &cg,
                            input_f,
                            output_f board_output, output_f message_output,
                            string command) {
      if (command.empty()) {
        board_output(cg.display_board(display_style));
        return true;
      }
      assert(command[0]=='/');
      command=command.substr(1, string::npos);
      if (command=="help") {
        message_output(display_moves(cg, cg.legal_moves())+"\n");
        return true;
      }
      else if (command=="undo") {
        ncg.undo_last_move();
        ncg.undo_last_move();
        message_output("undo to "+to_text(cg.last_move().number)+": "
                       +cg.last_move().move+"\n");
        board_output(cg.display_board(display_style));
      }
      else if (command=="quit")
        return false;
      else
        message_output("unknown command\n");
      return true;
    };
  }

}
