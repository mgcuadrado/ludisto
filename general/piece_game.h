#ifndef SXAKO_GENERAL_PIECE_GAME_HEADER_
#define SXAKO_GENERAL_PIECE_GAME_HEADER_

#include "piece_box.h"
#include "board.h"
#include "straight.h"
#include <map>

namespace sxako::piece_2d_game {

  using coord_t=s8; // not huge board
  using coord_times_coord_t=s16; // holds the product of any two coords
  using Location=std::tuple<coord_t, coord_t>;
  using Displacement=delta_t<Location>;
  using LocationRange=coord_to_coord_range_t<Location>;

}

namespace sxako {

  inline void move_push(Move &m, piece_2d_game::Location l)
    { m << get_x(l) << get_y(l); }
  inline void move_get(MoveStream &ms, piece_2d_game::Location &l) {
    auto
      x=ms.get<piece_2d_game::coord_t>(),
      y=ms.get<piece_2d_game::coord_t>();
    l={x, y};
  }

}

namespace sxako::piece_2d_game {

  // commands are encoded in a "Move" (a "Move" is a sequence of commands); a
  // command consists of a command code (type "command_code_t") followed by its
  // arguments, if any; "MoveHandler" reads a "Move" and dispatches to
  // "CommandHandler"s based on the command codes
  using command_code_t=u8;

  template <command_code_t code=0, typename command_filter_f=void>
  class CommandHandler;

  template <>
  class CommandHandler<> {
  public:
    virtual ~CommandHandler()=default;
    virtual void handle(Board &b, MoveStream &ms) const=0;
    virtual void consume_args(MoveStream &ms) const=0;
  };

  class MoveHandler;

  // the template arguments of a command handler are the command code, and then
  // all its arguments; the packing ("write()") and unpacking ("handle()",
  // which relays them to the appended handlers and to "handle_main()") of the
  // arguments is done automatically by "CommandHandler<>"
  template <command_code_t code_, typename... A>
  class CommandHandler<code_, std::function<void (Board &, A...)>>
    : public CommandHandler<>,
      private Sequence<std::function<void (Board &, A...)>> {
  public:
    static command_code_t const code=code_;
    // the constructor installs the command handler into the move handler
    CommandHandler(MoveHandler &move_handler);
    using Sequence<std::function<void (Board &, A...)>>::append;
    // pack the arguments
    static Move write(A... a) {
      Move m;
      m << code;
      write_arguments(m, a...);
      return m;
    }
    // unpack the arguments from the "MoveStream", and call first the appended
    // handlers, then the main handler
    void handle(Board &b, MoveStream &ms) const override {
      std::tuple<A...> a;
      read_tuple(ms, a, std::index_sequence_for<A...>());
      for (auto f: this->elements)
        call(f, b, a, std::index_sequence_for<A...>());
      call_handle_main(b, a, std::index_sequence_for<A...>());
    }
    // unpack the arguments from the "MoveStream" but do nothing with them;
    // this is used by "MoveHandler::extract_codes()"
    void consume_args(MoveStream &ms) const override {
      std::tuple<A...> a;
      read_tuple(ms, a, std::index_sequence_for<A...>());
    }
    // "handle_main()" is the main effect of the command on the state; it's
    // called last; before it, other handlers are called in sequence, so they
    // can react to the command and its arguments
    virtual void handle_main(Board &, A...) const { }
  private:
    template <typename F, typename... R>
    static void write_arguments(Move &m, F f, R... r) {
      m << f;
      write_arguments(m, r...);
    }
    static void write_arguments(Move &) { }

    template <typename T, size_t... i>
    static void read_tuple(MoveStream &ms, T &t, std::index_sequence<i...>)
      { read_arguments(ms, get<i>(t)...); }
    template <typename F, typename... R>
    static void read_arguments(MoveStream &ms, F &f, R &...r) {
      f=ms.get<F>();
      read_arguments(ms, r...);
    }
    static void read_arguments(MoveStream &) { }

    template <typename F, typename T, size_t... i>
    static void call(F const &f, Board &b,
                     T const &t, std::index_sequence<i...>)
      { f(b, get<i>(t)...); }
    template <typename T, size_t... i>
    void call_handle_main(Board &b, T const &t,
                          std::index_sequence<i...>) const
      { handle_main(b, get<i>(t)...); }
  };

