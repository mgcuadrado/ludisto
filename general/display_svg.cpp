#include "display_svg.h"
#include <fstream>

using namespace std;

namespace sxako::svg {

  string definitions(set<string> def_names, pair<float, float> width_height) {
    string result;
    for (string name: def_names) {
      ifstream def_if("share/"+name+".svg");
      if (def_if) {
        list<string> lines;
        for (string line; getline(def_if, line); lines.push_back(line));
        // remove two first and last:
        for (auto it=next(lines.begin(), 2); next(it) not_eq lines.end(); ++it)
          result+=*it;
      }
      else
        result+=
          "  <g id=\""+name+"\"><text x=\""+to_text(width_height.first/2.)
          +"\" y=\""+to_text(width_height.second*(8./9.))+"\" "
          "style=\"text-anchor:middle; font-size:"+to_text(width_height.second)
          +"px\">?</text></g>";
    }
    return result;
  }

  std::string const
    begin=
      "<svg xmlns=\"http://www.w3.org/2000/svg\"\n"
      "     xmlns:xlink=\"http://www.w3.org/1999/xlink\">\n"
      "<!-- check for attribution: https://github.com/niklasf/python-chess/blob/master/chess/svg.py -->",
    def_begin="<defs>\n",
    def_end="</defs>\n",
    end="</svg>\n";

}
