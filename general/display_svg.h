#ifndef SXAKO_DISPLAY_SVG_HEADER
#define SXAKO_DISPLAY_SVG_HEADER

#include "display.h"
#include "base.h"
#include <list>
#include <set>

namespace sxako {

  namespace svg {

    extern std::string const begin, def_begin, def_end, end;

    std::string definitions(std::set<std::string> def_names,
                            std::pair<float, float> width_height);

  }

  inline auto svg_int_coords(int svg_square_side) {
    return [svg_square_side]
      (auto location, auto range) {
        return std::make_tuple((get_x(location)-get_x(range).first)
                                 *svg_square_side,
                               (get_y(range).second-get_y(location))
                                 *svg_square_side);
      };
  }

  inline auto svg_checkered_location_to_empty_square(
      std::string light_square, std::string dark_square) {
    return [light_square, dark_square]
      (auto location, auto range) {
        auto sum_d=((get_x(location)-get_x(range).first)
                    +(get_y(location)-get_y(range).first));
        return sum_d%2 ? light_square : dark_square;
      };
  }

  inline auto svg_caissa_britannia_location_to_empty_square(
    std::string white_square,
    std::string blue_square, std::string red_square) {
    return [white_square, blue_square, red_square]
      (auto location, auto range) {
        auto sum_d=((get_x(location)-get_x(range).first)
                    +(get_y(location)-get_y(range).first));
        return
          sum_d%2
          ? white_square
          : get_x(location)%2
          ? blue_square
          : red_square;
      };
  }

  template <typename AccessT,
            typename IntCoords, typename LocationToEmptySquare,
            typename SquareToText, typename Occupied>
  std::string display_svg_straight_2d(
      AccessT const &access,
      IntCoords const &int_coords,
      LocationToEmptySquare location_to_empty_square,
      SquareToText const &square_to_text,
      Occupied const &occupied,
      std::pair<float, float> width_height,
      info_t const &) {
    std::string body;
    std::set<std::string> squares;
    for (auto l: access.addr.all_enumerated_displayed_coords) {
      auto c=int_coords(l, access.addr.range);
      std::string location_s=
        "x=\""+to_text(get_x(c))+"\" y=\""+to_text(get_y(c))+"\"";
      std::string empty_square_def=
        location_to_empty_square(l, access.addr.range);
      body+="<use xlink:href=\"#"+empty_square_def+"\" "+location_s+" />\n";
      squares.insert(empty_square_def);
      if (access.contains(l)) {
        auto s=access[l];
        std::string square_def=square_to_text(s);
        if (occupied(s)) {
          body+="<use xlink:href=\"#"+square_def+"\" "+location_s+" />\n";
          squares.insert(square_def);
        }
      }
    }
    return
      svg::begin+svg::def_begin
      +svg::definitions(squares, width_height)
      +svg::def_end+body+svg::end;
  }

}

#endif
