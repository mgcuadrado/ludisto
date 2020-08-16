#ifndef SXAKO_STRAIGHT_HEADER_
#define SXAKO_STRAIGHT_HEADER_

#include "grid.h"
#include <map>

namespace sxako {

  template <index_t dim>
  using def_coord_t=tuple_array_t<s8, dim>;

  namespace Impl {
    template <typename Coord>
    struct CoordRangeImpl;
    template <typename... T>
    struct CoordRangeImpl<std::tuple<T...>>
      { using type=std::tuple<std::pair<T, T>...>; };
  }
  template <typename T>
  using coord_to_coord_range_t=typename Impl::CoordRangeImpl<T>::type;

  static constexpr index_t invalid_index=std::numeric_limits<index_t>::max();

  // For "StraightIndexing<dim>, "relative_cell_index(c)" is
  //
  //     r_0
  //
  // where
  //
  //     r_d = 0
  //
  //     r_n = ( x_n - m_n ) + s_n r_{n+1}
  //
  // with "d"=="dim", "m_n" is the minimum value for "x_n", "s_n" is the
  // number of possible values of "x_n", "c"=="{x_0, ..., x_{d-1}", and all
  // indices are zero-based.
  //
  // The recursion unfolds into
  //
  //     r_k = \sum_{i=k}^{d-1} ( ( x_i - m_i ) \product_{j=k}^{i-1} s_j )
  //
  // The expression for "r_0" can therefore be expanded to
  //
  //     r_0 = \sum{i=0}^{d-1} ( P_i x_i ) + Z
  //
  // where the terms "P_i" and "Z" can be precomputed as
  //
  //     P_i = \product_{j=1}^{i-1} s_j
  //
  //     Z = - \sum{i=0}^{d-1} ( P_i m_i )
  template <index_t dim, typename CoordT=def_coord_t<dim>>
  class StraightIndexing {
  public:
    using coord_t=CoordT;
    using coord_range_t=coord_to_coord_range_t<coord_t>;
    using size_t=std::array<index_t, dim>;

    template <typename T>
    StraightIndexing(T const &range_)
      : StraightIndexing(all<coord_t>(), range_) { }
    template <typename T>
      StraightIndexing(contains_t<coord_t> const &board_cells, T const &range_)
      : range(range_), size(range_to_size(range)),
        details(size, range, board_cells),
        cell_storage(details.cell_storage),
        enumerated_coords(enumerated_coords_from_range(range)),
        all_enumerated_coords(details.all_enumerated_coords),
        reverse_all_enumerated_coords(revert(all_enumerated_coords)),
        all_enumerated_displayed_coords(cartesian(enumerated_coords)) { }
    template <typename... T, typename... U>
    StraightIndexing(std::pair<T, U> const &...p)
      : StraightIndexing(std::make_tuple(p...)) { }
    template <typename... T, typename... U>
    StraightIndexing(contains_t<coord_t> const &board_cells,
                     std::pair<T, U> const &...p)
      : StraightIndexing(board_cells, std::make_tuple(p...)) { }

    coord_range_t const range;
    size_t const size;
  private:
    struct Details {
      Details(size_t const &size, coord_range_t const &range,
              contains_t<coord_t> board_cells) {
        auto m=tuple_to_array<s_index_t>(tuple_first(range));
        P[0]=1;
        for (index_t i=1; i<dim; ++i)
          P[i]=P[i-1]*size[i-1];
        Z=0;
        for (index_t i=0; i<dim; ++i)
          Z-=P[i]*m[i];

        cell_storage=0;
        for (coord_t c: r_cartesian(enumerated_coords_from_range(range))) {
          index_t i=noncompact_relative_cell_index(c);
          compact_indices.resize(std::max(compact_indices.size(), i+1));
          if (board_cells(c)) {
            all_enumerated_coords.push_back(c);
            compact_indices[i]=cell_storage++;
          }
          else
            compact_indices[i]=invalid_index;
        }
      }
      index_t relative_cell_index(coord_t const &c) const {
        assert(noncompact_relative_cell_index(c)<compact_indices.size());
        return compact_indices[noncompact_relative_cell_index(c)];
      }

