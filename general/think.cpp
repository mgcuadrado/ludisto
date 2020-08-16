#include "think.h"
#include "stats.h"
#include <map>
#include <unordered_map>
#include <limits>
#include <memory>
#include <iostream> // FIXME

using namespace std;

namespace sxako {

  Evaluation::Evaluation(string name, string help,
                         Evaluation::get_t get)
    : name(name), help(help), get(get) { }

  Evaluation::Evaluation(string name, string help,
                         Evaluation::get_t get,
                         EvaluationMap &evaluation_map)
    : Evaluation(name, help, get) { evaluation_map.insert(*this); }

  namespace {

    string const param_delim=":", value_delim="=";

  }

  params_t parse(string params_s) {
    params_t params;
    while (not params_s.empty()) {
      auto colon=params_s.find(param_delim);
      string param=params_s.substr(0, colon);
      auto equal=param.find(value_delim);
      params.push_back(
        {param.substr(0, equal),
         (equal==string::npos
          ? string()
          : param.substr(equal+value_delim.length()))});
      if (colon==string::npos)
        break;
      params_s.erase(0, colon+param_delim.length());
    }
    return params;
  }

  void EvaluationMap::insert(Evaluation e) {
    if (not evaluations.insert({e.name, e}).second)
      throw invalid_argument("couldn't add Evaluation \""+e.name+"\"");
  }

  list<string> EvaluationMap::list() const {
    std::list<string> result;
    for (auto const &ne: evaluations)
      result.push_back(ne.first);
    return result;
  }

  bool EvaluationMap::contains(string name) const
    { return evaluations.find(name) not_eq evaluations.end(); }

  string EvaluationMap::help(string name) const
    { return find(name).help; }

  evaluation_function_t
  EvaluationMap::operator()(std::string name_and_params) const {
    auto e=name_and_params.find(param_delim);
    string name=name_and_params.substr(0, e); // the first element is the name
    string params_s= // the rest is the parameters
      e==string::npos ? string() : name_and_params.substr(e+1);
    return find(name)(params_s);
  }

  Evaluation EvaluationMap::find(std::string name) const {
    if (contains(name))
      return evaluations.at(name);
    else
      throw invalid_argument("unknown evaluation \""+name+"\"");
  }

  namespace {

    template <typename T>
    auto read(T &t) { return [&t](string s) { t=from_text<T>(s); }; }

  }

  AlgorithmParams::AlgorithmParams(string params_s) {
    map<string, function<void (string)>> param_map={
      {"method",
       [this](string s) {
          if (s=="w") search=search_t::whole_tree;
          else if (s=="p") search=search_t::pruning;
          else if (s=="t") search=search_t::pruning_and_transposition;
          else if (s=="m") search=search_t::monte_carlo;
          else throw invalid_argument(
                       "wrong params (unknown method \""+s+"\")");
       }},
      {"wi", read(window.init)},
      {"wf", read(window.factor)},
      {"wn", read(window.max_n)},
      {"l", read(level)},
      {"bd", read(boldness.depth)},
      {"bs", read(boldness.score)},
      {"be", read(boldness.emboldening)},
      {"rs",
       [this](string s) {
          random.seed=
          s.empty()
          ? random_generator_t::result_type(random_device()())
          : from_text<random_generator_t::result_type>(s);
       }},
      {"rd", read(random.deviation)},
      {"rm", read(random.max_factor)},
    };
    params_t params=parse(params_s);
    for (auto p: params)
      if (not p.arg.empty()) {
        if (param_map.find(p.arg) not_eq param_map.end())
          param_map.find(p.arg)->second(p.value);
        else
          throw invalid_argument("wrong params (unknown param \""+p.arg+"\")");
      }
  }

  namespace {

    int inf_level=1000000;
    score_t const
      beyond_max_score=1e24,
      inf_score=beyond_max_score*inf_level;
    score_t nan_score=numeric_limits<score_t>::signaling_NaN();

    score_t flip(score_t s) {
      if (s<=beyond_max_score)
        return -s;
      else
        return s<0 ? -(s+beyond_max_score) : -(s-beyond_max_score);
    }

