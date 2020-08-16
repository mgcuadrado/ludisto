#include "draughts.h"
#include "fide.h"
#include "straight.h"
#include "display_svg.h"
#include "multi.h"

using namespace std;

namespace sxako::draughts {

  using namespace piece_2d_game;
  using namespace draughts;

  using promotion_f=std::function<series<Square> (Board const &, Location)>;
  bool add_piece_move(Piece2DGameData const &d, Board const &b,
                      add_legal_move_f const &add_move,
                      Move const &m, Location to,
                      list<Location> captures,
                      promotion_f promotion) {
    series<Square> promotion_squares;
    if (promotion and (promotion_squares=promotion(b, to)).size()) {
      for (auto p: promotion_squares)
        if (add_move(make_suffix(string(1, d.piece_of(p)))+m+make_set(to, p),
                     captures))
          return true;
      return false;
    }
    else
      return add_move(m, captures);
  }

  using PawnSpecs=std::tuple<series<Displacement>, promotion_f>;
  void piece_push(Piece2DGameData const &d,
                  PawnSpecs const &pawn_specs,
                  add_legal_move_f const &add_move,
                  Board const &b, Location l,
                  Square l_s, Square change_to,
                  bool only_capture, Location) {
    if (only_capture)
      return;
    series<Displacement> const &steps=get<0>(pawn_specs);
    promotion_f const &promotion=get<1>(pawn_specs);
    for (Displacement dis: steps) {
      Location to=l+dis;
      if (Square s;
          b(d.squares).get_if_contains(to, s) and not d.is_occupied(s)) {
        if (add_piece_move(d, b, add_move,
                           make_move(l, to, l_s, change_to), to,
                           {}, promotion))
          return;
      }
    }
  }

  namespace {

    template <typename C, typename T>
    C added(C const &c, T const &t) {
      C result=c;
      result.push_back(t);
      return result;
    }

    bool add_or_follow_pawn_capture(Piece2DGameData const &d,
                                    series<Displacement> const &steps,
                                    promotion_f const &promotion,
                                    add_legal_move_f const &add_move,
                                    Board const &b,
                                    Move const &m,
                                    list<Location> const &captures,
                                    Color c, Location l,
                                    Square l_s, Square change_to) {
      bool no_capture=true;
      for (Displacement dis: steps) {
        Location capture=l+dis, to=capture+dis;
        if (Square capture_s, to_s;
            b(d.squares).get_if_contains(capture, capture_s)
            and b(d.squares).get_if_contains(to, to_s)
            and d.is_occupied(capture_s) and d.color_of(capture_s) not_eq c
            and not d.is_occupied(to_s)) {
          Board b_capturing=b;
          Move this_m=make_capture(capture)+make_move(l, to);
          d.move_handler.handle(b_capturing, this_m);
          if (add_or_follow_pawn_capture(
                d, steps, promotion,
                add_move, b_capturing,
                m+this_m, added(captures, capture), c, to, l_s, change_to))
            return true;
          no_capture=false;
        }
      }
      if (no_capture) {
        if (m.size()) {
          Move this_m=
            l_s==change_to ? m : m+make_set(l, change_to);
          if (add_piece_move(d, b, add_move, this_m, l, captures, promotion))
            return true;
        }
      }
      return false;
    }

  }

  void piece_capture(Piece2DGameData const &d,
                     PawnSpecs const &pawn_specs,
                     add_legal_move_f const &add_move,
                     Board const &b, Location l,
                     Square l_s, Square change_to,
                     bool, Location) {
    series<Displacement> const &steps=get<0>(pawn_specs);
    promotion_f const &promotion=get<1>(pawn_specs);
    Color c=d.color_of(l_s);
    add_or_follow_pawn_capture(d, steps, promotion, add_move,
                               b, Move(), {}, c, l, l_s, change_to);
  }

  struct Move_pawns : Move_base {
    Move_pawns(Piece2DGameData &d,
               std::array<promotion_f, 2> promotions)
      : Move_base(d), promotions(promotions) { }
    std::array<promotion_f, 2> const promotions;
    static auto const white=0, black=1;
    SimpleMoves
      w_p_m{per_square, pb["w_d-man"], piece_push,
            make_tuple(w_pawn_steps, promotions[white])},
      b_p_m{per_square, pb["b_d-man"], piece_push,
            make_tuple(b_pawn_steps, promotions[black])},
      w_p_c{per_square, pb["w_d-man"], piece_capture,
            make_tuple(w_pawn_steps, promotions[white])},
      b_p_c{per_square, pb["b_d-man"], piece_capture,
            make_tuple(b_pawn_steps, promotions[black])};
  };

  struct Move_kings : Move_base {
    using Move_base::Move_base;
    promotion_f const no_promotion=
      [](Board const &, Location) { return series<Square>{}; };
    SimpleMoves
      w_k_m{per_square, pb["w_d-king"], piece_push,
            make_tuple(king_steps, no_promotion)},
      b_k_m{per_square, pb["b_d-king"], piece_push,
            make_tuple(king_steps, no_promotion)},
      w_k_c{per_square, pb["w_d-king"], piece_capture,
            make_tuple(king_steps, no_promotion)},
      b_k_c{per_square, pb["b_d-king"], piece_capture,
            make_tuple(king_steps, no_promotion)};
  };

