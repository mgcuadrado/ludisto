#include "board.h"
#include <iostream>
#include <sstream>
#include <map>

namespace sxako {

  using namespace std;

  ostream &operator<<(ostream &os, Color c) {
    assert(c==Color::white or c==Color::black);
    return os << (c==Color::white ? "white" : "black");
  }

  ostream &operator<<(ostream &os, Rules::Outcome o) {
#define case(i) {Rules::Outcome::i, #i}
    static map<Rules::Outcome, string>
      rep{case(playing),
          case(draw),
          case(last_move_won),
          case(last_move_lost)};
    return os << rep.at(o);
#undef case
  }

  std::hash<std::string> Board::hasher;

  namespace {

    template <typename LM, typename WR>
    Move parse_move_default(LM const &legal_moves, WR const &write_move,
                            Board const &b, string const &move_s) {
      bool found_move=false;
      Move result;
      for (auto lm: legal_moves(b)) {
        if (move_s==write_move(b, lm))
          return lm;
        if (move_s==write_move(b, lm).substr(0, move_s.size())) {
          if (found_move)
            throw invalid_argument(
                    "several candidates for move \""+move_s+"\"");
          result=lm;
          found_move=true;
        }
      }
      if (found_move)
        return result;
      else
        throw invalid_argument(
                "no matching candidate for move \""+move_s+"\"");
    }

  }

  decltype(declval<Rules>().parse_move)
  parse_move_default(
      decltype(std::declval<Rules>().legal_moves) const &legal_moves,
      decltype(std::declval<Rules>().write_move) const &write_move) {
    return [legal_moves, write_move](Board const &b, string const &s)
             { return parse_move_default(legal_moves, write_move, b, s); };
  }

  string display_moves(Game const &g, Moves const &moves) {
    ostringstream oss;
    list<string> moves_string;
    for (auto m: moves)
      moves_string.push_back(g.write_move(m));
    moves_string.sort();
    oss << "{";
    for (auto m: moves_string)
      oss << " " << m;
    oss << " }";
    return oss.str();
  }

}