  // the move handler reads commands from a move and dispatches them to the
  // appended command handlers, in the form of a "MoveStream" (each command
  // handler takes from the stream the arguments it needs, leaving the stream
  // cursor at the next command); it can also extract the command codes from a
  // move (which is useful if you want to check if a particular move contains a
  // particular command; another approach to get the same result would be to
  // set up a specific move handler with command handlers for the same commands
  // as the original one, that instead of handling the moves, record
  // information about them)
  class MoveHandler {
  public:
    void handle(Board &b, Move const &m) const {
      MoveStream ms(m);
      while (ms)
        command_handlers.at(ms.get<command_code_t>())->handle(b, ms);
    }
    series<command_code_t> extract_codes(Move const &m) const {
      series<command_code_t> result;
      MoveStream ms(m);
      while (ms) {
        auto command_code=ms.get<command_code_t>();
        result.push_back(command_code);
        command_handlers.at(command_code)->consume_args(ms);
      }
      return result;
    }
  private:
    template <command_code_t c, typename command_filter_f>
      friend class CommandHandler;
    // install a command handler; called from the command handler constructor
    void install(command_code_t code, CommandHandler<> const *ch);
    std::map<command_code_t, CommandHandler<> const *> command_handlers;
  };

  template <command_code_t code, typename... A>
  CommandHandler<code, std::function<void (Board &, A...)>>
  ::CommandHandler(MoveHandler &move_handler)
    { move_handler.install(code, this);  }


  using namespace piece_box;

  struct Piece2DGameData;

  // the legal moves filters are a sequence of filters that modify a list of
  // legal moves starting from the current board status; each filter can access
  // the whole list of legal moves, adding to it or removing from it
  using legal_moves_filter_f=
    std::function<void (Moves &, Board const &)>;
  class LegalMovesFilters
    : private Sequence<legal_moves_filter_f> {
  public:
    using Sequence<legal_moves_filter_f>::append;
    Moves legal_moves(Board const &b) const {
      Moves moves;
      for (auto f: elements)
        f(moves, b);
      return moves;
    }
  };

