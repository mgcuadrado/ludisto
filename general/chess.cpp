#include "chess.h"
#include "display_svg.h"
#include "multi.h"
#include <regex>
#ifndef NDEBUG
#include <unordered_set>
#endif

using namespace std;

namespace sxako::chess {
namespace {

  namespace FideChess {

    PieceBox const fide_piece_box{
      color_piece_squares(
        {
          {{"i_pawn", "pawn"}, {"P", "p"}, 'p'},
          {{"i_king", "king"}, {"K", "k"}, 'k'},
          {{"i_rook", "rook"}, {"R", "r"}, 'r'},
          {{"knight"}, {"N", "n"}, 'n'},
          {{"bishop"}, {"B", "b"}, 'b'},
          {{"queen"},  {"Q", "q"}, 'q'}
        },
        {{Color::white, "w_"}, {Color::black, "b_"}})};

    Square operator ""_sq(char const *name, size_t)
      { return fide_piece_box[name]; }

    struct FideData
      : Piece2DGameData {
      FideData()
        : Piece2DGameData(fide_piece_box,
                          make_pair(1, 8), make_pair(1, 8)) { }

      Move_i_rook_king_and_castling i_rook_king_and_castling_m{*this};
      Move_rook_knight_bishop_queen rook_knight_bishop_queen_m{*this};
      Move_king_and_check_mate king_m_and_check_mate{*this};

      promotion_f const white_promotion=[this](Board const &, Location l) {
        return
          get_y(l)==get_y(squares.range).second
          ? series<Square>{
              "w_rook"_sq, "w_knight"_sq, "w_bishop"_sq, "w_queen"_sq}
          : series<Square>{};
      };
      promotion_f const black_promotion=[this](Board const &, Location l) {
        return
          get_y(l)==get_y(squares.range).first
          ? series<Square>{
              "b_rook"_sq, "b_knight"_sq, "b_bishop"_sq, "b_queen"_sq}
          : series<Square>{};
      };
      Move_pawns_i_pawns_en_passant pawns_i_pawns_en_passant_m {
        *this,
        {white_promotion, black_promotion},
        {w_i_pawn_advance_steps, b_i_pawn_advance_steps}};

      LimitedRepetition limited_repetition{
        *this, 50, {"w_i_pawn"_sq, "w_pawn"_sq, "b_i_pawn"_sq, "b_pawn"_sq}};
    } fide_data;

    /// evaluation

    material_table_t const fide_material_table={
      {' ', 0.0},
      {'p', 1.0},
      {'r', 5.0},
      {'n', 2.9},
      {'b', 3.0},
      {'q', 9.0},
      {'k', 0.0}};

    namespace michniewski_impl {

      // see https://www.chessprogramming.org/Simplified_Evaluation_Function

      // all tabulated values in centipawns

      DataSpec<Kind::table> tables_spec;

      using B8x8=Addressing<Kind::table, Straight2D<>, int>;
      auto b8x8=make_tuple(make_pair(1, 8), make_pair(1, 8));
      B8x8 const pawn_diff(tables_spec, b8x8);
      B8x8 const knight_diff(tables_spec, b8x8);
      B8x8 const bishop_diff(tables_spec, b8x8);
      B8x8 const rook_diff(tables_spec, b8x8);
      B8x8 const queen_diff(tables_spec, b8x8);
      B8x8 const king_begin_diff(tables_spec, b8x8);
      B8x8 const king_end_diff(tables_spec, b8x8);

      auto const
        pawn_v=100,
        rook_v=500,
        knight_v=320,
        bishop_v=330,
        queen_v=900;

      map<Piece, int> const piece_value=
        {{' ', 0},
         {'p', pawn_v},
         {'r', rook_v},
         {'n', knight_v},
         {'b', bishop_v},
         {'q', queen_v},
         {'k', 0}};

