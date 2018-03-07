
# Composition of function objects

_(or Exercise in Monads)_

Language reference for the C++ concept [`FunctionObject`](http://en.cppreference.com/w/cpp/concept/FunctionObject) defines it as an object that implements function call operator &mdash; `operator()`. That's pretty simple, yet impressively powerful concept at the same time.

Function call operator can take arbitrary number of arguments including ellipsis (`...`), can return value of any type, and can appear in any context. If defined for a type that models `FunctionObject` concept  it can be used as an [efficient](http://www.open-std.org/jtc1/sc22/wg21/docs/18015.html) replacement for pointers to functions &mdash; as of C++11 _generated_ by widely known lambda expression syntax sugar.

In fact, types that model `FunctionObject` concept express quite close relationship to the functional definition of a function. An "ordinary" function in functional language (like Haskell) is identified by its signature that confesses the whole truth about its behaviour. That is, we have a "black box" (an object that encapsulates given function) and a "synopsis" (bound signature). Functions are to be applied, and that holds true in functional world, mostly. Mostly, because, unlike as in C, _partial_ application is allowed and favoured. "Partial" here means passing some of the arguments for the application while leaving "gaps" for the others (cf. [`std::bind`](http://en.cppreference.com/w/cpp/utility/functional/bind) and use of [`placeholders`](http://en.cppreference.com/w/cpp/utility/functional/placeholders)). In the result, we receive a _generated_ function with a _state_ ready to take the non-applied arguments. Partial application is easily achievable with function objects.

## Composition

Designing for composition is the true key to success. To be able to compose anything we need to know the _interfaces_, and the simplest model of an interface is a function signature. Having two functions: `f` that produces some output of type `T`, and `g` that takes an argument of the same type `T`, we are able to compose them directly, `g` after `f`. Functions that do not compose directly require additional "processing" that _translates_ the output from the current one to an input acceptable by the next one in the composition chain. 

_Translation_ touches types, never the actual values. That means, there may happen that `std::vector` will be iterated, `std::optional` content will be checked, or `std::tuple` will be "exploded" via `std::apply`. In the other words: the _container_ for the actual values will be unwrapped, and wrapped again after function application completes. Note that, function is a container too &mdash; applied partially stores its arguments and, if applied fully, a result value. Consider application of `g` after `f` where:

```c++
std::vector<T> f(...) { return { {}, {}, {} }; }  // f produces three values of type T
U              g(T t) { return U{t};           }  // g makes U value out of T value
```

No direct composition for those two functions exists: the result type of `f` does not match the argument type of `g`. It does not matter here whether they model `FunctionObject` concept or not. However, indirect composition (i.e. translation) is possible, and depends on the logic inside the translator. We have to invent that (appropriate) logic ourselves (here, in fact, it is already given, see note at the end of this chapter).

Generally, in our case, the translation can do the following: it applies `g` to each one value from the 
sequence returned by `f` (i.e. unwraps the container). Note that we have just created an _implicit state_ between `f` and `g` &mdash; a layer of an abstraction that produces a "cloud" of (unordered) values using the values _pulled_ from the container and the function `f`.

In order to make a transition form the implicit state to the terminal one we have to transfer the values from that glue layer to the output of `g` after `f`. That's not trivial: `g` returns a value of type `U` while we have whole cloud of such values. No, we shall not arbitrary select one of them and discard the rest. Information may not be lost. To enforce that, we will wrap the cloud of values into the original container ("ordered" in a sequence), `std::vector`.

```
std::vector<U> h(...) { return g( f(...) ); }  // g after f -- pseudo-code
//                              ^        ^
//                              |________| translation happens here!
```

Congratulations, now you know what the [Monad](http://hackage.haskell.org/package/base-4.10.1.0/docs/Control-Monad.html#t:Monad) is for. Translation layer is created with `>>=` (monadic bind) that combines two functions. Wrapping does `return` (and `lift` which "changes" function signatures to operate in a monadic context). Unwrapping is done through `<-` (which sequentially pulls values form a monadic container).

Due to fact we have to strictly follow maths in a Monad world, following steps are executed within the `>>=` abstraction:
* pull (`<-`) from the result of `f` application, and _for each one pulled_:
   * apply `g` and wrap back in the original container (here: `std::vector`) _immediately_,
* wrap the just created cloud of values back into the original container.

Output for `g` after `f` produced in that way will be of `std::vector<std::vector<U>>` type. That's not acceptable, because `g` operates on a vector of values returned by `f`. The resulting abstraction shall be flattened through `join` operation: `std::vector<std::vector<U>>` becomes `std::vector<U>` (cf. Scala's `flatMap`).

## Function objects' composition

In the following example we will compose two function objects `f` and `g` of types `F` and `G` respectively &mdash; `g` after `f`. Object of type `F` if applied to a value of type `T` may fail to produce an output value of the desired type thus its result type is wrapped into [`std::optional`](http://en.cppreference.com/w/cpp/utility/optional).

```c++
struct F
{
    std::optional<T> operator() (T t);
};

struct G
{
    T operator() (T t);
};
```

Unfortunately, direct composition `g` after `f` is not possible:

```
error: no match for call to '(G) (std::optional<int>)'
     g(f({}));
            ^
note:   no known conversion for argument 1 from 'std::optional<int>' to 'T'
```

Dedicated layer of abstraction that will do the required argument conversion should solve that issue. We can easily write code like this:

```c++
F f;
G g;

auto r1 = f({});

if (r1)
{
    auto r2 = g(r1.value());
    // do something with r2
}
```

which does not solve the problem, though. Every subsequent composed function introduces an extra nested `if` block with temporary variables. That's the best recipe for maintenance nightmare. Let's define the presented composition problem in terms of iteration, application and binding.

### Iteration

Composition of functions is performed in the known direction. Initial input data is passed to the first function in the composition chain, function application is performed, the resulting value becomes "initial" input data for the second function in the chain, and so on. In general, we are iterating over a _sequence_ of items representing functions, and during the iteration we are performing function application, and binding of the results to the subsequent function. To be able to iterate over, we have to choose an appropriate abstraction that enables us to do so.

C++ language defines _iterable_ abstraction by means of [`SequenceContainer`](http://en.cppreference.com/w/cpp/concept/SequenceContainer) concept that we would have had a chance to reuse if it had been defined for heterogeneous containers. That is, neither `std::vector` nor `std::array` models of that concept accept items of multiple different types at once. Actually, `std::tuple` is the only true heterogeneous container in C++17. We will wrap `std::tuple` into an abstraction that models `FunctionObject`:

```c++
template<class... Ts>
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
  
    return T{};
    //     ^~~ required to make type inference possible
  }


  constexpr Chain() = default;
  constexpr Chain(Ts&&... ts) : sequence{std::forward<Ts>(ts)...} {}

  sequence_type sequence;
};
```

&mdash; and use it as follows:

```c++
Chain<F, G> chain;
// or Chain<F, G> chain = {F{}, G{}};

const auto result = chain(T{});
```

Side note. Someone would say: let's force users to derive from a base class with a `virtual` member function that will forward to the respective function call operator. That's a sign of a bad design! Here is why:
* we would have to define a multitude of such `virtual` member functions &mdash; for every used function signature,
* we would introduce a _strict_ relationship between the completely unrelated types (here: function objects),
* we would add a layer of indirection that affects runtime (dynamic dispatch),
* we would have to maintain all that tons of code.

### Application

The body of function call operator of `Chain` shall do:
* `std::get<0>(sequence)` function object application to the passed `args`,
* translation of the result value and application of `std::get<1>(sequence)` to it.

The above two steps shall be performed for every subsequent item in the `sequence`. Iteration over the sequence requires an implicit state to be transferred between each iteration step. That state represents the last known result of an function application. The state is a subject of the _translation_ process that will be introduced soon. 

Let's consider an application chain for a direct composition. Following example code additionally enables application of an object of `std::tuple` type to a function that expects "exploded" tuple input:

```c++
template<class... Ts>
struct Chain
{
  using sequence_type = std::tuple<Ts...>;
  
  template<class... As>
  constexpr decltype(auto) operator()(As&&... args)
  {
    static_assert(std::tuple_size_v<sequence_type> > 0, "Empty chain");

    return run<0>(std::forward<As>(args)...);
  }

// ...

  template<std::size_t I, class... As>
  constexpr auto run(As&&... args)
  {
    // all items but last 
    if constexpr (I < std::tuple_size_v<sequence_type>)
    {
      // explode a tuple
      if constexpr (std::conjunction_v<is_tuple<As...>
                                     , can_apply<decltype(std::get<I>(sequence)), As...>>)
      {
        return run<I + 1>(std::apply(std::get<I>(sequence)
                        , std::forward<As>(args)...));
      }
      // direct application
      else
      {
        return run<I + 1>(std::invoke(std::get<I>(sequence)
                                    , std::forward<As>(args)...));
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

// ...
  }
```

&mdash; where `can_apply` checks whether `std::apply` can be performed, and `is_tuple` checks whether given type is an instance of `std::tuple` template:

```c++
template<class U>
struct is_tuple : std::false_type {};

template<class... Us>
struct is_tuple<std::tuple<Us...>> : std::true_type  {};


template<class F, class T, class = std::void_t<>>
struct can_apply : std::false_type {};

template<class F, class T>
struct can_apply<F, T, std::void_t<
        decltype(std::apply(std::declval<F>(), std::declval<T>()))
    >> : std::true_type {};
```

We can build the following composition chain:

```c++
struct A { std::tuple<T, T> operator() (T t)        { return {t+1,     t+2}; } };
struct B { std::tuple<T, T> operator() (T t1, T t2) { return {t1 + t2, t2 }; } };
struct C { T                operator() (T t1, T t2) { return {t1 + t2     }; } };

// example:   c . b . a 1   is (2, 3) -> (5, 3) -> 8

Chain<A, B, C> chain;
chain(T{1});
```

Note that `std::apply` used to "explode" a tuple is not a _translation_ of the implicit state in a monadic sense ([bind for `Tuple`](https://hackage.haskell.org/package/base-4.10.1.0/docs/src/GHC.Base.html#local-6989586621679017734)). It logically performs [`uncurry`](http://hackage.haskell.org/package/base-4.10.1.0/docs/Prelude.html#v:uncurry) on the subsequent function, followed by application of a tuple through [`$`](http://hackage.haskell.org/package/base-4.10.1.0/docs/Prelude.html#v:-36-).

### Binding

Example translation defined for `std::optional<T>` value and a function that takes `T` argument and returns some value of type `U` is of shape:

```c++
template<class T, class F>
//  requires Callable<F, T>
constexpr auto mbind(std::optional<T>&& v, F&& f)
{
  return v ? std::make_optional(f(v.value())) : std::nullopt;
//                ^~~ wrap        ^~~ unwrap
}
```

&mdash; function object `f` is applied to `v` only if it holds a value. Since the result type of `mbind` must be consistent, the application result value is wrapped back into the original container. We can eliminate nested `if` checks easily with `mbind`:

```c++
struct F { std::optional<T> operator() (T t); };
struct G { T                operator() (T t); };

F f;
G g;

auto r1 = f({});
auto r2 = mbind(std::move(r1), g);
//   ^~~ is of type optional<T> (G is "lifted" into optional)
auto r3 = mbind(std::move(r2), G{});
//                        ^~~ unwrapped implicitly
```

`mbind` defined for `std::optional` works here as [`>>=` defined for `Maybe`](http://hackage.haskell.org/package/base-4.10.1.0/docs/src/GHC.Base.html#local-6989586621679017702) monad. There may be multiple `mbind` functions (`>>=`) defined for each container (`Monad` instance). Note that only a single layer of abstraction is unwrapped in order to fetch the stored value.


Above defined `mbind` works for unary functions only. To enable it for functions of arbitrary arity we have to take a closer look at the algorithm itself.

```c++
template<class T>
constexpr auto
//        ^~~ deduced result type
mbind(std::optional<T>&& v
//                       ^~~ argument
    , F&& f)
//        ^~~ function to which argument is going to be applied
{
//       .~~ do we have a value under argument?
  return v
//       .~~ process argument's value if exists
         ? std::make_optional(
//              ^~~ wrap
                                f(
//                              ^~~ apply
                                    v.value()
//                                    ^~~ unwrap
                                 )
                             )
         : std::nullopt;
//         ^~~ no value under argument, returns value of (deduced) optional<T> type
}
```

We apply `f` only if none of the arguments to be passed to it is `nullopt`, otherwise we return `nullopt`. Example implementation where the "container" is denoted by `R` parameter:

```c++
template<class R, class F, class... As>
constexpr std::enable_if_t<is_optional<R>::value, R> mbind_all(F&& f, As&&... args)
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
    return R{std::forward<std::remove_reference_t<decltype(v)>>(v)};
  };

  if/* constexpr*/ ((is_set(std::forward<As>(args)) && ...))  // extra parens required
  {
    constexpr auto unwrap = [](auto&& v)
    { 
      using arg_type = std::remove_reference_t<decltype(v)>;

      if constexpr (std::is_same_v<arg_type, std::nullopt_t>) throw std::invalid_argument{"BUG!"};
      else if constexpr (is_optional<arg_type>::value)        return v.value();
      else                                                    return v;
    };

    return wrap(f(unwrap(std::forward<As>(args))...));
  }
  else
  {
    return wrap(std::nullopt);
  }
}

// where

template<class U> struct is_optional                   : std::false_type {};
template<class U> struct is_optional<std::optional<U>> : std::true_type  {};
```

&mdash; and usage:

```c++
using T = int;  // for exposition only
using R = std::optional<T>;

mbind_all<R>([](T x, T y, T z){ return x + y + z; }, R{}, 2, 3);           // nullopt
mbind_all<R>([](T x, T y, T z){ return x + y + z; }, 1, 2, 3);             // 6
mbind_all<R>([](T x, T y, T z){ return x + y + z; }, 1, R{2}, 3);          // 6
mbind_all<R>([](T x, T y, T z){ return x + y + z; }, 1, std::nullopt, 3);  // does not compile (see note)
```

(\*) It is worth to note that `is_set` can be evaluated during compile-time, but its result not always can be (even so [`std::optional::has_value`](http://en.cppreference.com/w/cpp/utility/optional/operator_bool) is `constexpr`). That's the reason for non-constexpr `if-else` block that is controlled by the fold expression. This limitation has further implications. Both two branches of the mentioned conditional block are enabled, thus unwrapping code in `wrap(f(unwrap(args)...))` is going to be evaluated for `nullopt` values too. That's not allowed and leads to:

```
error: invalid use of void expression
         return wrap(f(unwrap(args)...));
                     ~^~~~~~~~~~~~~~~~~
```

## Composition through binding

Unfortunately, the following won't compile because we have explicitly fixed the `R` type, that does not match the result type from the of the lambda expression:

```c++
using R = std::optional<T>;  // T is int -- for exposition only
//    ^~~~.
mbind_all<R>([](T x, T y, T z){ return std::make_tuple("sum", x + y + z); }, R{1}, T{2}, T{3});
//                                     ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
/*
error: no matching function for call to 'std::optional<int>::optional(std::tuple<const char*, int>)'
     constexpr auto wrap = [](auto&& v) { return R(...
*/
```

In order to overcome that we would have to specify `R` as a parametrised type constructor, i.e. a template for which parameter we have to figure out. In our case it would be `std::optional<?>` where `?` is going to be computed based on the type of input arguments. Simply "unfixing" the `R` type triggers and error that is a consequence of non-constexpr `if` block controlled by `is_set` (see (\*)  for details):

```c++
template<template<class...> class R, class F, class... As>
constexpr auto mbind_all(F&& f, As&&... args)
{    
// ...
  constexpr auto wrap = [](auto&& v)
  {
      using arg_type = std::remove_reference_t<decltype(v)>;
        
      return R<arg_type>(std::forward<arg_type>(v));
  };
// ...
 if/* constexpr*/ ((is_set(std::forward<As>(args)) && ...))
  {
// ...
    return wrap(f(unwrap(std::forward<As>(args))...));
//         ^^^^
  }
  else
  {
    return wrap(std::nullopt);
//         ^^^^
  }
```

&mdash; gives:

```c++
mbind_all
  <
    std::optional  // a template that constructs fixed types like optional<int>
  >([](T x, T y, T z){ return std::make_tuple("sum", x + y + z); }, R{1}, T{2}, T{3});
/*
error: inconsistent deduction for auto return type:
    'std::optional<std::tuple<const char*, int> > and then 'std::optional<const std::nullopt_t>'
*/
```

We have to either disable one of the branches of the mentioned `if` block to make the deduction succeed, or calculate the common type, then use its default constructor (that works for `std::optional` and leads to an object that does not contain a value). Following code snippet implements the mentioned technique:

```c++
template<template<class...> class R, class F, class... As>
constexpr auto mbind_all(F&& f, As&&... args)
{
  constexpr auto is_set = [](auto&& v)
  {
    using arg_type = std::remove_reference_t<decltype(v)>;

    if constexpr (std::is_same_v<arg_type, std::nullopt_t>) return false;
    else if constexpr (is_optional<arg_type>::value)        return v.has_value();
    else                                                    return true;
  };

  constexpr auto unwrap = [](auto&& v)
  { 
    using arg_type = std::remove_reference_t<decltype(v)>;
    
    if constexpr (std::is_same_v<arg_type, std::nullopt_t>) throw std::invalid_argument{"BUG!"};
    else if constexpr (is_optional<arg_type>::value)        return v.value();
    else                                                    return v;
  };

  constexpr auto wrap = [](auto&& v)
  {
    using arg_type = std::remove_reference_t<decltype(v)>;
    
    return R<arg_type>{std::forward<arg_type>(v)};
  };

  using result_type = decltype(wrap(f(unwrap(std::forward<As>(args))...)));

  return (is_set(std::forward<As>(args)) && ...)
          ? wrap(f(unwrap(std::forward<As>(args))...))
          : result_type{};
//          ^^^^^^^^^^^^^
}
```

Note that `isset` and `unwrap` are `std::optional`-specific. To make `mbind_all` working for an arbitrary `R` the affected actions (`wrap`, `unwrap`, `is_set`) and value returned upon computation failure are abstracted into `mbind_all_impl` tagged with `R`:

```c++
template<template<class...> class R, class F, class... As>
constexpr auto mbind_all(F&& f, As&&... args)
{
  using impl = mbind_all_impl<R>;

  using result_type = decltype(impl::wrap(f(impl::unwrap(std::forward<As>(args))...)));

  return (impl::is_set(std::forward<As>(args)) && ...)
              ? impl::wrap(f(impl::unwrap(std::forward<As>(args))...))
              : result_type{impl::failure_value};
}
```
&mdash; where:

```c++
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

template<template<class...> class R>
struct mbind_all_impl;

template<>
struct mbind_all_impl<std::optional>
  : mbind_all_impl_base<std::optional>
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
```

Example usage:

```c++
auto [s, i] =
  mbind_all<std::optional>
    ([](T x, T y, T z){ return std::make_tuple("sum", x + y + z); }, R{1}, T{2}, T{3})
      .value();

std::cout << s << ' ' << i << '\n';  // sum 6

mbind_all<std::optional>  //                                       ,~~~ makes computation failed
  ([](T x, T y, T z){ return std::make_tuple("sum", x + y + z); }, R{}, T{2}, T{3})
    .value();  // terminate called after throwing an instance of 'std::bad_optional_access'
```

Note that the container `R` has to defined explicitly (or deduced from the passed function object signature by means of an additional abstraction), thus regular application happens where un/wrapping is not required. Consider the following composition "chain":

```c++
struct F { std::optional<T> operator() (T t); };
struct G { T                operator() (T t); };

F f;
G g;

auto r1 = f({});  // F: T -> optional<T>
auto r2 = mbind_all<std::optional>(g, std::move(r1));  // G: T -> T
//   ^~~ is of type optional<T> (G is "lifted" into optional)
auto r3 = mbind_all<std::optional>(G{}, std::move(r2));
//                                      ^~~ unwrapped implicitly
```

Interestingly, we cannot mix the containers of different types in a single `mbind_all` composition chain easily. We can "stack" them (like `std::vector<std::optional<T>>`), and then move between the created layers of abstraction. That requires additional tooling known as [Monad Transformers](https://en.wikibooks.org/wiki/Haskell/Monad_transformers), and is not covered in this article.

## Chaining function objects

Abstraction `mdind_all` can be incorporated into the `Chain` with a some effort. First step is to pass the `R` class template into the original type:

```c++
template<template<class...> class R, class... Ts>
//       ^^^^^^^^^^^^^^^^^^^^^^^^^^
struct Chain
{
...
```

The second step is to tweak the `run` member function's cases for non-terminal iteration steps:

```c++
// using impl = mbind_all_impl<R>;

if constexpr (I < std::tuple_size_v<sequence_type>)
{
  if constexpr (std::conjunction_v<
                    std::is_same<decltype(impl::failure_value), std::remove_reference_t<As>>...
                  >)
  {
    return impl::failure_value;
  }
  else if constexpr (std::conjunction_v<
                          is_tuple<As...>
                        , can_apply<decltype(std::make_tuple(std::get<I>(sequence))), As...>
                        >)
  {
    auto r =
      std::apply( do_mbind_all
//                ^^^^^^^^^^^^
                , std::tuple_cat(std::make_tuple(std::get<I>(sequence)), std::forward<As>(args)...) );

    return impl::is_set(r)
              ? impl::wrap(impl::unwrap(run<I + 1>(impl::unwrap(std::move(r)))))
//                         ^^^^^^^^^^^^
              : impl::failure_value;
  }
  else
  {
    auto r = mbind_all<R>(std::get<I>(sequence), std::forward<As>(args)...);

    return impl::is_set(r)
              ? impl::wrap(impl::unwrap(run<I + 1>(impl::unwrap(std::move(r)))))
//                         ^^^^^^^^^^^^
              : impl::failure_value;
  }
}
```

&mdash; where `do_mbind_all` does the thing described i.a. in [P0834R0](http://wg21.link/P0834R0), it lifts overload set into an object. Simply passing `mbind_all<R>` into `std::apply` leads to unresolved overload error. Here is the implementation of the mentioned lift action:

```c++
static constexpr auto do_mbind_all =
  [](auto&&... as)
  {
    return mbind_all<R>(std::forward<std::remove_reference_t<decltype(as)>>(as)...);
  };
```

Since `mbind_all` implicitly wraps the result value into `R` abstraction, we end up quickly with overly nested abstractions. That's fixed by additional unwrapping of the result value returned in the subsequent step.

See live code on Coliru [here](http://coliru.stacked-crooked.com/a/1f91f9fe4e7b069f).

#### About this document

February 23--28, 2018 &mdash; Krzysztof Ostrowski

[LICENSE](https://github.com/insooth/insooth.github.io/blob/master/LICENSE)
