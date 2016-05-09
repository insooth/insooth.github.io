
# Partial function and magic values

Every function `f` takes some arguments of equal or different types `A₀` to `Aₙ` and returns some value of type `R`. Even if `f` returns _nothing_ (like `void` or `Unit`); even if `f` is nullary and takes no arguments; even that we can write down such a function in C++ as:

```c++
template<class R, class... As> R f(As...);
```

The above is valid _for all_ the `R` and arbitrary number of `As` types; `f` is fully parametrically polymorphic. Since the set of actions carried by `f` must be valid _for all_ the types, such function is almost not implementable. We want to limit allowed type parameters to some _bounds_. This can be realised with [C++ Concepts](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4377.pdf) during compile-time or more traditionally, with subtypes. Unfortunately, C++ subtype polymorphism does not work with values, we would need to switch to references and sacrifice some execution time.

Ideally, type encodes the set of possible values of it. For example, having type `EvenUint8` we describe only even `uint8_t` values, and nothing more. Such detailed description is possible (although in limited form) in languages that support algebraic data types (sum types), and C++ is not one of them. It does not mean we should condemn all that _type-rich_ hype and fallback to C. Let's find a way to assure we receive and reply with meaningful values.

## Function arguments

Function's _domain_ defines all the possible arguments' values function can be called with. Function that works properly for all the values from its domain is a _total_ function. Unfortunately, most of the functions work correctly only for the selected values from the domain, they are _partial_.

For example, `void log(enum level, char*)` in C language is actually `void log(int, char*)` so it can be called with meaningless log level and pointer to hell, causing buffer overflows, etc. C++ adds level of type safety, so at least log level (without explicit cast of rubbish value) will be correct. Going further, we can substitute type of the second argument with `string_view` and prevent from buffer overflows. That is still too little, more work is required.

Let's consider following function that calculates distance between two points on the map:

```c++
unsigned distance(Point, Point);
```

Type `Point` is a tuple of three `double` elements `(lat, lon, alt)` and some extra operations. Objects of that type can be default constructed, constructed by specifying all or selected tuple elements. That means, its objects are not always well-formed, thus limited number of operations can be applied to them. Since `Point` type is used in multiple ways by other parts of the system its design cannot be changed.

Given that, `distance` works only for selected values from the whole domain of possible `Point` objects, it is a partial function. By doing patten matching on the arguments we can verify and reject incorrect input, in C++:

```c++
unsigned distance(Point src, Point dst)
{
  const bool can_proceed = /* `src` and `dst` of (lat, lon, _) */
       (true == src.has_lat())
    && (true == src.has_lon())
    && (true == dst.has_lat())
    && (true == dst.has_lon());

  return (true == can_proceed) ? /* calculated distance */ : 0;
}
```

We left validation of `Point`'s internal data up to the type constructors. Note that we do not check third `Point`'s internal tuple value `alt` because it does not play a role in the distance calculation algorithm. Augmented `distance` always verifies input arguments during runtime.

## Function return value

Function's return type defines _co-domain_ that includes all the possible values of the return type. Function `distance` returns all the possible `unsigned` values, and value `0` is marked as magic to indicate invalid input or other errors. That is a common technique in C programming to return magic value indicating failure. Unfortunately, `distance` does it wrong.

Consider output of `distance(p, p)`, where `p` is an object of type `Point`. Distance between two same points shall be zero metres, while zero indicates failure. We found severe bug: in certain cases correct input produces correct output that is indistinguishable from failure. Due to fact it happens only _in certain cases_ during execution, it makes debgging real challenge.

How to fix it? One option is to change the return type to [`optional`](http://en.cppreference.com/w/cpp/experimental/optional) or its hand-made counterpart `pair<bool, unsigned>` that contains meaningful value in `second` if `first` is `true`. Changing from `unsigned` to signed `int` and returning `-1` in case of an error seems promising, but it widens the co-domain and shrinks collection of meaningful values which is not good news. Depending on context in which `distance` is used, warnings about mixing signed and unsigned may arise.

We can apply defensive programming through [blessed split](https://github.com/insooth/insooth.github.io/blob/master/blessed-split.md) and explicitly shift validation to the caller thus making `distance` always succeeding:

```c++
/**
 * Returns distance in metres between @c src and @c dst points.
 *
 * @pre @c src has @e lon and @e lat
 * @pre @c dst has @e lon and @e lat
 */
unsigned distance(Point src, Point dst)
{
  assert(true == src.has_lat());
  assert(true == src.has_lon());
  assert(true == dst.has_lat());
  assert(true == dst.has_lon());

  return /* calculated distance */;
}
```

Another case is that we calculate using floating points and return an integral value. That leads to situation where infinite number of different inputs lead to the same output. With points on the map described by floating point coordinates such behaviour constitutes a problem.

## Express precisely

Either we move into `optional<unsigned>` or make `distance` always succeeding, we will fix the bug and never return the magic values anymore. One topic is left tough. We pointed out in the documentation that returned value represents distance _in metres_. Let's express that in the type using [Boost.Units](http://www.boost.org/doc/libs/1_60_0/doc/html/boost_units.html):

```c++
boost::units::si::length distance(Point, Point);
```

This may be seen as a breaking change to the interface, but it is worth to be applied. Having it applied, we explicitly deal with _metres_, there is no longer meaningless `unsigned`, so that we cannot simply shoot in the foot by combining units. Wrong combinations won't compile. That's huge step toward better designs.

#### About this document

May 7, 2016 -- Krzysztof Ostrowski

