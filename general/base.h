#ifndef SXAKO_BASE_HEADER_
#define SXAKO_BASE_HEADER_

#include <cstddef>
#include <cstdint>
#include <numeric>
#include <utility>
#include <vector>
#include <list>
#include <tuple>
#include <algorithm>
#include <sstream>
#include <functional>

namespace sxako {

  // error reporting to cout
  void report_error(std::string message);
  void report_error(std::string message, std::string error);

#define eat_a_semicolon static_cast<void>(0)
#define decl_eat_a_semicolon struct never_define_this_struct__dude_

#define begin_try_catch try { eat_a_semicolon
#define end_try_catch }                                    \
    catch (std::exception const &excp)                     \
      { report_error("caught exception: ", excp.what()); } \
    catch (char const *mess)                               \
      { report_error("caught error: ", mess); }            \
    catch (...)                                            \
      { report_error("uncaught error"); throw; }           \
     eat_a_semicolon

  using std::size_t;

  // a better name (for our purposes) for the often-used "initializer_list<>"
  template <typename T>
  using series=std::vector<T>;

  // do nothing with its arguments; useful when variadic arguments have to be
  // executed for their side effects (see "Impl::op_plus_equal_t_d()" below)
  template <typename... T>
  void swallow(T &&...) { }

  // check if any number of types are all the same
  template <typename... T>
  struct are_same : std::false_type { };
  template <>
  struct are_same<> : std::true_type { };
  template <typename T>
  struct are_same<T> : std::true_type { };
  template <typename FS, typename... R>
  struct are_same<FS, FS, R...> : are_same<FS, R...> { };

  static_assert((are_same<int, int, int>::value
                 and not are_same<int, int, bool>::value));

  // get the compiler's idea of the name of a type
  template <typename T>
  std::string type_name() { return typeid(T).name(); }
  // get the compiler's idea of the name of the type of an object
  template <typename T>
  std::string type_name(T const &) { return type_name<T>(); }

  template <size_t size, typename T>
  constexpr size_t array_size(T (&)[size]) { return size; }

  // "constant()" and "variable()" are used to avoid code duplication when
  // writing both a const version and a non-const version of a method; see
  // "Effective C++, 3d ed by Scott Meyers", Item 3 "Use const whenever
  // possible", "Avoid Duplication in const and Non-const Member Function"
  template <typename T>
  T const &constant(T &t) { return const_cast<T const &>(t); }
  template<typename T>
  T &variable(T const &t) { return const_cast<T &>(t); }

  // shorter names for types that we're going to use a lot
  using u8=uint8_t;   using s8=int8_t;
  using u16=uint16_t; using s16=int16_t;
  using u32=uint32_t; using s32=int32_t;
  using u64=uint64_t; using s64=int64_t;

  constexpr unsigned n_u8=256;

  using index_t=size_t;
  using s_index_t=std::make_signed_t<index_t>;

  // "tuple_array_t<T, dim>" is a tuple containing "dim" objects of type "T"
  template <typename T, index_t dim>
  struct tuple_array { // FIXME: this implementation seems too complex
  private:
    template <index_t i, typename U>
    using iterate=U;
    template <typename> struct type_impl;
    template <index_t... i>
    struct type_impl<std::index_sequence<i...>>
      { using type=std::tuple<iterate<i, T>...>; };
  public:
    using type=typename type_impl<std::make_index_sequence<dim>>::type;
  };
  template <typename T, index_t dim>
  using tuple_array_t=typename tuple_array<T, dim>::type;

  // a vector containing all "T" values between "begin" and "last" (inclusive);
  // "begin<=last" must hold
  template <typename T>
  constexpr auto range_from_to(T const &begin, T const &last) {
    std::vector<T> result(s_index_t(last)-s_index_t(begin)+1);
    for (s_index_t i=0, v=s_index_t(begin); v<=s_index_t(last); ++i, ++v)
      result[i]=T(v);
    return result;
  }
  // same thing with a pair containing "begin" and "last"
  template <typename T>
  constexpr auto range_from_to(std::pair<T, T> r)
    { return range_from_to(r.first, r.second); }

  // same thing when we're not sure if "begin<last" or not, but we still want
  // all values between them
  template <typename T>
  constexpr auto range_between(T const &t1, T const &t2)
    { return t1<t2 ? range_from_to(t1, t2) : range_from_to(t2, t1); }
  // same thing with a pair containing "t1" and "t2"
  template <typename T>
  constexpr auto range_between(std::pair<T, T> r)
    { return range_between(r.first, r.second); }

