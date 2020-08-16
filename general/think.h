#ifndef SXAKO_THINK_HEADER_
#define SXAKO_THINK_HEADER_

#include "board.h"
#include <map>
#include <random>

namespace sxako {

  // "score_t" is the general type for evaluating scores
  using score_t=float;
  struct MoveScore { // group together a move and its evaluated score
    Move move;
    score_t score;
  };

  // the evaluation function gives a score to a board, from the point of view
  // of the player who has just played
  using evaluation_function_t=std::function<score_t (Board const &)>;

  // "params_t" is a list of parameters to parameterise an evaluation function
  struct param_t { std::string arg, value; };
  using params_t=std::list<param_t>;
  params_t parse(std::string params_s);

  class EvaluationMap;

  // parameterisable family of evaluation functions, with a name (for the
  // evaluation dictionary) and a help text
  class Evaluation {
  public:
    // "get_t" is a function that takes a list of parameters and returns a
    // parameterised evaluation function
    using get_t=std::function<evaluation_function_t (params_t)>;

    // if an evaluation map is specified, the "Evaluation" adds itself to it:
    Evaluation(std::string name, std::string help, get_t get);
    Evaluation(std::string name, std::string help, get_t get,
               EvaluationMap &evaluation_map);

    std::string const name;
    std::string const help;

    // get a parameterised evaluation function from a list of parameters
    evaluation_function_t operator()(std::string params_s="") const
      { return get(parse(params_s)); }
  private:
    get_t const get;
  };

  // a dictionary for evaluations
  class EvaluationMap {
    friend class Evaluation;
    void insert(Evaluation);
    // list of evaluation names
  public:
    std::list<std::string> list() const;
    bool contains(std::string name) const;
    // help text for given evaluation
    std::string help(std::string name) const;
    // syntax: "name:arg_1:flag_2:option_3=value_4:..."; "name" alone is ok
    evaluation_function_t operator()(std::string name_and_params) const;
  private:
    Evaluation find(std::string name) const;
    std::map<std::string, Evaluation> evaluations;
  };

  class Player {
  public:
    Player(std::string name) : name(name) { }
    virtual ~Player()=default;
    std::string const name;
    virtual MoveScore get_move(Game const &g)=0;
  };

  using random_generator_t=std::minstd_rand;

  std::string const default_params_s=
    "method=p:l=2:bd=6:bs=.5:be=.1:rs=21:rd=.01:rm=2.5";
  struct AlgorithmParams {
    // syntax "method=<value>:<param>=<value>:...
    //   method: [wptm] (whole_tree, pruning (default),
    //     pruning_and_transposition, monte-carlo)
    //   wi: initial search window width (window.init)
    //   wf: window widening factor (window.factor)
    //   wn: number of windowed searches before going windoless (window.max_n)
    //   l: depth of thinking (level)
    //   bd: number of additional bold moves (boldness.depth)
    //   bs: additional boldness score (boldness.score)
    //   be: additional per-level emboldening (boldness.emboldening)
    //   rs: seed for random number generation (random.seed)
    //   rd: standard deviation of random evaluation (random.deviation)
    //   rm: maximum random deviation factor (random.max_factor)
    AlgorithmParams(std::string params_s="");
    enum class search_t
      { whole_tree, pruning, pruning_and_transposition, monte_carlo }
      search=search_t::pruning;
    struct window_t {
      score_t init=3.22f;
      float factor=1.42f;
      unsigned max_n=2;
    } window;

    int level=2;
    struct boldness_t {
      int depth=6;
      score_t score=.5;
      score_t emboldening=.1;
    } boldness;

    struct random_t {
      typename random_generator_t::result_type seed=21; // any number
      float deviation=.01;
      float max_factor=2.5;
    } random;
  };

  bool operator==(const AlgorithmParams &, const AlgorithmParams &);

  class ComputerPlayer
    : public Player {
  public:
    ComputerPlayer(
        std::string name,
        evaluation_function_t eval, std::string algo_params_s);
    MoveScore get_move(Game const &g) override;

    evaluation_function_t const eval;
    AlgorithmParams const algo_params;
  private:
    MoveScore get_move_tree_search(Game const &g);
    MoveScore get_move_monte_carlo(Game const &g);
    score_t last_best_score=0.; // for search windows
    random_generator_t random;
  };

}

#endif
