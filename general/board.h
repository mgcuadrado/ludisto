#ifndef SXAKO_BOARD_HEADER_
#define SXAKO_BOARD_HEADER_

#include "grid.h"
#include <cassert>
#include <iosfwd>
#include <list>
#include <functional>

namespace sxako {

  class Move;
  class MoveStream;
  template <typename T> void move_push(Move &, T);
  template <typename T> void move_get(MoveStream &, T &);

  // a move is specified as a general string of "u8" values; these values can
  // code for finer (e.g. bit) or coarser (e.g., "u16") types; general types
  // can be inserted with the "<<" operator, which delegates to "move_push()",
  // which can be specialised for any type; "move_push()" must be matched by
  // specialisations for "move_get()" (see "MoveStream")
  //
  // a move is a specification of changes to the game situation; they can be
  // very detailed (e.g., en-passant can be described as a movement followed by
  // a disappearance); if they are more synthetic (e.g., a specific move type
  // for en-passant), the application algorithm must do more work to translate
  // them into specific game situation changes; usually, moves shouldn't
  // specify changes to the cache; these should be done by the move handler
  class Move {
  public:
    using char_t=u8;

    template <typename T>
    Move &operator<<(T t) { move_push(*this, t); return *this; }

    u8 size() const { return u8(data.size()); }
    auto as_string() const { return data; }
    u8 operator[](size_t i) const { assert(i<size()); return data[i]; }

    Move &operator+=(Move const &m) {
      data.append(m.data);
      return *this;
    }
  private:
    friend bool operator==(Move const &a, Move const &b)
      { return a.data==b.data; }
    friend void move_push(Move &, char_t);
    std::basic_string<char_t> data;
  };
  // concatenation of two moves
  inline Move operator+(Move a, Move const &b) { return a+=b; }

  // a move stream wraps a move to support by-element extraction with
  // "get<>()"; "get<>()" extracts any type, delegating to "move_get()", which
  // must be specialised for that type; "move_get()" must be matched by
  // specialisations for "move_push()" (see "Move")
  class MoveStream {
  public:
    MoveStream(Move const &m) : m(m) { }

    operator bool() const { return cursor<m.size(); } // not EOF
    template <typename T=Move::char_t>
    T get() { T result; move_get(*this, result); return result; }
  private:
    friend void move_get(MoveStream &, Move::char_t &);
    Move m;
    size_t cursor=0;
  };

  // specialisations for the basic char type in "Move":
  inline void move_push(Move &m, Move::char_t d)
    { m.data.push_back(d); }
  inline void move_get(MoveStream &ms, Move::char_t &d)
    { d=ms.m[ms.cursor++]; }

  // other specialisations:
  inline void move_push(Move &m, s8 d)
    { m << reinterpret_cast<u8 &>(d); }
  inline void move_get(MoveStream &ms, s8 &d)
    { reinterpret_cast<u8 &>(d)=ms.get(); }

  inline void move_push(Move &m, u16 d)
    { m << *reinterpret_cast<u8 *>(&d) << *(reinterpret_cast<u8 *>(&d)+1) ; }
  inline void move_get(MoveStream &ms, u16 &d) {
    *reinterpret_cast<u8 *>(&d)=ms.get();
    *(reinterpret_cast<u8 *>(&d)+1)=ms.get();
  }

  // strings are encoded null-terminated:
  inline void move_push(Move &m, std::string s) {
    for (auto c: s)
      m << reinterpret_cast<u8 &>(c);
    m << u8(0);
  }
  inline void move_get(MoveStream &ms, std::string &s) {
    s="";
    while (char c=ms.get())
      s.push_back(c);
  }

  // list of possible moves from the same game situation:
  using Moves=std::list<Move>;

  class Board;

  // meta-game info: last move (number and representation)
  struct LastMove {
    unsigned number=0;
    std::string move;
  };

  // conventional identification for the two players:
  enum class Color : u8 { white, black };
  std::ostream &operator<<(std::ostream &os, Color c);
  inline Color enemy(Color c) {
    assert(c==Color::white or c==Color::black);
    return c==Color::white ? Color::black : Color::white;
  }
  constexpr auto color_range=std::make_pair(Color::white, Color::black);
  series<Color> const enumerated_colors={Color::white, Color::black};

  // the following type gathers the specification for a game (e.g, chess, or
  // shogi):
  struct Rules {
    // specification of the data required to maintain a game situation (game
    // state and associated cache); the state must be an exact specification of
    // the current situation; it is used for its id when looking for the best
    // move; the cache must be completely regenerable from the state
    using DataSpec=DataStoreSpec<Kind::state, Kind::manag, Kind::cache>;
    DataSpec data_spec;

    // addressing for the turn
    Addressing<Kind::state, SingleVar, Color> const turn;

    // set initial position for the board (except cache)
    std::function<void (Board &)> const initialize;
    // compute the cache from the state; used to initialise or check cache
    std::function<Data<Kind::cache> (Data<Kind::state> const &)> const
      compute_cache;

    // current outcome ("playing" means the game isn't finished)
    enum class Outcome { playing, draw, last_move_won, last_move_lost };
    friend std::ostream &operator<<(std::ostream &os, Rules::Outcome o);
    std::function<Outcome (Board const &)> const outcome;

    // apply a move to a game situation; must maintain the cache
    std::function<void (Board &, Move const &)> const move;

    // list of _all_ legal moves from a game situation
    std::function<Moves (Board const &)> const legal_moves;

