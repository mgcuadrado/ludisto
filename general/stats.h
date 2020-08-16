#ifndef SXAKO_STATS_HEADER_
#define SXAKO_STATS_HEADER_

#include "base.h"
#include <map>

namespace sxako {

  extern long unsigned n_evaluations, n_quick_evaluations;
  extern std::map<unsigned, long unsigned> max_transposition_table_size;

}

#endif
