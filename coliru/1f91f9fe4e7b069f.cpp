
// see LICENSE on insooth.github.io

#include <iostream>

#include <cstddef>  // size_t
#include <functional>  // invoke
#include <optional>  // optional, nullopt
#include <stdexcept>  // 
#include <tuple>  // tuple, tuple_size, apply, forward_as_tuple, tuple_cat
#include <type_traits>  // is_same, {true,false}_type, conjunction, enable_if
#include <utility>  // forward, declval


using T = int;  // for exposition only



template<class T, class F>
//  requires Callable<F, T>
constexpr auto mbind(std::optional<T>&& v, F&& f)
{
    return v ? std::make_optional(f(v.value())) : std::nullopt;
}


template<class U> struct is_optional                   : std::false_type {};
template<class U> struct is_optional<std::optional<U>> : std::true_type  {};

template<template<class...> class R>
struct mbind_all_impl;

template<template<class...> class R>
struct mbind_all_impl_base
{
    template<class V>
    static constexpr auto wrap(V&& v)
    {
        using arg_type = std::remove_reference_t<V>;
        
        return R<arg_type>{std::forward<arg_type>(v)};
    }
};

template<>
struct mbind_all_impl<std::optional> : mbind_all_impl_base<std::optional>
{
    template<class V>
    static constexpr auto is_set(V&& v)
    {
        using arg_type = std::remove_reference_t<V>;

        if constexpr (std::is_same_v<arg_type, std::nullopt_t>) return false;
        else if constexpr (is_optional<arg_type>::value)        return v.has_value();
        else                                                    return true;
    }
    
    template<class V>
    static constexpr auto unwrap(V&& v)
    {
        using arg_type = std::remove_reference_t<V>;
        
        if constexpr (std::is_same_v<arg_type, std::nullopt_t>) throw std::invalid_argument{"BUG!"};
        else if constexpr (is_optional<arg_type>::value)        return v.value();
        else                                                    return v;
    }
    
    static constexpr auto failure_value = std::nullopt;
};

template<template<class...> class R, class F, class... As>
constexpr auto mbind_all(F&& f, As&&... args)
{
    using impl = mbind_all_impl<R>;
    
    using result_type = decltype(impl::wrap(f(impl::unwrap(std::forward<As>(args))...)));

    return (impl::is_set(std::forward<As>(args)) && ...)
                ? impl::wrap(f(impl::unwrap(std::forward<As>(args))...))
                : result_type{impl::failure_value};
}






template<template<class...> class R, class F, class... As>
constexpr auto mbind_all_optional(F&& f, As&&... args)
{
  constexpr auto is_set = [](auto&& v)
  {
    using arg_type = std::remove_reference_t<decltype(v)>;

    if constexpr (std::is_same_v<arg_type, std::nullopt_t>) return false;
    else if constexpr (is_optional<arg_type>::value)        return v.has_value();
    else                                                    return true;
  };

  constexpr auto wrap = [](auto&& v)
  {
    using arg_type = std::remove_reference_t<decltype(v)>;
    
    return R<arg_type>{std::forward<arg_type>(v)};
  };


  constexpr auto unwrap = [](auto&& v)
  { 
    using arg_type = std::remove_reference_t<decltype(v)>;
    
    if constexpr (std::is_same_v<arg_type, std::nullopt_t>) throw std::invalid_argument{"BUG!"};
    else if constexpr (is_optional<arg_type>::value)        return v.value();
    else                                                    return v;
  };

  using result_type = decltype(wrap(f(unwrap(std::forward<As>(args))...)));

  return (is_set(std::forward<As>(args)) && ...)
          ? wrap(f(unwrap(std::forward<As>(args))...))
          : result_type{};
}




template<class... Ts>
struct DirectChain
{
    using sequence_type = std::tuple<Ts...>;
    
    template<class... As>
    constexpr decltype(auto) operator()(As&&... args)
    {
        static_assert(std::tuple_size_v<sequence_type> > 0, "Empty chain");

        // apply first function in sequence to args, pass the result
        // to the next one; repeat until last function which result
        // is returned to the user
  
        return run<0>(std::forward<As>(args)...);
    }


    template<class U>     struct is_tuple                    : std::false_type {};
    template<class... Us> struct is_tuple<std::tuple<Us...>> : std::true_type  {};