  // a special legal moves filter is the per-square legal moves filter; it
  // scans the board (the physical board, i.e., the "squares" addressing of the
  // data), and for each square value (e.g., white rook) generates the set of
  // moves that the square generates, and adds it to the list of legal moves;
  // move generators for a given square value are added with the "append()"
  // method; each move generator gets a function to add each move (type
  // "add_legal_move_f"), the current board situation, the starting location
  // and square value (a generator may handle several square values), and the
  // additional values "only_capture" (usually set to "false") and "to_capture"
  // (see below in the description of "is_under_attack()"); when adding a move,
  // the generator must pass the "add_legal_move_f" function the move itself,
  // plus an additional value "captures", which must be a (possibly empty) list
  // of locations that are captured by the move; the boolean value returned by
  // the "add_legal_move_f" function states whether the move generation can be
  // stoped now (the generator can return immediately if the "add_legal_move_f"
  // function returns "true")
  //
  // all reported moves are subject to illegality checks; the illegality checks
  // are added with the "append_illegality()" method; an illegality check is a
  // function that gets a board and a move, and returns "true" if the move is
  // illegal in that situation; the prototypical usage of illegality checks is
  // checking for chess checks (a move that leaves your king in check is
  // illegal)
  //
  // the per-square legal moves filter has two special capabilities, often used
  // in board games:
  //
  //     * determining whether a given location of a given colour is under
  //       attack; this is done with the "is_under_attack()" method; the board
  //       must be set up in the exact hypothetical configuration (for
  //       instance, if we want to determine wethen a castling is valid, we
  //       have to check whether the intermediate location for the king is
  //       safe; this must be done by copying the current board, and actually
  //       moving the king there, in case a generator relies on the location
  //       being occupied by the king); the method scans the board in the same
  //       way as for the regular "add legal moves" task, but passing a "true"
  //       value for "only_capture", and the location asked about as the value
  //       for "to_capture"; moves returned by the generators are not subject
  //       to the illegality checks, which is the usually required behaviour
  //       (in chess, your king is in chess by a rook, even if the actual
  //       capturing of your king by that rook would leave the other king under
  //       attack); as soon as a generator reports any move that would capture
  //       a piece at the location (as per the reported "captures" argument),
  //       the scan is finished, and a value of "true" is returned; if no such
  //       move is reported, the returned value is "false"; generators are not
  //       required to even look at "only_capture" and "to_capture", but they
  //       can look at them to quickly return if they're guaranteed not to be
  //       able to capture (e.g., a non-capturing move generator can return
  //       immediately if "only_capture" is "true"; a generator for rook-like
  //       straight moves can return immediately if "only_capture" is "true"
  //       and "to_capture" is in a different row and column from "from")
  //
  //     * determining if there is any valid legal move; this is done with the
  //       "is_there_any_legal_move()" method; the method scans the board in
  //       the same way as for the regular "add legal_moves" task, including
  //       the check for move illegality; as soon as a legal move is reported,
  //       the method returns "true" immediately; if no legal move is reported,
  //       the returned value is "false"
  //
  //     * determining if there is any valid legal capture move; this is done
  //       with the "is_there_any_legal_capture()" method, and works similarly
  //       to "is_there_any_legal_move()"
  using add_legal_move_f= // returns true if no more moves are needed
    std::function<bool (Move, std::list<Location> const &captures)>;
  using per_square_add_legal_moves_f=
    std::function<void (add_legal_move_f const &, Board const &,
                        Location from, Square from_s,
                        bool only_capture, Location to_capture)>;
  using is_move_illegal_f=
    std::function<bool (Board const &, Move)>;
  class PerSquareLegalMovesFilter { using this_t=PerSquareLegalMovesFilter;
  public:
    PerSquareLegalMovesFilter(Piece2DGameData &d);
    void append(Square s, per_square_add_legal_moves_f const &f);
    void append_illegality(is_move_illegal_f const &f);
    void all_squares_legal_moves_filter(Moves &moves, Board const &b) const;
    bool is_under_attack(Board const &b,
                         Location attackee, Color attackee_color) const;
    bool is_there_any_legal_move(Board const &b) const;
    bool is_there_any_legal_capture(Board const &b) const;
    Piece2DGameData const &d;
  private:
    bool is_move_illegal(Board const &b, Move m) const;
    std::array<std::list<per_square_add_legal_moves_f>, n_squares>
      square_legal_moves_filters;
    std::list<is_move_illegal_f> illegalities;
    void loop_through_squares(
      Board const &b, Color c,
      add_legal_move_f const &add_legal_move,
      bool only_capture, Location to_capture,
      std::function<bool ()> const &stop=f_const(false)) const;
  };

  class TurnChangeHandler {
  public:
    TurnChangeHandler(Piece2DGameData &d);
  };

  using set_cache_f=
    std::function<void (Data<Kind::cache> &cache,
                        Data<Kind::state> const &state)>;
  class SetCacheFunctions
    : private Sequence<set_cache_f> {
  public:
    using Sequence<set_cache_f>::append;
    Data<Kind::cache> compute_cache(DataSpec<Kind::cache> cache_spec,
                                    Data<Kind::state> const &state) const {
      Data<Kind::cache> cache(cache_spec);
      for (auto f: elements)
        f(cache, state);
      return cache;
    }
  };

  using outcome_filter_f=
    std::function<void (Rules::Outcome &, Board const &)>;
  class OutcomeFilters
    : private Sequence<outcome_filter_f> {
  public:
    using Sequence<outcome_filter_f>::append;
    Rules::Outcome outcome(Board const &b) const {
      Rules::Outcome outcome=Rules::Outcome::playing;
      for (auto f: elements)
        f(outcome, b);
      return outcome;
    }
  };

