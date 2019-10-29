
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

## Making phantom types

https://wiki.haskell.org/Phantom_type
https://en.cppreference.com/w/cpp/language/aggregate_initialization
Aggregate init of base classes

## How to partially apply a class template

Carry an additional information within the type
Nested class template to the rescue
https://en.cppreference.com/w/cpp/language/type_alias
Alias template supports only one level of nesting: size of a stack of parametrised types introduces by alias template is equal to one

## Deduction guides cannot be _partially applied_

CTADs
https://en.cppreference.com/w/cpp/language/type_alias

## Potential issue

Relation between tag and a member function under test is not checked. That is, convenience ensures that `Foo` identifies `foo` member function, nothing more. There is no explicit compile-time rule that prevents from assignment of `bar` to `Foo` tag.

We need to associate tag with a particular member function signature, and verify that during mock construction.

```c++
template<class T>
struct drop_const : std::common_type<T> {};

template<class R, class C, class... As>
struct drop_const<R (C::*)(As...) const> : std::common_type<R (C::*)(As...)> {};


template<class T, class U>
struct is_delegate : std::false_type {};

template<class R, class C1, class C2, class... As>
struct is_delegate<R (C1::*)(As...), R (C2::*)(As...)> : std::true_type {};

template<class Tag, class F>
constexpr auto is_delegate_v =
    is_delegate<typename Tag::type
              , typename drop_const<decltype(&F::operator())>::type
//                       ^~~ to accept mutable and non-mutable lambdas
              >::value;





struct Bar : std::common_type<int (Testable::*)(int)> {};
//                ^^~~ used as an identity_type

auto bar = [](int) -> int { return 0; };

static_assert(is_delegate_v<Bar, decltype(bar)>);  // OK

```


#### About this document

October 29, 2019 &mdash; Krzysztof Ostrowski

[LICENSE](https://github.com/insooth/insooth.github.io/blob/master/LICENSE)

