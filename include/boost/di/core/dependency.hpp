//
// Copyright (c) 2012-2015 Krzysztof Jusiak (krzysztof at jusiak dot net)
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef BOOST_DI_CORE_DEPENDENCY_HPP
#define BOOST_DI_CORE_DEPENDENCY_HPP

#include "boost/di/aux_/utility.hpp"
#include "boost/di/core/array.hpp"
#include "boost/di/scopes/exposed.hpp"
#include "boost/di/scopes/instance.hpp"
#include "boost/di/scopes/deduce.hpp"
#include "boost/di/concepts/scopable.hpp"
#include "boost/di/fwd.hpp"

namespace core {

BOOST_DI_HAS_TYPE(is_injector, deps);

template <class T>
struct array_type {
  using type = T;
};

template <class T>
struct array_type<T* []> {
  using type = T* [];
};

template <class T>
struct array_type<T[]> {
  using type = T* [];
};

template <class T>
using array_type_t = typename array_type<T>::type;

template <class, class>
struct dependency_concept {};

template <class T, class TDependency>
struct dependency_impl : aux::pair<T, TDependency> {};

template <class... Ts, class TName, class TDependency>
struct dependency_impl<dependency_concept<concepts::any_of<Ts...>, TName>, TDependency>
    : aux::pair<dependency_concept<Ts, TName>, TDependency>... {};

struct override {};

struct dependency_base {};

template <class TScope, class TExpected, class TGiven = TExpected, class TName = no_name,
          class TPriority = aux::none_type>
class dependency : dependency_base,
                   TScope::template scope<TExpected, TGiven>,
                   public dependency_impl<dependency_concept<TExpected, TName>,
                                          dependency<TScope, TExpected, TGiven, TName, TPriority>> {
  template <class, class, class, class, class>
  friend class dependency;
  using scope_t = typename TScope::template scope<TExpected, TGiven>;

  template <class T>
  using externable = aux::integral_constant<bool, !is_injector<aux::remove_reference_t<T>>::value &&
                                                      aux::is_same<TScope, scopes::deduce>::value &&
                                                      aux::is_same<TExpected, TGiven>::value>;

  template <class T>
  using specific = aux::integral_constant<bool, is_injector<T>::value || aux::is_array<TExpected, T>::value>;

  template <class T>
  struct ref_traits {
    using type = T;
  };

  template <int N>
  struct ref_traits<const char(&)[N]> {
    using type = TExpected;
  };

  template <class T>
  struct ref_traits<std::shared_ptr<T>&> {
    using type = std::shared_ptr<T>;
  };

 public:
  using scope = TScope;
  using expected = TExpected;
  using given = TGiven;
  using name = TName;
  using priority = TPriority;

  dependency() noexcept {}

  template <class T>
  explicit dependency(T&& object) noexcept : scope_t(static_cast<T&&>(object)) {}

  template <class TScope_, class TExpected_, class TGiven_, class TName_, class TPriority_>
  explicit dependency(const dependency<TScope_, TExpected_, TGiven_, TName_, TPriority_>& other) noexcept
      : scope_t(other) {}

  template <class T, BOOST_DI_REQUIRES(aux::is_same<TName, no_name>::value && !aux::is_same<T, no_name>::value) = 0>
  auto named() noexcept {
    return dependency<TScope, TExpected, TGiven, T, TPriority>{*this};
  }

  template <class T, BOOST_DI_REQUIRES(aux::is_same<TName, no_name>::value && !aux::is_same<T, no_name>::value) = 0>
  auto named(const T&) noexcept {
    return dependency<TScope, TExpected, TGiven, T, TPriority>{*this};
  }

  template <class T, BOOST_DI_REQUIRES_MSG(concepts::scopable<T>) = 0>
  auto in(const T&) noexcept {
    return dependency<T, TExpected, TGiven, TName, TPriority>{};
  }

  template <class T, BOOST_DI_REQUIRES(!specific<T>::value) = 0,
            BOOST_DI_REQUIRES_MSG(concepts::boundable<TExpected, T>) = 0>
  auto to() noexcept {
    return dependency<TScope, TExpected, T, TName, TPriority>{};
  }

  template <class... Ts, BOOST_DI_REQUIRES(aux::is_array<TExpected, Ts...>::value) = 0>
  auto to() noexcept {
    return dependency<TScope, array<array_type_t<TExpected>>, array<array_type_t<TExpected>, Ts...>, TName,
                      TPriority>{};
  }

  template <class T, BOOST_DI_REQUIRES_MSG(concepts::boundable<array_type_t<TExpected>, T>) = 0>
  auto to(std::initializer_list<T>&& object) noexcept {
    using dependency =
        dependency<scopes::instance, array<array_type_t<TExpected>>, std::initializer_list<T>, TName, TPriority>;
    return dependency{object};
  }

  template <class T, BOOST_DI_REQUIRES(externable<T>::value && !aux::is_narrowed<TExpected, T>::value) = 0,
            BOOST_DI_REQUIRES_MSG(concepts::boundable<TExpected, aux::decay_t<T>>) = 0>
  auto to(T&& object) noexcept {
    using dependency = dependency<scopes::instance, TExpected, typename ref_traits<T>::type, TName, TPriority>;
    return dependency{static_cast<T&&>(object)};
  }

  template <class T, BOOST_DI_REQUIRES(is_injector<T>::value) = 0>
  auto to(const T& object = {}) noexcept {
    using dependency = dependency<scopes::exposed<TScope>, TExpected, T, TName, TPriority>;
    return dependency{object};
  }

  template <class...>
  dependency& to(...) const noexcept;

  auto operator[](const override&) noexcept { return dependency<TScope, TExpected, TGiven, TName, override>{*this}; }

#if defined(__cpp_variable_templates)  // __pph__
  dependency& operator()() noexcept { return *this; }
#endif  // __pph__

 protected:
  using scope_t::is_referable;
  using scope_t::create;
  using scope_t::try_create;
  template <class>
  static void try_create(...);
};

template <class T>
struct is_dependency : aux::is_base_of<dependency_base, T> {};

}  // core

#endif
