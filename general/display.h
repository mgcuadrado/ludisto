#ifndef SXAKO_DISPLAY_HEADER_
#define SXAKO_DISPLAY_HEADER_

#include <sstream>
#include <tuple>
#include <map>

namespace sxako {

  class LastMove;

  enum class Colorish {
    regular,
    reddish, blueish, greenish, cyanish, magentaish, yellowish
  };

  using info_t=std::map<unsigned, std::string>;

  info_t info_turn_last_move(std::string const &turn,
                             LastMove const &last_move);

  template <typename AccessT>
  std::string display_border(AccessT const &access,
                             std::string left,
                             std::string middle,
                             std::string right) {
    std::string result=left;
    for (auto x: std::get<0>(access.addr.enumerated_coords))
      std::ignore=x, result+=middle;
    return result+=right;
  }

  template <typename AccessT, typename SquareToText, typename Occupied>
  std::string display_ascii_straight_2d(AccessT const &access,
                                        SquareToText const &square_to_text,
                                        Occupied const &occupied,
                                        info_t const &info,
                                        std::string outside_square=" ") {
    std::string const
      corner_nw=".-", corner_ne=".", corner_sw="'-", corner_se="'",
      border_top="--", border_bottom="--", border_left="| ", border_right="|",
      white_square="-", black_square="+";

    std::ostringstream oss;

    // top
    oss << display_border(access, corner_nw, border_top, corner_ne)
        << std::endl;

    // body
    int info_line=0;
    for (auto y: revert(std::get<1>(access.addr.enumerated_coords))) {
      oss << border_left;
      for (auto x: std::get<0>(access.addr.enumerated_coords)) {
        auto l=std::make_tuple(x, y);
        if (access.contains(l)) {
          auto s=access[l];
          if (occupied(s))
            oss << square_to_text(s);
          else
            oss << ((x+y)%2 ? white_square : black_square);
        }
        else
          oss << outside_square;
        oss << " ";
      }
      oss << border_right;

      ++info_line;
      if (info.find(info_line) not_eq info.end())
        oss << " " << info.at(info_line);

      oss << std::endl;
    }

    // bottom
    oss << display_border(access, corner_sw, border_bottom, corner_se)
        << std::endl;

    return oss.str();
  }

  template <typename AccessT, typename SquareBgToText, typename Occupied>
  std::string display_ansi_straight_2d(AccessT const &access,
                                       SquareBgToText const &square_bg_to_text,
                                       Occupied const &occupied,
                                       info_t const &info,
                                       std::string outside_square="  ") {
    static std::string const
      prefix="\033[",
      reset=prefix+"0m",
      cursor_position_upper_left=prefix+"H",
      erase_entire_display=prefix+"2J"+cursor_position_upper_left,
      color_normal=prefix+"0;39m",
      white_bg=prefix+"0;47;30m",
      black_bg=prefix+"0;40;37m",
      gray_fg=prefix+"0;38;5;244m",
      sel_white_bg=prefix+"0;48;5;195;38;5;0m",
      sel_black_bg=prefix+"0;48;5;23;38;5;15m";

    std::string const
      corner_nw=" ▄", corner_ne="▄ ", corner_sw=" ▀", corner_se="▀ ",
      border_top="▄▄", border_bottom="▀▀", border_left=" █", border_right="█ ",
      white_square="-", black_square="+";

    std::ostringstream oss;

    // oss << reset << cursor_position_upper_left;

    // top
    oss << gray_fg
        << display_border(access, corner_nw, border_top, corner_ne)
        << color_normal << std::endl;

    // body
    int info_line=0;
    for (auto y: revert(std::get<1>(access.addr.enumerated_coords))) {
      oss << gray_fg << border_left;
      for (auto x: std::get<0>(access.addr.enumerated_coords)) {
        bool white_square=(x+y)%2;
        oss << (white_square ? white_bg : black_bg);
        auto l=std::make_tuple(x, y);
        if (access.contains(l)) {
          auto s=access[l];
          if (occupied(s))
            oss << square_bg_to_text(s, white_square);
          else
            oss << "  ";
        }
        else
          oss << outside_square;
      }
      oss << gray_fg << border_right << color_normal;

      ++info_line;
      if (info.find(info_line) not_eq info.end())
        oss << " " << info.at(info_line);

      oss << std::endl;
    }

    // bottom
    oss << gray_fg
        << display_border(access, corner_sw, border_bottom, corner_se)
        << color_normal << std::endl;

    return oss.str();
  }

}

#endif
