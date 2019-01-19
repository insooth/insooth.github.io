

#include <iostream>


// see LICENSE on insooth.github.io

#include <tuple>  // apply, make_tuple, tuple_cat
#include <type_traits>  // is_same, remove_reference
#include <utility>  // forward


template<template<class> class P, class T>
constexpr decltype(auto) apply_if_impl(T&& t)
{
    if constexpr (P<std::remove_reference_t<T>>::value)
        return std::make_tuple(std::forward<T>(t));
    else
        return std::make_tuple();
}

template<template<class> class P, class F, class... As>
constexpr decltype(auto) apply_if(F&& f, As&&... args)
{
    return std::apply
            ( std::forward<F>(f)
            , std::tuple_cat(apply_if_impl<P>(std::forward<As>(args))...) );
}

using U = int;
using V = unsigned;

template<class T>
using P = std::disjunction<std::is_same<U, T>, std::is_same<V, T>>;

int main()
{
    // gives 4
    std::cout << apply_if<P>([](U a, V, U b) { return a + b; }
                            , float{}, U{1}, V{2}, bool{}, U{3}, char{});

    return 0;
}
