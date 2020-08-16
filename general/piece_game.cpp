#include "piece_game.h"
#include "display_svg.h"
#include <regex>

using namespace std;

namespace sxako::piece_2d_game {

  void MoveHandler::install(command_code_t code, CommandHandler<> const *ch) {
    if (command_handlers.find(code) not_eq command_handlers.end())
      throw std::runtime_error(
        std::string("there was already a handler for code '")+char(code)+"'");
    command_handlers[code]=ch;
  }

  PerSquareLegalMovesFilter::PerSquareLegalMovesFilter(Piece2DGameData &d)
    : d(d) {
    d.legal_moves_filters.append(
      f_funct(this, &this_t::all_squares_legal_moves_filter));
  }

  void PerSquareLegalMovesFilter
  ::append(Square s, per_square_add_legal_moves_f const &f)
    { square_legal_moves_filters[s].push_back(f); }

  void PerSquareLegalMovesFilter
  ::append_illegality(is_move_illegal_f const &f)
    { illegalities.push_back(f); }

  void PerSquareLegalMovesFilter
  ::loop_through_squares(Board const &b, Color c,
                         add_legal_move_f const &add_legal_move,
                         bool only_capture, Location to_capture,
                         std::function<bool ()> const &stop) const {
    for (auto l: (c==Color::white
                  ? d.squares.all_enumerated_coords
                  : d.squares.reverse_all_enumerated_coords)) {
      Square s=b(d.squares)[l];
      if (d.is_occupied(s) and d.color_of(s)==c)
        for (auto f: square_legal_moves_filters[s]) {
          f(add_legal_move, b, l, s, only_capture, to_capture);
          if (stop())
            return;
        }
    }
  }

  void PerSquareLegalMovesFilter
  ::all_squares_legal_moves_filter(Moves &moves, Board const &b) const {
    add_legal_move_f add_legal_move=
      [this, &moves, &b](Move m, list<Location> const &) {
        if (not is_move_illegal(b, m))
          moves.push_back(m);
        return false;
      };
    loop_through_squares(b, b(d.turn), add_legal_move, false, {});
  }

  bool PerSquareLegalMovesFilter
  ::is_under_attack(Board const &b,
                    Location attackee, Color attackee_color) const {
    bool under_attack=false;
    add_legal_move_f add_legal_move=
      [&under_attack, attackee](Move, list<Location> const &captures) {
        for (auto l: captures)
          if (attackee==l)
            return under_attack=true;
        return false;
      };
    loop_through_squares(b, enemy(attackee_color), add_legal_move,
                         true, attackee, f_var(&under_attack));
    return under_attack;
  }

  bool PerSquareLegalMovesFilter
  ::is_there_any_legal_move(Board const &b) const {
    bool any_legal_move=false;
    add_legal_move_f add_legal_move=
      [this, &any_legal_move, &b](Move m, list<Location> const &) {
        if (not is_move_illegal(b, m))
          return any_legal_move=true;
        return false;
      };
    loop_through_squares(b, b(d.turn), add_legal_move,
                         false, {}, f_var(&any_legal_move));
    return any_legal_move;
  }

  bool PerSquareLegalMovesFilter
  ::is_there_any_legal_capture(Board const &b) const {
    bool any_legal_capture=false;
    add_legal_move_f add_legal_move=
      [this, &any_legal_capture, &b](Move m, list<Location> const &captures) {
        if (not captures.empty() and not is_move_illegal(b, m))
          return any_legal_capture=true;
        return false;
      };
    loop_through_squares(b, b(d.turn), add_legal_move,
                         false, {}, f_var(&any_legal_capture));
    return any_legal_capture;
  }

  bool PerSquareLegalMovesFilter
  ::is_move_illegal(Board const &b, Move m) const {
    for (is_move_illegal_f f: illegalities)
      if (f(b, m))
        return true;
    return false;
  }


  TurnChangeHandler::TurnChangeHandler(Piece2DGameData &d) {
    d.post_push_front_turn_handler(
      [&d](Board &b) { b(d.turn)=enemy(b(d.turn)); });
  }

  LabelCommandHandler::LabelCommandHandler(Piece2DGameData &d)
    : CommandHandler(d.move_handler) { }

  SuffixCommandHandler::SuffixCommandHandler(Piece2DGameData &d)
    : CommandHandler(d.move_handler) { }

  MoveCommandHandler::MoveCommandHandler(Piece2DGameData &d)
    : CommandHandler(d.move_handler), d(d) { }
  void MoveCommandHandler
  ::handle_main(Board &b, Location from, Location to) const {
    Square
      &s_from=b(d.squares)[from],
      &s_to=b(d.squares)[to];
    s_to=s_from;
    s_from=empty;
  }

