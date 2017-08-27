# The role of the visitor pattern

Among all the various design patterns widely used by the C++ language community, probably only the visitor pattern takes advantage of the language type system in such a sophisticated way. Visitor pattern provides a solution to a recurring problem found in object-oriented designs (and in any other that use runtime-supported subtype polymorphism), namely the determination of the actual type of the object referenced by some interface type. This article will revisit the core idea behind that design pattern, mention couple of existing implementations, and describe a C++14 implementation of the visitor that aims to be flexible and type-safe as much as the template-based solution is, but remains in the domain of fixed (non-template) types favoured by the classic interfaces based on the `virtual` member functions.

## The task

Consider types `B` and `C` that implement interface `A`, and a function `provide` that returns an object of a type of a randomly selected one of them.

```c++
struct A { virtual std::string name() const = 0; };

A& provide();  // returns *ANY* object derived from type A


struct B : A { std::string name() const override { return "B"; } };
struct C : A { std::string name() const override { return "C"; } };
// ... more derived types may follow
```

We would like to call the dedicated action based on the actual type of the object returned by the `provide` function. That is, we are interested in the type of the most derived class, not in the interface type. Naïve solution may be:

```c++
decltype(auto) x    = provide();
auto           name = x.name();  // name is a "tag" that identifies the type


if      ("B" == x.name()) { /* action for B */ }
else if ("C" == x.name()) { /* action for C */ }
else    /* unsupported */ { assert(false);     }
```

Visitor pattern provides a solution to that problem using solely the C++ type system. That is, there are no magic "tags" that are used to restore the (derived) type erased by the use of an interface. *Erased* in this context relates to a difference between the type seen by the compiler and the the actual type of the object stored in the memory. In that sense, it defines the gist of the (dynamic) subtype polymorphism as implemented in the object-oriented languages.

## The types

Basically, the compiler assigns a type to the given entity without analysing the details of a data flow. That means, the type is assigned directly based on information provided explicitly by the user, or it is calculated through the evaluation of a meta program, through a type inference algorithm or using the argument type deduction (cf. function templates).

```c++
void choice(A&) {}
void choice(B&) {}

struct B : A
{
  virtual void choose() // NOTE: virtual is irrelevant here
  {
    // compiler knows that choose() is a member function of B,
    // thus type of `this` is unambiguously set to B* DURING
    // compilation time ("statically")

    choice(*this); // calls choice( B &)
  }
// ...
};


B b;        // obvious thing: "static" type of `b` is B

A& x = b;   // compiler assigns `x` the type of A&, although the "real" type of `x` is B

// value of type B is described by the OTHER type (here it is the A
// which is the supertype (base class) of the B) -- that is the whole
// idea behind the subtype polymorphism

choice(x);  // calls choice( A &)

// `x` is "dynamically" polymorphic handle to *ANY* object derived from A

```

There is no way to determine statically (i.e. during compile time) the actual type of the object having a polymorphic handle to it. Note, type casting is not considered since it escapes the type system. Visitor pattern shines here as a proper tool for determining the type of a value, i.e. it is used as a tool for "restoring" the type erased by a polymorphic handle. That tool is commonly used by STL (e.g. `std::variant`, `std::function`, etc).


## The bindings

Calling member functions against a ("dynamic" subtype) polymorphic handle instructs the compiler to locate the address of the function in question, and to insert it explicitly in the binary, that's so called "early" bind. There is no difference between binding functions in the context of polymorphic and non-polymorphic values, unless the given member function is marked as a `virtual`. Virtual member functions are not early bound, i.e. they are "late" bound. Instead of inserting a fixed address pointing the required function explicitly in the binary, compiler inserts an address to the function that *will* redirect to the required member function. Redirection happens in the runtime. Only the runtime is able to judge about the actual type of an object pointed by the polymorphic handle. C++ allows only polymorphic handles of reference and pointer type, there is *no late binding for values* (slicing happens).


```c++
struct A { virtual std::string name() const = 0; };
//         ^^^^^^^

struct B : A
{
  void choose() { /* ... */ };

  std::string name() const override { return "B"; }
//                         ^^^^^^^^

  virtual void foo() { /* ... */ }
//^^^^^^^
};

struct C : B
{
  void foo() override { /* ... */ }
//           ^^^^^^^^

// ....
};


// POLYMORPHIC

B b;

A& x = b;

x.name();   // late binding: calls B::name(), even if the type of `x` is A

x.choose(); // early binding: error: no such function A::choose()


// NON POLYMORPHIC

C c;

B z = c;    // wrong: `z` is not polymorphic, `z` is just a part of `c` object's memory

z.foo();   // early binding: calls B::foo() *NOT* C::foo(), the `c` type has been upcasted to B type
```