    // evaluate outcome from the point of view of the player who has just played
    score_t eval_outcome(Rules::Outcome o) {
      switch(o) {
      case Rules::Outcome::draw:
        return 0.;
      case Rules::Outcome::last_move_won:
        return +1.;
      case Rules::Outcome::last_move_lost:
        return -1.;
      default:
        return nan_score;
      }
    }

    enum class memo_flag { exact, lower, upper };
    using memoization_t=
      map<unsigned, unordered_map<string, pair<MoveScore, memo_flag>>>;
    using bottom_memoization_t=unordered_map<string, score_t>;

    using random_increment_f=function<score_t (string id)>;

    // the best move considering level 1 is the one that gives you the highest
    // evaluation; the best move considering level n (with n>1) is the one that
    // gives you the highest evaluation after your opponent plays his best
    // answer considering level n-1
    //
    // best_score(b, 0)=eval(b)
    // best_score(b, 1)=max(best_score(b, 0))=max(eval(b))
    // best_score(b, n>1)=max(-best_score(b, n-1))
    //
    // in order to have the recursive formula hold for n=1 too, we must modify
    // level 0 like this:
    //
    // best_score(b, 0)=-eval(b)
    // best_score(b, n>0)=max(-best_score(b, n-1))
    MoveScore find_best_move(Game const &g, Board const &b,
                             score_t current_score,
                             evaluation_function_t eval,
                             int level,
                             AlgorithmParams::boldness_t const &boldness,
                             score_t alpha, score_t beta,
                             AlgorithmParams const &p,
                             memoization_t &memo,
                             bottom_memoization_t &bottom_memo,
                             random_increment_f const &random_increment) {
#define if_transposition                                                     \
      if (p.search==AlgorithmParams::search_t::pruning_and_transposition)
#define if_pruning                                                           \
      if (p.search==AlgorithmParams::search_t::pruning                       \
          or p.search==AlgorithmParams::search_t::pruning_and_transposition)

      assert(level>0); // at level==1 we don't recurse down; see below

      /// score_t alpha_orig=alpha;
      string b_id;
      score_t alpha_orig=alpha;

      if_transposition {
        b_id=b.id();
        auto memo_it=memo[level].find(b_id);
        if (memo_it not_eq memo[level].end()) {
          // found=true;
          auto move_score=memo_it->second.first;
          switch (memo_it->second.second) {
          case memo_flag::exact: return move_score;
          case memo_flag::upper: beta=min(beta, move_score.score); break;
          case memo_flag::lower: alpha=max(alpha, move_score.score); break;
          }
          if (alpha>=beta)
            return move_score;
        }
      }

      MoveScore
        current_best{Move(), -inf_score},
        current_best_immediate=current_best;

      score_t bold_score_threshold=-inf_score;
      if (level<=boldness.depth) {
        bold_score_threshold=
          current_score
          +boldness.score+boldness.emboldening*(boldness.depth-level);
        // no-action is better than any bold action in a quiescent situation:
        current_best.score=current_score;
      }

      auto outcome=g.outcome(b);
      if (outcome not_eq Rules::Outcome::playing)
        current_best.score=flip(eval_outcome(outcome)*inf_score);
      else {
        Moves all_moves=g.legal_moves(b);
        if (all_moves.empty())
          throw logic_error("can't move");

        Board nb(g.rules.data_spec);
        string nb_id;

        // all moves-and-score's, except unbold if already in the bold moves
        // levels:
        list<MoveScore> all_moves_with_scores;
        for (auto m: all_moves) {
          nb=b;
          g.move(nb, m);
          nb_id=nb.id();
          score_t move_score;
          if_transposition {
            auto memo_it=bottom_memo.find(nb_id);
            if (memo_it not_eq bottom_memo.end())
              move_score=flip(memo_it->second);
            else {
              move_score=eval(nb)+random_increment(nb_id);
              ++n_quick_evaluations;
              bottom_memo[nb_id]=flip(move_score);
            }
          }
          else {
            move_score=eval(nb)+random_increment(nb_id);
            ++n_quick_evaluations;
          }
          if (move_score>=bold_score_threshold)
            all_moves_with_scores.push_back({m, move_score});
          if (move_score>=current_best_immediate.score)
            current_best_immediate={m, move_score};
        }

        if (level==1 or all_moves_with_scores.empty())
          // we've already got the best move:
          current_best=current_best_immediate;
        else {
          all_moves_with_scores.sort(
            [](MoveScore const &a, MoveScore const &b)
              { return a.score>b.score; }); // descending scores
          for (auto ms: all_moves_with_scores) { // from highest to lowest score
            Move m=ms.move;
            nb=b;
            g.move(nb, m);
            score_t ns=
              flip(find_best_move(g, nb, flip(ms.score), eval,
                                  level-1, boldness,
                                  -beta, -alpha,
                                  p, memo, bottom_memo, random_increment)
                   .score);

            if (ns>current_best.score)
              current_best={m, ns};

            if_pruning {
              alpha=max(current_best.score, alpha);
              if (alpha>=beta)
                break;
            }
          }
        }
      }

      if_transposition {
        memo_flag mf=
          current_best.score<=alpha_orig
          ? memo_flag::upper
          : current_best.score>=beta
          ? memo_flag::lower
          : memo_flag::exact;
        memo[level][b_id]={current_best, mf};
      }

      return current_best;

#undef if_pruning
#undef if_transposition
    }

