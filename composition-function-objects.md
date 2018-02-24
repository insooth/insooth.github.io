
# Composition of function objects

Language reference for the C++ concept [`FunctionObject`](http://en.cppreference.com/w/cpp/concept/FunctionObject) defines it as an object that implements function call operator &mdash; `operator()`. That's pretty simple, yet impressively powerful concept at the same time.

Function call operator can take arbitrary number of arguments including ellipsis (`...`), can return value of any type, and can appear in any context. If defined for a type that models `FunctionObject` concept  it can be used as an [efficient](http://www.open-std.org/jtc1/sc22/wg21/docs/18015.html) replacement for pointers to functions &mdash; as of C++11 _generated_ by widely known lambda expression syntax sugar.

In fact, types that model `FunctionObject` concept express quite close relationship to the functional definition of a function. An "ordinary" function in functional language (like Haskell) is identified by its signature that confesses the whole truth about its behaviour. That is, we have a "black box" (an object that encapsulates given function) and a "synopsis" (bound signature). Functions are to be applied, and that holds true in functional world, mostly. Mostly, because, unlike as in C, _partial_ application is allowed and favoured. "Partial" here means passing some of the arguments for the application while leaving "gaps" for the others (cf. [`std::bind`](http://en.cppreference.com/w/cpp/utility/functional/bind) an use of [`placeholders`](http://en.cppreference.com/w/cpp/utility/functional/placeholders)). In the result, we receive a _generated_ function with a _state_ ready to take the non-applied arguments. Partial application is easily achievable with function objects.

## Composition

Designing for composition is the true key to success. To be able to compose anything we need to know the _interfaces_, and the simplest model of an interface is a function signature. Having two functions: `f` that produces some output of type `T`, and `g` that takes an argument of the same type `T`, we are able to compose them directly, `g` after `f`. Functions that do not compose directly require additional "processing" that _translates_ output from current one to an input acceptable by the next one in the composition chain. 

_Translation_ touches types, never the actual values. That means, there may happen that `std::vector` will be iterated, `std::optional` content will be checked, or `std::tuple` will be "exploded" via `std::apply`. In the other words: the _container_ for the actual values will be unwrapped, and wrapped again after function application completes. Note that, function is a container too &mdash; applied partially stores its arguments and, if applied fully, a result value. Consider application of `g` after `f` where:

```c++
std::vector<T> f(...) { return {{}, {}, {}}; }  // f produces three values of type T
U              g(T t) { return U{t};         }  // g makes U value out of T value
```

No direct composition exists for those two functions: result type of `f` does not match the argument type of `g`. It does not matter here whether they model `FunctionObject` concept or not. Indirect composition (i.e. translation) is possible, and depends on the logic inside the translator. We have to invent the appropriate logic ourselves (here, in fact, it is already given, see note at the end of this chapter).

Generally, translation here can do the following: applies `g` to each one value from the 
sequence returned by `f` (i.e. unwraps the container). Note that we have just created an _implicit state_ between `f` and `g` &mdash; a layer of an abstraction that produces a "cloud" (unordered container) of values using the values _pulled_ from the container and function `f`.

In order to make a transition form the implicit state to the terminal one we have to transfer the values from that glue layer to the output of `g` after `f`. That's not trivial: `g` returns a value of type `U` while we have whole cloud of such values. No, we shall not arbitrary select one of them and discard the rest. Information may not be lost. To enforce that, we will wrap the cloud of values into the original container ("ordered" in a sequence), `std::vector`.

```
std::vector<U> h(...) { return g( f(...) ); }  // g after f -- pseudo-code
//                              ^        ^
//                              |________| translation happens here!
```

Congratulations, now you know what the [Monad](http://hackage.haskell.org/package/base-4.10.1.0/docs/Control-Monad.html#t:Monad) is for. Translation layer is created with `>>=` (monadic bind) that combines two functions. Wrapping does `return` (and `lift` which "changes" function signatures to operate in a monadic context). Unwrapping is done through `<-` (which sequentially pulls values form a monadic container).

Due to fact we have to strictly follow maths, following steps are executed within the `>>=` abstraction:
* pull (`<-`) from the result of `f` application, and _for each one pulled_:
   * apply `g` and wrap back in the original container (here: `std::vector`) _immediately_,
* wrap the just created cloud of values back into the original container.

Produced in that way output for `g` after `f` will be of `std::vector<std::vector<U>>` type. That's not acceptable: `g` operates on a vector of values returned by `f`. The resulting abstraction shall be flattened through `join` operation: `std::vector<std::vector<U>>` becomes `std::vector<U>` (cf. Scala `flatMap`).

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

// example:  c . b. a 1  is (2, 3) -> (5, 3) -> 8

Chain<A, B, C> chain;
chain(T{1});
```

Note that `std::apply` used to "explode" a tuple is not a _translation_ of the implicit state in a monadic sense ([bind for `Tuple`](https://hackage.haskell.org/package/base-4.10.1.0/docs/src/GHC.Base.html#local-6989586621679017734)). It logically performs [`uncurry`](http://hackage.haskell.org/package/base-4.10.1.0/docs/Prelude.html#v:uncurry) on the subsequent function, followed by application of a tuple through [`$`](http://hackage.haskell.org/package/base-4.10.1.0/docs/Prelude.html#v:-36-).


### Binding

#### About this document

February 23, 2018 &mdash; Krzysztof Ostrowski

[LICENSE](https://github.com/insooth/insooth.github.io/blob/master/LICENSE)