      index_t cell_storage;
      std::vector<coord_t> all_enumerated_coords;
    private:
      index_t noncompact_relative_cell_index(coord_t const &c) const
        { return noncompact_relative_cell_index_impl<0>(c); }

     template <index_t i>
      std::enable_if_t<i==dim, index_t>
      noncompact_relative_cell_index_impl(coord_t) const
        { return Z; }
      template <index_t i>
      std::enable_if_t<(i+1<=dim), index_t>
      noncompact_relative_cell_index_impl(coord_t const &c) const {
        return
          noncompact_relative_cell_index_impl<i+1>(c)
          +std::get<i>(P)*s_index_t(std::get<i>(c));
      }

      std::array<s_index_t, dim> P;
      s_index_t Z;
      std::vector<index_t> compact_indices;
    } const details;
  public:
    index_t const cell_storage;
    decltype(enumerated_coords_from_range(range)) const enumerated_coords;
    std::vector<coord_t> const
      all_enumerated_coords, reverse_all_enumerated_coords,
      all_enumerated_displayed_coords;

    bool contains(coord_t const &c) const {
      return
        range_contains(c)
        and relative_cell_index(c) not_eq invalid_index;
    }
  protected:
    index_t relative_cell_index(coord_t const &c) const {
      assert(range_contains(c));
      return details.relative_cell_index(c);
    }
  private:
    template <index_t i>
    std::enable_if_t<i==dim, bool>
    range_contains_impl(coord_t const &) const
      { return true; }
    template <index_t i>
    std::enable_if_t<i not_eq dim, bool>
    range_contains_impl(coord_t const &c) const {
      return
        is_in_range(std::get<i>(c), std::get<i>(range))
        and range_contains_impl<i+1>(c);
    }
    bool range_contains(coord_t const &c) const
      { return range_contains_impl<0>(c); }
  };

  template <typename T1=s8>
  using Straight=StraightIndexing<1, std::tuple<T1>>;
  template <typename T1=s8, typename T2=s8>
  using Straight2D=StraightIndexing<2, std::tuple<T1, T2>>;
  template <typename T1=s8, typename T2=s8, typename T3=s8>
  using Straight3D=StraightIndexing<3, std::tuple<T1, T2, T3>>;
  template <typename T1=s8, typename T2=s8, typename T3=s8, typename T4=s8>
  using Straight4D=StraightIndexing<4, std::tuple<T1, T2, T3, T4>>;

  /// straight-based utilities

  // find the first non-empty square starting from "start", with successive
  // jumps of "displ"; an empty square is one in "empty_squares"; returns the
  // last square checked, which will be an empty square if we got out of the
  // board, or the non-empty square found otherwise
  template <typename DataAccess,
            typename coord_t, typename displ_t,
            typename square_t, index_t n>
  square_t first_non_empty(DataAccess const &access,
                           coord_t const &start, displ_t const &displ,
                           std::array<square_t, n> const &empty_squares) {
    coord_t l=start+displ;
    square_t result=empty_squares[0];
    while (access.get_if_contains(l, result) and is_in(result, empty_squares))
      l+=displ;
    return result;
  }
  // shortcut for "first_non_empty()" when there's only one kind of empty
  // square
  template <typename DataAccess,
            typename coord_t, typename displ_t, typename square_t>
  square_t first_non_empty(DataAccess const &access,
                           coord_t const &start, displ_t const &displ,
                           square_t const &empty_square) {
    return first_non_empty(access, start, displ,
                           std::array<square_t, 1>{empty_square});
  }

  /// data assignment from containers

  // "m" can be a map or a list of pairs
  template <typename AccessT, typename M,
            typename=std::void_t<typename M::value_type::first_type,
                                 typename M::value_type::second_type>>
  void assign_data(AccessT &&access,
                   M const &m) {
    for (auto cv: m)
      access[cv.first]=cv.second;
  }

  template <typename AccessT, typename L>
  void assign_data(AccessT &&access,
                   L const &l,
                   typename AccessT::cell_t const &value) {
    for (auto c: l)
      access[c]=value;
  }

  namespace Impl {

