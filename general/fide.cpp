#include "fide.h"

using namespace std;

namespace sxako::fide {

  LimitedRepetition
  ::LimitedRepetition(Piece2DGameData &d,
                      size_t max_past_size,
                      series<Square> irreversible_squares)
      : d(d), max_past_size(max_past_size),
        irreversible_squares(irreversible_squares),
        past_size(d.manag_spec),
        past_hash(d.manag_spec, std::make_pair(1, max_past_size)) {
      d.move_command_handler.append(f_funct(this, &this_t::move_handler));
      d.post_push_front_turn_handler(f_funct(this, &this_t::turn_handler));
      d.outcome_filters.append(f_funct(this, &this_t::outcome_filter));
      d.initialization.append(f_funct(this, &this_t::reset));
    }

  void LimitedRepetition
  ::move_handler(Board &b, Location from, Location to) const {
    if (is_in(b(d.squares)[from], irreversible_squares)
        or (d.is_occupied(b(d.squares)[to]) not_eq empty))
      b(past_size)=0;
  }

  void LimitedRepetition
  ::turn_handler(Board &b) const {
    auto new_ps=++b(past_size);
    // this may be false when checking hypothetical moves (like "is there any
    // legal move?"):
    if (new_ps<=max_past_size)
      b(past_hash)[new_ps]=b.id_hash();
  }

  void LimitedRepetition
  ::outcome_filter(Rules::Outcome &o, Board const &b) const {
    if (o not_eq Rules::Outcome::playing)
      return;
    auto ps=b(past_size);
    if (ps>=max_past_size) {
      o=Rules::Outcome::draw;
      return;
    }
    auto last_id_hash=b(past_hash)[ps];
    for (auto i=ps-1; i>=1; --i)
      if (b(past_hash)[i]==last_id_hash) {
        o=Rules::Outcome::draw;
        return;
      }
  }
  void LimitedRepetition
  ::reset(Board &b) const
    { b(past_hash)[b(past_size)=1]=b.id_hash(); }


  Mate::Mate(Piece2DGameData &d,
             TrackPiece const &track_kings,
             PerSquareLegalMovesFilter const &per_square_legal_moves_filter)
    : d(d),
      track_kings(track_kings),
      per_square_legal_moves_filter(per_square_legal_moves_filter)
    { d.outcome_filters.append(f_funct(this, &this_t::outcome_filter)); }

  void Mate::outcome_filter(Rules::Outcome &o, Board const &b) const {
    if (o==Rules::Outcome::playing
        and not per_square_legal_moves_filter.is_there_any_legal_move(b)) {
      Color c=b(d.turn);
      Location king_l=b(track_kings.tracked_location)[c];
      o=per_square_legal_moves_filter.is_under_attack(b, king_l, c)
        ? Rules::Outcome::last_move_won
        : Rules::Outcome::draw;
    }
  }


  DrawIfRoyalsAlone::DrawIfRoyalsAlone(Piece2DGameData &d,
                                       series<Square> royal_squares)
    : d(d), royal_squares(royal_squares)
    { d.outcome_filters.append(f_funct(this, &this_t::outcome_filter)); }

  void DrawIfRoyalsAlone
  ::outcome_filter(Rules::Outcome &o, Board const &b) const {
    if (o==Rules::Outcome::playing) {
      for (auto s: d.piece_box.enumerated_occupied_squares)
        if (not is_in(s, royal_squares) and b(d.square_count.counts)[s])
          return;
      o=Rules::Outcome::draw;
    }
  }


  AvoidCheck
  ::AvoidCheck(Piece2DGameData &d,
               TrackPiece const &track_kings,
               PerSquareLegalMovesFilter &per_square_legal_moves_filter)
    : d(d),
      track_kings(track_kings),
      per_square_legal_moves_filter(per_square_legal_moves_filter) {
    per_square_legal_moves_filter.append_illegality(
      f_funct(this, &this_t::in_check));
  }

