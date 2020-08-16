#ifndef SXAKO_GRID_HEADER_
#define SXAKO_GRID_HEADER_

#include "base.h"
#include <cstring>
#include <type_traits>
#include <cassert>

namespace sxako {

  // data items can be single values or sets of values arranged in a grid-like
  // fashion; data items have a type ("cell_t") and an addressing type
  // ("coord_t", an index into the data item, analogous to the coordinates in a
  // regular grid); data items are grouped together in a number of common
  // repositories represented by the class "Data<kind>", which is _not_
  // templatised on "cell_t" or "coord_t"; that means all data items of the
  // same kind (see below) are usually kept together in the same "Data" object,
  // no matter what their "cell_t" or "coord_t" are; so, for instance, for a
  // simple game, all the data items that describe the state (i.e. current
  // situation of the game) are kept in a "Data<Kind::state>", whereas cached
  // data items to speed up computations are kept in a "Data<Kind::cache>";
  // since different data items reside in the same data repository, the
  // accesses (the layout of the data items in the data repository) must be
  // coordinated; this is achieved by the "DataSpec<kind>" class; the following
  // classes define the grid concept:
  //
  //   * "Kind" states the intended usage of the data; data with different
  //     "Kind" values get different treatments;
  //
  //   * "Data<kind>" is a common data repository for any number of data items
  //
  //   * "DataSpec<kind>" tallies the data item entries for a data repository;
  //     this organises the data repository layout automatically
  //
  //   * "Addressing<kind, Indexing, cell_t>" is data item addressing, a way to
  //     reach into a data repository to retrieve a given data item;

  enum class Kind {
    state, // what distinguishes one situation from any other
    manag, // management, meta-state information that doesn't affect best move
           // determination but does affect other aspects of the game, such as
           // outcome (through position repetition); may include turn number
    cache, // regenerable from the state, used to speed up computations
    table  // static, constant information used to evaluate positions
  };

  // a "DataSpec<kind>" is accessed sequentially by different data item
  // addresings, each of which informs the "DataSpec<>" of its own size and
  // gets an data repository access offset; when creating a "Data<kind>", its
  // size is given by the matching "DataSpec<kind>"; once a "DataSpec<>" is
  // read, it becomes "finalised", and it won't accept any additional data item
  // addressing; all sizes at this level are in bytes
  template <Kind kind>
  class DataSpec {
  public:
    // this returns the total tallied size and finalises the "DataSpec<>" if it
    // wasn't already done
    index_t char_size() const {
      finalized=true;
      return current_char_size;
    }
  private:
    // a data item addressing gives "get_offset()" its own size, and gets the
    // offset into the data repository
    index_t get_offset(index_t char_storage) {
      if (finalized)
        throw std::logic_error(
                "attempted to add more storage to a finalized "
                +type_name(*this)+"\"");
      index_t offset=current_char_size;
      current_char_size+=char_storage;
      return offset;
    }
    template <Kind kind_, typename Indexing, typename CellT>
    friend class Addressing;
    mutable bool finalized=false;
    index_t current_char_size=0;
  };

  // "Addressing<kind, Indexing, cell_t>" is a wrapping around "Indexing", that
  // manages the communication with "DataSpec<kind>", and the offset when
  // accessing the data item; "Indexing" is not templatised on "cell_t'
  template <Kind kind, typename Indexing, typename CellT>
  class Addressing
    : public Indexing {
  public:
    // required from Indexing:
    //     typedef coord_t;                // addressing type
    //     index_t const cell_storage;     // size of store measured in cells
    //     bool contains(coord_t c) const; // does "c" belong to the data item?
    //     index_t relative_cell_index(coord_t c) const; // measured in cells
    using cell_t=CellT;
    using Indexing::coord_t;
    using Indexing::cell_storage;
    using Indexing::contains;
    using Indexing::relative_cell_index;
    template <typename... A>
    Addressing(DataSpec<kind> &ds, A &&...a)
      : Indexing(a...),
        char_storage(cell_storage*sizeof(cell_t)),
        offset(ds.get_offset(char_storage)) { }
    index_t address(typename Indexing::coord_t const &c) const {
      assert(contains(c));
      assert(relative_cell_index(c)<char_storage);
      return rel_to_abs(relative_cell_index(c));
    }
  private:
    index_t rel_to_abs(index_t rel) const { return offset+rel*sizeof(cell_t); }
    index_t const char_storage;
    index_t const offset;
  };

