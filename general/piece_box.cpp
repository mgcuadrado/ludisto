#include "piece_box.h"

using namespace std;

namespace sxako::piece_box {

  Square PieceBox::first_square_from_shorthand(std::string shorthand) const {
    for (Square s=0; s<pieces.size(); ++s)
      if (pieces[s].shorthand==shorthand)
        return s;
    throw runtime_error("no square with shorthand \""+shorthand+"\"");
  }

  map<char, Square>
  PieceBox::glossary_shorthand_char_to_first_square() const {
    map<char, Square> result;
    for (auto s_p: list_square_shorthand()) {
      string shorthand=s_p.second;
      if (shorthand.size() not_eq 1)
        throw runtime_error(
          "shorthands for the glossary must be exactly one letter");
      char shorthand_char=shorthand[0];
      if (result.find(shorthand_char)==result.end())
        result[shorthand_char]=s_p.first;
    }
    return result;
  }

  void PieceBox::add_piece_kind(PieceInfo pk) {
    Square square=pieces.size();
    pieces.push_back(
      {pk.name, pk.shorthand, pk.piece, pk.color, pk.occupied});
    if (square_map.find(pk.name) not_eq square_map.end())
      throw runtime_error(
        "piece with name \""+pk.name+"\" already added");
    square_map[pk.name]=square;
    inv_square_map[square]=pk.name;
    (pk.occupied ? occupied_squares : empty_squares).push_back(square);
    p_square_range=make_pair(0, size()-1);
  }
  void PieceBox::add_piece_kind_list(list<PieceInfo> pkl) {
    for (PieceInfo pk: pkl)
      add_piece_kind(pk);
  }

  list<PieceInfo> empty_squares(list<string> names,
                                string shorthand, Piece piece) {
    list<PieceInfo> result;
    for (string name: names) // any colour:
      result.push_back({name, shorthand, piece, Color::white, false});
    return result;
  }
  list<PieceInfo> empty_squares(string name,
                                string shorthand, Piece piece)
    { return empty_squares(list<string>{name}, shorthand, piece); }

  list<PieceInfo>
  piece_squares(list<string> names,
                string shorthand, Piece piece, Color color) {
    list<PieceInfo> result;
    for (string name: names)
      result.push_back({name, shorthand, piece, color}); // any colour
    return result;
  }
  list<PieceInfo>
  piece_squares(string name,
                string shorthand, Piece piece, Color color) {
    return piece_squares(list<string>{name},
                         shorthand, piece, color);
  }

  list<PieceInfo>
  color_piece_squares(list<tuple<list<string>, vector<string>, Piece>>
                        list_of_name_suffixes_and_shorthands_and_pieces,
                      vector<pair<Color, string>>
                        color_prefixes) {
    list<PieceInfo> result;
    for (size_t i=0; i<color_prefixes.size(); ++i) {
      auto color_prefix=color_prefixes[i];
      Color color=color_prefix.first;
      string prefix=color_prefix.second;
      for (auto name_suffixes_and_shorthands_and_piece:
             list_of_name_suffixes_and_shorthands_and_pieces) {
        auto name_suffixes=get<0>(name_suffixes_and_shorthands_and_piece);
        auto shorthands=get<1>(name_suffixes_and_shorthands_and_piece);
        if (shorthands.size() not_eq color_prefixes.size())
          throw runtime_error("nonmatching shorthands and colours");
        string shorthand=shorthands[i];
        Piece piece=get<2>(name_suffixes_and_shorthands_and_piece);
        list<string> square_names;
        for (string name: name_suffixes)
          square_names.push_back(prefix+name);
        result.splice(result.end(),
                      piece_squares(square_names, shorthand, piece, color));
      }
    }
    return result;
  }

}
