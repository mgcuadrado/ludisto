#include "base.h"
#include <iostream>

using namespace std;

namespace sxako {

  void report_error(string message)
    { cout << endl << message << endl; }
  void report_error(string message, string error)
    { cout << endl << message << ": " << error << endl; }

  input_f input(std::istream &is)
    { return [&is]() { string s; getline(is, s); return s; }; }
  output_f output(std::ostream &os)
    { return [&os](string s) { os << s; }; }

  input_f const cin_input=input(cin);
  output_f const cout_output=output(cout), cerr_output=output(cerr);

}