  bool AvoidCheck::in_check(Board const &b, Move m) const {
    Color attackee_color=b(d.turn);
    Board bm=b;
    d.board_move(bm, m);
    return per_square_legal_moves_filter.is_under_attack(
             bm,
             bm(track_kings.tracked_location)[attackee_color],
             attackee_color);
  }


  bool add_pawn_move(Piece2DGameData const &d, Board const &b,
                     add_legal_move_f const &add_move,
                     Move m, Location to,
                     list<Location> captures,
                     promotion_f promotion) {
    series<Square> promotion_squares;
    if (promotion and (promotion_squares=promotion(b, to)).size()) {
      for (auto p: promotion_squares)
        if (add_move(make_suffix(string(1, d.piece_of(p))) +m+make_set(to, p),
                     captures))
          return true;
      return false;
    }
    else
      return add_move(m, captures);
  }

  PawnSpecs pawn_specs(series<Displacement> advance_steps,
                       series<Displacement> capture_steps,
                       promotion_f promotion)
    { return make_tuple(advance_steps, capture_steps, promotion); }

  void move_pawn(Piece2DGameData const &d,
                 // "spec" is a tuple: a series of displacements is
                 // used for advancing; a second one is used for capturing; a
                 // function that determines promotion options based on ending
                 // location:
                 PawnSpecs const &pawn_specs,
                 add_legal_move_f const &add_move,
                 Board const &b, Location l,
                 Square l_s, Square change_to, // same: no change
                 bool only_capture, Location) {
    Color c=d.color_of(l_s);
    series<Displacement> const &
      non_capturing_steps=get<0>(pawn_specs),
      capturing_steps=get<1>(pawn_specs);
    promotion_f const &promotion=get<2>(pawn_specs);
    for (Displacement dis: capturing_steps) {
      Location to=l+dis;
      if (Square s;
          b(d.squares).get_if_contains(to, s)
          and d.is_occupied(s) and d.color_of(s) not_eq c) {
        if (add_pawn_move(d, b, add_move,
                          make_capture(to)+make_move(l, to, l_s, change_to),
                          to, {to},
                          promotion))
          return;
      }
    }
    if (not only_capture) // non-capturing can't attack "to_capture"
      for (Displacement dis: non_capturing_steps) {
        Location to=l+dis;
        if (Square s;
            b(d.squares).get_if_contains(to, s) and not d.is_occupied(s)) {
          if (add_pawn_move(d, b, add_move,
                            make_move(l, to, l_s, change_to), to, {},
                            promotion))
            return;
        }
        else
          break;
      }
  }


  EnPassant
  ::EnPassant(Piece2DGameData &d,
              PerSquareLegalMovesFilter &per_square_legal_moves_filter,
              MoveCommandHandler &move_command_handler,
              series<Square> jumping_squares,
              series<pair<Square, series<Displacement>>>
              capturing_squares_steps)
    : d(d),
      jumping_squares(jumping_squares),
      capturing_squares_steps(capturing_squares_steps),
      on(d.state_spec), started(d.state_spec), ended(d.state_spec) {
    move_command_handler.append(f_funct(this, &this_t::move_handler));
    d.initialization.append(f_funct(this, &this_t::reset));
    for (auto s_s: capturing_squares_steps) {
      auto capturing_steps=s_s.second;
      per_square_legal_moves_filter.append(
        s_s.first,
        [this, capturing_steps]
        (add_legal_move_f const &add_move, Board const &b, Location l, Square,
         bool, Location) {
          legal_moves_filter(add_move, b, l, capturing_steps);
        });
    }
  }