    template <typename AccessT, typename T,
              typename F, typename Tu>
    void assign_data_impl(AccessT &&access,
                          T const &t,
                          F const &f,
                          Tu const &tu) {
      auto c=f(tu, t);
      if (access.contains(c))
        access[c]=t;
    }

    template <typename AccessT, typename Array,
              typename F, typename Tu, typename VF, typename... VR>
    void assign_data_impl(AccessT &&access,
                          Array const &a, F const &f,
                          Tu const &tu,
                          std::vector<VF> const &range_first,
                          std::vector<VR> const &...range_rest) {
      if (a.size() not_eq range_first.size())
        throw std::logic_error("non-matching sizes for assign_data()");
      for (size_t i=0; i<range_first.size(); ++i)
        assign_data_impl(access, a[i], f,
                         std::tuple_cat(tu, std::make_tuple(range_first[i])),
                         range_rest...);
    }

  }

  // "f" must be a function with the following signature:
  //     typename AccessT::coord_t f(tuple<V...> const &c,
  //                                 typename AccessT::cell_t const &value)>
  //
  // this function can for instance verify that when the coordinates are not
  // contained in the board (i.e., when "not access.contain(c)"), the value is
  // a special marker specifically marking out-of-board-bonds locations in the
  // array
  template <typename AccessT, typename T, std::size_t n,
            typename F,
            typename... V,
            // avoid this "f" eating up a vector from the other overload:
            typename=std::enable_if_t<not is_vector<F>::value>>
  void assign_data(AccessT &&access, std::array<T, n> const &a,
                   F const &f,
                   std::vector<V> const &...range)
    { Impl::assign_data_impl(access, a, f, std::make_tuple(), range...); }

  // usual case where "f" simply returns the coordinate unchanged
  template <typename AccessT, typename T, std::size_t n,
            typename... V>
  void assign_data(AccessT &&access, std::array<T, n> const &a,
                   std::vector<V> const &...range) {
    assign_data(
      access, a,
      [](typename AccessT::coord_t const &c, typename AccessT::cell_t const &)
        { return c; },
      range...);
  }

  template <typename T>
  std::vector<T> range(T const &from, T const &to) {
    using S=std::make_signed_t<T>;
    S from_s=S(from), to_s=S(to);
    std::vector<T> result(std::abs(from_s-to_s)+1);
    for (size_t i=0; i<result.size(); ++i)
      result[i]=T((from_s>to_s) ? from_s-i : from_s+i);
    return result;
  }

  template <index_t... i>
  auto reorder_indices()
    { return [](auto c, auto) { return std::make_tuple(std::get<i>(c)...); }; }

  // see "chess.cpp" for a syntax example; the characters for the border must
  // be matched exactly: "-" for the horizontal borders, "|" for the vertical
  // borders, "." for the top corners, "'" for the bottom corners
  template <typename Access, typename S>
  void parse_2d_board(Access &&a,
                      std::string const &board_s,
                      std::map<char, S> const &glossary) {
    size_t i=0;

    auto check_index=[&board_s, &i]() {
      if (i>=board_s.size())
        throw std::runtime_error("read past end of board description");
    };
    auto expect=[&board_s, &i, check_index](char c) {
      check_index();
      if (board_s[i] not_eq c)
        throw std::runtime_error(
          std::string("expected char \"")+c+"\" at position "+to_text(i)
          +" but found \""+board_s[i]+"\" instead");
      ++i;
    };
    auto translate=[&board_s, &glossary, &i, check_index]() {
      check_index();
      if (glossary.find(board_s[i]) not_eq glossary.end())
        return glossary.at(board_s[i++]);
      else
        throw std::runtime_error(
          std::string("unexpected char \"")+board_s[i]
          +"\" at position "+to_text(i));
    };

    expect('.');
    for (size_t j=0; j<2*a.addr.size[0]+1; ++j)
      expect('-');
    expect('.');
    for (auto y: revert(get_y(a.addr.enumerated_coords))) {
      expect('|');
      expect(' ');
      for (auto x: get_x(a.addr.enumerated_coords)) {
        a[{x, y}]=translate();
        expect(' ');
      }
      expect('|');
    }
    expect('\'');
    for (size_t j=0; j<2*a.addr.size[0]+1; ++j)
      expect('-');
    expect('\'');
  }

}

#endif
