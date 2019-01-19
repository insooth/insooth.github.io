
// see LICENSE on insooth.github.io

#include <iostream>
#include <variant>
#include <functional>
#include <optional>

template<class X, class Y>
using P = std::function<Y (X)>;

using A = int;
using B = int;

struct S { A a; B b; };
using T = std::variant<B, double>;

// class Profunctor p where 
//   dimap :: (s -> a) -> (b -> t) -> p a b -> p s t

template<class AS, class BT, class AB>
auto dimap(AS f, BT g, AB h)
{
    return [=](auto s) { return g(h(f(s))); }; 
}


// NOTE: not lifted to a generic abstraction: for exposition only
T runDimap(std::function<A (S)> f
         , std::function<T (B)> g
         , P<A, B> h
         , S s)
{
    return dimap(f, g, h)(s); // g(h(f(s)))
}

// NOTE: runDimap can be delayed with return of a lambda
//       expression that does the actual runDimap


int main()
{
    auto t = 
        runDimap([](S s) { return s.a; }   // pre-processing, getter via Lens
               , [](B b) { return T{b}; }  // posprocessing, setter via Prism
               , [](A a) { return B{a + 100}; }  // conversion via Iso
               , S{11, 22} // we go from S to T
               );
    
    std::cout << std::get<B>(t) << '\n';
    
    // conversion sequence: S::a to B{a + 10},
    //                      then to T,
    //                      then to optional<B> with value if T holds B of value > 100
    
    auto id = [](auto x) { return x; };
    
    auto toMaybeB = 
        dimap(id
            , [](T t) { return (std::holds_alternative<B>(t) && (std::get<B>(t) > 100)) 
                                    ? std::optional<B>{std::get<B>(t)}  
                                    : std::nullopt;
                      }
            , dimap([](S s) { return s.a; }
                  , [](B b) { return T{b}; }
                  , [](A a) { return B{a + 10}; }
                  )
              );
    
    std::cout << toMaybeB(S{100, 2}).value_or(-1) << '\n';  // OK
    std::cout << toMaybeB(S{0, 2}).value_or(-1) << '\n';    // NOK
    
    return 0;
}
