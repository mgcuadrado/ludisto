#include "chess.h"
#include "display_svg.h"
#include "multi.h"
#include <regex>

using namespace std;

namespace sxako::chess {

  namespace CaissaBritannia {

    series<Displacement> const
      bishop_no_capture_steps=
             {{ 0, +1},
         {-1,  0}, {+1,  0},
              { 0, -1}},
      lion_steps=king_steps,
      unicorn_steps=
                  {{-1, +2}, {+1, +2},
         {-2, +1}, {-1, +1}, {+1, +1}, {+2, +1},
         {-2, -1}, {-1, -1}, {+1, -1}, {+2, -1},
                   {-1, -2}, {+1, -2}},
      dragon_steps=
        {{-2, +2}, { 0, +2}, {+2, +2},
         {-2,  0},           {+2,  0},
         {-2, -2}, { 0, -2}, {+2, -2}};

    struct Move_rook_knight_bishop : Move_base {
      using Move_base::Move_base;
      SimpleMoves
          rook_m{per_square, {pb["w_rook"], pb["b_rook"]},
                   straight, rook_steps},
          knight_m{per_square, {pb["w_knight"], pb["b_knight"]},
                   one_step, knight_steps},
          bishop_m_c{per_square, {pb["w_bishop"], pb["b_bishop"]},
                     straight, bishop_steps},
          bishop_m_no_c{per_square, {pb["w_bishop"], pb["b_bishop"]},
                        can_move_one_step_no_capture, bishop_no_capture_steps};
    };

    struct Move_lion_unicorn_dragon : Move_base {
      using Move_base::Move_base;
      SimpleMoves
        lion_m   {per_square, {pb["w_lion"], pb["b_lion"]},
                  straight_capture_over, lion_steps},
        unicorn_m{per_square, {pb["w_unicorn"], pb["b_unicorn"]},
                  straight, unicorn_steps},
        dragon_m {per_square, {pb["w_dragon"], pb["b_dragon"] },
                  straight, dragon_steps};
    };

    struct Move_royal_queen : Move_base {
      using Move_base::Move_base;
      SimpleMoves
      queen_m{per_square, {pb["w_queen"], pb["b_queen"]},
              straight_royal, queen_steps};
    };

    PieceBox const caissa_britannia_piece_box{
      color_piece_squares(
        {
          {{"i_pawn", "pawn"}, {"P", "p"}, 'p'},
          {{"king"},    {"K", "k"}, 'k'},
          {{"rook"},    {"R", "r"}, 'r'},
          {{"knight"},  {"N", "n"}, 'n'},
          {{"bishop"},  {"B", "b"}, 'b'},
          {{"lion"},    {"L", "l"}, 'l'},
          {{"unicorn"}, {"U", "u"}, 'u'},
          {{"dragon"},  {"D", "d"}, 'd'},
          {{"queen"},   {"Q", "q"}, 'q'}
        },
        {{Color::white, "w_"}, {Color::black, "b_"}})};

    Square operator ""_sq(char const *name, size_t)
      { return caissa_britannia_piece_box[name]; }