  // "SingleVar" is an indexing for a single variable; it could have been (and
  // indeed was, at the beginning) defined as a specialisation of
  // "StraightIndexing" (see "straight") for "dim=0", but defining it here
  // simplifies the code to get the syntax shortcut "data(addressing)"
  // returning a "cell_t" (see below)
  class SingleVar {
  public:
    using coord_t=std::tuple<>;
    index_t const cell_storage=1;
    bool contains(coord_t) const { return true; }
    index_t relative_cell_index(coord_t) const { return 0; }
  };

  // "bool_cell<cell_t>" is used to check if a cell is contained in an
  // addressing, and if so, get its contents, in one go; it is implicitly
  // convertible to bool, so it can be used like this:
  //
  //     if (auto contained_cell=d(a).get_if_contains(location))
  //       if (contained_cell.cell==attacker)
  //         ...
  template <typename CellT>
  struct bool_cell {
    bool_cell(bool contained) : contained(contained) { }
    bool_cell(bool contained, CellT cell)
      : contained(contained), cell(cell) { }
    bool contained;
    CellT cell;
    operator bool() const { return contained; }
  };

  // data repository for a given "Kind"; "Data<kind>" can be created from a
  // "DataSpec<kind>", or as a copy of another "Data<kind>"; it also supports
  // assignment; the syntax to access a cell of a data item from a data
  // repository is "d(a)[c]" where "Data<kind> d" is the data repository,
  // "Addressing<...> a" is the data item addressing, and "c" is the index of
  // the cell within that data item; for single-variable data items (with
  // "SingleVar" indexing), the syntax is simply "d(a)"; "d(a).contains(c)"
  // returns the same value as "a.contains(c)", but parallels the "d(a)[c]"
  // syntax; both pieces of info ("d(a).contains(c)" and "d(a)[c]") can be
  // accessed at the same time, using "d(a).get_if_contains(c)" (see
  // "bool_cell<cell_t>" above), or "d(a).get_if_contains(c, cell)"; naturally,
  // the cell info is not returned or set if the index is not contained;
  // finally, if the data item addressing is in the form of a pointer, you can
  // still use "d(a)"
  template <Kind kind>
  class Data {
    template <typename DataContents, typename AddrT>
    friend class Access;
  public:
    // intermediate object to support the syntax "data(addr)[coord]":
    template <typename DataContents, typename AddrT>
    class Access {
      friend class Data;
      Access(DataContents &data, AddrT const &addr) : data(data), addr(addr) { }
      DataContents &data;
    public:
      using addr_t=AddrT;
      addr_t const &addr;
      using coord_t=typename addr_t::coord_t;
      using cell_t=typename addr_t::cell_t;

      auto &operator[](coord_t const &c)
        { return data.at(addr, c); }
      auto const &operator[](coord_t const &c) const
        { return data.at(addr, c); }
      bool contains(coord_t const &c) const
        { return addr.contains(c); } // "data" not used, but parallels "[]"
      bool_cell<cell_t> get_if_contains(coord_t const &c) const {
        // turns out "relative_cell_index()" is so fast it doesn't pay off to
        // avoid calling it twice (from "contains()" and from "operator[]()"
        // below)
        if (contains(c))
          return {true, operator[](c)};
        else
          return {false};
      }
      auto get_if_contains(coord_t const &c, cell_t &cell) const {
        // ditto
        if (contains(c))
          return cell=operator[](c), true;
        else
          return false;
      }
    };