  void EnPassant::move_handler(Board &b, Location from, Location to) const {
    if (get_x(from)==get_x(to)
        and abs(get_y(from)-get_y(to))>1
        and is_in(b(d.squares)[from], jumping_squares)) {
      b(on)=true;
      b(started)=from;
      b(ended)=to;
    }
    else
      reset(b);
  }
  void EnPassant::legal_moves_filter(
       add_legal_move_f const &add_move,
       Board const &b,
       Location l,
       series<Displacement> const &capturing_steps) const {
    auto l_started=b(started), l_ended=b(ended);
    assert(get_x(l_started)==get_x(l_ended));
    auto x=get_x(l_started);
    auto y_started=get_y(l_started), y_ended=get_y(l_ended);
    for (Displacement dis: capturing_steps) {
      Location to=l+dis;
      if (get_x(to)==x
          and ((get_y(to)<y_started and get_y(to)>y_ended)
               or (get_y(to)>y_started and get_y(to)<y_ended))) {
        // FIXME: maybe add "change_to" to the "make_move()" command, for
        // generality
        if (add_move(make_capture(l_ended)+make_move(l, to), {l_ended}))
          return;
      }
    }
  }
  void EnPassant::reset(Board &b) const {
    b(on)=false;
    b(started)=b(ended)={};
  }


  KingCastling::KingCastling(
      Piece2DGameData &d,
      series<tuple<Square, Square, Square, Square>>
        initial_and_moved_king_and_initial_and_moved_rook)
    : d(d), per_square_legal_moves_filter(d.per_square) {
    Square i_king, m_king, i_rook, m_rook;
    for (auto i_k_m_k_i_r_m_k:
           initial_and_moved_king_and_initial_and_moved_rook) {
      tie(i_king, m_king, i_rook, m_rook)=i_k_m_k_i_r_m_k;
      d.per_square.append(
        i_king,
        [this, i_king, m_king, i_rook, m_rook]
        (add_legal_move_f const &add_move, Board const &b,
         Location l, Square, bool only_capture, Location) {
          legal_moves_filter(add_move, b, l, i_king, m_king, i_rook, m_rook,
                             only_capture);
        });
    }
  }

  void KingCastling::legal_moves_filter(
       add_legal_move_f const &add_move,
       Board const &b,
       Location l,
       Square i_king, Square m_king, Square i_rook, Square m_rook,
       bool only_capture) const {
    // no capturing by castling, since both ending squares must be empty:
    if (only_capture)
      return;
    Color c=d.color_of(i_king);
    s8 king_y=get_y(l);
    auto x_range=get_x(d.squares.range);
    auto
      left_final_king_x=x_range.first+2,
      right_final_king_x=x_range.second-1;
    if (not per_square_legal_moves_filter.is_under_attack(b, l, c))
      for (auto dis_final_xs: {make_pair(-1, left_final_king_x),
                               make_pair(+1, right_final_king_x)}) {
        s8 direction=dis_final_xs.first;
        // look for the rook:
        Displacement dr(direction, 0);
        Location lr=l+dr;
        Square sr=empty;
        while (b(d.squares).get_if_contains(lr, sr)
               and not d.is_occupied(sr))
          lr=lr+dr;
        if (sr not_eq i_rook)
          continue; // first non-empty isn't an unmoved rook

        // check there's nothing except the king and the rook in all the
        // involved locations
        s8
          final_king_x=dis_final_xs.second,
          final_rook_x=final_king_x-direction,
          range_x_min=
            min({final_king_x, final_rook_x, get_x(l), get_x(lr)}),
          range_x_max=
            max({final_king_x, final_rook_x, get_x(l), get_x(lr)});
        Square se;
        for (s8 x: range_from_to(range_x_min, range_x_max))
          if (d.is_occupied(se=b(d.squares)[{x, king_y}])
              and not is_among(se, i_king, i_rook))
            goto next_direction; // longue vie à goto!

        { // check king's path is secure
          Board b_castling=b;
          Location last_l=l;
          for (s8 x: range_between(get_x(l), final_king_x)) {
            Location new_l{x, king_y};
            if (last_l not_eq new_l)
              d.move_handler.handle(b_castling, make_move(last_l, new_l));
            if (per_square_legal_moves_filter
                  .is_under_attack(b_castling, {x, king_y}, c))
              goto next_direction; // longue vie à not' Sire!
            last_l=new_l;
          }
        }

        {
          string label_castling=direction>0 ? "O-O" : "O-O-O";
          // the move can't consist of two regular "make_move()"s, because
          // in chess960, you can have the final destination of the king be
          // the same as the original location of the rook; so, what you'd
          // do is you'd move the king (and capture your own rook), and
          // then move from the rook's original location, where the king
          // now is
          if (add_move(make_label(label_castling)
                        +make_set(lr, empty)
                        +(get_x(l)==final_king_x
                          ? Move()
                          : make_move(l, {final_king_x, king_y}))
                        +make_set({final_king_x, king_y}, m_king)
                        +make_set({final_rook_x, king_y}, m_rook),
                       {}))
              return;
        }

      next_direction:;
      }
  }

