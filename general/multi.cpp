#include "multi.h"

using namespace std;

namespace sxako {

  GameSpec::GameSpec(string name, string help, string default_params,
                     Rules_f rules_f, EvaluationMap evaluation_map)
    : name(name), help(help), default_params(default_params), rules_f(rules_f),
      evaluation_map(evaluation_map) { }

  void GameSpecMap::insert(GameSpec gs) {
    if (not game_specs.insert({gs.name, gs}).second)
      throw invalid_argument("couldn't add GameSpec \""+gs.name+"\"");
  }

  list<string> GameSpecMap::list() const {
    std::list<string> result;
    for (auto const &nge: game_specs)
      result.push_back(nge.first);
    return result;
  }

  namespace {

    string just_name(string s) { return s.substr(0, s.find(':')); }

  }

  bool GameSpecMap::contains(string name) const
    { return game_specs.find(just_name(name)) not_eq game_specs.end(); }

  GameSpec GameSpecMap::operator()(string name) const {
    if (contains(name))
      return game_specs.at(just_name(name));
    else
      throw invalid_argument("unknown game spec \""+name+"\"");
  }

  GameSpecMap &game_spec_map() {
    static GameSpecMap result;
    return result;
  }

}

