
# Phantom lambda type, partially applied class template, and deduction guides to support dependency injection

This article is a follow-up to the article [_Creating testable interfaces_](https://github.com/hliberacki/hliberacki.github.io/blob/master/testable_interfaces.md) written by Hubert Liberacki, and the metaprogramming ideas the author shared with me through live coding session.

## Recap

By making the class under test a class template parameterised by an "injected type" (cf. [article on customisation points](https://github.com/insooth/insooth.github.io/blob/master/customisation-points-support-interfaces.md)) we delegate member functions, and remaining class internals if required, to some type `T` that promises to provide the behaviour we expect (a `concept` as of C++20). In the GMock-world this technique is called "hi-perf dependency injection" (complementary techniques [are described in a dedicated article](https://github.com/insooth/insooth.github.io/blob/master/testable-design.md)).

```c++
template<class Injected>
class Testable
{

 public:
 
    void foo() { d.foo(); }

    int bar(int i) { return (d.foo(), d.bar(i)); }
//                          ^^^^^^^^^^^^^^^^^^^ don't try this at work
 private:
 
    Injected d;
};
```

This article puts the focus on the construction of the `Injected` type, and presents a neat way to produce it directly out of the user-provided (free) actions realised with function objects.

## Mock constructor

Mock constructor is a template that, once fixed with `Fs` type of function objects, can be passed to `Testable` as `Injected` parameter. It is a convenient wrapper for a `tuple` of `Fs`.

```c++
template<class... Fs>
struct M
{
    M(Fs... fs) : fs{fs...} {}
    
    //! @see Testable interface
    void foo()     { /* ... */ }
    int bar(int i) { /* ... */ }
    
    std::tuple<Fs...> fs;
};
```

`M` involves [CTAD](https://en.cppreference.com/w/cpp/language/class_template_argument_deduction) with the compiler-generated deduction guide that makes the following possible:

```c++
M m
{
    [] { /* no need to specify Fs types */ }
  , [](int) -> int { return 0; }
// ...more to come
};
```

The general question arises here: how to uniquely associate a particular lambda expression of type `Fs` with the `Testable` interface member functions during construction of a mock `M`?

## Making phantom types

[Haskell wiki](https://wiki.haskell.org/Phantom_type) defines _phanton type_ as:

> A phantom type is a parametrised type whose parameters do not all appear on the right-hand side of its definition

Which translates to a C++ class template in which not all of the passed parameters are relised in the memory (i.e. they exist only during compilation time). Phantom type is a type with a tag available only during compile-time attached. 

```c++
template<class Tag, class U> struct box { U u; };
//                                      ^~~ right-hand side starts here (nested typedefs do not count)


box<struct One, int> x{123};
box<struct Two, int> y{123};

static_assert(sizeof(decltype(x)) == sizeof(decltype(y)));  // equal memory consumption
assert(0 == std::memcmp(&x, &y, sizeof x));  // same data
assert(0 == std::memcmp(&x, &y, sizeof y));  // same data
static_assert( ! std::is_same_v<decltype(x), decltype(y)>); // different types
```

We will produce a tagged lambda by means of a phantom type, and inheritance to take over the behaviour of a passed lambda `F`. [Aggregate initialisation](https://en.cppreference.com/w/cpp/language/aggregate_initialization) of a public base class will be used. 


```c++
template<class Tag, class F>
struct box : F
{
    using F::operator();  // 

    // keep type info for 
    using type     = F;
    using tag_type = Tag;
};
```

Example usage:

```
box<struct Foo, ???> x{ []{ /* noop */ } };  // impossible to know the type of a lambda

auto f = []{ /* noop */ };
box<struct Boilerplate, decltype(f)> y{std::move(f)};
```

Since we know the tag, and the type of the passed lambda is not known until we define it, a helper function will be helpful.

```c++
template<class Tag, class F>
constexpr auto boxify(F f)
{
    return box<Tag, F>{f};
}

// example
auto f = boxify<struct Clean>([] {});
```

## Deduction guides cannot be _partially applied_

We may be tempted to write a deduction guide instead of a `boxify` "make" function.

```
// invalid
template<class Tag, class F>
box(F) -> box<Tag, F>;

box<Tag> wrong{[]{}};
```

That will not work, [CppReference quotes the ISO C++ standard](https://en.cppreference.com/w/cpp/language/class_template_argument_deduction):

> Class template argument deduction is only performed if no template argument list is present. If a template argument list is specified, deduction does not take place.

## How to partially apply a class template

Carry an additional information within the type
Nested class template to the rescue
https://en.cppreference.com/w/cpp/language/type_alias
Alias template supports only one level of nesting: size of a stack of parametrised types introduces by alias template is equal to one

## Potential issue

Relation between tag and a member function under test is not checked. That is, convenience ensures that `Foo` identifies `foo` member function, nothing more. There is no explicit compile-time rule that prevents from assignment of `bar` to `Foo` tag.

We need to associate tag with a particular member function signature, and verify that during mock construction.

```c++
template<class T, class U>
struct is_equiv : std::false_type {};

template<class R, class C1, class C2, class... As>
struct is_equiv<R (C1::*)(As...), R (C2::*)(As...)> : std::true_type {};

template<class T>
struct drop_const : std::common_type<T> {};

template<class R, class C, class... As>
struct drop_const<R (C::*)(As...) const> : std::common_type<R (C::*)(As...)> {};

template<class Tag, class F>
using is_delegate = 
    is_equiv<typename Tag::type
           , typename drop_const<decltype(&F::operator())>::type
//                    ^~~ to accept mutable and non-mutable lambdas
           >;

template<class Tag, class F>
constexpr auto is_delegate_v = is_delegate<Tag, F>::value;
```

A concrete example follows.

```c++
struct Bar : std::common_type<int (Testable::*)(int)> {};
//                ^^~~ used as an identity_type

auto bar = [](int) -> int { return 0; };

static_assert(is_delegate_v<Bar, decltype(bar)>);  // OK
```

Actual check shall be done inside the mock's constructor by means of a static assert.


```c++
template<class... Fs>
struct M
{
    M(Fs... fs) : fs{fs...}
    {
        static_assert(std::conjunction_v<
            is_delegate<typename Fs::tag_type, typename Fs::type>...
            >);
    }
// ...
};
```


#### About this document

October 29, 2019 &mdash; Krzysztof Ostrowski

[LICENSE](https://github.com/insooth/insooth.github.io/blob/master/LICENSE)

