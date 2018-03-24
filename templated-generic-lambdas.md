
# Emulating templated generic lambda expressions

One of the greatest advantages of [lambda expressions](http://en.cppreference.com/w/cpp/language/lambda) is their locality. It is really comfortable to reason about the code if the most of its parts is in the near scope, and the inversion of control is limited. On the other hand, optimiser's life is much easier with lambdas (they are just syntactic sugar, thus their actual structure is defined by the compiler). Moreover, lambdas' opaque mangled names can [drastically reduce compile times](https://lists.boost.org/Archives/boost/2014/06/214215.php). Parametrically polymorphic (generic) lambda expressions introduce additional flexibility in algorithm implementation and minimise maintenance work.

Paradoxically, generic lambda expressions in C++17 are somewhat _too_ generic. That characteristic announces itself in inability to fix the lambda parameters beforehand. Unlike in function templates, we cannot specify the parameter types explicitly to effectively disable template argument deduction from the passed arguments. Trying to define a lambda inside a function is not possible in C++17:

```c++
template<class T>
auto wrong = [](auto x) { return T{x}; };
// error: a template declaration cannot appear at block scope

// NOTE: Above works fine at the class (with static added) and global/namespace scopes,
//       but it introduces a variable template rather than a templated generic lambda.
```

Some steps towards templated generic lambdas will taken in C++20 as proposed in [P0428](http://wg21.link/p0428). Unfortunately, explicit specification of parameter types is still [not possible](https://godbolt.org/g/Uf8mCV). For the time of being, a simple technique that emulates templated generic lambdas proposed in this article may be reused.

## Motivation

Imagine a variant container `c` that carries a value of either `T` or `U` types that model [`SequenceContainer`](http://en.cppreference.com/w/cpp/concept/SequenceContainer) concept. Having a generic lambda expression that appends a value `v` of a deduced type (_convertible_ to either `T::value_type` or `U::value_type`) we would like to access the currently set value in the variant. To able to do that, we need to know the type of the value which is set. Consider:

```c++
auto foo = [](std::variant<T, U>& c, auto v)
{
    try
    {
        //                 ,~~ SequenceContainer requirement
        std::get< ??? >(c).push_back(v);
        //        ^~~ depends on v
    }
    catch (std::bad_variant_access&) { /* ... */ }
    catch (std::bad_alloc&)          { /* ... */ }
    
    return c;
};
```

Of course, one can take brute-force to obtain the required type information. For instance, one may call [`get_if`](http://en.cppreference.com/w/cpp/utility/variant/get_if) and check the returned result value for all the possible cases; or may continue to catch `bad_variant_access` exceptions until there is no more exceptions thrown. For all the possible cases, hard-coded. By doing that, one can reach new levels of silliness. Don't do that. It is even worse than banning the use of language features that improve code readability and safety because of buggy IDE's built-in syntax checker.

# Solution

We would like to pass the additional information into `foo`. In order to do that we have to embed the required type information into a dummy value of some type (in which are not interested, in fact), and pass that value as an extra argument to the lambda. In the body of the generic lambda the passed type information is recovered.

```c++
auto bar = [](std::variant<T, U>& c, auto v, auto w)
//                                           ^^^^^^
{
    using W = typename decltype(w)::type;  // recovery
    
    try
    {
        std::get<W>(c).push_back(v);
        //      ^^^
    }
    catch (std::bad_variant_access&) { /* ... */ }
    catch (std::bad_alloc&)          { /* ... */ }
    
    return c;
};
```

We can pass any object as third argument to `bar`, the only constraint is that the type of that value has to expose nested `typedef` named `type`. Its our responsibility to pass as little data in `w` as possible. Ideally, we choose a type that [is empty](http://en.cppreference.com/w/cpp/types/is_empty). That's easily achievable with `std::common_type` acting as an identity metafunction:

```c++
template<class T>
using identity_type = std::common_type<T>;
```

Example usage:

```
std::variant<T, U> c{U{}};  // U::value_type accepts hours

bar(c, 24h, identity_type<U>{});
```

Live code available on [Coliru](http://coliru.stacked-crooked.com/a/c60d2c30bb6b1993).

#### About this document

March 24, 2018 &mdash; Krzysztof Ostrowski

[LICENSE](https://github.com/insooth/insooth.github.io/blob/master/LICENSE)