    using random_number_t=function<size_t (size_t)>;

    score_t play_random_to_the_end(Game const &g, Board &b,
                                   random_number_t const &random_number,
                                   size_t &moves_left,
                                   int factor=+1) {
      auto outcome=g.outcome(b);
      if (outcome not_eq Rules::Outcome::playing)
        return factor*eval_outcome(outcome);

      if (moves_left)
        --moves_left;
      Moves all_moves=g.legal_moves(b);
      Move m=*next(all_moves.begin(), random_number(all_moves.size()));
      g.move(b, m);
      return play_random_to_the_end(g, b, random_number, moves_left, -factor);
    }

    namespace MonteCarlo {

      struct move_tree_node_t {
        Move const move;
        Board const resulting_board;
        move_tree_node_t *const parent=nullptr;
        score_t total_score=0.;
        size_t n_simulations=0;
        shared_ptr<vector<move_tree_node_t>> children=nullptr;
        size_t n_unexpanded_children=0;
      };

    }

    void print_tree(Game const &g,
                    MonteCarlo::move_tree_node_t const &n, int level=0) {
      if (n.n_simulations) {
        for (auto i: loop(level))
          cout << "  ";
        cout << " " << g.write_move(n.move) << ": "
             << n.total_score << "/" << n.n_simulations << " "
             << n.total_score/n.n_simulations << endl;
        if (n.children)
          for (auto c: *n.children)
            print_tree(g, c, level+1);
      }
    }

    template <typename URGB>
    MoveScore monte_carlo_best_move(Game const &g,
                                    random_number_t const &random_number,
                                    URGB &&random,
                                    size_t n_moves) {
      using namespace MonteCarlo;
      move_tree_node_t root{Move(), g.board()};
      size_t moves_left=n_moves;
      while (moves_left) {
        /// selection
        move_tree_node_t *node=&root;
        // wander down the tree at random
        while (node->children and not node->n_unexpanded_children)
          node=&(*node->children)[random_number(node->children->size())];
        /// expansion
        auto outcome=g.outcome(node->resulting_board);
        score_t score;
        if (outcome not_eq Rules::Outcome::playing)
          score=eval_outcome(outcome);
        else {
          if (not node->children) {
            Moves all_moves=g.legal_moves(node->resulting_board);
            assert(not all_moves.empty()); // because "outcome==playing"
            // shuffle moves
            vector<Move> // a list can't be shuffled: random access
              all_moves_shuffled(all_moves.begin(), all_moves.end());
            shuffle(all_moves_shuffled.begin(), all_moves_shuffled.end(),
                    random);
            // create all children
            node->n_unexpanded_children=all_moves_shuffled.size();
            node->children=make_shared<vector<move_tree_node_t>>();
            node->children->reserve(node->n_unexpanded_children);
            for (Move const &m: all_moves_shuffled) {
              Board b=node->resulting_board;
              // FIXME: we could evaluate outcome here for each child
              g.move(b, m);
              node->children->push_back({m, b, node});
            }
          }
          // visit first unexplored child (from the end because it's easier):
          assert(node->n_unexpanded_children);
          node=&(*node->children)[--node->n_unexpanded_children];
          /// simulation
          Board b_play=node->resulting_board;
          score=play_random_to_the_end(g, b_play, random_number, moves_left);
        }
        /// backpropagation
        int factor=1;
        while (node) {
          node->total_score+=factor*score;
          ++node->n_simulations;
          node=node->parent;
          factor=-factor;
        }
      }

      // best move
      score_t max=numeric_limits<score_t>::lowest();
      Move best;
      for (auto mns: *root.children) {
        auto s=mns.total_score/mns.n_simulations;
        if (s>max) {
          max=s;
          best=mns.move;
        }
      }
      // debugging info:
      print_tree(g, root);

      return {best, max};
    }

  }