  CaptureCommandHandler::CaptureCommandHandler(Piece2DGameData &d)
    : CommandHandler(d.move_handler), d(d) { }
  void CaptureCommandHandler
  ::handle_main(Board &b, Location captured) const
    { b(d.squares)[captured]=empty; }

  SetCommandHandler::SetCommandHandler(Piece2DGameData &d)
    : CommandHandler(d.move_handler), d(d) { }
  void SetCommandHandler
  ::handle_main(Board &b, Location l, Square s) const {
    Square &s_l=b(d.squares)[l];
    s_l=s;
  }


  namespace {
    string brief(Location l)
      { return string(1, 'A'+(get_x(l)-1))+to_string(get_y(l)); }
  }

  string write_move(Board const &, Move const &m) { // FIXME: generalise
    ostringstream oss;
    string suffix="";
    MoveStream ms(m);
    string last_position="";

    while (ms) { // FIXME: the parsing of command arguments should be automatic
      switch (ms.get<u8>()) {
      case label_code:
        return ms.get<string>();
      case suffix_code:
        suffix=ms.get<string>();
        break;
      case move_code:
        {
          string from=brief(ms.get<Location>()), to=brief(ms.get<Location>());
          (from==last_position ? oss : oss << from) << to;
          last_position=to;
        } break;
      case capture_code:
        ms.get<Location>();
        break;
      case set_code:
        ms.get<Location>();
        ms.get<Square>();
        break;
      default:
        throw logic_error("not a move (wrong code)");
      }
    }
    return oss.str()+suffix;
  }


  SquareCount::SquareCount(Piece2DGameData &d,
                           pair<Square, Square> square_count_squares_range)
    : d(d),
      counts(d.cache_spec, square_count_squares_range) {
    d.set_cache_functions.append(f_funct(this, &this_t::set_cache));
    d.move_command_handler.append(f_funct(this, &this_t::move_handler));
    d.capture_command_handler.append(f_funct(this, &this_t::capture_handler));
    d.set_command_handler.append(f_funct(this, &this_t::set_handler));
  }

  SquareCount::SquareCount(Piece2DGameData &d)
    : SquareCount(d, d.squares_range) { }

  void SquareCount::set_cache(Data<Kind::cache> &cache,
                 Data<Kind::state> const &state) const {
    for (auto l: d.squares.all_enumerated_coords)
      ++cache(counts)[state(d.squares)[l]];
  }

  void SquareCount::move_handler(Board &b, Location from, Location to) const {
    ignore=from;
    Square s_to=b(d.squares)[to];
    --b(counts)[s_to];
    ++b(counts)[empty];
  }
  void SquareCount
  ::capture_handler(Board &b, Location captured) const {
    Square captured_s=b(d.squares)[captured];
    --b(counts)[captured_s];
    ++b(counts)[empty];
  }
  void SquareCount::set_handler(Board &b, Location l, Square new_s) const {
    Square s_l=b(d.squares)[l];
    --b(counts)[s_l];
    ++b(counts)[new_s];
  }


  void Piece2DGameData::board_move(Board &b, Move const &m) const {
    for (auto f: pre_turn_handlers)
      f(b);
    move_handler.handle(b, m);
    for (auto f: post_turn_handlers)
      f(b);
  }


  namespace {

    template <typename T>
    T sign(T t) { return t<0 ? -1 : t>0 ? +1 : 0; }

    bool simple_reachable(Displacement displacement, Displacement by) {
      coord_times_coord_t
        d_x=get<0>(displacement), d_y=get<1>(displacement),
        b_x=get<0>(by), b_y=get<1>(by);
      return
        (sign(d_x)==sign(b_x)) and (sign(d_y)==sign(b_y))
        and (d_x*b_y==d_y*b_x);
    }

  }

  void can_move_straight(Piece2DGameData const &d,
                         series<Displacement> const &steps,
                         add_legal_move_f const &add_move,
                         Board const &b, Location l,
                         Square l_s, Square change_to, // same: no change
                         bool only_capture, Location to_capture) {
    Color c=d.color_of(l_s);
    for (Displacement dis: steps) {
      if (only_capture and not simple_reachable(to_capture-l, dis))
        continue;
      Location to=l+dis;
      Square s=empty;
      while (b(d.squares).get_if_contains(to, s) and not d.is_occupied(s)) {
        if (add_move(make_move(l, to, l_s, change_to), {}))
          return;
        to=to+dis;
      }
      if (d.is_occupied(s) and d.color_of(s) not_eq c) {
        if (add_move(make_capture(to)+make_move(l, to, l_s, change_to), {to}))
          return;
      }
    }
  }

