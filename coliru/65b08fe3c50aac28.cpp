
// see LICENSE on insooth.github.io

#include <iostream>  // cout

#include <optional>  // optional
#include <vector>  // vector


#include <tuple>  // get, tuple
#include <type_traits>  // remove_reference



template<template<class...> class S>
struct stringify_context;

template<template<class...> class S, class C = stringify_context<S>>
struct stringify
{
    template<template<class...> class S1, class T>
    struct deconstruct;
    
    template<template<class...> class S1, class... Ts>
    struct deconstruct<S1, S1<Ts...>> { using type = std::tuple<Ts...>; };
    
    template<template<class...> class S1, class... Ts>
    using deconstruct_t = typename deconstruct<S1, Ts...>::type;
    
    
    template<class F, class... Ts>
    /* requires RegularInvocable<F> */
    struct box { F name; }; 
    
    
    template<class F, class Ts> struct boxify;
        
    template<class F, class... Ts>
    struct boxify<F, std::tuple<Ts...>> { using type = box<F, Ts...>; };    
    
    template<class F, class... Ts>
    using boxify_t = typename boxify<F, Ts...>::type;


    template<class T>
    constexpr const char* operator() (T&&) const
    {
        using boxified = boxify_t<
            typename C::action_type
          , deconstruct_t<S, std::remove_reference_t<T>>
          >;
        
        return std::get<boxified>(C::names).name();
    }
};



// ---


template<>
struct stringify_context<std::optional>
{
    using self_type = stringify_context<std::optional>;
    using stringify_type = stringify<std::optional, self_type>;
    
    using action_type = const char* (*)();  // fixed "F" parameter to box

    template<class... Ts>
    using boxed = stringify_type::box<action_type, Ts...>;

    static constexpr std::tuple<
        boxed<int>
      , boxed<double>
      > names= {
        {[] { return "Maybe[Int]"; }}
      , {[] { return "Maybe[Double]"; }}
      };
};

template<>
struct stringify_context<std::vector>
{
    using self_type = stringify_context<std::vector>;
    using stringify_type = stringify<std::vector, self_type>;
    
    using action_type = const char* (*)();  // fixed "F" parameter to box

    template<class... Ts>
    using boxed = stringify_type::box<action_type, Ts...>;

    static constexpr std::tuple<
        boxed<int, std::allocator<int>>
      , boxed<double, std::allocator<double>>
      > names= {
        {[] { return "[Int]"; }}
      , {[] { return "[Double]"; }}
      };
};

int main()
{
    std::optional<int> oi;
    std::optional<double> od;
    
    std::cout << stringify<std::optional>{}(oi) << '\n';
    std::cout << stringify<std::optional>{}(od) << '\n';
    
    std::vector<int> vi;
    std::vector<double> vd;
    
    std::cout << stringify<std::vector>{}(vi) << '\n';
    std::cout << stringify<std::vector>{}(vd) << '\n';
    
    return 0;
}


/*
g++ -std=c++17 -O2 -Wall -pedantic -pthread -fsanitize=address -fno-omit-frame-pointer main.cpp && ./a.out
Maybe[Int]
Maybe[Double]
[Int]
[Double]
*/