  namespace Impl {
    template <typename Tu, index_t... i>
    constexpr auto tuple_rest_impl(Tu const& t, std::index_sequence<i...>)
      { return std::make_tuple(std::get<i+1>(t)...); }
  }
  // return its tuple argument without its first element (if "get<0>(t)" is
  // "car(t)", then "tuple_rest(t)" is "cdr(t)")
  template <typename F, typename... R>
  constexpr auto tuple_rest(std::tuple<F, R...> const &t)
    { return Impl::tuple_rest_impl(t, std::index_sequence_for<R...>()); }

  // return its vector argument reversed
  template <typename T>
  std::vector<T> revert(std::vector<T> const &v) {
    std::vector<T> result=v;
    std::reverse(result.begin(), result.end());
    return result;
  }

  // check whether the argument is an STL vector
  template <typename V>
  struct is_vector : std::false_type { };
  template <typename... T>
  struct is_vector<std::vector<T...>> : std::true_type { };

  // check whether the argument is an STL tuple
  template <typename>
  struct is_tuple : std::false_type { };
  template <typename ...T>
  struct is_tuple<std::tuple<T...>> : std::true_type {};

  // cartesian product of a collection (tuple) of vectors, as a vector of all
  // the possible tuples formed with the elements of the original vectors:
  //
  //     cartesian(tuple(V1, ..., Vn))
  //       == vector(all possible tuple(element from V1, ..., element from V2))
  inline auto cartesian(std::tuple<>)
    { return std::vector<std::tuple<>>{std::make_tuple()}; }
  template <typename F, typename... R>
  auto cartesian(std::tuple<std::vector<F>, std::vector<R>...> const &t) {
    std::vector<std::tuple<F, R...>> result;
    auto rest=cartesian(tuple_rest(t));
    for (auto f: std::get<0>(t))
      for (auto r: rest)
        result.push_back(std::tuple_cat(std::make_tuple(f), r));
    return result;
  }

  // "r_cartesian()" is just like cartesian, but with the for loops inverted,
  // which gives the same result in a different order; i used it for
  // "StraightIndex<>" temporarily in order to match a regression test that was
  // apparently depending on the hash of the board; to be deprecated
  inline auto r_cartesian(std::tuple<>)
    { return std::vector<std::tuple<>>{std::make_tuple()}; }
  template <typename F, typename... R>
  auto r_cartesian(std::tuple<std::vector<F>, std::vector<R>...> const &t) {
    std::vector<std::tuple<F, R...>> result;
    auto rest=r_cartesian(tuple_rest(t));
    for (auto r: rest)
      for (auto f: std::get<0>(t))
        result.push_back(std::tuple_cat(std::make_tuple(f), r));
    return result;
  }

  namespace Impl {
    template <index_t i, typename D, typename T, index_t n>
    std::enable_if_t<i==n, bool>
    is_in_impl(D const &, std::array<T, n> const &)
      { return false; }
    template <index_t i, typename D, typename T, index_t n>
    std::enable_if_t<(i<n), bool>
    is_in_impl(D const &d, std::array<T, n> const &a)
      { return d==a[i] or is_in_impl<i+1>(d, a); }
  }
  // is "d" one of the elements of array "a"
  template <typename D, typename T, index_t n>
  bool is_in(D const &d, std::array<T, n> const &a)
    { return Impl::is_in_impl<0>(d, a); }
  template <typename D, typename T>
  bool is_in(D const &d, std::initializer_list<T> const &l) {
    for (T const &t: l)
      if (d==t)
        return true;
    return false;
  }
  template <typename D, typename T>
  bool is_in(D const &d, std::list<T> const &l) {
    for (T const &t: l)
      if (d==t)
        return true;
    return false;
  }
  template <typename D, typename T>
  bool is_in(D const &d, std::vector<T> const &v) {
    for (T const &t: v)
      if (d==t)
        return true;
    return false;
  }

  // is "d" (the first argument) one of the remaining arguments
  template <typename D>
  bool is_among(D const &) { return false; }
  template <typename D, typename F, typename... R>
  bool is_among(D const &d, F const &f, R const &...r)
    { return d==f or is_among(d, r...); }

  // is "t" in the range specified by the pair "p" (begin, last; inclusive)
  template <typename T, typename P>
  bool is_in_range(T const &t, P const &p) // both ends inclusive
    { return p.first<=t and t<=p.second; }