Through the use of a `virtual` member function we can access the actual object having a polymorphic handle to it. That is called the single dispatch on `this` pointer (dispatch always happens during runtime). The `this` pointer must be adjusted to match the actual object pointed by the polymorphic handle, and the required member function defined for the determined object must be found and bound. All that happens during runtime, and all that has its costs.


# The visitation

We visit using a visitor object, and we can visit only a set of the "visitable" types, that is all the types that implement the the following interface:

```c++
struct Visitable { virtual void visit(Visitor& v) = 0; };
//                                    ^~~ type of an object that performs visitation

```

The `Visitor` type is a function object that can be applied against objects of types derived from the type `A` (cf. string comparison in the naïve solution).


```c++
struct Visitor
{
  void operator()(B& b) { /* action for B */ }
  void operator()(C& c) { /* action for C */ }

  // the else-path (unsupported object) is a compile error,
  // instead of a runtime assert as it was in the naïve solution
};
```

It is not always possible to make `Visitable` type a superclass of all the types we would like to (cf. third-party libraries), thus we introduce it in the middle of the inheritance tree. That forces us to use the `dynamic_cast` to cross cast from the received polymorphic handle to the `A` type (returned by the `provide` function) to the `Visitable` type.


```c++
struct B : A, Visitable
{
  std::string name() const override { return "B"; }
  void visit(Visitor& v) override   { v(*this); }
//                                      ^~~ statically determined type...
};

struct C : A, Visitable
{
  std::string name() const override { return "C"; }
  void visit(Visitor& v) override   { v(*this); }
//                                    ^~~ ...early bind of the operator()
};


Visitor v;

decltype(auto) x = provide();

// having polymorphic handle to A we can downcast
// using static_cast to B or C, but not to Visitable
// 
//  +---+
//  | A |
//  +---+
//    ^
//    |
//    | up/down cast
//    |
//  +---+  cross cast  +-----------+
//  | B |------------->| Visitable |
//  +---+              +-----------+
//
// dynamic_cast for references throws bad_cast if cannot cast


dynamic_cast<Visitable&>(x).visit(v);  // late bind: calls B::visit or C::visit
```

Virtual member function `visit` is used to access the actual object pointed by the polymorphic handle. Having done the dispatch through that function we execute its body. Since all the types in its body are already calculated by the compiler and put in the binary, the passed `v` visitor function object is called against the `*this` object of the derived type (here it is either `B` or `C`). The visitor's function call operators are bound early too.


# The limitations