    struct CaissaBritanniaData
      : Piece2DGameData {
      CaissaBritanniaData()
        : Piece2DGameData(caissa_britannia_piece_box,
                          make_pair(1, 10), make_pair(1, 10)) { }

      Move_king king_m{*this};
      Move_rook_knight_bishop rook_knight_bishop_m{*this};
      Move_lion_unicorn_dragon lion_unicorn_dragon_m{*this};
      Move_royal_queen queen_m{*this};

      TrackPiece track_queens{*this, move_command_handler, 'q'};
      AvoidCheck avoid_check{*this, track_queens, per_square};
      Mate mate{*this, track_queens, per_square};

      series<Square>
      promotion(Board const &b, Location l,
                coord_t y_promotion,
                Square knight, series<pair<Square, u8>> candidates_and_n) {
        series<Square> result;
        if (get_y(l)==y_promotion) {
          result.push_back(knight);
          for (auto cn: candidates_and_n)
            if (b(square_count.counts)[cn.first]<cn.second)
              result.push_back(cn.first);
        }
        return result;
      }
      promotion_f white_promotion=[this](Board const &b, Location l) {
        return promotion(
                 b, l,
                 get_y(squares.range).second,
                 "w_knight"_sq,
                 {{"w_king"_sq, 1}, {"w_rook"_sq, 2}, {"w_bishop"_sq, 2},
                  {"w_lion"_sq, 2}, {"w_unicorn"_sq, 2}, {"w_dragon"_sq, 2}});
      };
      promotion_f black_promotion=[this](Board const &b, Location l) {
        return promotion(
                 b, l,
                 get_y(squares.range).first,
                 "b_knight"_sq,
                 {{"b_king"_sq, 1}, {"b_rook"_sq, 2}, {"b_bishop"_sq, 2},
                  {"b_lion"_sq, 2}, {"b_unicorn"_sq, 2}, {"b_dragon"_sq, 2}});
      };
      Move_pawns_i_pawns_en_passant pawns_i_pawns_en_passant_m {
        *this,
        {white_promotion, black_promotion},
        {w_i_pawn_advance_steps, b_i_pawn_advance_steps}};

      LimitedRepetition limited_repetition{
        *this, 50, {"w_i_pawn"_sq, "w_pawn"_sq, "b_i_pawn"_sq, "b_pawn"_sq}};
    } caissa_britannia_data;

    /// evaluation

    material_table_t const caissa_britannia_material_table={
      {' ', 0.0},
      {'p', 1.0},
      {'r', 5.0},
      {'n', 2.9},
      {'b', 3.0},
      {'q', 0.0},
      {'k', 3.0}, // FIXME: keine Ahnung
      {'l', 5.0}, // FIXME
      {'u', 5.0}, // FIXME
      {'d', 5.0}  // FIXME
    };


    EvaluationMap caissa_britannia_evaluation_map;

    EvaluationJustMaterial const
    just_material_e(caissa_britannia_evaluation_map,
                    caissa_britannia_data,
                    caissa_britannia_material_table);
    EvaluationMaterialAndPosition const
    material_and_position_e(caissa_britannia_evaluation_map,
                            caissa_britannia_data,
                            caissa_britannia_material_table);

    /// initialisation

    void initialize_caissa_britannia(Board &b) {
      auto const glossary=
        caissa_britannia_piece_box.glossary_shorthand_char_to_first_square();
      string board_s=
        ".---------------------."
        "| d r u b q k b u r d |"
        "|   l             l   |"
        "| p p p p p p p p p p |"
        "|                     |"
        "|                     |"
        "|                     |"
        "|                     |"
        "| P P P P P P P P P P |"
        "|   L             L   |"
        "| D R U B Q K B U R D |"
        "'---------------------'";
      parse_2d_board(b(caissa_britannia_data.squares), board_s, glossary);

      b(caissa_britannia_data.turn)=Color::white;

      caissa_britannia_data.initialize(b);
    }

  }

  auto svg_location_to_empty_square=
    svg_caissa_britannia_location_to_empty_square(
      "whitish-square", "bluish-square", "reddish-square");

  display_style_map_t const display_style_map={
    {"ascii",   display_ascii()},
    {"svg",     display_svg(svg_location_to_empty_square)}
    };

  namespace CaissaBritannia {

    Rules caissa_britannia_rules() {
      static Piece2DGameData const &d=caissa_britannia_data;
      static Rules result{
        {
          d.state_spec,
          d.manag_spec,
          d.cache_spec
        },
        d.turn,
        initialize_caissa_britannia,
        [](Data<Kind::state> const &state) { return d.compute_cache(state); },
        [](Board const &b) { return d.outcome(b); },
        [](Board &b, Move const &m) { d.board_move(b, m); },
        [](Board const &b) { return d.legal_moves(b); },
        write_move,
        parse_move_default([](Board const &b) { return d.legal_moves(b); },
                           write_move),
        [](Board const &b, LastMove const &lm, std::string style)
          { return display_board(display_style_map, d, b, lm, style); }
      };
      return result;
    }

    GameSpec chess_game_spec(
      game_spec_map(),
      "caissa_britannia",
      "Caïssa Britannia\n",
      "material_and_position:.04",
      caissa_britannia_rules(),
      caissa_britannia_evaluation_map);

  }

}
