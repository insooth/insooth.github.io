# How to deconstruct a template instance

I was asked recently the question about the template instance's parameters inspection possibilities in order to be able to stringify the types and integral constants used to create that instance.

Namely, having a template with _N_ parameters, and an instance (an object or just a type) of that template we would like to print the parameters with which that instance was created.

## Deconstruct through pattern matching

The task is to match against the type constructor (here: a template) of a particular instance of a template, and to extract the parameters that were used to create that instance (in other words: to _deconstruct_ the instance). Having the parameters extracted we want to apply them to a user-specified action. Since no modifications of the parameters are allowed (all are constant), we may either transform those constants into the other constants, or apply a type constructor to them.

Template-based pattern matching will be used to deconstruct an instance of a given (known) template. Extracted parameters will be stored in a heterogeneous container, a `std::tuple`. A non-intrusive approach to deconstruction will be presented (i.e. without modifications to the existing data types, and with no use of nested types likes `value_type` or `result_type`). An example for a single-parameter template follows:

```c++
template<template<class> class S, class T>
struct deconstruct;

template<template<class> class S, class T>
struct deconstruct<S, S<T>> { using type = std::tuple<T>; };

template<template<class> S, class T>
using deconstruct_t = typename deconstruct<S, T>::type;

// optional<double> o
// is_same_v<tuple<double>, deconstruct_t<optional, decltype(o)>> is true
```
Deconstruction scales well to an arbitrary number of type constructor's parameters:

```c++
template<template<class...> class S, class T>
struct deconstruct;

template<template<class...> class S, class... Ts>
struct deconstruct<S, S<Ts...>> { using type = std::tuple<Ts...>; };

template<template<class...> class S, class... Ts>
using deconstruct_t = typename deconstruct<S, Ts...>::type;

// vector<int> v
// is_same_v<tuple<int, std::allocator<int>>, deconstruct_t<vector, decltype(v)>> yields true
```

## Intercept the instance's parameters

Using the deconstruction utility we can emulate one of the features that can be implemented by means of reflexion. Having an instance of a template, we will print the parameters used to make that instance, i.e.: passing `std::vector<int>` to the designed utility we will get a string of `int std::allocator<int>`.

Without the reflexion feature compiler is not likely to provide us with stringified names of the types, thus providing an appropriate mapping form types to their stringified names is up to us.

Constant expression that returns the name (here: `...`) of a type is an action of a form:

```c++
constexpr const char* name() { return "..."; }
// a lambda expression in general:    [] { return "..."; }
```

We would like to call such actions once given a type we support. Since type names are guaranteed by C++ to be unique, we can use `tuple` to store the _box_ objects that wrap the action while they can be indexed by the supported instance's parameters.

```c++
template<class F, class... Ts> /* requires RegularInvocable<F> */ struct box { F name; }; 
// example:    box<std::add_pointer_t<decltype(name)>, int, std::allocator<int>> b{name};

// NOTE: F is fixed for all the boxes in the collection bound to the specific type constructor, thus having
//    tuple<box<F, int>, box<F, double>> names
//         = {{[] { return "int"; }, {[] { return "double"; }}}
// we are able to search for types Ts easily:
//    std::get<box<F, Ts>>(names).name();
```
There should be provided a way to construct `box` out of an instance of a template using deconstruction, that's what `boxify` does:

```c++
template<class F, class Ts> struct boxify;

template<class F, class... Ts>
struct boxify<F, std::tuple<Ts...>> { using type = box<F, Ts...>; };    

template<class F, class... Ts>
using boxify_t = typename boxify<F, Ts...>::type;

// vector<int> v
// is_same<box<F, int, std::allocator<int>>, boxify_t<F, deconstruct_t<decltype(v)>>> yields true
```

## Stringify 

We know how to intercept the instance's parameters with `deconstruct_t`, and we are able to keep the intercepted data in a searchable heterogeneous collection of `box` objects. Let's abstract out the mentioned collection into a stringification context that will be injected into a stringifier abstraction (will be defined at later point later).

```c++
template<template<class...> class S>
struct stringify_context;

template<>
struct stringify_context<std::optional>
{
    using action_type = const char* (*)();  // fixed "F" parameter to box
    template<class... Ts> using boxed = box<action_type, Ts...>;

    static constexpr std::tuple<
        boxed<int>
      , boxed<double>
      > names= {
        {[] { return "Maybe[Int]"; }}
      , {[] { return "Maybe[Double]"; }}
      };
};
```

There exists multiple stringification contexts, one per type constructor. The stringification engine uses the context to get the actual string representation of the parameters

```c++
template<template<class...> class S, class C = stringify_context<S>>
struct stringify
{
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

// optional<int> o
// stringify<optional>{}(o)    yields Maybe[Int]
```

## Live code

Code available on [Coliru](http://coliru.stacked-crooked.com/a/65b08fe3c50aac28).


#### About this document

April 24, 2019 &mdash; Krzysztof Ostrowski

[LICENSE](https://github.com/insooth/insooth.github.io/blob/master/LICENSE)

