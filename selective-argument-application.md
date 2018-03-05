
# Selective argument application

Consider a chain of actions that are going to be composed with each other in order to produce a result of a certain type. Once fed with an input, such a chain, either gives some output or signals computation failure. Generally, there is no insight into the data passed between actions unless we inject a prepared action that intercepts the intermediate values (breaking the chain is not an option). Let's have a look at the chain that directly composes `g` after `f`, and for which we want to intercept the intermediate value using the prepared action `l`:

```c++
struct F { T operator() (U, V); };
struct G { T operator() (T);    };

F f;
G g;

(void) g( f(u, v) );
//       ^~~ intermediate value is here

(void) g(l(f(u, v)));
//       ^~~ intercepts the result of f
```

&mdash; where `l` can be either a generic lambda expression or a function object of the signature `auto (auto&&)`. Action `l` forwards  received input value (result of `f` application) transparently to `g`). Extra work performed by `l` includes "recording" of the value that is forwarded. Recording means here storing a value in the external container (like logger stream) for later processing.

That was an easy task for us. Let's design an interceptor that records the _selected_ arguments passed to any action transparently, then applies that action to the arguments, and eventually passes the result value to the next action.

## Application

Applicator does application, and C++17 offers two generic applicators:
* `std::invoke(F, Args...)` for direct application of `f` to a number of arguments, and...
* `std::apply(F, tuple<Args...>)` that unpacks arguments from the given `tuple` container (aka "explosion") and does the application using the former one applicator.

Here is the "interceptor" function object that works for any action of type `F`:

```c++
template<class F>
//  requires Callable<F>
struct Middleman
{
  template<class... Args>
  decltype(auto) operator() (Args&&... args)
  {
    // *** record the values of the selected arguments ***

    return _f(std::forward<Args>(args)...);
  }

  constexpr Middleman(F& f) : _f(f) {}

  std::reference_wrapper<T> _f;
};
```

`Middleman` can be understood as a function with the signature `auto (F&, auto&&...)` that is partially applied to an object of type `F` by means of its constructor. Example usage:

```c++
F f;
G g;
M m{f};  // partial application

(void) g( m(u, v) );
//        ^~~ f replaced with m
```

The missing part is the recorder invocation. The `record` action, that does the actual recording, accepts values of types for which some predicate `P` is satisfied only. That is, we have to filter out the passed arguments' sequence before applying it to the `record` action.

## Filtering

Let's define an example predicate as such that is only satisfied if the passed type `T` is `int`:

```c++
template<class T>
constexpr bool P = std::is_same_v<int, T>;
```

Given that, we can generate the "truth table", here modelled as multiple `bool` values wrapped into a heterogeneous container:

```c++
template<class... Args>
decltype(auto) qualify(Args&&... args)
{
  return std::make_tuple(P<std::remove_reference_t<Args>>...);
}

// qualify(int,  std::string, bool,  int)
// gives  (true, false,       false, true)
```

Based on the sequence of `bool`s produced by `qualify` function we are able to filter out uninteresting values &ndash; leaving the "true" ones untouched, and substituting the "false" ones with gaps. We have to produce a _valid_ value in those both cases. The easiest way is to wrap every value from the sequence into a container that is allowed to have an empty state, and the flatten the produced abstraction (astute reader will see a Monad here). In the following chart, the `()` denotes such a container:

```
(int,   std::string, bool,  int  )
(true,  false,       false, true )  qualify
((int), (),          (),    (int))  wrap
(int,                       int  )  flatten
```

Note that, we cannot use `std::vector` as a container since it cannot hold values of different types at the same time (i.e. it is homogeneous). One of the most common valid choices here is `std::tuple`, where `std::make_tuple` does `wrap`, and `std::tuple_cat` does `flatten`. With the help of `std::apply` we "explode" the container that holds the arguments and we pass them to the `record` function. Example:

```c++
template<class F>
//  requires Callable<F>
struct Middleman
{
  template<class... Args>
  decltype(auto) operator() (Args&&... args)
  {
    (void) std::apply(record, std::tuple_cat(wrap(args)...));
//                                 ^~~ flatten

    return _f(std::forward<Args>(args)...);
  }
...
```

&mdash; where `wrap` uses the `P` predicate, and does:

```c++
template<class T>
constexpr decltype(auto) wrap(T&& t)
{
    if constexpr (P<std::remove_reference_t<T>>)
        return std::make_tuple(std::forward<T>(t));
    else
        return std::make_tuple();
}
```

## Lifting

We can easily derive a reusable abstraction from the presented in this article technique and call it `apply_if`:

```c++
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
    return std::apply(std::forward<F>(f), std::tuple_cat(apply_if_impl<P>(args)...));
}
```

Example of a selective application of arguments of types `U` or `V` solely follows:

```c++
template<class T>
using P = std::disjunction<std::is_same<U, T>, std::is_same<V, T>>;

auto result = apply_if<P>([](U, V, U) { return true; }
                        , float{}, U{}, V{}, bool{}, U{}, char{});  // true
```

See live code on [Coliru](http://coliru.stacked-crooked.com/a/0e945cdc2ebec777).


#### About this document

March 3, 2018 &mdash; Krzysztof Ostrowski

[LICENSE](https://github.com/insooth/insooth.github.io/blob/master/LICENSE)
