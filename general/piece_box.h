#ifndef SXAKO_GENERAL_PIECE_BOX_HEADER_
#define SXAKO_GENERAL_PIECE_BOX_HEADER_

#include "board.h"
#include <map>

namespace sxako::piece_box {

  using Square=u8;
  size_t constexpr n_squares=n_u8;
  using Piece=char;

  Square const empty=0;

  struct PieceInfo {
    std::string name; // square name; unique
    std::string shorthand; // ascii symbol ("w_lion" -> "L"; "b_lion" -> "l")
    Piece piece; // kind of piece ("w_lion" -> 'l'; "b_lion" -> 'l')
    Color color;
    bool occupied=true; // not empty; traversable square
  };

  // a piece box is a repository for information related to kinds of squares
  // and pieces; you can fill its constructor a list of piece or piece set
  // specifications (see helpers below: "empty_squares()" and
  // "piece_squares()"), and then use the information to get various views on
  // the piece box
  class PieceBox {
  public:
    // the piece information can be put in the piece box only during
    // construction
    template <typename... A>
    PieceBox(A... a) { add_piece_kinds(a...); }

    // access square value by square name; suggestion:
    //
    //
    //     PieceBox piece_box(...);
    //
    //     Square operator ""_sq(char const *name, size_t)
    //       { return piece_box[name]; }
    //
    // will enable the syntax
    //
    //     "b_pawn"_sq
    Square operator[](std::string name) const { return square_map.at(name); }
    // number of squares in the piece box
    auto size() const { return pieces.size(); }
    // range for data addresses indexed on square type
    std::pair<Square, Square> const &square_range=p_square_range;

    std::string shorthand(Square s) const { return pieces[s].shorthand; }
    // first declared square with given shorthand; used for initial board
    // setup, so if several squares have the same shorthand, the "initial"
    // version must be specified first
    Square first_square_from_shorthand(std::string shorthand) const;

    // the following lists are used to build tables:
    std::list<std::pair<Square, std::string>> list_square_shorthand() const
      { return list_square_member(&PieceInfo::shorthand); }
    std::list<std::pair<Square, Piece>> list_square_piece() const
      { return list_square_member(&PieceInfo::piece); }
    std::list<std::pair<Square, Color>> list_square_color() const
      { return list_square_member(&PieceInfo::color); }
    std::list<std::pair<Square, bool>> list_square_occupied() const
      { return list_square_member(&PieceInfo::occupied); }

    std::vector<Square> const &enumerated_occupied_squares=occupied_squares;
    std::vector<Square> const &enumerated_empty_squares=empty_squares;

    std::string square_name(Square s) const { return inv_square_map.at(s); }
    // glossary for board setup
    std::map<char, Square> glossary_shorthand_char_to_first_square() const;
  private:
    void add_piece_kinds() { }
    template <typename... R>
    void add_piece_kinds(PieceInfo pk, R... r)
      { add_piece_kind(pk); add_piece_kinds(r...); }
    template <typename... R>
    void add_piece_kinds(std::list<PieceInfo> pkl, R... r)
      { add_piece_kind_list(pkl); add_piece_kinds(r...); }

    void add_piece_kind(PieceInfo pk);
    void add_piece_kind_list(std::list<PieceInfo> pkl);

    template <typename T>
    std::list<std::pair<Square, T>> list_square_member(T PieceInfo::*t) const {
      std::list<std::pair<Square, T>> result;
      for (Square s=0; s<size(); ++s)
        result.push_back(std::make_pair(s, pieces[s].*t));
      return result;
    }

    // the following attributes start with the piece info for empty square:
    std::vector<PieceInfo> pieces=
      {{"empty", " ", ' ', Color::white, false}}; // any colour
    std::map<std::string, Square> square_map={{"empty", 0}};
    std::map<Square, std::string> inv_square_map={{0, "empty"}};
    std::vector<Square> occupied_squares={}, empty_squares={0};
    std::pair<Square, Square> p_square_range{0, 0};
  };

  // declare empty squares for the piece box
  std::list<PieceInfo>
  empty_squares(std::list<std::string> names,
                std::string shorthand=" ", Piece piece=' ');
  std::list<PieceInfo>
  empty_squares(std::string name,
                std::string shorthand=" ", Piece piece=' ');

  // declare occupied squares for the piece box
  std::list<PieceInfo>
  piece_squares(std::list<std::string> names,
                std::string shorthand, Piece piece, Color color);
  std::list<PieceInfo>
  piece_squares(std::string name,
                std::string shorthand, Piece piece, Color color);

  // declare square for several colours; example:
  //
  //     color_piece_squares(
  //       {
  //         // declare "w_i_pawn", "b_i_pawn", "w_pawn", "b_pawn":
  //         {{"i_pawn", "pawn"}, {"P", "p"}, 'p'},
  //         {{"i_king", "king"}, {"K", "k"}, 'k'},
  //         {{"i_rook", "rook"}, {"R", "r"}, 'r'},
  //         // declare "w_knight", "b_knight":
  //         {{"knight"}, {"N", "n"}, 'n'},
  //         {{"bishop"}, {"B", "b"}, 'b'},
  //         {{"queen"},  {"Q", "q"}, 'q'}
  //       },
  //       {{Color::white, "w_"}, {Color::black, "b_"}}) // colour prefixes
  std::list<PieceInfo>
  color_piece_squares(std::list<std::tuple<std::list<std::string>,
                                           std::vector<std::string>,
                                           Piece>>
                        list_of_name_suffixes_and_shorthands_and_pieces,
                      std::vector<std::pair<Color, std::string>>
                        color_prefixes);

}

#endif
