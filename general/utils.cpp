#include "utils.h"
#include "think.h"
#include "human.h"
#include "stats.h"
#include "multi.h"
#include "CLI11.hpp"
#include <iostream>

using namespace std;

namespace sxako {

  namespace {

    template <typename T>
    T round_score(T s) { return round(s*1000.)/1000.; }

  }

  string match_up(GameSetup gs, function<bool ()> go_on) {
    Game &g=*gs.game;
    shared_ptr<Player> turn_player=gs.player_a, wait_player=gs.player_b;
    gs.board_output(g.display_board(gs.display_style));
    while (g.outcome()==Rules::Outcome::playing and go_on()) {
      if (gs.help_move)
        gs.message_output(display_moves(g, g.legal_moves())+"\n");
      auto move_score=turn_player->get_move(g);
      if (move_score.move.size()==0) // an empty move signals a game abort
        break;
      g.move(move_score.move);
      gs.message_output(to_text(g.last_move().number)+": "
                        +g.write_move(move_score.move)
                        +" ("+to_text(round_score(move_score.score))+")\n");
      gs.board_output(g.display_board(gs.display_style));
      swap(turn_player, wait_player);
    }
    string n_moves="("+to_text(g.last_move().number)+" moves)";
    switch (g.outcome()) {
    case Rules::Outcome::playing:
      cout << "still playing " << n_moves << endl;
      return "playing";
    case Rules::Outcome::draw:
      gs.message_output("it's a draw "+to_text(n_moves)+"\n");
      return "draw";
    case Rules::Outcome::last_move_won:
      gs.message_output("\""+wait_player->name+"\" won "
                        +to_text(n_moves)+"\n");
      return wait_player->name;
    case Rules::Outcome::last_move_lost:
      gs.message_output("\""+wait_player->name+"\" lost "
                        +to_text(n_moves)+"\n");
      return turn_player->name;
    default: throw logic_error("unknown outcome");
    }
  }

  GameSetup game_setup(int argc, char *argv[],
                       input_f human_input,
                       output_f board_output,
                       output_f message_output,
                       string style_override) {
    CLI::App app{"App description"};
    app.get_formatter()->column_width(32);

    bool help_after_each_move;
    app.add_flag("-H,--help-move", help_after_each_move,
                 "show help (legal moves) after each move");

    bool interactive;
    app.add_flag("-i,--interactive", interactive,
                 "interactive play after tests");

    bool use_ansi_and_unicode;
    app.add_flag("-u,--unicode", use_ansi_and_unicode,
                 "use ANSI and Unicode for displaying the board");

    string a_algo_params_s, b_algo_params_s;
    app.add_option("-P,--a-params", a_algo_params_s,
                   "additional algo params for player A (\"think.h\")",
                   true);
    app.add_option("-p,--b-params", b_algo_params_s,
                   "additional algo params for player B (\"think.h\")",
                   true);

    bool stats;
    app.add_flag("-s,--stats", stats,
                 "show stats at the end");

    string rules_name="chess";
    app.add_option("-r,--rules", rules_name,
                   "select game rules",
                   true);
    string a_evaluation="", b_evaluation="";
    app.add_option("-V,--a-evaluation", a_evaluation,
                   "evaluation function specification for player (A)",
                   true)
      ->take_last();
    app.add_option("-v,--b-evaluation", b_evaluation,
                   "evaluation function specification for player (B)",
                   true)
      ->take_last();

    bool list_games;
    app.add_flag("-l,--list", list_games,
                 "list known games");

    string show_game;
    auto show_game_opt=
      app.add_option("-g,--show-game", show_game,
                     "show game help and evaluation functions");
    string show_evaluation;
    app.add_option("-e,--show-evaluation", show_evaluation,
                   "show evaluation help")
      ->needs(show_game_opt);

    // CLI11_PARSE(app, argc, argv); // can't do this since we aren't in main()
    try {
      app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
      // exit with given exit value; may be 0 (successful), e.g. with "-h":
      exit(app.exit(e));
    }

    if (not show_evaluation.empty()) {
      cout << "about evaluation \"" << show_evaluation
           << "\" for game \"" << show_game << "\":" << endl << endl;
      auto gs=game_spec_map()(show_game);
      cout << gs.evaluation_map.help(show_evaluation);
      exit(0);
    }

    if (not show_game.empty()) {
      cout << "about game \"" << show_game << "\":" << endl << endl;
      auto gs=game_spec_map()(show_game);
      cout << gs.help << endl;
      cout << "available evaluation functions:" << endl;
      for (auto en: gs.evaluation_map.list())
        cout << "  " << en << endl;
      exit(0);
    }

    if (list_games) {
      cout << "known games:" << endl;
      for (auto n: game_spec_map().list())
        cout << "  " << n << endl;
      exit(0);
    }

    if (not a_algo_params_s.empty())
      a_algo_params_s=":"+a_algo_params_s;
    if (not b_algo_params_s.empty())
      b_algo_params_s=":"+b_algo_params_s;

    if (interactive) {
      a_algo_params_s+=":rs";
      b_algo_params_s+=":rs";
    }

    string
      a_algo_params(default_params_s+a_algo_params_s),
      b_algo_params(default_params_s+b_algo_params_s);

    string display_style=
      style_override.empty()
      ? (use_ansi_and_unicode ? "unicode" : "")
      : style_override;

    auto game_spec=game_spec_map()(rules_name);
    shared_ptr<Game> game=make_shared<Game>(game_spec.rules_f(rules_name));

    shared_ptr<Player> player_a, player_b;
    if (interactive)
      player_a=
        make_shared<StreamPlayer>( // human
         "human", human_input, board_output, message_output,
         human_command_handler(*game, display_style));
    else
      player_a=
        make_shared<ComputerPlayer>( // computer
          "computer a",
          game_spec.evaluation(a_evaluation),
          a_algo_params);
    player_b=
      make_shared<ComputerPlayer>( // computer
        "computer b",
        game_spec.evaluation(b_evaluation),
        b_algo_params);

    return {game, player_a, player_b,
            display_style,
            board_output, message_output,
            help_after_each_move, stats};
  }

  int play_game(int argc, char *argv[],
                input_f human_input,
                output_f board_output,
                output_f message_output,
                go_on_t go_on,
                string style_override) {
    GameSetup gs=game_setup(argc, argv,
                            human_input, board_output, message_output,
                            style_override);

    match_up(gs, go_on);

    if (gs.stats) {
      cout << "number of situation evaluations: " << n_evaluations << endl
           << "number of quick evaluations for move ordering: "
             << n_quick_evaluations << endl;
      cout << "max sizes of transposition tables per level:" << endl;
      for (auto const &m: max_transposition_table_size)
        cout << "    " << m.first << ": " << m.second << endl;
    }

    return 0;
  }

}