      template <typename VR>
      auto generate_tables(VR const &vertical_range) {
        Data<Kind::table> tables(tables_spec);
        using A8x8=array<array<int, 8>, 8>;
        auto load_diff_table=[&tables, vertical_range](auto a, A8x8 data) {
          assign_data(tables(a), data,
                      reorder_indices<1, 0>(),
                      vertical_range, range(1, 8));
        };

        load_diff_table(pawn_diff,
                        {{{  0,   0,   0,   0,   0,   0,   0,   0},
                          { 50,  50,  50,  50,  50,  50,  50,  50},
                          { 10,  10,  20,  30,  30,  20,  10,  10},
                          {  5,   5,  10,  25,  25,  10,   5,   5},
                          {  0,   0,   0,  20,  20,   0,   0,   0},
                          {  5,  -5, -10,   0,   0, -10,  -5,   5},
                          {  5,  10,  10, -20, -20,  10,  10,   5},
                          {  0,   0,   0,   0,   0,   0,   0,   0}}});
        load_diff_table(knight_diff,
                        {{{-50, -40, -30, -30, -30, -30, -40, -50},
                          {-40, -20,   0,   0,   0,   0, -20, -40},
                          {-30,   0,  10,  15 , 15,  10,   0, -30},
                          {-30,   5,  15,  20,  20,  15,   5, -30},
                          {-30,   0,  15,  20,  20,  15,   0, -30},
                          {-30,   5,  10,  15,  15,  10,   5, -30},
                          {-40, -20,   0,   5,   5,   0, -20, -40},
                          {-50, -40, -30, -30, -30, -30, -40, -50}}});
        load_diff_table(bishop_diff,
                        {{{-20, -10, -10, -10, -10, -10, -10, -20},
                          {-10,   0,   0,   0,   0,   0,   0, -10},
                          {-10,   0,   5,  10,  10,   5,   0, -10},
                          {-10,   5,   5,  10,  10,   5,   5, -10},
                          {-10,   0,  10,  10,  10,  10,   0, -10},
                          {-10,  10,  10,  10,  10,  10,  10, -10},
                          {-10,   5,   0,   0,   0,   0,   5, -10},
                          {-20, -10, -10, -10, -10, -10, -10, -20}}});
        load_diff_table(rook_diff,
                        {{{  0,   0,   0,   0,   0,   0,   0,   0},
                          {  5,  10,  10,  10,  10,  10,  10,   5},
                          { -5,   0,   0,   0,   0,   0,   0,  -5},
                          { -5,   0,   0,   0,   0,   0,   0,  -5},
                          { -5,   0,   0,   0,   0,   0,   0,  -5},
                          { -5,   0,   0,   0,   0,   0,   0,  -5},
                          { -5,   0,   0,   0,   0,   0,   0,  -5},
                          {  0,   0,   0,   5,   5,   0,   0,   0}}});
        load_diff_table(queen_diff,
                        {{{-20, -10, -10,  -5,  -5, -10, -10, -20},
                          {-10,   0,   0,   0,   0,   0,   0, -10},
                          {-10,   0,   5,   5,   5,   5,   0, -10},
                          { -5,   0,   5,   5,   5,   5,   0,  -5},
                          {  0,   0,   5,   5,   5,   5,   0,  -5},
                          {-10,   5,   5,   5,   5,   5,   0, -10},
                          {-10,   0,   5,   0,   0,   0,   0, -10},
                          {-20, -10, -10,  -5,  -5, -10, -10, -20}}});
        load_diff_table(king_begin_diff,
                        {{{-30, -40, -40, -50, -50, -40, -40, -30},
                          {-30, -40, -40, -50, -50, -40, -40, -30},
                          {-30, -40, -40, -50, -50, -40, -40, -30},
                          {-30, -40, -40, -50, -50, -40, -40, -30},
                          {-20, -30, -30, -40, -40, -30, -30, -20},
                          {-10, -20, -20, -20, -20, -20, -20, -10},
                          { 20,  20,   0,   0,   0,   0,  20,  20},
                          { 20,  30,  10,   0,   0,  10,  30,  20}}});
        load_diff_table(king_end_diff,
                        {{{-50, -40, -30, -20, -20, -30, -40, -50},
                          {-30, -20, -10,   0,   0, -10, -20, -30},
                          {-30, -10,  20,  30,  30,  20, -10, -30},
                          {-30, -10,  30,  40,  40,  30, -10, -30},
                          {-30, -10,  30,  40,  40,  30, -10, -30},
                          {-30, -10,  20,  30,  30,  20, -10, -30},
                          {-30, -30,   0,   0,   0,   0, -30, -30},
                          {-50, -30, -30, -30, -30, -30, -30, -50}}});
        return tables;
      }

