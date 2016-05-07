
# Partial function and magic values

Every function `f` takes some arguments of equal or different types `A₀` to `Aₙ` and returns some value of type `R`. Even if `f` returns _nothing_ (like `void` or `Unit`); even if `f` is nullary and takes no arguments; even that we can write down such a function in C++ as:

```c++
template<class R, class... As> R f(As...);
```

The above is valid _for all_ the `R` and arbitrary number of `As` types; `f` is fully parametrically polymorphic. Since the set of actions carried by `f` must be valid _for all_ the types, such function is almost not implementable. We want to limit allowed type parameters to some _bounds_. This can be realised with [C++ Concepts](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4377.pdf) during compile-time or more traditionally, with subtypes. Unfortunately, C++ subtype polymorphism does not work with values, we would need to switch to references and sacrifice some execution time.

Ideally, type encodes the set of possible values of it. For example, having type `EvenUint8` we describe only even `uint8_t` values, and nothing more. Such detailed description is possible (although in limited form) in languages that support algebraic data types (sum types), and C++ is not one of them. It does not mean we should throw away all that _type-rich_ hype and fallback to C. Let's find a way to assure we receive and reply with meaningful values.

## Function arguments

Function _domain_ defines all the possible arguments' values function can be called with. Function that works properly for all the values from its domain is a _total_ function. Unfortunately, most of the functions work correctly only for the selected values from the domain, they are _partial_.

For example, `void log(enum level, char*)` in C language is actually `void log(int, char*)` so it can be called with meaningless log level and pointer to hell, causing buffer overflows, etc. C++ adds level of type safety, so at least log level (without explicit cast of rubbish value) will be correct. Going further, we can substitute type of the second argument with `string_view` and prevent from buffer overflows. That is still too little, [more work is required](https://github.com/insooth/insooth.github.io/blob/master/blessed-split.md).

Let's consider following function that calculates distance in meters between two points on the map, or `0` in case of a failure:

```c++
/** Returns distance in meters if both points are correct, otherwise 0. */
unsigned distance(Point, Point);
```

Type `Point` is a tuple `(lat, lon, alt)` with some extra operations.

TBD

## Function return value

TBD
