
# Start with testable design right now

People tend to have _work done_ quickly. That is usually too fast for the most of them. Even if that unit tests are prevalent in software development we produce bugs. How is that possible when we have so much opportunities to test?

**Pro tip**: testable things can be tested well.

What makes the following function being testable

```c++
R foo(Ts... args);
```

is the design of its arguments, return values and body.

## Return values

Function that returns ``void`` gives little possibilities to check whether it succeeds or not. We can only observe side effects, which is the worst what can happen to us. In fact, family of functions from something to ``void`` usually involve side effects, like ``void C::push_back`` where ``C`` is one of the types that model [`SequenceContainer`](http://en.cppreference.com/w/cpp/concept/SequenceContainer "C++ concepts: SequenceContainer") concept. That's not a rule, for example ``std::ostream::write`` returns reference to self (which can be understood as _updated world_), so that w can build [fluent interfaces](https://en.wikipedia.org/wiki/Fluent_interface "Fluent interface"); and ``std::map::insert`` returns a pair that i.a. indicates whether the collection was actually modified (so that ``insert`` always succeeds).

Function's return value shall be informational. That means, if we transform value of type `T` into `U` we design function from `T` into `U` rather than from `T` into `E` or `void`. This is correct:

```c++
T convert(U&& u);
```

because can be readed easily as _convert U into T_. Note that function names that encode types, or are not simple on-word verbs, lead to unreadable API designs. For instance this is not the best choice:

```c++
T from_t_into_u(U&& u);
```

because we already know from the function signature from _what_ to _what_ is it, but we still don't know what it actually does.

What about input arguments? Simple answer is: almost never. For example, this function converts passed `u` into `t` where `t` is modified in place and returns an status of type `E` indicating range of possible errors or success (like `enum class` or simple `bool` value):

```c++
E convert(U&& u, T& t);
```
which requires pre-allocation of the `t` at the caller side, even if there is not way to convert `U` into `T`. What we should do if algorithm cannot be applied? We can throw na exception (not the best idea: no support from type system, C++ is not Java) or change the signature and handle errors gracefully. We can approach the latter as in the presented example that returns status of type `E`, or use sort of `monadic` style with [`std::optional`](http://en.cppreference.com/w/cpp/utility/optional "std::optional") or `Either` that contains error in its `first` or well-formed value in its `second` ("right") like:

```c++
std::pair<E, T> convert(U&& u);
```
There is a drawback of the above solution: `T` must model [`DefaultConstructible`](http://en.cppreference.com/w/cpp/concept/DefaultConstructible "C++ concepts: DefaultConstructible" ) concept. To fix that we can provide combination of the input argument:

```c++
std::pair<E, T> convert(U&& u, T&& t = T{});
```

that takes moved-in-`convert` value `t` (which requires `T` to model [`CopyConstructible](http://en.cppreference.com/w/cpp/concept/CopyConstructible "C++ concepts: CopyConstructible") or [`MoveConstructible`](http://en.cppreference.com/w/cpp/concept/MoveConstructible "C++ concepts: MoveConstructible") concept) or constructs it with default constructor (and thus `T` must model `DefaultConstructible` concept). Since most of the values are copy-constructible we extend range of the accepted types significantly. That is:

```c++
const auto r = convert<A>(100);
```

won't compile if `A` has deleted default constructor, but the following will work fine (see working example on [Coliru](http://coliru.stacked-crooked.com/a/03b34268f493cf22 "Coliru Viewer")):

```c++
A a{/* ... */};
const auto r = convert(100, a);
```

Monadic style with `optional` (aka `Maybe`) or `Either` compose easily, without if blocks checking results in beetween (error is forwarded, computation stops on the first error), as an example converting of value `V` into JSON string that contains some `metadata` and the value itself:

```c++
V v{/* ... */};
const auto r = stringify(join(metdata, serialize(v)));
```
where (`E` is error type, `N` is internal C++ representation of JSON structure, `either` is `std::pair`):

```c++
either<E, std::string> stringify(either<E, N>);
either<E, N> join(either<E, N>);
either<E, N> join(either<E, N>, either<E, N>);
either<E, N> serialize(V);
```
The above boils down to the programming principles: single responsibility and composability. Single responsibility makes software easier to understand thus easier to test.

# Input arguments

# Data types

- isolation
- explicit dependencies

# Possibilities

- runtime polymorphism, classic
- hi-perf dependency injection (cf. policy based design)
- linker magic
- variant driven (mid of policy-based design and runtime polymorphism)
- ?
 
What about the types, that are passed by value?

#### About this document

June 8, 2016 -- Krzysztof Ostrowski
