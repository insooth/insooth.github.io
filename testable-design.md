
# Start with testable design right now

People tend to have _work done_ quickly. That is usually too fast for the most of them. Even if that unit tests are prevalent in software development we produce bugs. How is that possible when we have so much opportunities to test?

**Pro tip**: testable things can be tested well.

What makes the following function being testable

```c++
R foo(Ts... args);
```

is the design of its arguments, return values and scope of responsibility taken by it.

## Return values

Function that returns ``void`` gives little possibilities to check whether it succeeds or not. We can only observe side effects, which is the worst what can happen to us. In fact, family of functions from something to ``void`` usually involve side effects, like ``void C::push_back`` where ``C`` is one of the types that model [`SequenceContainer`](http://en.cppreference.com/w/cpp/concept/SequenceContainer "C++ concepts: SequenceContainer") concept. That's not a rule, for example ``std::ostream::write`` returns reference to self (which can be understood as _updated world_), so that w can build [fluent interfaces](https://en.wikipedia.org/wiki/Fluent_interface "Fluent interface"); and ``std::map::insert`` returns a pair that i.a. indicates whether the collection was actually modified (so that ``insert`` always succeeds).

Function's return value shall be informational. That means, if we transform value of type `T` into `U` we design function from `T` into `U` rather than from `T` into `E` or `void`. This is correct:

```c++
T convert(const U& u);    // or:     auto convert(const U& u) -> T;
```

because can be readed easily as _convert U into T_ (taking fundamental types by reference is not what we would like to do &mdash; thus we will need to control overload set with `enable_if` to choose `convert` that takes value rather than reference).

Note that function names that encode types, or are not simple one-word verbs, lead to unreadable API designs. For instance this is not the best choice:

```c++
T from_t_into_u(const U& u);
```

because we already know from the function signature _from what to what_ it is, but we still don't know what it actually does.

What about input (taken by non-const reference or pointer) arguments? Simple answer is: almost never use them. For example, this function converts passed `u` into `t` where `t` is modified in place and returns an status of type `E` indicating range of possible errors or success (like `enum class` or simple `bool` value):

```c++
E convert(const U& u, T& t);
```
which requires pre-allocation of the `t` at the caller side, even if there is not way to convert `U` into `T`. What we should do if algorithm cannot be applied? We can throw na exception (not the best idea: no support from type system, C++ is not Java) or change the signature and handle errors gracefully. We can approach the latter as in the presented example that returns status of type `E`, or use sort of _monadic_ style with [`std::optional`](http://en.cppreference.com/w/cpp/utility/optional "std::optional") or `Either` that contains error in its `first` or well-formed value in its `second` ("right") like:

```c++
std::pair<E, T> convert(const U& u);
```
There is a drawback of the above solution: `T` must model [`DefaultConstructible`](http://en.cppreference.com/w/cpp/concept/DefaultConstructible "C++ concepts: DefaultConstructible" ) concept. To fix that we can provide combination of the input argument:

```c++
std::pair<E, T> convert(const U& u, T&& t = T{});
```

that takes moved-in-`convert` value `t` (which requires `T` to model [`CopyConstructible`](http://en.cppreference.com/w/cpp/concept/CopyConstructible "C++ concepts: CopyConstructible") or [`MoveConstructible`](http://en.cppreference.com/w/cpp/concept/MoveConstructible "C++ concepts: MoveConstructible") concept) or constructs it with default constructor (and thus `T` must model `DefaultConstructible` concept). Since most of the values are copy-constructible we extend range of the accepted types significantly. That is:

```c++
const auto r = convert<A>(100);
```

won't compile if `A` has deleted default constructor, but the following will work fine (see working example on [Coliru](http://coliru.stacked-crooked.com/a/03b34268f493cf22 "Coliru Viewer")):

```c++
A a{/* ... */};
const auto r = convert(100, a);
```

Monadic style with `optional` (aka `Maybe`) or `Either` compose easily, without if blocks that check results in beetween (error is forwarded, computation stops on the first error), as an example converting of value `V` into JSON string that contains some `metadata` and the value itself:

```c++
V v{/* ... */};
const auto r = stringify(join(metdata, serialize(v)));
```
where (`E` is error type, `N` is internal C++ representation of JSON structure, `either` is `std::pair`; possible references in arguments' types stripped):

```c++
either<E, std::string> stringify(either<E, N>);
either<E, N> join(N, either<E, N>);
either<E, N> serialize(V);
```
The above boils down to the programming principles: single responsibility and composability. Single responsibility makes software easier to understand thus easier to test.

# Arguments

Arguments should be as much strictly typed as possible, ideally using types that allow control over implicit conversions. We should think about defined function in "forall" terms, i.e. if they were defined for whole domain of possible arguments ([read more about that](https://github.com/insooth/insooth.github.io/blob/master/partial-functions-magic-values.md "https://github.com/insooth/insooth.github.io/blob/master/partial-functions-magic-values.md")).

Having all the functions polymorphic is great, but usually comes with huge cost. Polymorphism as understood in object-oriented world is based on interfaces that are required to be exposed by subtypes. In C++ it works only for "designators", so for pointers (and references). For instance:

```c++
T foo(A& a);
```

will work fine for all the types covariant to `A`, and if `A` is an abstract class (contains at least one pure `virtual` member function) we deal with "interface" &mdash; the main idea of the object-oriented design. We can simply use that idea to inject our prepared value of type `M` that implements `A` interface (in other words: derives `public`ly from `A` and `override`s its `virtual` member functions). That's the way how most of the mocking frameworks work (e.g. widely used [GMock](https://github.com/google/googlemock/blob/master/googlemock/docs/CookBook.md#creating-mock-classes "Creating Mock Classes")), and that's the way we can test with no particular mocking framework at all. We can even mock `protected` and `private` `virtual` member functions too due to fact their actual location is resolved during runtime. Should we make then types of all the arguments being references or pointers? Certainly not &mdash; it is usually bad idea (and no benefit) for fundamental types.

What if we have no interface defined and want to inject mocked value of unrelated type? We can use here implicit conversion to destination type. Implicit conversion can be achieved through conversion `operator` or conversion constructor. For example (see working code on [Coliru](http://coliru.stacked-crooked.com/a/fd8f5725a5c34459 "Coliru Viewer")):

```c++
T bar(A a);
```

will accept unrelated `B` if:

```c++
struct B
{
  operator A() { return A{}; }
};
```

or `A` defines conversion constructor:

```c++
struct A
{
  A(const B&) { /* ... */ }
};
```

Unfortunately it gives us nothing, because by converting to non-mocked type, we loose altered behaviour we ship with the mock. Type that is mocked will preserve the altered behaviour only if exposes constructor that is dependency-injection aware.

If our design is based on parametric polymorphism, so that we rely on injected types through templates, we can easily substitute any of the types with mocks. For example:

```c++
template<class T, class U>
void injectable(T t, U u);
```

can be called with any `T` and any `U`. We can use type deduction and ask compiler to figure out types of passed arguments for us. If we want to limit the set of acceptable type of some semantics, we shall use predefined one or define custom [concept](http://en.cppreference.com/w/cpp/language/constraints "Constraints and concepts").

Type deduction will not work with all templated code out-of-the-box, due to fact that templates match the exact type as it was passed. This is the main concern for lambda expression that are _convertible_ to `std::function` but implicit conversion won't work in the templated code:

```c++
template<class A>
T baz(std::function<void (A)>&&);

baz([](int){});  // error: lambda is not derived from std::function
```

Type `A` is deduced and substituted, no conversion from lambda do argument type happens. To overcome that, we can disable deduction with `identity` metafunction defined as:

```c++
template<class T>
using identity_t = std::common_type_t<T>;
```

then the following (see working code on [Coliru](http://coliru.stacked-crooked.com/a/a0c9122f40a9ac38 "Coliru Viewer")) works fine (note explicit type specification in a call to `baz` which is required):

```c++
template<class A>
T baz(identity_t<std::function<void (A)>>&&);

baz<int>([](int){});
```

We can reuse that knowledge in mocking, but it will lead us to changes in the tested functions' signatures which we rather should not try to do. That shares similar concern as subtype polymorphic arguments described at the begining of this paragraph.

[GMock guide defines several ways](https://github.com/google/googlemock/blob/master/googlemock/docs/CookBook.md#mocking-nonvirtual-methods "Mocking Nonvirtual Methods") to test non-virtual/free functions as a whole, which is much more than simply injecting dependencies.

# Body

Less code requires less tests. That is, our goal is to keep bodies of functions as short (in terms of lines of code) and simple as possible. Simple here means:
* use of already tested tools (like STL, Boost or internal libraries) &mdash; favouring composition over modification,
* keeping control flow as straight as possible &mdash; thus reducing [cyclomatic complexity](https://en.wikipedia.org/wiki/Cyclomatic_complexity "Cyclomatic complexity") and number of paths (~test cases) to be tested,
* expressing all the possible side effects that can be introduced by function explicitly (in types, comments, through abusing `const` qualifier, etc.) &mdash; no surprises and hidden dependencies,
* introducing single reposibility &mdash; split complex thing into simple steps that compose,
* using contracts and constraints (like assertions and [Concepts](http://en.cppreference.com/w/cpp/language/constraints "Constraints and concepts")).

Programming by adding new `if-else` blocks is a sign of bad design. We should strive to design algorithms that work [for all the data from the arguments' domains](https://github.com/insooth/insooth.github.io/blob/master/partial-functions-magic-values.md "Partial function and magic values"). That's usually hard to achieve and we end up with multiple _special cases_. We shall have as little as possible special cases. We can try to limit number of them by rearranging or reducing boolean expressions that activate special cases &mdash; doing that usually improves code readability. If that does not help, we probably have misunderstood single responsibility or reinventing the wheel, redesign is required.

# Data types

Well designed software is possible to be tested _in isolation_, that is each its module can be tested independently from the system. Ideally, such software shall be possible to be built for the development system, not only for the target, thus enabling full set of developer tools (like debuggers, sanitizers, etc.).


Parts of the system that are not under the isolated tests are _mocked_, so that they are replaced by software that simulates (or emulates) the environment. It is very important to enable system for mocks thus making it mockable, i.e. all its dependencies can be replaced by mocks. Following `class` has non-explicit dependencies which are not seen at first sight and require special way of mocking:

```c++
class X
{
 public:
    X();
    void run(const std::chrono::milliseconds& timeout) const;
 private:
    std::string s;
};

X::X()
{
    G::make_instance(s);
}

void X::run(const std::chrono::milliseconds& timeout) const
{
    G::get(s)->run(timeout);
}
```

Class `X` has hidden dependency in `G` and its internals. One can find a way to use shared libraries and force linker to select mocked symbol instead of the original one. That's possible, but it hides the problem: we have produced non-testable design. We need to redesign to make it possible to inject dependency.

## The fastest: _hi-perf dependency injection_

We can inject dependency into `X` through type, effectively making `X` a template that takes a strategy and binds to it at compile time:

```c++
template<class T = G>
class X : T
{
    using T::run;
    using T::make_instance;

 public:

    X()
    {
      T::make_instance(s);
    }

    void run(const std::chrono::milliseconds& timeout) const
    {
      T::get(s)->run(timeout);
    }

 private:
    std::string s;
};
```

Presented technique is known as [policy-based design](https://en.wikipedia.org/wiki/Policy-based_design "Policy-based design"), and in GMock world [hi-perf dependency injection](https://github.com/google/googletest/blob/master/googlemock/docs/CookBook.md#mocking-nonvirtual-methods "Mocking Nonvirtual Methods").

## The (slowest) classic: runtime polymorphism

Classic object-oriented way is to introduce an interface:

```c++
struct I
{
    virtual void make_instance(std::string&&) = 0;
    virtual void run(const std::chrono::milliseconds& timeout) const = 0;
};
```

and then take an object which is the concrete implementation (strategy) of `I` at run time as one of the `X` constructor's argument.

## The faster than slowest: Variant-driven

Data type [variant<T0, T1, ...>](https://isocpp.org/blog/2015/11/the-variant-saga-a-happy-ending "The Variant Saga: A happy ending?") stores exactly one value of the type taken from set of allowed types passed as paramters to template at compile time. We can use that knowledge, combine it with [PIMPL idiom](https://en.wikipedia.org/wiki/Opaque_pointer "Opaque pointer"), and add into `X` one new member:

```c++
class X
{
    X() : m_impl{G{}} {}
    explicit constexpr X(Mock&& m) : m_impl{std::move(m)} {}

    /* ... */

    variant<Mock, G> m_impl;
};
```

[Boost's `variant`](www.boost.org/doc/libs/1_61_0/doc/html/variant.html "Boost.Variant") can work with incomplete types, so that we can create illusion of PIMPL.

The hard part is the way to _visit_ variant, forward arguments (here `timeout`) to the visitor and finally call that function. We do not use runtime dispatch through virtual functions as in the `I` interface, but variant does such dispatch internally anyway to select the value currently stored. If `I` contains multiple virtual member functions, variant comes with benefit in reduction of virtual function calls. On the other hand, we are expilicitly bound to the `Mock` and `G` types, thus reducing flexibility.

## The hack: linker magic

Let's say we want to inject mocked `G::make_instance` only, without making changes to the original code. What we actually want to do is to instruct linker to select symbol of `G::make_instance` from translation unit with mocks.

Trick with linker requires split of tests and tested code into (dynamic) shared libraries. If we define an implementation with mocked body in library with test code:

```c++
void G::make_instance(std::string)
{
    /* ... mocked body ...*/
}
```

compile it, and then link against shared library with tested code that has original implementation of the above member function, we will always use mocked one since it was already linked into our test code.

#### About this document

June 8-16, 2016 -- Krzysztof Ostrowski

[LICENSE](https://github.com/insooth/insooth.github.io/blob/master/LICENSE)