      auto const
        white_tables=generate_tables(range(8, 1)),
        black_tables=generate_tables(range(1, 8));

      // how to interpret Michniewski's words "for the king positional
      // evaluation, the end game begins when both sides have no queens or
      // every side which has a queen has additionally no other pieces or one
      // minorpiece maximum" in a way that doesn't cause a disruption in the
      // evaluation?  i'll ramp between the "begin" and "end" values using
      // the material of the own army; i think when he says "both sides have
      // no queens" he's referring to a normal game in which you don't
      // exchange queens before you have exchanged other pieces and some
      // pawns, say 5; so, i'll decide we are in full end when we have lost
      // the queen and some other pieces (-14-other), or when we have lost 2
      // rooks and 3 minor pieces and some pawns (-25); if the "other pieces"
      // exchanged before the queens are 1 rook and 2 minor pieces, we agree
      // on -25ish; on the other hand, i wouldn't even think of quitting the
      // "begin" evaluation before the castling zone is in somewhat bad
      // shape, which should happen when we've lost 3 pawns, 2 minor pieces,
      // and 1 rook i.e. -14; so, let's ramp linearly between the following
      // two breakpoints: q+r+b+n+5p, q+b+4p
      int constexpr
        begin_breakpoint=queen_v+rook_v+bishop_v+knight_v+5*pawn_v,
        end_breakpoint=queen_v+bishop_v+3*pawn_v;

      float king_endness(int material) {
        return
          (material>begin_breakpoint)
          ? 0
          : (material<end_breakpoint)
          ? 1
          : float(material-begin_breakpoint)
            /float(end_breakpoint-begin_breakpoint);
      }

      score_t eval_color(Piece2DGameData const &d, Board const &b,
                         Data<Kind::table> const &tables, Color c) {
        int material=0;
        for (auto s: d.piece_box.enumerated_occupied_squares)
          if (d.color_of(s)==c)
            material+=
              b(d.square_count.counts)[s]*int(piece_value.at(d.piece_of(s)));

        Location king_l;
        int position=0;
        for (auto l: d.squares.all_enumerated_coords) {
          Square s=b(d.squares)[l];
          if (d.is_occupied(s) and d.color_of(s)==c)
            switch (d.piece_of(s)) {
            case 'p': position+=tables(pawn_diff)[l];   break;
            case 'n': position+=tables(knight_diff)[l]; break;
            case 'b': position+=tables(bishop_diff)[l]; break;
            case 'r': position+=tables(rook_diff)[l];   break;
            case 'q': position+=tables(queen_diff)[l];  break;
            case 'k': king_l=l; break;
            default: break;
            }
        }

        float king_end_factor=king_endness(material);
        score_t king_position=
          (1.-king_end_factor)*tables(king_begin_diff)[king_l]
          +king_end_factor*tables(king_end_diff)[king_l];

        return (material+position+king_position)/100.;
      }

    }

    score_t michniewski(Board const &b) {
      Piece2DGameData const &d=fide_data; // FIXME: generalise
      using namespace michniewski_impl;
      score_t
        white_s=eval_color(d, b, white_tables, Color::white),
        black_s=eval_color(d, b, black_tables, Color::black);
      if (b(d.turn)==Color::white) // i've just played
        return black_s-white_s; // i'm black
      else
        return white_s-black_s; // i'm white
    }

    EvaluationMap chess_evaluation_map;

    EvaluationJustMaterial const
      just_material_e(chess_evaluation_map,
                      fide_data, fide_material_table);
    EvaluationMaterialAndPosition const
      material_and_position_e(chess_evaluation_map,
                              fide_data, fide_material_table);

    Evaluation const michniewski_e(
      "michniewski",
      R"(syntax: michniewski

Michniewski evaluation
  )",
      [](params_t p) {
        if (not p.empty())
          throw invalid_argument(
                  "wrong params for \"michniewski\" (should have been empty)");
        return michniewski;
      },
      chess_evaluation_map
    );

    /// initialisation

    void initialize_chess(Board &b) {
      auto const glossary=
        fide_piece_box.glossary_shorthand_char_to_first_square();
      string board_s=
        ".-----------------."
        "| r n b q k b n r |"
        "| p p p p p p p p |"
        "|                 |"
        "|                 |"
        "|                 |"
        "|                 |"
        "| P P P P P P P P |"
        "| R N B Q K B N R |"
        "'-----------------'";
      parse_2d_board(b(fide_data.squares), board_s, glossary);

      b(fide_data.turn)=Color::white;

      fide_data.initialize(b);
    }

  }

