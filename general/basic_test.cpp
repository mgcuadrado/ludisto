#include <iostream>
#include "straight.h"
#include "think.h"

using namespace std;
using namespace sxako;

void print_tuple_impl(ostream &, tuple<>) { }
template <typename T>
void print_tuple_impl(ostream &os, tuple<T> const &t) { os << get<0>(t); }
template <typename F, typename S, typename... R>
void print_tuple_impl(ostream &os, tuple<F, S, R...> const &t) {
  os << get<0>(t) << ",";
  print_tuple_impl(os, tuple_rest(t));
}
template <typename... T>
ostream &operator<<(ostream &os, tuple<T...> const &t) {
  os << "(";
  print_tuple_impl(os, t);
  return os << ")";
}

template <typename T>
ostream &operator<<(ostream &os, vector<T> const &v) {
  os << "{";
  for (auto e: v)
    os << " " << e;
  return os << " }";
}

enum class Fruit : u8 { apple, banana, cherry, date };

ostream &operator<<(ostream &os, Fruit f) {
  switch (f) {
  case Fruit::apple: return os << "apple";
  case Fruit::banana: return os << "banana";
  case Fruit::cherry: return os << "cherry";
  case Fruit::date: return os << "date";
  default: throw "what fruit izzat?";
  }
}

ostream &operator<<(ostream &os, params_t const &params) {
  os << "{";
  for (auto p: params)
    os << " (" << p.arg << "|" << p.value << ")";
  return os << " }";
}

int main(int, char *[]) {
  cout << boolalpha;
  {
    cout << "non-chess tests" << endl;
    cout << make_tuple(1, 'b') << endl;
    cout << cartesian(make_tuple(vector<int>{1, 2, 3},
                                 vector<char>{'a', 'b', 'c'})) << endl;
    cout << cartesian(make_tuple(vector<int>{8, 9},
                                 vector<char>{'a', 'b'},
                                 vector<bool>{false, true})) << endl;
    for (auto i: range(Fruit::apple, Fruit::banana))
      cout << " " << i;
    cout << endl;
    for (auto i: range(Fruit::date, Fruit::apple))
      cout << " " << i;
    cout << endl;
    for (auto i: range(Fruit::banana, Fruit::banana))
      cout << " " << i;
    cout << endl;

    DataSpec<Kind::table> test_data_spec;
    Addressing<Kind::table, Straight2D<int, char>, float> const
      test_addressing(test_data_spec, make_pair(1, 2), make_pair('a', 'c'));
    Data<Kind::table> test_data(test_data_spec);

    array<array<float, 2>, 3> test_array_1={
      {{1.1, 1.2},
       {2.3, 3.4},
       {5.5, 8.6}}
    };
    assign_data(test_data(test_addressing), test_array_1,
                [](tuple<char, int> t, float)
                  { return make_tuple(get<1>(t), get<0>(t)); },
                range('c', 'a'), range(1, 2));
    for (auto c: test_addressing.all_enumerated_coords)
      cout << c << ":" << test_data(test_addressing)[c] << endl;

    array<array<float, 3>, 2> test_array_2={
      {{2.1, 3.2, 5.3},
       {7.4, 1.5, 3.6}}
    };
    assign_data(test_data(test_addressing), test_array_2,
                range(2, 1), range('a', 'c'));
    for (auto c: test_addressing.all_enumerated_coords)
      cout << c << ":" << test_data(test_addressing)[c] << endl;

    cout << parse("abc:cde=7:f:") << endl;
    cout << parse(".04:34") << endl;
    cout << parse("pruning") << endl;
    cout << parse("") << endl;
    cout << parse(":") << endl;

    cout << "cheking default algorithm params: "
         << (AlgorithmParams()==AlgorithmParams(default_params_s)) << endl;
  }
}