  void can_move_one_step(Piece2DGameData const &d,
                         series<Displacement> const &steps,
                         add_legal_move_f const &add_move,
                         Board const &b, Location l,
                         Square l_s, Square change_to, // same: no change
                         bool only_capture, Location to_capture) {
    Color c=d.color_of(l_s);
    for (Displacement dis: steps) {
      Location to=l+dis;
      if (only_capture and (to not_eq to_capture))
        continue;
      if (Square s; b(d.squares).get_if_contains(to, s)) {
        if (not d.is_occupied(s)) {
          if (add_move(make_move(l, to, l_s, change_to), {}))
            return;
        }
        else if (d.color_of(s) not_eq c) {
          if (add_move(make_capture(to)+make_move(l, to, l_s, change_to),
                       {to}))
            return;
        }
      }
    }
  }

  void can_move_one_step_no_capture(
      Piece2DGameData const &d,
      series<Displacement> const &steps,
      add_legal_move_f const &add_move,
      Board const &b, Location l,
      Square, Square change_to, // same: no change
      bool only_capture, Location) {
    if (only_capture)
      return;
    for (Displacement dis: steps) {
      Location to=l+dis;
      if (Square s;
          b(d.squares).get_if_contains(to, s) and not (d.is_occupied(s))) {
        if (add_move(make_move(l, to, s, change_to), {}))
          return;
      }
    }
  }

  void can_move_straight_and_screen_capture(
      Piece2DGameData const &d,
      series<Displacement> const &steps,
      add_legal_move_f const &add_move,
      Board const &b, Location l,
      Square l_s, Square change_to, // same: no change
      bool only_capture, Location to_capture) {
    Color c=d.color_of(l_s);
    for (Displacement dis: steps) {
      if (only_capture and not simple_reachable(to_capture-l, dis))
        continue;
      Location to=l+dis;
      Square s=empty;
      // non-capturing part of the move:
      while (b(d.squares).get_if_contains(to, s) and not d.is_occupied(s)) {
        if (add_move(make_move(l, to, l_s, change_to), {}))
          return;
        to=to+dis;
      }
      if (d.is_occupied(s)) { // capturing part of the move
        s=empty;
        to=to+dis;
        while (b(d.squares).get_if_contains(to, s) and not d.is_occupied(s))
          to=to+dis;
        if (d.is_occupied(s) and d.color_of(s) not_eq c) {
          if (add_move(make_capture(to)+make_move(l, to, l_s, change_to),
                       {to}))
            return;
        }
      }
    }
  }

  void can_move_straight_royal(
      Piece2DGameData const &d,
      series<Displacement> const &steps,
      add_legal_move_f const &add_move,
      Board const &b, Location l,
      Square l_s, Square change_to, // same: no change
      bool only_capture, Location to_capture) {
    Color c=d.color_of(l_s);
    for (Displacement dis: steps) {
      if (only_capture and not simple_reachable(to_capture-l, dis))
        continue;
      Board b_move=b;
      Location last_l=l, to=l+dis;
      Square s=empty;
      while (b(d.squares).get_if_contains(to, s) and not d.is_occupied(s)) {
        if (not only_capture) {
          d.move_handler.handle(b_move, make_move(last_l, to));
          if (d.per_square.is_under_attack(b_move, to, c))
            break; // FIXME: refactor with "can_move_straight()" if possible
        }
        if (add_move(make_move(l, to, l_s, change_to), {}))
          return;
        last_l=to;
        to=to+dis;
      }
      if (d.is_occupied(s) and d.color_of(s) not_eq c) {
        if (add_move(make_capture(to)+make_move(l, to, l_s, change_to), {to}))
          return;
      }
    }
  }

  TrackPiece::TrackPiece(Piece2DGameData &d,
                         MoveCommandHandler &move_command_handler,
                         Piece p)
    : d(d), p(p),
      tracked_location(d.cache_spec, color_range) {
    d.set_cache_functions.append(f_funct(this, &this_t::set_cache));
    move_command_handler.append(f_funct(this, &this_t::move_handler));
  }

  void TrackPiece::set_cache(Data<Kind::cache> &cache,
                             Data<Kind::state> const &state) const {
    for (Color c: enumerated_colors)
      cache(tracked_location)[c]=Location();
    for (auto l: d.squares.all_enumerated_coords)
      if (d.piece_of(state(d.squares)[l])==p)
        cache(tracked_location)[d.color_of(state(d.squares)[l])]=l;
  }