  PieceBox const draughts_piece_box{
    color_piece_squares(
      {
        {{"d-man"}, {"o", "x"}, 'p'},
        {{"d-king"}, {"O", "X"}, 'k'}
      },
      {{Color::white, "w_"}, {Color::black, "b_"}})};

  Square operator ""_sq(char const *name, size_t)
    { return draughts_piece_box[name]; }

  struct DraughtsData
    : Piece2DGameData {
    DraughtsData()
      : Piece2DGameData(draughts_piece_box,
                        even<Location>(), make_pair(1, 8), make_pair(1, 8)) { }

    promotion_f const white_promotion=[this](Board const &, Location l) {
      return
        get_y(l)==get_y(squares.range).second
        ? series<Square>{"w_d-king"_sq}
        : series<Square>{};
    };
    promotion_f const black_promotion=[this](Board const &, Location l) {
      return
        get_y(l)==get_y(squares.range).first
        ? series<Square>{"b_d-king"_sq}
        : series<Square>{};
    };
    Move_pawns pawn_m{*this, {white_promotion, black_promotion}};
    Move_kings king_m{*this};

    // FIXME: "ForceCaptureIfPossible" uses an inefficient algorithm; it would
    // be better to first generate all capturing moves (using
    // "piece_capture()"), and then, iff "moves" is empty, generate all
    // non-capturing moves; this is impossible with the current implementation
    // for move generation; well, this could be done if we instantiate another
    // per-square generator and add it to the same move generator; the sequence
    // would be "first per-square", "is it empty?", "if so, second per-square"
    ForceCaptureIfPossible force_capture_if_possible{*this};
    LoseIfNoLegalMove lose_if_no_legal_move{*this};
  } draughts_data;

  /// evaluation

  fide::material_table_t const draughts_material_table={
      {' ', 0.0},
      {'p', 1.0},
      {'k', 3.0}};

  EvaluationMap draughts_evaluation_map;

  fide::EvaluationMaterialAndPosition const
    material_and_position_e(draughts_evaluation_map,
                            draughts_data, draughts_material_table);

  /// initialisation

  void initialize_draughts(Board &b) {
    auto const glossary=
      draughts_piece_box.glossary_shorthand_char_to_first_square();
    string board_s=
      ".-----------------."
      "|   x   x   x   x |"
      "| x   x   x   x   |"
      "|   x   x   x   x |"
      "|                 |"
      "|                 |"
      "| o   o   o   o   |"
      "|   o   o   o   o |"
      "| o   o   o   o   |"
      "'-----------------'";
    parse_2d_board(b(draughts_data.squares), board_s, glossary);

    b(draughts_data.turn)=Color::white;

    draughts_data.initialize(b);
  }

  /// board display

  string display_ansi_unicode(Piece2DGameData const &d,
                              Board const &b, LastMove const &last_move,
                              string style) {
    static map<Piece, pair<string, string>> const unicode_pieces={
      {'p', {"⛀ ", "⛂ "}},
      {'k', {"⛁ ", "⛃ "}}
    };
    ignore=style;
    auto square_bg_to_text=
    [&d](Square s, bool white_background) {
      auto pp=unicode_pieces.at(d.piece_of(s));
      return ((d.color_of(s)==Color::white)==white_background)
             ? pp.first
             : pp.second;
    };
    return display_ansi_straight_2d(
             b(d.squares),
             square_bg_to_text,
             [&d](Square s) { return d.is_occupied(s); },
             info_turn_last_move(to_text(b(d.turn)), last_move));
  }

  auto svg_location_to_empty_square=
    svg_checkered_location_to_empty_square("light-square", "dark-square");

  display_style_map_t const display_style_map={
    {"ascii",   display_ascii()},
    {"unicode", display_ansi_unicode},
    {"svg",     display_svg(svg_location_to_empty_square)}
  };

  Rules english_draughts_rules() {
    Piece2DGameData const &d=draughts_data;
    auto initialize=initialize_draughts;
    return {
      {
        d.state_spec,
        d.manag_spec,
        d.cache_spec
      },
      d.turn,
      initialize,
      [&d](Data<Kind::state> const &state) { return d.compute_cache(state); },
      [&d](Board const &b) { return d.outcome(b); },
      [&d](Board &b, Move const &m) { d.board_move(b, m); },
      [&d](Board const &b) { return d.legal_moves(b); },
      write_move,
      parse_move_default([&d](Board const &b) { return d.legal_moves(b); },
                         write_move),
      [&d](Board const &b, LastMove const &lm, std::string style)
        { return display_board(display_style_map, d, b, lm, style); }
    };
  }

  GameSpec draughts_game_spec(game_spec_map(),
                              "english_draughts", "English draughts\n",
                              "material_and_position:.04",
                              english_draughts_rules(),
                              draughts_evaluation_map);

}