  using initialization_f=
    std::function<void (Board &)>;
  class Initialization
    : private Sequence<initialization_f> {
  public:
    using Sequence<initialization_f>::append;
    void initialize(Board &b) {
      for (auto f: elements)
        f(b);
    }
  };

  using label_command_handler_f=
    std::function<void (Board &b, std::string label)>;
  struct LabelCommandHandler
    : public CommandHandler<'"', label_command_handler_f> {
    LabelCommandHandler(Piece2DGameData &d);
    void handle_main(Board &, std::string) const override { }
  };

  using suffix_command_handler_f=
    std::function<void (Board &b, std::string label)>;
  struct SuffixCommandHandler
    : public CommandHandler<'%', suffix_command_handler_f> {
    SuffixCommandHandler(Piece2DGameData &d);
    void handle_main(Board &, std::string) const override { }
  };

  using move_command_handler_f=
    std::function<void (Board &b, Location from, Location to)>;
  struct MoveCommandHandler
    : public CommandHandler<'m', move_command_handler_f> {
    MoveCommandHandler(Piece2DGameData &d);
    Piece2DGameData &d;
    void handle_main(Board &b, Location from, Location to) const override;
  };

  using capture_command_handler_f=
    std::function<void (Board &b, Location captured)>;
  struct CaptureCommandHandler
    : public CommandHandler<'c', capture_command_handler_f> {
    CaptureCommandHandler(Piece2DGameData &d);
    Piece2DGameData &d;
    void handle_main(Board &b, Location captured) const override;
  };

  using set_command_handler_f=
    std::function<void (Board &b, Location l, Square s)>;
  struct SetCommandHandler
    : public CommandHandler<'s', set_command_handler_f> {
    SetCommandHandler(Piece2DGameData &d);
    Piece2DGameData &d;
    void handle_main(Board &b, Location l, Square s) const override;
  };

  constexpr auto
    label_code=LabelCommandHandler::code,
    suffix_code=SuffixCommandHandler::code,
    move_code=MoveCommandHandler::code,
    capture_code=CaptureCommandHandler::code,
    set_code=SetCommandHandler::code;

  inline Move make_label(std::string s)
    { return LabelCommandHandler::write(s); }
  inline Move make_suffix(std::string s)
    { return SuffixCommandHandler::write(s); }
  inline Move make_move(Location from, Location to)
    { return MoveCommandHandler::write(from, to); }
  inline Move make_capture(Location captured)
    { return CaptureCommandHandler::write(captured); }
  inline Move make_set(Location l, Square s)
    { return SetCommandHandler::write(l, s); }

  inline Move make_move(Location from, Location to,
                        Square s, Square change_to) {
    return
      s==change_to
      ? make_move(from, to)
      : make_move(from, to)+make_set(to, change_to);
  }

  // human move representation:
  std::string write_move(Board const &b, Move const &m);

  struct SquareCount { using this_t=SquareCount;
    SquareCount(Piece2DGameData &d,
                std::pair<Square, Square> square_count_squares_range);
    SquareCount(Piece2DGameData &d);
    Piece2DGameData const &d;
    Addressing<Kind::cache, Straight<Square>, u8> const &operator()() const { return counts; }
    Addressing<Kind::cache, Straight<Square>, u8> const counts;
  private:
    void set_cache(Data<Kind::cache> &cache,
                   Data<Kind::state> const &state) const;
    void move_handler(Board &b, Location from, Location to) const;
    void capture_handler(Board &b, Location captured) const;
    void set_handler(Board &b, Location l, Square new_s) const;
  };

  struct Piece2DGameData {
    Piece2DGameData(Piece2DGameData const &)=delete;
    template <typename... SquaresA>
    Piece2DGameData(PieceBox const &piece_box_,
                    SquaresA &&...squares_a)
      : piece_box(piece_box_),
        squares(state_spec, squares_a...),
        square_count(*this, piece_box.square_range) {
      assign_data(table(piece), piece_box.list_square_piece());
      assign_data(table(color), piece_box.list_square_color());
      assign_data(table(occupied), piece_box.list_square_occupied());
    }

    PieceBox const &piece_box;