  /// board display

  string display_ansi_unicode(Piece2DGameData const &d,
                              Board const &b,
                              LastMove const &last_move,
                              string style) {
    static map<Piece, pair<string, string>> const unicode_pieces={
      {'p', {"♙ ", "♟ "}},
      {'r', {"♖ ", "♜ "}},
      {'n', {"♘ ", "♞ "}},
      {'b', {"♗ ", "♝ "}},
      {'q', {"♕ ", "♛ "}},
      {'k', {"♔ ", "♚ "}},
      {' ', {"  ", "  "}}
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

  /// rules

  template <typename I>
  Rules general_chess_rules(Piece2DGameData const &d, I initialize) {
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

  namespace FideChess {

    Rules chess_rules() {
      static Rules result=
        general_chess_rules(fide_data, initialize_chess);
      return result;
    }

    GameSpec chess_game_spec(
      game_spec_map(),
      "chess",
      "chess\n",
      "michniewski",
      chess_rules(),
      chess_evaluation_map);

  }

  namespace Chess960 {

    // https://en.wikipedia.org/wiki/Chess960_numbering_scheme
    string chess960_scharnagl(unsigned scharnagl_index) {
      if (scharnagl_index>=960)
        throw runtime_error(
          "the Scharnagl index must be between 0 and 959 (was "
          +to_text(scharnagl_index)+")");
      unsigned
        n=scharnagl_index,
        n2=n/4, b1=n%4,
        n3=n2/4, b2=n2%4,
        n4=n3/6, q=n3%6;
      string result="        ";

      auto set_nth_free=[&result](unsigned n, char s) { // zero-based
        unsigned i=0;
        while (result[i] not_eq ' ')
          ++i;
        while (n) {
          ++i;
          --n;
          while (result[i] not_eq ' ')
            ++i;
        }
        result[i]=s;
      };

      result[2*b1+1]='B'; // light bishop
      result[2*b2]='B'; // dark bishop
      set_nth_free(q, 'Q');
      array<pair<unsigned, unsigned>, 10> const knight_table=
        {{{0, 1}, {0, 2}, {0, 3}, {0, 4},
          {1, 2}, {1, 3}, {1, 4},
          {2, 3}, {2, 4},
          {3, 4}}};
      assert(n4<10);
      set_nth_free(knight_table[n4].second, 'N'); // first 2nd...
      set_nth_free(knight_table[n4].first, 'N');  // lest 1st shift it
      set_nth_free(0, 'R');
      set_nth_free(0, 'K');
      set_nth_free(0, 'R');
      return result;
    }

    [[maybe_unused]] bool check_scharnagl=[]() {
      assert(chess960_scharnagl(518)=="RNBQKBNR");
#ifndef NDEBUG
      unordered_set<string> all_960;
      for (unsigned i=0; i<960; ++i)
        all_960.insert(chess960_scharnagl(i));
      assert(all_960.size()==960);
#endif
      return true;
    }();

    void initialize_chess960(Board &b, string s_index) {
      using namespace FideChess;
      unsigned scharnagl_index;
      if (s_index.empty()) {
        random_device rd;
        random_generator_t rg(rd());
        scharnagl_index=uniform_int_distribution<unsigned>(0, 959)(rg);
      }
      else
        scharnagl_index=from_text<unsigned>(s_index);
      string white_pieces=
        chess960_scharnagl(scharnagl_index);
      string white_row=" ", black_row=" ";
      for (auto c: white_pieces) {
        white_row+=string()+char(toupper(c))+" ";
        black_row+=string()+char(tolower(c))+" ";
      }

      auto const glossary=
        fide_piece_box.glossary_shorthand_char_to_first_square();
      string board_s=
        ".-----------------."
        "|"+  black_row  +"|"
        "| p p p p p p p p |"
        "|                 |"
        "|                 |"
        "|                 |"
        "|                 |"
        "| P P P P P P P P |"
        "|"+  white_row  +"|"
        "'-----------------'";
      parse_2d_board(b(fide_data.squares), board_s, glossary);

      b(fide_data.turn)=Color::white;
      fide_data.initialize(b);
    }

    Rules chess960_rules(string name) {
      string s_index;
      if (name.find(':') not_eq string::npos)
        s_index=name.substr(name.find(':')+1);
      static Rules result=
        general_chess_rules(
          FideChess::fide_data,
          [s_index](Board &b) { return initialize_chess960(b, s_index); });
      return result;
    }

    GameSpec chess960_game_spec(
      game_spec_map(),
      "chess960",
      "Fischer's chess 960; arg: Scharnagl index (default: random)\n",
      "michniewski",
      chess960_rules,
      FideChess::chess_evaluation_map);

  }

  namespace ChessAttack {

    auto const chess_attack_piece_box=FideChess::fide_piece_box;

    Square operator ""_sq(char const *name, size_t)
      { return chess_attack_piece_box[name]; }

    struct ChessAttackData
      : Piece2DGameData {
      ChessAttackData()
        : Piece2DGameData(chess_attack_piece_box,
                          make_pair(1, 5), make_pair(1, 6)) { }

      Move_i_rook_king_and_castling i_rook_king_and_castling_m{*this};
      Move_rook_knight_bishop_queen rook_knight_bishop_queen_m{*this};
      Move_king_and_check_mate king_m_and_check_mate{*this};

      series<Square>
      promotion(Board const &b, Location l,
                coord_t y_promotion, series<Square> candidates) {
        series<Square> result;
        if (get_y(l)==y_promotion)
          for (Square s: candidates)
            if (b(square_count.counts)[s]==0)
              result.push_back(s);
        return result;
      }
      promotion_f white_promotion=[this](Board const &b, Location l) {
        return promotion(
                 b, l,
                 get_y(squares.range).second,
                 {"w_rook"_sq, "w_knight"_sq, "w_bishop"_sq, "w_queen"_sq});
      };
      promotion_f black_promotion=[this](Board const &b, Location l) {
        return promotion(
                 b, l,
                 get_y(squares.range).first,
                 {"b_rook"_sq, "b_knight"_sq, "b_bishop"_sq, "b_queen"_sq});
      };
      Move_pawns_i_pawns_en_passant pawns_i_pawns_en_passant_m {
        *this,
        {white_promotion, black_promotion},
        {w_i_pawn_advance_steps, b_i_pawn_advance_steps}};

      LimitedRepetition limited_repetition{
        *this, 50, {"w_i_pawn"_sq, "w_pawn"_sq, "b_i_pawn"_sq, "b_pawn"_sq}};
    } chess_attack_data;

    void initialize_chess_attack(Board &b) {
      auto const glossary=
        chess_attack_piece_box.glossary_shorthand_char_to_first_square();
      string board_s=
        ".-----------."
        "| r n b q k |"
        "| p p p p p |"
        "|           |"
        "|           |"
        "| P P P P P |"
        "| R N B Q K |"
        "'-----------'";
      parse_2d_board(b(chess_attack_data.squares), board_s, glossary);

      b(chess_attack_data.turn)=Color::white;

      chess_attack_data.initialize(b);
    }

    Rules chess_attack_rules() {
      static Rules result=
        general_chess_rules(chess_attack_data, initialize_chess_attack);
      return result;
    }

    EvaluationMap chess_attack_evaluation_map;

    Evaluation const material_and_position_chess_attack_e( // FIXME: un-copy-paste
      "material_and_position_chess_attack",
      R"(syntax: material_and_position:[factor]

piece value plus factor times centeredness or advancedness
)",
      [](params_t p) {
        float factor=.04;
        if (not p.empty()) {
          if (not (p.size()==1 and p.front().value.empty()))
            throw invalid_argument(
                    "wrong params for \"material_and_position\" (should have "
                    "zero params, or one arg-only param)");
          factor=from_text<float>(p.front().arg);
        }
        return
          [factor](Board const &b) {
            return material_and_position(
                     chess_attack_data, b,
                     FideChess::fide_material_table,
                     factor);
          };
      },
      chess_attack_evaluation_map
    );

    GameSpec chess_attack_game_spec(
      game_spec_map(),
      "chess_attack",
      "ChessAttack\n",
      "material_and_position_chess_attack:.04",
      chess_attack_rules(),
      chess_attack_evaluation_map);

  }

}
}
