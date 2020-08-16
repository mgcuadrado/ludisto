#include "multi.h"
#include "CLI11.hpp"

#include <iostream>

using namespace std;
using namespace sxako;

template <typename T>
T round_score(T s) { return round(s*1000.)/1000.; }

int main(int argc, char *argv[]) {
  begin_try_catch;

  CLI::App app{"App description"};
  app.get_formatter()->column_width(32);

  bool use_ansi_and_unicode;
  app.add_flag("-u,--unicode", use_ansi_and_unicode,
               "use ANSI and Unicode for displaying the board");

  CLI11_PARSE(app, argc, argv);

  auto chess_name="chess";
  auto chess_spec=game_spec_map()(chess_name);
  auto chess_rules=chess_spec.rules_f(chess_name);
  auto just_material=chess_spec.evaluation("just_material");
  auto material_and_position=chess_spec.evaluation("material_and_position");
  auto michniewski=chess_spec.evaluation("michniewski");

  string display_style=use_ansi_and_unicode ? "unicode" : "";

  cout << boolalpha;

  cout << Game(chess_rules).display_board(display_style);

  auto show_value=[&](Game const &game) {
    cout << "value for " << enemy(game.turn()) << ": "
         << round_score(just_material(game.board()))
         << "/" << round_score(material_and_position(game.board()))
         << "/" << round_score(michniewski(game.board()))
         << endl;
  };

  auto try_move=[show_value, display_style](Game &game, string move_s) {
    bool valid=true;
    Move m;
    try {
      m=game.parse_move(move_s);
      if (move_s not_eq game.write_move(m))
        cout << "error: " << move_s << " not_eq " << game.write_move(m) << endl;
      game.move(m);
    }
    catch (exception const &error) {
      valid=false;
      cout << "error: " << move_s << ": " << error.what() << endl;
    }
    cout << "move " << move_s << ": " << valid << endl;
    if (valid) {
      cout << game.display_board(display_style);
      show_value(game);
      cout << "possible moves: " << display_moves(game, game.legal_moves())
           << endl;
    }
  };

#define play(move) try_move(game, #move)

  bool interactive=false;
  if (not interactive) {
    {
      Game game(chess_rules);
      cout << "some move tests" << endl;
      show_value(game);
      play(A2A3);
      play(A2A3); // invalid: white's turn
      play(B7B5);
      play(C2C4);
      play(B5B4);
      play(C4C5);
      play(E7E5);
      play(D2D3);
      play(D7D5);
      play(C5D6);
      play(F8E7);
      play(B1C3);
      play(G8F6);
      play(C1E3);
      play(O-O);
      play(D1D2);
      play(H7H6);
      play(O-O-O);
      play(B4A3);
      play(B2B3);
      play(A3A2);
      play(H2H3);
      play(A2A1);
      play(A2A1r);
      cout << endl;
    }
    {
      Game game(chess_rules);
      cout << "undo before any move" << endl;
      game.undo_last_move();
      cout << "test all rules" << endl;
      play(E2E4);
      play(D7D5);
      play(E4E5);
      play(F7F5);
      play(E5F6);
      play(D5D4);
      play(C2C4);
      play(B8A6);
      play(G1H3);
      play(E7E6);
      play(D1H5);
      play(E8D7);
      play(F6F7);
      play(G7G6);
      play(F7G8q);
      play(D7E8);
      play(F1D3);
      play(G6H5);
      play(F2F3);
      play(D8H4);
      play(H3F2);
      play(C8D7);
      play(B1A3);
      play(E8D8);
      play(B2B3);
      play(D8C8);
      play(C1B2);
      play(C8B8);
      play(A1B1);
      play(B8C8);
      play(B1A1);
      play(D7E8);
      play(O-O);
      play(F8D6);
      play(G8H8);
      cout << "outcome: " << game.outcome() << endl;

      play(H4H2);
      cout << "outcome: " << game.outcome() << endl;
      cout << "one step back..." << endl;

      game.undo_last_move();
      play(H4G4);
      play(H8E8);
      cout << "outcome: " << game.outcome() << endl;

      cout << endl;
    }
  }

  end_try_catch;
}