  namespace Impl {
    template <typename Tu, index_t... i>
    auto range_to_size_impl(Tu const &t, std::index_sequence<i...>) {
      return std::array<index_t, sizeof...(i)>{
               index_t(s_index_t(std::get<i>(t).second)
                       -s_index_t(std::get<i>(t).first)+1)...};
    }
  }
  // if the argument is a tuple of pairs, representing (inclusive) ranges,
  // return a tuple of the size of the ranges:
  //
  //     range_to_size(tuple({a_1, b_1}, ..., {a_n, b_n}))
  //       == tuple(b_1-a_1+1, ..., b_n-a_n+1)
  template <typename... T>
  auto range_to_size(std::tuple<T...> const &tu)
    { return Impl::range_to_size_impl(tu, std::index_sequence_for<T...>()); }

  namespace Impl {
    template <typename Tu, index_t... i>
    auto tuple_first_impl(Tu const &t, std::index_sequence<i...>)
      { return std::make_tuple(s_index_t(std::get<i>(t).first)...); }
    template <typename Tu, index_t... i>
    auto tuple_second_impl(Tu const &t, std::index_sequence<i...>)
      { return std::make_tuple(s_index_t(std::get<i>(t).second)...); }
  }
  // for a tuple of pairs, return a tuple with all the ".first" of the pairs
  template <typename... T>
  auto tuple_first(std::tuple<T...> const &t)
    { return Impl::tuple_first_impl(t, std::index_sequence_for<T...>()); }
  // for a tuple of pairs, return a tuple with all the ".second" of the pairs
  template <typename... T>
  auto tuple_second(std::tuple<T...> const &t)
    { return Impl::tuple_second_impl(t, std::index_sequence_for<T...>()); }

  namespace Impl {
    template <typename C, typename Tu, index_t... i>
    std::array<C, std::tuple_size<Tu>::value>
    tuple_to_array_impl(Tu const &t, std::index_sequence<i...>)
      { return {C(std::get<i>(t))...}; }
  }
  // for a tuple, return all its elements converted to the same type "C", as an
  // array
  template <typename C, typename...T>
  auto tuple_to_array(std::tuple<T...> const &t) {
    return Impl::tuple_to_array_impl<C>(t, std::index_sequence_for<T...>());
  }

  namespace Impl {
    template <typename Tu, index_t... i>
    auto enumerated_coords_from_range_impl(Tu const &t,
                                           std::index_sequence<i...>)
      { return std::make_tuple(range_from_to(std::get<i>(t))...); }
  }
  // for a tuple of pairs representing (inclusive) ranges, return a tuple of
  // vectors, each containing the elements of each range
  template <typename... T>
  auto enumerated_coords_from_range(std::tuple<T...> const &t) {
    return Impl::enumerated_coords_from_range_impl(
             t, std::index_sequence_for<T...>());
  }

  template <typename T>
  struct possibly_unused {
    possibly_unused(T const &t) : value(t) { }
    ~possibly_unused() { } // the destructor prevents unused-variable warnings
    operator T() const { return value; }
  private:
    T const value;
  };

  template <typename T>
  std::vector<possibly_unused<T>> loop(T begin, T end) {
    std::vector<possibly_unused<T>> result;
    result.reserve(end-begin);
    for (T i=begin; i not_eq end; ++i)
      result.push_back(i);
    return result;
  }

  template <typename T>
  auto loop(T end) { return loop(T(), end); }

  /// delta_t

  // if "coord_t" is a tuple, "delta_t<coord_t>" is a new type representing the
  // difference (as per "operator-()") between two instances of "coord_t";
  // "delta_t<coord_t>" supports "get<>()" (same syntax as for tuples), and the
  // following operations involving "coord_t t1, t2" and "delta_t<coord_t> d1,
  // d2" (more operations can be defined easily, but i think they wouldn't be
  // used):
  //
  //     d1=-d2;
  //     t1+=d1;
  //     t2=t1+d1;
  //     t2=t1-d1;
  //     d1=t2-t1;
  template <typename>
  struct delta_t;
  template <typename... T>
  class delta_t<std::tuple<T...>> {
  public:
    constexpr delta_t(T const &...t) : value(t...) { }
  private:
    template <size_t i, typename... T_>
    friend constexpr auto get(delta_t<std::tuple<T_...>> const &d);
    template <size_t i, typename... T_>
    friend constexpr auto &get(delta_t<std::tuple<T_...>> &d);

    std::tuple<std::make_signed_t<T>...> value;
  };

  template <size_t i, typename... T>
  constexpr auto get(delta_t<std::tuple<T...>> const &d)
    { return std::get<i>(d.value); }
  template <size_t i, typename... T>
  constexpr auto &get(delta_t<std::tuple<T...>> &d)
    { return std::get<i>(d.value); }

