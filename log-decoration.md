
# Decorating with a side effect

In an ideal world of software, all the side effects are isolated, and we verify pure actions only. Unfortunately, in a real world software, side effects are ubiquitous, and they inherently polluting pure code. Logging is one of such overused features that leads to costly side effects (consider distributed logging DLT prevalent in automotive industry). This article describes a technique that is used to extract side effects brought by logging, and then compose with them back in a well defined manner.

## Extract

Generally speaking, all the information that triggers logging action inside a given function shall be exposed to the caller of that function in its result value (and/or type). Example modelling of that case is described in [Log less, log once](https://github.com/insooth/insooth.github.io/blob/master/log-less-log-once.md) article. In short, in the mentioned article we lift a result type of a function to extract information that triggers a log:

```
 original:          lifted into `Either e bool`:
bool foo(T)  ==>  pair<InfoForLogger, bool> foo(T)
```

Consider the benefits that come from this approach. Control path inside a function is much cleaner, it is not disturbed by calls to a logger. Testing is much easier, and there is no need to mock the entire log infrastructure. Moreover, it is a good starting point to pure functional code.

## Decorate

Value of a lifted type carries information that is consumed by a logger to trigger generation of a log message (and performing accompanied side effect). In the most basic scenario, such information is an error code (a number), and logger simply prints the preconfigured messages associated with that number. Consider:

```
 original:          lifted into `Either e bool`:         do a side effect and unlift:
bool foo(T)  ==>  pair<InfoForLogger, bool> foo(T)  ==>  make_log(foo(T{))) -> bool
```

where `auto make_log(auto)` is an action that extracts `InfoForLogger` from the `foo`'s result type, does the actual logging, and returns the original `foo`s result. That is, _unlifted_ is equal to the original, logging is just an optional decoration.

Example interface `make_log` uses customisaton point `run_log` that can be optionally stateful (`Logger` instance will be forwarded to its constructor), and which shall expose function call operator that takes `T` value, does logging, and _transforms_ lifted `T` into unlifted representation. Customisaton point for equal `T` may be tagged with a `Tag` type to differentiate equal `T` types.

```c++
/** @{ Tagged. */
template<class Tag, class T>
constexpr decltype(auto) make_log(T&& arg);

// with explicit logger
template<class Tag, class T, class Logger>
constexpr decltype(auto) make_log(T&& arg, Logger&& logger);
/** @} */

/** @{ Optional tagged. */
template<class T, class Tag = void>
constexpr decltype(auto) make_log(T&& arg);

// with explicit logger
template<class T, class Logger, class Tag = void>
constexpr decltype(auto) make_log(T&& arg, Logger&& logger);
```

Implementaton and usage:

```c++
template<>
struct run_log<std::pair<InfoForLogger, R>, void, void>
{
    R operator()(const std::pair<InfoForLogger, R>& arg) const
    {
      // logic that uses arg.first to perform a log
      
      return arg.second;  // transform
    }
};

// const bool r = make_log(foo());
```

Note, that `make_log` assumes that `T` contains all the information required to _unlift_ the type. If it does not so, an extension to `run_log` and `make_log` shall be made to allow user-specified optional result type for those both callables.

## Full example

Live code is available [on Coliru](http://coliru.stacked-crooked.com/a/7552b2711231016b).


#### About this document

January 25, 2020 &mdash; Krzysztof Ostrowski

[LICENSE](https://github.com/insooth/insooth.github.io/blob/master/LICENSE)