  void TrackPiece::move_handler(Board &b, Location from, Location to) const {
    Square s=b(d.squares)[from];
    if (d.piece_of(s)==p)
      b(tracked_location)[d.color_of(s)]=to;
  }


  LoseIfNoRoyal::LoseIfNoRoyal(Piece2DGameData &d,
                               series<Square> white_royal_squares,
                               series<Square> black_royal_squares)
    : d(d), square_count(d.square_count),
      white_royal_squares(white_royal_squares),
      black_royal_squares(black_royal_squares)
    { d.outcome_filters.append(f_funct(this, &this_t::outcome_filter)); }

  void LoseIfNoRoyal::outcome_filter(Rules::Outcome &o, Board const &b) const {
    if (o not_eq Rules::Outcome::playing)
      return;
    Color c=b(d.turn);
    for (auto s: (c==Color::white
                  ? white_royal_squares
                  : black_royal_squares))
      if (b(square_count.counts)[s])
        return;
    o=Rules::Outcome::last_move_won;
  }

  LoseIfNoLegalMove::LoseIfNoLegalMove(Piece2DGameData &d)
    : per_square(d.per_square)
    { d.outcome_filters.append(f_funct(this, &this_t::outcome_filter)); }

  void LoseIfNoLegalMove
  ::outcome_filter(Rules::Outcome &o, Board const &b) const {
    if (o not_eq Rules::Outcome::playing)
      return;
    if (not per_square.is_there_any_legal_move(b))
      o=Rules::Outcome::last_move_won;
  }

  ForceCaptureIfPossible::ForceCaptureIfPossible(Piece2DGameData &d)
    : move_handler(d.move_handler)
    { d.legal_moves_filters.append(f_funct(this, &this_t::moves_filter)); }

  void ForceCaptureIfPossible
  ::moves_filter(Moves &moves, Board const &) const {
    bool any_capture=false;
    for (auto const &m: moves) {
      for (auto code: move_handler.extract_codes(m))
        if (code==capture_code) {
          any_capture=true;
          goto end_scanning_moves;
        }
    }
  end_scanning_moves:
    if (any_capture)
      moves.remove_if(
        [this](Move const &m) {
          for (auto code: move_handler.extract_codes(m))
            if (code==capture_code)
              return false;
          return true;
        });
  }


  string display_board(display_style_map_t const &display_style_map,
                       Piece2DGameData const &d,
                       Board const &b,
                       LastMove last_move,
                       string style) {
    display_f_t display_f=display_ascii(); // default
    if (not style.empty()) {
      auto first_space=style.find(' ');
      display_f=display_style_map.at(style.substr(0, first_space));

      if (first_space==string::npos)
        style="";
      else
        style=style.substr(first_space+1);
    }

    return display_f(d, b, last_move, style);
  }

  display_f_t display_ascii() {
    return [](Piece2DGameData const &d,
              Board const &b,
              LastMove const &last_move,
              string style) {
      ignore=style;
      auto square_to_text=
        [&d](Square s) { return d.piece_box.shorthand(s); };
      return display_ascii_straight_2d(
               b(d.squares),
               square_to_text,
               [&d](Square s) { return d.is_occupied(s); },
               info_turn_last_move(to_text(b(d.turn)), last_move));
    };
  }

  display_f_t display_svg(
    std::function<std::string (Location, LocationRange)>
      location_to_empty_square) {
    return [location_to_empty_square](Piece2DGameData const &d,
                                      Board const &b,
                                      LastMove const &last_move,
                                      string style) {
      int const svg_square_side=10;
      ignore=style;
      auto square_to_svg=
      [&d](Square s) {
        string result=d.piece_box.square_name(s);
        // turn e.g. "w_i_pawn" into "white-pawn":
        for (auto rr: {make_pair(regex("^(.)_i_"), "$1_"),
                       make_pair(regex("^w_"), "white-"),
                       make_pair(regex("^b_"), "black-")})
          result=regex_replace(result, rr.first, rr.second);
        return result;
      };
      return display_svg_straight_2d(
               b(d.squares),
               svg_int_coords(svg_square_side),
               location_to_empty_square,
               square_to_svg,
               [&d](Square s) { return d.is_occupied(s); },
               {svg_square_side, svg_square_side},
               info_turn_last_move(to_text(b(d.turn)), last_move));
    };
  }

}