The above presented visitor pattern implementation has a notable drawback: it is not composable. There is no way to alter the visitor actions on-the-fly. That may be solved by turning the `Visitor` function call operators to `virtual` member functions, and then by using derived types that provide alternative actions' implementations. Another idea is to lift the `visit` member function to a member function template that accepts any visitor type (i.e. either generate multiple virtual member functions per every visitor type, or strip the `virtual` keyword and turn the `visit` into a member function template). The former solution is the classic OOP approach to visitor pattern that introduces double dispatch (firstly through `visit`, secondly through virtual function call operator defined by the `Visitor`). The latter is the [Alexandrescu's](http://loki-lib.sourceforge.net/html/a00685.html) visitor that [can be realised through "static polymorphism" aka CRTP](https://stackoverflow.com/questions/7876168/using-the-visitor-pattern-with-template-derived-classes) too.

The solution proposed in this article keeps `visit` member function virtual, and its argument type fixed. Customisation point is located in the `Visitor` type itself. User-defined actions are injected during construction of a visitor object. Actions have signatures well defined by their actual type is erased with a help of a polymorphic function wrapper, `std::function`.

```c++
class Visitor
{

 public:

  template<class BAction, CAction>
  Visitor(BAction b, CAction c)
    :
      actOnB{std::move(b)}
    , actOnC{std::move(c)}
  {}

  void operator()(B& b) { actOnB(b); }
  void operator()(C& c) { actOnC(c); }

 private:

  std::function<void (B&)> actOnB;
  std::function<void (C&)> actOnC;
};


Visitor v
  {
    [](B& b) { /* action for B*/ }
  , [](C& c) { /* action for C*/ }
  };


decltype(auto) x = provide();

dynamic_cast<Visitable&>(x).visit(v);  // late bind: calls B::visit or C::visit
```

Visitor with injectable actions is a flexible solution that needs further work. Following drawbacks are easy to be noticed:
* adding support for new visitable types added recently to the inheritance hierarchy (subtypes of `A`) is cumbersome,
* all the actions for all the supported visitable types must be specified (even if not desired),
* actions for the supported visitable types must be specified in strictly defined order,
* application of the visitor to an unsupported type generates hard-to-grasp compile time errors.

First two bullets apply to the presented previously alternative visitor implementations too.

# The solution

One of the goals that will improve the overall user experience with the `Visitor` is the reduction of the amount of work required to add support for new visitable types. It would be great to have a single place in code where the change has to be done, like a set of types for which the `Visitor` is allowed to work, and it would be great to have the rest of the `Visitor` contents automatically generated by the compiler. Let's define the single point of extension as:

```c++
using visitable_types = std::tuple<B, C>;  // tuple works here as a "type list" of UNDECORATED types
```

In order to support an additional visitable type, user has to tweak the `Visitor` class constructor, then add a dedicated function call operator, and extend the class members with a polymorphic function wrapper that will keep the action passed by the user. All that code has to be generated by the compiler having `visitable_types` "type list" as a sole input. To achieve that, we will write a couple of meta-programs (i.e. programs that operate on types and constant expressions).

## The constructor

We are required to work with any number of different types of actions, so that we will define the constructor as a variadic template that stores all the actions in the `std::tuple` of actions. That is, instead of multiple members that keep the passed actions, we are going to use a heterogeneous iterable container (`std::tuple`) of actions, in which every action can be accessed through the action type.

```c++
using action_types =
  std::tuple<
    std::function<void ( B &)>  // see actOn* members from the previous example
  , std::function<void ( C &)>
  >;
```

Careful reader will notice immediately that the action type is parametrised by a visitable type only, and it can be easily lifted to a type constructor:

```c++
template<class T> 
using action_type = std::function<void ( T &)>;
//                                       ^~~ any type from visitable_types can be put here
```

Having defined a parametric `action_type` as a type constructor that produces an action type for any passed `T`, we can generate the `action_types` type with a meta-program called `make_action_types` that simply iterates over the `visitable_types` type list and applies a subsequent visitable type to `action_type` type constructor.

```c++
using action_types = make_action_types_t<visitable_types>;
```

where

```c++
template<class Ts>
struct make_action_types;

template<class... Ts>
struct make_action_types<std::tuple<Ts...>>
{
  using type = std::tuple<action_type<Ts>...>;
//                        ^~~ generates an action type
};

template<class Ts>
using make_action_types_t =
    typename make_action_types<Ts>::type;
```

And now, we can define all the actions we are going to use as a `Visitor` object's member called `actions` of the generated type `action_types`.

The `action_type` data type is a parametrically polymorphic type, so that it can be fixed with any type `T`. That's not acceptable for us, because we want to have a set of possible `T` types limited to those stored in the `visitable_types` list of types. To ensure that, we will perform a compile-time check in the constructor body. The constructor itself takes a variadic number of values of the unspecified types (denoted as `Fs...`). Those values must model [`Callable`](http://en.cppreference.com/w/cpp/concept/Callable) concept to be accepted by the `std::function`. 

```c++
template<class... Fs>
Visitor(Fs... fs)
{
    static_assert(are_actions_allowed<visitable_types, Fs...>::value
                , "Unexpected action passed");

// ...
```

Meta-program `are_actions_allowed<AllowedVisitableTypes, PassedActionType...>` implements the following algorithm:
* for each type `F` in `PassedActionType...` types it determines type type of the `F`'s first argument (called later `arg0_type`) by inspecting `F::operator()`, and then...
* it looks for the `arg0_type` in the `visitable_types` type list, and returns that search result (boolean value) to the user.

```c++
template<class T, class F, class... Fs>
struct are_actions_allowed
{
  static const bool value =
        is_within<std::remove_reference_t<arg0_type<F>>, T>::value  // process head
     && are_actions_allowed<T, Fs...>::value;                       // process tail
};

template<class T, class F>
struct are_actions_allowed<T, F>  // recursion stop condition
{
  static const bool value =
        is_within<std::remove_reference_t<arg0_type<F>>, T>::value;
};
```

where `is_within` returns `true` if `T` is included in the type list `U`, otherwise `false`

```c++
template<class T, class U>
struct is_within
  : std::false_type
{};

template<class T, class... Ts>
struct is_within<T, std::tuple<T, Ts...>>
  : std::true_type
{};

template<class T, class U, class... Ts>
struct is_within<T, std::tuple<U, Ts...>>
  : is_within<T, std::tuple<Ts...>>
{};
```

and 

```c++
template<class F>
using arg0_type = typename action_traits<F>::template arg_type<0>;
```

where

```c++
template<class F>
struct action_traits
  : action_traits<
      decltype(&std::remove_reference_t<F>::operator())
//                                      ^~~ here we look inside opaque action type
    >
{};

template<class R, class T, class... As>  // mutable lambda case
struct action_traits<R (T::*)(As...)>
  : action_traits<R (T::*)(As...) const>
//                ^~~ decomposed action's type
{};

template<class R, class T, class... As>
struct action_traits<R (T::*)(As...) const>
{
  static constexpr auto arity = sizeof...(As);

  using result_type = R;

  template<unsigned I>
  using arg_type = std::tuple_element_t<I, std::tuple<As...>>;
};
```

We are done with user-input validation required in the constructor. It is the time to store the passed actions. In order to do that we need to find a storage inside the `actions` member based on the passed action types. Basically, we want to do this:

```c++
template<class... Fs>  // types Fs... are deduced from the passed arguments
Visitor(Fs... fs)
{
  static_assert(are_actions_allowed<visitable_types, Fs...>::value
              , "Unexpected action passed");

  fill_in(actions, std::forward<Fs>(fs)...);
}
```

The `fill_in` function inspects the passed actions's types in order, and based on their types it accesses the `actions` tuple using `std::get` to save that action. Unfortunately, if the user passes a lambda as an action, the action type does not match the one generated by `action_type` type constructor. It is due to fact, that the `action_type` is a polymorphic wrapper for an object of a type that models `Callable` concept that is convertible to the given function signature.

```c++
template<class T> 
using action_type = std::function<void ( T &)>;
//                                ^^^^^^^^^^^
```

We have to inspect the type of the action passed in the constructor in order to generate a type that will be used in indexing the the `actions` tuple. Here comes an example for an action `f` of a type `F`, where the type `F` is one of the types in the `Fs...` types.

```c++
std::get< action_type<arg0_type<F>> >(actions) = f;
//        ^^^^^^^^^^^^^^^^^^^^^^^^^              ^~~ object of type F convertible to the generated action type
```

We need to apply that algorithm to all the objects passed in the constructor. This will not compile in C++14:

```c++
template<class T, class... Fs>
static void fill_in(T&& t, Fs&&... fs)
{
  ( std::get< action_type<arg0_type<Fs>> >(std::forward<T>(t)) = std::forward<Fs>(fs) )...;
}
```

Here is the solution that works:

```c++
template<class T, class F>
static void assign_action(T&& t, f&& f)   // a case of a single action assignment
{
  std::get<action_type<arg0_type<F>>>(std::forward<T>(t)) = std::forward<F>(f);
}

template<class T, class... Fs>
static void fill_in(T&& t, Fs&&... fs)
{
  int dummy[sizeof...(Fs)] =
  {
    (assign_action(std::forward<T>(t), std::forward<Fs>(fs)), 0)...
  };

  (void) dummy;
}
```

The `fill_in` function is insensitive to the order of passed actions and the number of actions passed, which is good for us, because it mitigates the drawbacks we noticed in the original solution.

## The applicator

By lifting the function call operator to a template that takes any type `T` from `visitable_types` we are able to generate required code on-the-fly.

```c++
template<class T>
void operator()(T& t)
{
  std::get<action_type<T>>(actions)(t);
}
```

Unfortunately, if there is no such an action for a type `T`, the compiler generates quite a lot of error messages. We can control that by enabling above presented applicator only for `T` types that are found in the `visitable_types` type list:

```c++
template<class T>
auto operator()(T& t)
    -> std::enable_if_t<
          is_within<T, visitable_types>::value
//        ^~~ if this is not true, then this function WILL NOT be generated
        , void
        >
{
/...
```
*and* by providing a fallback function call operator which is impossible to be used by the user:

```c++
void operator()(...) = delete;  // passed type is not in visitable types!
```

If the passed `T` is not found within `visitable_types`, the compiler will choose *deleted* fallback `operator()` member function that accepts anything. That will cause a compile time error with the message that is clean and short:

```
error: use of deleted function 'void Visitor::operator()(...)'
note: declared here
     void operator()(...) = delete;  // passed type is not in visitable types!
          ^~~~~~~~
```

It may happen that some of the actions are not specified by the user explicitly, even if there are entries in the `visitable_types` type list that allows such actions to be specified. Sometimes it may be not desirable to specify all the actions for all the visitable types. In such cases, an action is optional, and if the `Visitor` object is applied against a visitable type that has no respective action specified, no action shall be taken at all. Here is the solution to that case:

```c++
template<class T>
auto operator()(T& t)
    -> std::enable_if_t<
          is_within<T, visitable_types>::value
        , void
        >
{
  if (std::get<action_type<T>>(actions))  // action specified?
  {
    std::get<action_type<T>>(actions)(t);
  }
  /** else: noop -- action not specified */
}
```

## Full example

Live code is available on [Coliru](http://coliru.stacked-crooked.com/a/75e79c330de5facf).

#### About this document

August 27, 2017 &mdash; Krzysztof Ostrowski
