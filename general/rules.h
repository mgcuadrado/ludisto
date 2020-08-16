#ifndef SXAKO_GENERAL_RULES_HEADER_
#define SXAKO_GENERAL_RULES_HEADER_

#include <vector>
#include <initializer_list>

namespace sxako {

  template <typename S, typename MF>
  class MovesDictionary {
  public:
    using SU=std::underlying_type_t<S>; // FIXME: generalise for non-enum?
    MovesDictionary<S, MF> &set(S s, MF mf) {
      auto su=SU(s);
      map.resize(std::max(size_t(su+1), map.size()));
      map[su]=mf;
      return *this;
    }
    MovesDictionary<S, MF> &set(std::initializer_list<S> ss, MF mf)
      { for (auto s: ss) set(s, mf); return *this; }
    MF operator[](S s) const {
      auto su=SU(s);
      assert(su<map.size());
      return map[su];
    }
  private:
    std::vector<MF> map;
  };

}

#endif