    template<class F, class T, class = std::void_t<>>
    struct can_apply : std::false_type {};
    
    template<class F, class T>
    struct can_apply<F, T, std::void_t<decltype(std::apply(std::declval<F>(), std::declval<T>()))>> : std::true_type {};




    template<std::size_t I, class... As>
    constexpr auto run(As&&... args)   // canot be decltype(auto) -- elseif makes this impossible
    {
        // first, second but not last item in sequence
        if constexpr (I < std::tuple_size_v<sequence_type>)
        {
            // explode tuple?
            if constexpr (std::conjunction_v<is_tuple<As...>, can_apply<decltype(std::get<I>(sequence)), As...>>)   // cannot use &&: requires both expressions valid
            {
                return run<I + 1>(std::apply(std::get<I>(sequence), std::forward<As>(args)...));
            }
            else
            {
                return run<I + 1>(std::invoke(std::get<I>(sequence), std::forward<As>(args)...));
            }
 
        }
        // last one item in sequence
        else if constexpr (I == std::tuple_size_v<sequence_type>)
        {
            // just return first one: the implicit state
            return std::get<0>(std::forward_as_tuple(std::forward<As>(args)...));
        }
        else
        {
            throw std::invalid_argument{"BUG!"};
        }
    }


    constexpr DirectChain() = default;
    constexpr DirectChain(Ts&&... ts) : sequence{std::forward<Ts>(ts)...} {}
    
    sequence_type sequence;
};





template<template<class...> class R, class... Ts>
struct Chain
{
    using sequence_type = std::tuple<Ts...>;
    
    template<class... As>
    constexpr decltype(auto) operator()(As&&... args)
    {
        static_assert(std::tuple_size_v<sequence_type> > 0, "Empty chain");

        // apply first function in sequence to args, pass the result
        // to the next one; repeat until last function which result
        // is returned to the user
  
        return run<0>(std::forward<As>(args)...);
    }

    // mbind_all<R> passed to std::apply leads to unresolved overload error
    // abstraction for this is proposed in p0834r0
    static constexpr auto do_mbind_all =
        [](auto&&... as)
        {
            return mbind_all<R>(std::forward<std::remove_reference_t<decltype(as)>>(as)...);
        };


    template<class U>     struct is_tuple                    : std::false_type {};
    template<class... Us> struct is_tuple<std::tuple<Us...>> : std::true_type  {};

/*
main.cpp:241:84: error: no matching function for call to 'tuple_cat(std::tuple<F>, int)'
 static_assert(std::conjunction_v<is_tuple<As...>, can_apply<decltype(std::tuple_cat(std::make_tuple(std::get<I>(sequence)), std::declval<As>()...))>>);
                                                                      ~~~~~~~~~~~~~~^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

can's simply do 
    can_apply<decltype(tuple_cat(..., declval<As>()...)
because with As = int 
    expresion will cause compilation error (arg types to tuple_cat does not match)
-- need to delay the expression check until we are sure it will be accepted by tuple_cat

*/

    template< class F, class T, bool>
    struct can_apply_impl;
    
    template<class F, class T>
    struct can_apply_impl<F, T, true> : std::conjunction<decltype(std::apply(do_mbind_all, std::tuple_cat(std::declval<F>(), std::declval<T>())))> {};

    template<class F, class T, class = std::void_t<>>
    struct can_apply : std::false_type {};
    
    template<class F, class T>
    struct can_apply<F, T, std::void_t<can_apply_impl<F, T, is_tuple<T>::value>>> : std::true_type {};

        
    using impl = mbind_all_impl<R>;
    