  /// evaluation
  namespace {

    score_t value(material_table_t const &material_table,
                  Piece p, Color p_c, Color eval_c)
      { return (p_c==eval_c ? +1. : -1.)*material_table.at(p); }

  }

  // no need to evaluate outcome here; that's the responsibility of the
  // search algorithm
  score_t just_material(Piece2DGameData const &d, Board const &b,
                        material_table_t const &material_table) {
    Color
      enemy_c=b(d.turn), // i've just played
      my_c=enemy(enemy_c);
    score_t score=0;
    for (auto s: d.piece_box.enumerated_occupied_squares) {
      score_t square_value=
        value(material_table, d.piece_of(s), d.color_of(s), my_c);
      score+=b(d.square_count.counts)[s]*square_value;
    }
    return score;
  }

  EvaluationJustMaterial
  ::EvaluationJustMaterial(EvaluationMap &evaluation_map,
                           Piece2DGameData const &d,
                           material_table_t const &material_table)
    : Evaluation(
        "just_material",
        R"(syntax: just_material

only the piece value (a constant number for each piece) is considered
    )",
        [&d, &material_table](params_t p) {
          if (not p.empty())
            throw invalid_argument(
              "wrong params for \"just_material\" (should have been empty)");
          return [&d, &material_table](Board const &b)
            { return just_material(d, b, material_table); };
        },
        evaluation_map) { }

  constexpr float squared(float x) { return x*x; }

  constexpr float distance(Location l, float x, float y)
    { return sqrt(squared(get_x(l)-x)+squared(get_y(l)-y)); }

  score_t material_and_position(Piece2DGameData const &d, Board const &b,
                                material_table_t const &material_table,
                                float factor) {
    auto const square_range=d.squares.range;
    float const mid_x=
      (get<0>(square_range).first+get<0>(square_range).second)/2.;
    float const mid_y=
      (get<1>(square_range).first+get<1>(square_range).second)/2.;
    float const
      w_prom_y=get<1>(square_range).second,
      b_prom_y=get<1>(square_range).first;
    Color my_c=enemy(b(d.turn)); // i've just played
    score_t score=just_material(d, b, material_table);
    for (auto l: d.squares.all_enumerated_coords) {
      Square s=b(d.squares)[l];
      Piece s_p=d.piece_of(s);
      Color s_c=d.color_of(s);
      if (s_p=='p')
        score-=
          factor*value(material_table, s_p, s_c, my_c)
          *distance(l, mid_x, s_c==Color::white ? w_prom_y : b_prom_y);
      else
        score-=
          factor*value(material_table, s_p, s_c, my_c)
          *distance(l, mid_x, mid_y);
    }
    return score;
  }

  EvaluationMaterialAndPosition
  ::EvaluationMaterialAndPosition(EvaluationMap &evaluation_map,
                                  Piece2DGameData const &d,
                                  material_table_t const &material_table)
    : Evaluation(
        "material_and_position",
        R"(syntax: material_and_position:[factor]

piece value plus factor times centeredness or advancedness
    )",
        [&d, &material_table](params_t p) {
          float factor=.04;
          if (not p.empty()) {
            if (not (p.size()==1 and p.front().value.empty()))
              throw invalid_argument(
                      "wrong params for \"material_and_position\" (should have "
                      "zero params, or one arg-only param)");
            factor=from_text<float>(p.front().arg);
          }
          return [factor, &d, &material_table](Board const &b)
            { return material_and_position(d, b, material_table, factor); };
        },
        evaluation_map) { }

}
