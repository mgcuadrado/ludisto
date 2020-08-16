#ifndef SXAKO_GENERAL_ANY_HEADER_
#define SXAKO_GENERAL_ANY_HEADER_

#include <ludisto/general/base.h>
#include <memory>

namespace sxako {

  /// small "any" implementation (from "oxys/base/any.h")

  namespace Impl {
    template <typename=void> struct any_holder_t;
    template <> struct any_holder_t<> {
      virtual ~any_holder_t()=default;
      virtual std::shared_ptr<any_holder_t> clone() const=0;
    };
    template <typename T> struct any_holder_t {
      any_holder_t(T const &val) : val(val) { }
      virtual std::shared_ptr<any_holder_t> clone() const override
        { return std::make_shared<any_holder_t<T>>(val); }
      T val;
    };
  }

  class any {
  public:
    template <typename T> any(T const &t)
      : holder(std::make_shared<Impl::any_holder_t<T>>(t)) { }
    any(any const &a) : holder(a.clone_holder()) { }
    any &operator=(any const &a) { holder=a.clone_holder(); return *this; }
    operator bool() const { return bool(holder); }
    template <typename T> bool is() const { return bool(cast_holder<T>()); }
    template <typename T> T val() const { return cast_check_holder<T>()->val; }
  private:
    std::shared_ptr<Impl::any_holder_t<>> clone_holder() const
      { return holder ? holder->clone() : holder; }
    template <typename T>
    std::shared_ptr<Impl::any_holder_t<T>> cast_holder() const
      { return std::dynamic_pointer_cast<Impl::any_holder_t<T>>(holder); }
    template <typename T>
    std::shared_ptr<Impl::any_holder_t<T>> cast_check_holder() const {
      auto result=cast_holder<T>();
      if (result)
        return result;
      else
        throw std::runtime_error(
          "\"any<>\" couldn't cast its value to \""+type_name<T>()+"\"");
    }

    std::shared_ptr<Impl::any_holder_t<>> holder;
  };

}

#endif