    template<std::size_t I, class... As>
    constexpr auto run(As&&... args)   // canot be decltype(auto) -- elseif makes this impossible
    {
        // first, second but not last item in sequence
        if constexpr (I < std::tuple_size_v<sequence_type>)
        {
            // explode tuple?
            if constexpr (std::conjunction_v<std::is_same<decltype(impl::failure_value), std::remove_reference_t<As>>...>)
            {
                return impl::failure_value;
            }
            else if constexpr (std::conjunction_v<is_tuple<As...>, can_apply<decltype(std::make_tuple(std::get<I>(sequence))), As...>>)   // cannot use && since both expressions must be valid
            {
                auto r = std::apply(do_mbind_all, std::tuple_cat(std::make_tuple(std::get<I>(sequence)), std::forward<As>(args)...));
                
                // without unwrap after f in wrap(unwrap(f(unwrap(x)))) extra layer of abstraction will be added each time
                
                return impl::is_set(r)
                            ? impl::wrap(impl::unwrap(run<I + 1>(impl::unwrap(std::move(r)))))
                            : impl::failure_value;
            }
            else
            {
                auto r = mbind_all<R>(std::get<I>(sequence), std::forward<As>(args)...);
                
                return impl::is_set(r)
                            ? impl::wrap(impl::unwrap(run<I + 1>(impl::unwrap(std::move(r)))))
                            : impl::failure_value;
            }
 
        }
        // last one item in sequence
        else if constexpr (I == std::tuple_size_v<sequence_type>)
        {
            // just return first one: the implicit state
            return std::get<0>(std::forward_as_tuple(std::forward<As>(args)...));
        }
        else
        {
            throw std::invalid_argument{"BUG!"};
        }
    }


    constexpr Chain() = default;
    constexpr Chain(Ts&&... ts) : sequence{std::forward<Ts>(ts)...} {}
    
    sequence_type sequence;
};



struct F { std::optional<T> operator() (T t) { return {100}; } };
struct L { std::optional<T> operator() (T t) { return std::nullopt; } };
struct G { T operator() (T t)                { ++t; return t; } };
struct H { T& operator() (T& t)              { ++t; return t; } };
struct X { std::tuple<T> operator() (T t)    { return {t+1}; } };

struct A { std::tuple<T, T> operator() (T t)        { return {t+1,     t+2}; } };
struct B { std::tuple<T, T> operator() (T t1, T t2) { return {t1 + t2, t2 }; } };
struct C { T                operator() (T t1, T t2) { return {t1 + t2     }; } };


int main()
{   
    // c . b . a 1   is (2, 3) -> (5, 3) -> 8
    DirectChain<A, B, C> chain1;
    std::cout << "chain1: " << chain1(T{1}) << '\n';
    
    // g . g . f
    F f;
    G g;    
    auto r1a = f({});  // F: T -> optional<T>
    auto r2a = mbind_all<std::optional>(g, std::move(r1a));  // G: T -> T
    //   ^~~ is of type optional<T> (G is "lifted" into optional)
    auto r3a = mbind_all<std::optional>(G{}, std::move(r2a));
    //                                  ^~~ unwrapped implicitly
    std::cout << "g . g . f: " << r3a.value_or(T{-1}) << '\n';

    using R = std::optional<T>;
    
    auto [s, i] = mbind_all<std::optional>([](T x, T y, T z){ return std::make_tuple("sum", x + y + z); }, R{1}, T{2}, T{3}).value();
    std::cout << "tuple wrapped: " << s << ' ' << i << '\n';
    //mbind_all<std::optional>([](T x, T y, T z){ return std::make_tuple("sum", x + y + z); }, R{}, T{2}, T{3}).value();    
    
    auto r1b = mbind_all<std::optional>(F{}, T{1});  // F: T -> optional<T> 
    std::cout << "f: " << r1b.value().value_or(-1) << '\n';
    //           ^^ optional<optional<T>>
    auto r2b = mbind_all<std::optional>(G{}, r1b.value());  // G: T -> T
    //                                 ^^^ liftM (see below)
    constexpr auto lift = [](R&& v) { return G{}(mbind_all_impl<std::optional>::unwrap(v)); };  // G': optional<T> -> T
    auto r2b1 = mbind_all<std::optional>(lift, r1b);
    //   ^^ optional<T> -- G' result type is "lifted" implicitly
    std::cout << "g . f: " << r2b.value() << '\n';
    std::cout << "liftM g . f: " << r2b1.value() << '\n';

    Chain<std::optional, F, G> chain2;
    std::cout << "chain2: " << chain2(T{1}).value_or(-1) << '\n';

    Chain<std::optional, F, G> chain2a;
    std::cout << "chain2a: " << chain2a(R{T{1}}).value_or(-1) << '\n';

    Chain<std::optional, A, B, C> chain3;
    std::cout << "chain3: " << chain3(T{1}).value() << '\n';

    Chain<std::optional, G, G, G> chain4;
    std::cout << "chain4: " << chain4(R{}).value_or(-1) << '\n';

    return 0;
}