    DataSpec<Kind::state> state_spec;
    DataSpec<Kind::manag> manag_spec;
    DataSpec<Kind::cache> cache_spec;
    DataSpec<Kind::table> table_spec;

    // basic state
    Addressing<Kind::state, SingleVar, Color> const turn{state_spec};
    Addressing<Kind::state, Straight2D<>, Square> const squares;

    // conversion tables
    std::pair<Square, Square> const squares_range{0, n_squares-1};
    Addressing<Kind::table, Straight<u8>, Piece> const
      piece{table_spec, squares_range}; // square to piece
    Addressing<Kind::table, Straight<u8>, Color> const
      color{table_spec, squares_range}; // square to color
    Addressing<Kind::table, Straight<u8>, bool> const
      occupied{table_spec, squares_range}; // occupied square
    Data<Kind::table> table{table_spec};

    Piece piece_of(Square s) const { return table(piece)[s]; }
    Color color_of(Square s) const { return table(color)[s]; }
    bool is_occupied(Square s) const { return table(occupied)[s]; }

    LegalMovesFilters legal_moves_filters;
    Moves legal_moves(Board const &b) const
      { return legal_moves_filters.legal_moves(b); }
    PerSquareLegalMovesFilter per_square{*this};

    SetCacheFunctions set_cache_functions;
    Data<Kind::cache> compute_cache(Data<Kind::state> const &state) const
      { return set_cache_functions.compute_cache(cache_spec, state); }

    OutcomeFilters outcome_filters;
    Rules::Outcome outcome(Board const &b) const
      { return outcome_filters.outcome(b); }

    Initialization initialization;
    void initialize(Board &b)
      { initialization.initialize(b); }

    using turn_handler_f=
      std::function<void (Board &)>;
  private:
    std::list<turn_handler_f> pre_turn_handlers, post_turn_handlers;
  public:
    void pre_push_back_turn_handler(turn_handler_f f)
      { pre_turn_handlers.push_back(f); }
    void post_push_front_turn_handler(turn_handler_f f)
      { post_turn_handlers.push_front(f); }

    TurnChangeHandler turn_change_handler{*this};

    // move handler
    MoveHandler move_handler;
    // command handlers
    LabelCommandHandler label_command_handler{*this};
    SuffixCommandHandler suffix_command_handler{*this};
    MoveCommandHandler move_command_handler{*this};
    CaptureCommandHandler capture_command_handler{*this};
    SetCommandHandler set_command_handler{*this};
    SquareCount square_count;

    void board_move(Board &b, Move const &m) const;
  };

  struct SimpleMoves {
    template <typename G, typename S>
    SimpleMoves(PerSquareLegalMovesFilter &per_square_legal_moves_filter,
                series<std::pair<Square, Square>> sps,
                G generator,
                S steps) {
      Piece2DGameData const &d=per_square_legal_moves_filter.d;
      for (auto sp: sps) {
        Square s=sp.first, change_to=sp.second;
        per_square_legal_moves_filter.append(
          s,
          [generator, &d, steps, change_to]
          (add_legal_move_f const &add_move, Board const &b,
           Location l, Square l_s, bool only_capture, Location to_capture) {
            generator(d, steps,
                      add_move, b,
                      l, l_s, change_to, only_capture, to_capture);
          });
      }
    }
    template <typename... A>
    SimpleMoves(PerSquareLegalMovesFilter &per_square_legal_moves_filter,
                series<Square> ss,
                A ...a)
      : SimpleMoves(per_square_legal_moves_filter,
                    to_pair_series(ss),
                    a...) { }
    template <typename... A>
    SimpleMoves(PerSquareLegalMovesFilter &per_square_legal_moves_filter,
                Square s,
                A ...a)
      : SimpleMoves(per_square_legal_moves_filter,
                    series<Square>{s},
                    a...) { }
  private:
    static auto to_pair_series(series<Square> ss) {
      series<std::pair<Square, Square>> result;
      for (Square s: ss)
        result.push_back(std::make_pair(s, s));
      return result;
    }
  };