    Data(DataSpec<kind> const &ds)
      : size(ds.char_size()), data(new char[size]()) { }
    Data(Data<kind> const &d)
      : size(d.size), data(new char[size]) { memcpy(data, d.data, size); }
    ~Data() { delete[] data; }
    Data &operator=(Data<kind> const &d) {
      assert(size==d.size);
      memcpy(data, d.data, size);
      return *this;
    }

    // a dump of the raw contents of the data repository
    std::string id() const { return std::string(data, size); }

    // for a single var ("SingleVar"), return the cell directly...:
    template <Kind k, typename cell_t>
    cell_t const &operator()(Addressing<k, SingleVar, cell_t> const &a) const
      { return at(a, SingleVar::coord_t()); }
    template <Kind k, typename cell_t>
    cell_t &operator()(Addressing<k, SingleVar, cell_t> const &a)
      { return at(a, SingleVar::coord_t()); }

    // ... otherwise, return an intermediate object supporting // "operator[]":
    template <Kind k, typename I, typename cell_t>
    Access<Data<kind> const, Addressing<k, I, cell_t>>
    operator()(Addressing<k, I, cell_t> const &a) const { return {*this, a}; }
    template <Kind k, typename I, typename cell_t>
    Access<Data<kind>, Addressing<k, I, cell_t>>
    operator()(Addressing<k, I, cell_t> const &a) { return {*this, a}; }

    // i'll indulge into some petty syntactic sugar, for the case where we have
    // a pointer (smart or dumb) to an addressing rather than the addressing
    // itself
    template <typename Ptr, typename=decltype(*std::declval<Ptr>())>
    decltype(auto) operator()(Ptr const &p) const { return operator()(*p); }
    template <typename Ptr, typename=decltype(*std::declval<Ptr>())>
    decltype(auto) operator()(Ptr const &p) { return operator()(*p); }

  private:
    template <typename Indexing, typename CellT, typename C>
    CellT const &at(Addressing<kind, Indexing, CellT> const &a, C c) const
      { return reinterpret_cast<CellT const &>(data[a.address(c)]); }
    template <typename Indexing, typename CellT, typename C>
    CellT &at(Addressing<kind, Indexing, CellT> const &a, C c)
      { return variable(constant(*this).at(a, c)); }
    index_t const size;
    char *const data;
  };

  // compare by comparing the dumps of their raw contents
  template <Kind kind>
  bool operator==(Data<kind> const &a, Data<kind> const &b)
    { return a.id()==b.id(); }

  // "DataStore<...>" is syntactic sugar for a group of data repositories for a
  // list of "Kind" values (one repository per kind); the matching group of
  // data specs is "DataStoreSpec<...>"; in order to avoid defining the list of
  // kinds twice, "DataStoreSpec<...>" is templatised on the kinds, but
  // "DataStore<...>" is templatised no "DataStoreSpec<...>"; for a
  // "DataStore<...> ds", its "Data<k>" is selected by "ds.data<k>()"; see
  // "board" for an example of usage

  template <Kind...>
  struct DataStoreSpec { };

  template <Kind fk, Kind... rk> // "f": first; "r": rest
  struct DataStoreSpec<fk, rk...> {
    DataStoreSpec(DataSpec<fk> const &fds, DataSpec<rk> const &...rds)
      : fds(fds), rds(rds...) { }
    DataSpec<fk> fds;
    DataStoreSpec<rk...> rds;
  };

  template <typename>
  class DataStore;

  template <>
  class DataStore<DataStoreSpec<>> {
  public:
    DataStore(DataStoreSpec<> const &) { }
  };

