#ifndef SXAKO_GENERAL_MULTI_HEADER_
#define SXAKO_GENERAL_MULTI_HEADER_

#include "think.h"
#include "board.h"

namespace sxako {

  class GameSpecMap;

  using Rules_f=std::function<Rules (std::string spec)>;

  struct GameSpec {
    GameSpec(std::string name, std::string help, std::string default_params,
             Rules_f rules_f, EvaluationMap evaluation_map);
    GameSpec(std::string name, std::string help, std::string default_params,
             Rules rules, EvaluationMap evaluation_map)
      : GameSpec(name, help, default_params,
                 [rules](std::string) { return rules; }, evaluation_map) { }
    template <typename... A>
    GameSpec(GameSpecMap &game_spec_map, A &&...a);
    // GameSpec(std::string name, std::string help, std::string default_params,
    //          Rules_f rules, EvaluationMap evaluation_map,
    //          GameSpecMap &game_spec_map);

    std::string const name;
    std::string const help;
    std::string const default_params;
    Rules_f const rules_f;
    EvaluationMap evaluation_map;
    // get the parameterised evaluation function from its name-and-params
    // specification; if empty, the value of "default_params" is used instead
    evaluation_function_t evaluation(std::string name_and_params) {
      return evaluation_map(name_and_params.empty()
                            ? default_params
                            : name_and_params);
    }
  };

  class GameSpecMap {
    friend class GameSpec;
    void insert(GameSpec);
  public:
    std::list<std::string> list() const;
    bool contains(std::string name) const;
    GameSpec operator()(std::string name) const;
  private:
    std::map<std::string, GameSpec> game_specs;
  };

  GameSpecMap &game_spec_map();

  template <typename... A>
  GameSpec::GameSpec(GameSpecMap &game_spec_map, A &&...a)
    : GameSpec(a...)
    { game_spec_map.insert(*this); }
}

#endif