  // can move like a rook:
  void can_move_straight(Piece2DGameData const &d,
                         series<Displacement> const &steps,
                         add_legal_move_f const &add_move,
                         Board const &b, Location l,
                         Square l_s, Square change_to, // same: no change
                         bool only_capture, Location to_capture);

  // can move like a knight:
  void can_move_one_step(Piece2DGameData const &d,
                         series<Displacement> const &steps,
                         add_legal_move_f const &add_move,
                         Board const &b, Location l,
                         Square l_s, Square change_to, // same: no change
                         bool only_capture, Location to_capture);

  void can_move_one_step_no_capture(
    Piece2DGameData const &d,
    series<Displacement> const &steps,
    add_legal_move_f const &add_move,
    Board const &b, Location l,
    Square l_s, Square change_to, // same: no change
    bool only_capture, Location to_capture);

  // can move like a xiangqi cannon:
  void can_move_straight_and_screen_capture(
    Piece2DGameData const &d,
    series<Displacement> const &steps,
    add_legal_move_f const &add_move,
    Board const &b, Location l,
    Square l_s, Square change_to, // same: no change
    bool only_capture, Location to_capture);

  // can move like a royal queen (like in Caïssa Britannia):
  void can_move_straight_royal(
    Piece2DGameData const &d,
    series<Displacement> const &steps,
    add_legal_move_f const &add_move,
    Board const &b, Location l,
    Square l_s, Square change_to, // same: no change
    bool only_capture, Location to_capture);

  struct TrackPiece { using this_t=TrackPiece;
    TrackPiece(Piece2DGameData &d,
               MoveCommandHandler &move_command_handler,
               Piece p);
    Piece2DGameData const &d;
    Piece const p;
    Addressing<Kind::cache, Straight<Color>, Location> const tracked_location;
  private:
    void set_cache(Data<Kind::cache> &cache,
                   Data<Kind::state> const &state) const;
    void move_handler(Board &b, Location from, Location to) const;
  };

  // a base class to group moves, that already includes or abbreviates some of
  // the most usual features
  struct Move_base {
    Move_base(Piece2DGameData &d)
      : d(d), per_square(d.per_square), pb(d.piece_box) { }
    Piece2DGameData &d;
    PerSquareLegalMovesFilter &per_square;
    PieceBox const &pb;
    static constexpr auto
      straight=can_move_straight,
      one_step=can_move_one_step,
      straight_capture_over=can_move_straight_and_screen_capture,
      straight_royal=can_move_straight_royal;
  };


  struct LoseIfNoRoyal { using this_t=LoseIfNoRoyal;
    LoseIfNoRoyal(Piece2DGameData &d,
                  series<Square> white_royal_squares,
                  series<Square> black_royal_squares);
    Piece2DGameData const &d;
    SquareCount const &square_count;
    series<Square> const white_royal_squares, black_royal_squares;
  private:
    void outcome_filter(Rules::Outcome &o, Board const &b) const;
  };

  struct LoseIfNoLegalMove { using this_t=LoseIfNoLegalMove;
    LoseIfNoLegalMove(Piece2DGameData &d);
    // we may have to ask the move handler rather than the per-square move
    // generator:
    PerSquareLegalMovesFilter const &per_square;
  private:
    void outcome_filter(Rules::Outcome &o, Board const &b) const;
  };

  struct ForceCaptureIfPossible { using this_t=ForceCaptureIfPossible;
  public:
    ForceCaptureIfPossible(Piece2DGameData &d);
    MoveHandler const &move_handler;
  private:
    void moves_filter(Moves &moves, Board const &b) const;
  };

  /// display

  using display_f_t=
    std::function<std::string (Piece2DGameData const &d,
                               Board const &b,
                               LastMove const &last_move,
                               std::string style)>;

  using display_style_map_t=std::map<std::string, display_f_t>;
  std::string display_board(display_style_map_t const &display_style_map,
                            Piece2DGameData const &d,
                            Board const &b,
                            LastMove last_move,
                            std::string style);

  display_f_t display_ascii();
  display_f_t display_svg(
    std::function<std::string (Location, LocationRange)>
      location_to_empty_square);

}

#endif
