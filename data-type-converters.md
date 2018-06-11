# Writing data type converters

Converter can be understood as a function that encapsulates given algorithm that transforms a type into an another type. Simplest converters in the C++ world of classes are modelled by _conversion_ constructors and operators. They work pretty well as long as the designer of a class knows all the types to which its abstraction can be converted. If it comes to integrate a third-party library that exposes a series of types _almost_ equal to the types used by us, and the source code of the third-party library _must not_ be modified, we end up producing tons of out-of-an-abstraction converters.

Aside from picking the right name for the conversion function (here many fail), through choosing the adequate signature for such function (multiple fails here too), the converter idea itself is definitely worth a closer look. 


Wrong | Better
---   | ---
`void convertAppleToOrange(const Apple&, Orange&)` | `Orange convert(const Apple&)`
`Orange* /* allocates memory */ convertAppleToNewOrange(const Apple&)` | `std::unique_ptr<Orange> convert(const Apple&)`
`B convert(A&)` | `B convert(const A&)` or (even better if in generic context) `B convert(A)`
`/* may fail (gives false) but do not throw */` `bool convert(const A&, B&)`| `std::optional<B> convert(const A&)` or (if never throws) `std::optional<B> convert(const A&) noexcept`
`/* using Int = int; */` `EnumA convert(Int)` and `EnumB convert(Int)`  | `convert<EnumA>(Int)` and `convert<EnumB>(Int)` that are instances of `template<class T> std::enable_if_t<any_v<T, EnumA, EnumB>, T> convert(Int)` where `template<class T, class... Ts> class any;`
repeated `for (const auto& item : items) others.push_back(convert(item))` | generalise over all the [`SequenceContainer`](http://en.cppreference.com/w/cpp/concept/SequenceContainer) models: `SequenceContainer convert(const SequenceContainer&) { /* map convert(...) over input */ }`

## How it works

It is relatively easy to write converters between simple abstractions like enumerations and [fundamental types](http://en.cppreference.com/w/cpp/language/types). Since we are interested in conversions of _data values_, reference/pointer/function type and type qualifiers (like `const`) transformations are not discussed in this article. What's left in our scope are array types and classes. Having in mind that an array (or any other sequence of data, like `std::vector` or `std::map`) is just a way to organise values of some type, we can easily generalise the conversion to a mapping of an item-dedicated (fine-grained) `convert` function over the original sequence. Things are getting complicated for nested types and custom `class`es. Things are getting even more complicated for _sum_ types like `std::optional` or `std::variant` (just think about data access).

Let's forget about the headache-provoking details for a while and approach the problem using top-down strategy. Question is: what does it mean _to convert_ a data type? We definitely need to inspect the data type structure, i.e. _unwrap_ its abstraction or just some layers of it. Conversion comprises (roughly) three steps:
* querying the original data type to select the interesting part,
* recursively traversing the selected part and applying the fine-grained conversions,
* assembling the converted bricks to generate the data of the desired type.

The middle part can be done in parallel, providing that there is no inter-data dependencies that prevent from that. If done in parallel, the last step can be understood as a synchronisation point for asynchronous conversion tasks.

To generalise even further, we may think of a conversion as a pair of actions, where first is a "getter" applied to the original data, and the second is the "setter" applied to the data returned by the "getter". The "setter" in the example changes the received data's type. That's the idea of [Profunctor Optics](https://arxiv.org/abs/1703.10857) in fact &mdash; in our case &mdash; combined with a _state_ which is used to build up the result data.

## What can be a converter

If the converter function is an action from some type `A` to some other type `B` &mdash; in particular &mdash; `B convert(A)`, then is the member accessor a converter? Yes, it is! For instance, a _getter_ that accesses a member of type `A` of some non-sum type `S` (like `std::pair`) can be understood as a function from `S` to `A`; and, a _setter_ that modifies some value of a compound type `S` by updating one of its members of type `B` produces either the original compound type `S` or (in general) some other type `T`. That's mostly how the object-oriented paradigm is usually implemented in the C language. Consider:

Haskell | C | C++
--- | --- | ---
`view :: s →  a` | `const A* s__get_a(const struct S*)` | `/* S s; */ s.a` where `.` is the infix equivalent to `view`
`update :: (b, s) → t` | `T* s__set_b(struct S*, const B*)` where `T` equals `S` in the simplest case | `s.b = bb` where `T` equals `S`

The C++'s `s.b = bb` turns out to be a very interesting construction. While `s.b` mimics `view`, the `<expr> = bb` is the infix equivalent to `update` (of signature `(s, b) → s`). Let's take a closer look ([live on Coliru](http://coliru.stacked-crooked.com/a/4f6dfd77685f73b1)):

```
struct S { A a; B b; };

s.a;  // requires information on both S and A types -- gives a value of type A
      // in imaginary prefix notation: template<class A, class S> A .(const S&)


B bb;
s.b;      // gives value of type B, information about both S and B types is required
s.b = ... // ...but now left of "=" is of type B&
s.b = bb; // type of this expression should be S back
          //   NOTE: C++ yields here value of type B
          //         effectively disabling further composition like: (s.b = bb).a = aa
          // in imaginary prefix notation: template<class S, class B> S& =(S&, B)
```

In analogous way, accessors to sum types (like `std::variant` or `std::optional`) can be derived.

In the Optics world, a minimal "converter" with interface that exposes two actions, `S → A` &mdash; "from", "view", "match", "downcast", "get", "extract", etc.; and `B → T` &mdash; "to", "update", "build", "upcast", "set", etc.; where `A` and `B` are any types that may even combine with `S`; is called `Adapter` or `Iso`. Converters for sum types are called `Prism`s, while for a C++ "regular" types (_product_ like `std::pair` or `std::tuple`) are called `Lens`. To convert types that can be iterated over (like `std::vector` or `std::map`), `Traversal`s are used.

The Profunctor abstraction wraps given Optic (a "converter") into a composable abstraction. The wrapper adds layers of pre- and post-processing to the converter itself, and forces the user to provide implementation that does the actual composition.

That is, in the simplest case, having a pre-processing function `f` from `A'` to `A`, and a post-processing function `g` from `B` to `B'` we can compose given converter `h` from `A` to `B` by simply composing the actions &mdash; `g . h . f` (`g` after `h` after `f`).

The required composition is done by the Profunctor interface function called [`dimap`](http://hackage.haskell.org/package/profunctors-5.2.2/docs/Data-Profunctor.html#v:dimap) that translates some `A'` values into some `B'` values using the supplied converter. The magic here, is that the abstraction produced by `dimap` is a Profunctor's model itself (an Optic with additional data processing), thus it can be composed with another pre- and post-processing actions, and so on.

Slightly contrived example for `std::function` (i.e. `→`) acting as the simplest model of Profunctor concept ([live code on Coliru](http://coliru.stacked-crooked.com/a/bd38e6b94d99300e)) follows:

```c++
template<class X, class Y> using P = std::function<Y (X)>;

using A = int;
using B = int;

struct S { A a; B b; };
using  T = std::variant<B, double>;

T runDimap(std::function<A (S)> f
         , std::function<T (B)> g
         , P<A, B> h
         , S s)
{
    return g(h(f(s)));
}

auto t = 
    runDimap([](S s) { return s.a; }         // pre-processing
           , [](B b) { return T{b}; }        // posprocessing
           , [](A a) { return B{a + 100}; }  // conversion
           , S{11, 22}                       // we go from S to T
           );

// t is {111}
```

Composition is pretty straightforward:

```c++
template<class AS, class BT, class AB>
auto dimap(AS f, BT g, AB h)
{
    return [=](auto s) { return g(h(f(s))); }; 
}

// conversion sequence: S::a to B{a + 10},
//                      then to T,
//                      then to optional<B> with value if T holds B of value > 100

auto toMaybeB = 
  dimap([](auto x) { return x; }  // identity function
      , [](T t)    { return (std::holds_alternative<B>(t) && (std::get<B>(t) > 100))
                              ? std::optional<B>{std::get<B>(t)}  // redundant check: for exposition only
                              : std::nullopt; }
      , dimap([](S s) { return s.a; }
            , [](B b) { return T{b}; }
            , [](A a) { return B{a + 10}; }
            )
        );

// toMaybeB(S{100, 2}) is {110}
// toMaybeB(S{0, 2})   is std::nullopt
```

## Isos, Lenses, Prisms and Traversals

Unfortunately, at the time of writing, there is no production-ready C++ library that implements law-abiding Profunctor Optics available. Several experimental implementations exists, but those typically do not include Prisms and have limited support for Traversals.

The idea of Profunctor Optics is worth spreading as a general composable way of accessing and transforming nested data structures. Provided in this article example application followed by code snippets presents only a tip of the iceberg of the expressive power that Optics offer.

#### About this document

May 19, 2018 &mdash; Krzysztof Ostrowski

[LICENSE](https://github.com/insooth/insooth.github.io/blob/master/LICENSE)