    // two-way conversion between move and string; depends on the current game
    // situation
    std::function<std::string (Board const &, Move const &)> write_move;
    std::function<Move (Board const &, std::string move_s)> parse_move;
    // display a situation; an empty "style" means pure ASCII
    std::function<std::string (Board const &, LastMove const &,
                               std::string style)>
      const display_board;
  };

  // "Board" keeps all the information needed by the game, including a cache to
  // speed up computations; meta-game info (move number, last move) is kept and
  // updated by "Game"
  class Board {
  public:
    Board(Rules::DataSpec const &data_spec) : data(data_spec) { }

    // to access the cell at location "l" in data grid "g" of board "b", use
    // "b(g)[l]"; for grids with empty coordinate types, the cell is accessed
    // by "b(g)"
    template <Kind kind, typename Indexing, typename Cell>
    decltype(auto) operator()(Addressing<kind, Indexing, Cell> const &a) const
      { return data.data<kind>()(a); }
    template <Kind kind, typename Indexing, typename Cell>
    decltype(auto) operator()(Addressing<kind, Indexing, Cell> const &a)
      { return data.data<kind>()(a); }

    // see syntactic sugar for "Data<>::operator()":
    template <typename Ptr>
    decltype(auto) operator()(Ptr const &p) const { return operator()(*p); }
    template <typename Ptr>
    decltype(auto) operator()(Ptr const &p) { return operator()(*p); }

    // id identifying the current state; good for keeping track of evaluated
    // situations; since it's a string, it's readily hashable
    std::string id() const { return data.data<Kind::state>().id(); }
    auto id_hash() const { return hasher(id()); }

    // reset cache to reflect current state, using the provided function; used
    // by "Game" to initialise
    template <typename F>
    void reset_cache(F const &f)
      { data.data<Kind::cache>()=f(data.data<Kind::state>()); }
    // compare cache to what it should be according to current state, using the
    // provided function; used by "Game" to assert coherence
    template <typename F>
    bool check_cache(F const &f) const
      { return data.data<Kind::cache>()==f(data.data<Kind::state>()); }
  private:
    static std::hash<std::string> hasher;
    DataStore<Rules::DataSpec> data;
  };

  // "Game" keeps a set of rules, a matching "Board" game situation, and
  // meta-game data such as last move; it exposes some of the "Rules"
  // functionality, for its own kept board or for a different board we may be
  // thinking of (e.g., when exploring moves)
  class Game {
  public:
    // the "Board" is initialised as per the rules
    Game(Rules const &rules)
      : rules(rules), situation(rules.data_spec) {
      rules.initialize(situation.board);
      situation.board.reset_cache(rules.compute_cache);
    }

    Rules const rules;

    LastMove last_move() const { return situation.last_move; }

    // const access to current board
    Board const &board() const { return situation.board; }

    // turn (who's to play)
    Color turn() const { return board()(rules.turn); }

    // outcome computation
    Rules::Outcome outcome(Board const &b) const { return rules.outcome(b); }
    Rules::Outcome outcome() const { return outcome(board()); }

    // apply move to game situation:
    void move(Board &b, Move const &m) const {
      rules.move(b, m);
      assert(situation.board.check_cache(rules.compute_cache));
    }
    void move(Move const &m) {
      history.push_back(situation);
      ++situation.last_move.number;
      situation.last_move.move=rules.write_move(situation.board, m);
      move(situation.board, m);
    }

    // undo last move of the game (not available for game-less board)
    void undo_last_move() {
      if (not history.empty()) {
        situation=history.back();
        history.pop_back();
      }
    }

    // list of legal moves from game situation:
    Moves legal_moves(Board const &b) const { return rules.legal_moves(b); }
    Moves legal_moves() const { return legal_moves(board()); }

    // two-way conversion between move and string
    std::string write_move(Board const &b, Move const &m) const
      { return rules.write_move(b, m); }
    Move parse_move(Board const &b, std::string move_s) const
      { return rules.parse_move(b, move_s); }
    std::string write_move(Move const &m) const
      { return write_move(board(), m); }
    Move parse_move(std::string move_s) const
      { return parse_move(board(), move_s); }

    // display situation
    std::string display_board(Board const &b, LastMove const &lm,
                              std::string style="") const
      { return rules.display_board(b, lm, style); }
    std::string display_board(Board const &b, std::string style="") const
      { return display_board(b, LastMove(), style); }
    std::string display_board(std::string style="") const
      { return display_board(board(), last_move(), style); }
  private:
    struct Situation {
      Situation(Rules::DataSpec const &data_spec) : board(data_spec) { }
      Board board;
      LastMove last_move;
    } situation;
    std::list<Situation> history;
  };

  // default "parse_move()" implementation: return the only legal move that
  // starts with the supplied move-as-a-string; fail if there are zero or
  // several matches
  decltype(std::declval<Rules>().parse_move)
  parse_move_default(
    decltype(std::declval<Rules>().legal_moves) const &legal_moves,
    decltype(std::declval<Rules>().write_move) const &write_move);

  // display a list of moves
  std::string display_moves(Game const &g, Moves const &moves);

}

namespace std {

  // make "Move" hashable
  template <>
  struct hash<sxako::Move> {
    size_t operator()(sxako::Move const &m) const {
      auto data=m.as_string();
      std::string data_s(reinterpret_cast<char const *>(data.c_str()),
                         data.size());
      return std::hash<std::string>()(data_s);
    }
  };

}

#endif