  bool operator==(const AlgorithmParams &a, const AlgorithmParams &b) {
#define attributes(p)                                                    \
    p.search, p.window.init, p.window.factor, p.window.max_n,            \
    p.level, p.boldness.depth, p.boldness.score, p.boldness.emboldening, \
    p.random.seed, p.random.deviation, p.random.max_factor
    return make_tuple(attributes(a))==make_tuple(attributes(b));
#undef attributes
  }

  ComputerPlayer::ComputerPlayer(
      string name,
      evaluation_function_t eval, string algo_params_s)
    : Player(name),
      eval(eval), algo_params(algo_params_s),
      random(algo_params.random.seed) { }

  MoveScore ComputerPlayer::get_move_tree_search(Game const &g) {
    normal_distribution<score_t> distr(0., 1.);
    auto random_seed=random(); // a new xor-seed for the whole move computation
    auto random_increment=
      [this, random_seed, &distr](string id) {
        auto id_hash=hash<string>()(id);
        random_generator_t this_id_random(random_seed xor id_hash);
        distr.reset(); // otherwise, the random depends on "distr" history
        while (true) {
          score_t result=distr(this_id_random);
          if (abs(result)<algo_params.random.max_factor)
            return result*algo_params.random.deviation;
        }
      };

    score_t
      alpha=last_best_score-algo_params.window.init/2.,
      beta=last_best_score+algo_params.window.init/2.;
    MoveScore result;
    bottom_memoization_t bottom_memo;
    for (unsigned window=0; window<=algo_params.window.max_n; ++window) {
      // the memoisation must restart for each window, since the window affects
      // the computed scores (not so for the bottom memoisation):
      memoization_t memo;
      if (window==algo_params.window.max_n) {
        alpha=-inf_score;
        beta=+inf_score;
      }
      result=find_best_move(g, g.board(), 0., eval,
                            algo_params.level+algo_params.boldness.depth,
                            algo_params.boldness,
                            alpha, beta,
                            algo_params, memo, bottom_memo, random_increment);

      for (auto const &m: memo)
        max_transposition_table_size[m.first]=
          max(max_transposition_table_size[m.first], m.second.size());

      if (result.score>=beta)
        beta=alpha+algo_params.window.factor*(beta-alpha);
      else if (result.score<=alpha)
        alpha=beta+algo_params.window.factor*(alpha-beta);
      else
        break;
    }
    last_best_score=result.score;
    return result;
  }

  MoveScore ComputerPlayer::get_move_monte_carlo(Game const &g) {
    auto random_number=[this](size_t n)
      { return uniform_int_distribution<size_t>(0, n-1)(random); };
    return monte_carlo_best_move(g, random_number, random, algo_params.level);
  }

  MoveScore ComputerPlayer::get_move(Game const &g) {
    if (algo_params.search==AlgorithmParams::search_t::monte_carlo)
      return get_move_monte_carlo(g);
    else
      return get_move_tree_search(g);
  }

}