  namespace Impl {
    template <typename D, index_t... i>
    auto op_minus_d(D const &d, std::index_sequence<i...>)
      { return D(-get<i>(d)...); }
    template <typename T, typename D, index_t... i>
    auto op_plus_equal_t_d(T &t, D const &d, std::index_sequence<i...>)
      { swallow(std::get<i>(t)+=get<i>(d)...); return t; }
    template <typename T, typename D, index_t... i>
    auto op_plus_t_d(T const &t, D const &d, std::index_sequence<i...>)
      { return T(std::get<i>(t)+get<i>(d)...); }
    template <typename T, typename D, index_t... i>
    auto op_minus_t_d(T const &t, D const &d, std::index_sequence<i...>)
      { return T(std::get<i>(t)-get<i>(d)...); }
    template <typename T, index_t... i>
    auto op_minus_t_t(T const &t, T const &u, std::index_sequence<i...>)
      { return delta_t<T>(std::get<i>(t)-std::get<i>(u)...); }
  }
  template <typename... T>
  auto operator-(delta_t<std::tuple<T...>> const &d)
    { return Impl::op_minus_d(d, std::index_sequence_for<T...>()); }
  template <typename... T>
  auto operator+=(std::tuple<T...> &t, delta_t<std::tuple<T...>> const &d)
    { return Impl::op_plus_equal_t_d(t, d, std::index_sequence_for<T...>()); }
  template <typename... T>
  auto operator+(std::tuple<T...> const &t, delta_t<std::tuple<T...>> const &d)
    { return Impl::op_plus_t_d(t, d, std::index_sequence_for<T...>()); }
  template <typename... T>
  auto operator-(std::tuple<T...> const &t, delta_t<std::tuple<T...>> const &d)
    { return Impl::op_minus_t_d(t, d, std::index_sequence_for<T...>()); }
  template <typename... T>
  auto operator-(std::tuple<T...> const &t, std::tuple<T...> const &u)
    { return Impl::op_minus_t_t(t, u, std::index_sequence_for<T...>()); }

  // check whether the argument is a "delta_t<coord_t>"
  template <typename>
  struct is_delta : std::false_type { };
  template <typename ...T>
  struct is_delta<delta_t<std::tuple<T...>>> : std::true_type {};

  // get_x, get_y, get_z, get_dx, get_dy, get_dz (shortcuts for "get<>()")
  template <typename... T>
  constexpr auto get_x(std::tuple<T...> const &tu) { return std::get<0>(tu); }
  template <typename... T>
  constexpr auto &get_x(std::tuple<T...> &tu) { return std::get<0>(tu); }
  template <typename... T>
  constexpr auto get_y(std::tuple<T...> const &tu) { return std::get<1>(tu); }
  template <typename... T>
  constexpr auto &get_y(std::tuple<T...> &tu) { return std::get<1>(tu); }
  template <typename... T>
  constexpr auto get_z(std::tuple<T...> const &tu) { return std::get<2>(tu); }
  template <typename... T>
  constexpr auto &get_z(std::tuple<T...> &tu) { return std::get<2>(tu); }

  /// functions for text-based input and output
  using input_f=std::function<std::string ()>;      // reads by lines
  using output_f=std::function<void (std::string)>; // doesn't add "\n"

  input_f input(std::istream &is); // reader function for "is"
  output_f output(std::ostream &os); // writer function for "os"
  extern input_f const cin_input; // reader function for standard "cin"
  extern output_f const  // writer functions for standard "cout" and "cerr"
    cout_output, cerr_output;

  /// to_text()
  template <typename T>
  std::string to_text(T const &t) {
    std::ostringstream oss;
    oss << t;
    return oss.str();
  }

  /// from_text()
  template <typename T>
  T from_text(std::string const &s) {
    T t;
    std::istringstream iss(s);
    iss >> t >> std::ws;
    if (not iss.eof())
      throw std::invalid_argument(
              "string \""+s+"\" has extra content for conversion to \""
              +type_name<T>()+"\"");
    return t;
  }

  // a templatised class for lists of things that can be added to and scanned
  template <typename T>
  class Sequence {
  public:
    void append(T const &t) { container.push_back(t); }
    std::list<T> const &elements=container;
  private:
    std::list<T> container;
  };

  // whatever we need from "cppfunct"
  namespace micro_cppfunct {

    template <typename O, typename R, typename... A>
    auto f_funct(O const *o, R (O::*m)(A...) const)
      { return [o, m](A... a) { return (o->*m)(a...); }; }

    template <typename R>
    auto f_const(R r)
      { return [r]() { return r; }; }

    template <typename R>
    auto f_var(R const *r_ptr)
      { return [r_ptr]() { return *r_ptr; }; }

  }

  using namespace micro_cppfunct;

}

#endif