  template <Kind fk, Kind... rk> // "f": first; "r": rest
  class DataStore<DataStoreSpec<fk, rk...>>
    : public DataStore<DataStoreSpec<rk...>> {
    using parent=DataStore<DataStoreSpec<rk...>>;
  public:
    DataStore(DataStoreSpec<fk, rk...> const &dss)
      : parent(dss.rds), fd(dss.fds) { }

    // match: return this data
    template <Kind k>
    std::enable_if_t<k==fk, Data<k> &> data() { return fd; }
    template <Kind k>
    std::enable_if_t<k==fk, Data<k> const &> data() const { return fd; }

    // no match: relay to parent
    template <Kind k>
    std::enable_if_t<k not_eq fk, Data<k> &> data()
      { return parent::template data<k>(); }
    template <Kind k>
    std::enable_if_t<k not_eq fk, Data<k> const &> data() const
      { return parent::template data<k>(); }
  private:
    Data<fk> fd;
  };

  /// board containedness functions and their algebra

  // for each coordinate (within a specified range), state whether it is
  // contained in the board
  template <typename coord_t>
  using contains_t=std::function<bool (coord_t)>;

  // FIXME: if i turn "all()" and "none()" into templated variables, there's a
  // memory problem; if i solve it, check also odd and even

  // all coordinates
  template <typename coord_t>
  contains_t<coord_t> all() { return [](coord_t) { return true; }; }
  // no coordinate
  template <typename coord_t>
  contains_t<coord_t> none() { return [](coord_t) { return false; }; }

  namespace Impl {
    inline bool is_in_rectangle_tuple(std::tuple<>, std::tuple<>)
      { return true; }
    template <typename C, typename R>
    bool is_in_rectangle_tuple(C c, R r) {
      return
        is_in_range(get<0>(c), get<0>(r))
        and is_in_rectangle_tuple(tuple_rest(c), tuple_rest(r));
    }
  }
  // a rectangle (any-dimensional rectangular range) contained between two
  // coordinates (inclusive)
  template <typename coord_t, typename... T>
  contains_t<coord_t>
  rectangle(std::tuple<T...> r) { // r is a tuple of pairs
    return [r](coord_t c)
      { return Impl::is_in_rectangle_tuple(c, r); };
  }

  template <typename coord_t, typename... T, typename... U>
  contains_t<coord_t> rectangle(std::pair<T, U>... p) // individual ranges
    { return rectangle<coord_t>(std::make_tuple(p...)); }

  // a single cell
  template <typename coord_t>
  contains_t<coord_t>
  single(coord_t s) { return [s](coord_t c) { return s==c; }; }

  namespace Impl {
    inline s_index_t sum_tuple(std::tuple<>) { return 0; }
    template <typename coord_t>
    s_index_t sum_tuple(coord_t c)
      { return s_index_t(std::get<0>(c))+sum_tuple(tuple_rest(c)); }
  }
  // coordinates whose sum of elements is odd
  template <typename coord_t>
  contains_t<coord_t> odd()
    { return [](coord_t c) { return Impl::sum_tuple(c)%2==1; }; }
  // coordinates whose sum of elements is even
  template <typename coord_t>
  contains_t<coord_t> even()
    { return [](coord_t c) { return Impl::sum_tuple(c)%2==0; }; }

  // union: "a+b" (coordinates in "a" or in "b", or in both)
  template <typename coord_t>
  contains_t<coord_t> operator+(contains_t<coord_t> a, contains_t<coord_t> b)
    { return [a, b](coord_t c) { return a(c) or b(c); }; }
  // intersection: "a*b" (coordinates that are both in "a" and in "b")
  template <typename coord_t>
  contains_t<coord_t> operator*(contains_t<coord_t> a, contains_t<coord_t> b)
    { return [a, b](coord_t c) { return a(c) and b(c); }; }
  // removal: "a-b" (coordinates in "a" but not in "b")
  template <typename coord_t>
  contains_t<coord_t> operator-(contains_t<coord_t> a, contains_t<coord_t> b)
    { return [a, b](coord_t c) { return a(c) and not b(c); }; }

}

#endif
