# The role of the visitor pattern

Among all the various design patterns widely used by the C++ language community, probably only the visitor pattern takes advantage of the language type system in such a sophisticated way. Visitor pattern provides a solution to a recurring problem found in object-oriented designs (and in any other that use runtime-supported subtype polymorphism), namely the determination of the actual type of the object referenced by some interface type. This article will revisit the core idea behind that design pattern, mention couple of existing implementations, and describe a C++14 implementation of the visitor that aims to be flexible and safe as much as the template-based solution but remains a fixed (non-template) type favourable by classic interfaces based on `virtual` member functions.

## The task

Consider types `B` and `C` that implement interface `A` and a function `provide` that returns an object of randomly selected one of them.

```c++
struct A { virtual std::string name() const = 0; };

A& provide();  // returns *ANY* object derived from type A


struct B : A { std::string name() const override { return "B"; } };
struct C : A { std::string name() const override { return "C"; } };
// ... more derived types may follow
```

We would like to perform dedicated action based on the actual type of object returned by `provide` function. That is, we are interested in the type of the most derived class, not in the interface type. Naïve solution may be:

```c++
decltype(auto) x    = provide();
auto           name = x.name();  // name is a "tag" that identifies the type


if      ("B" == x.name()) { /* action for B */ }
else if ("C" == x.name()) { /* action for C */ }
else    /* unsupported */ { assert(false);     }
```

Visitor pattern provides a solution to this problem that uses C++ type system only. That is, there is no magic "tags" that are used to restore the (derived) type erased by use of an interface. Erased in this context relates to a difference between the type seen by the compiler and the the actual type of the object stored in the memory. In that sense, it defines (dynamic) subtype polymorphism common in object-oriented languages.

## The types

Basically, compiler assigns a type to given entity without analysing the details of a data flow. That means, type is assigned directly based on information given explicitly by the user, or is calculated through evaluation of meta program, type inference or argument deduction (cf. function templates).

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

A& x = b;   // compiler assigns `x` type of A&, although "real" type of `x` is B

// value of type B is described by the OTHER type (here it is the A
// which is the supertype (base class) of the B) -- that is the whole
// idea behind the subtype polymorphism

choice(x);  // calls choice( A &)

// `x` is "dynamically" polymorphic handle to *ANY* object derived from A

```

There is no way to determine statically (i.e. during compile time) the actual type of the object having a polymorphic handle to it. Note, type casting is not considered since it escapes the type system. Visitor pattern shines here as a proper tool for determining the type of a value, i.e. it is used as a tool for "restoring" the type erased by a polymorphic handle.


## The bindings

Calling member functions against ("dynamic" subtype) polymorphic handle instructs compiler to locate the address of the function in question, and insert it explicitly in the binary, that's so called "early" bind. There is no difference between binding functions for polymorphic and non-polymorphic values, unless given member function is marked as a `virtual`. Virtual member functions are not early bound, i.e. they are "late" bound. Instead of inserting a fixed address pointing the required function explicitly in the binary, compiler inserts an address to the function that *will* redirect to the required member function. Redirection happens in the runtime. Only runtime is able to judge about the actual type of an object pointed by the polymorphic handle. C++ allows only polymorphic handles of reference and pointer type, there is *no late binding for values* (slicing happens).


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

x.name();   // late binding: calls B::name(), even if type of `x` is A

x.choose(); // early binding: error: no such function A::choose()


// NON POLYMORPHIC

C c;

B z = c;    // wrong: `z` is not polymorphic, `z` is just a part of `c` object's memory

z.foo();   // early binding: calls B::foo() *NOT* C::foo(), `c` type has been upcasted to B
```

Through use of a `virtual` member function we can access the actual object having polymorphic handle to it. That is called single dispatch on `this` pointer (dispatch always happens during runtime). The `this` pointer must be adjusted to match the actual object pointed by polymorphic handle, and the required member function defined for the determined object must be found and bound. All that happens during runtime, and all that has its costs.


# The visitation

We visit using visitor object, and we can visit only a set of "visitable" types, that is all the types that implement the following interface:

```c++
struct Visitable { virtual void visit(Visitor& v) = 0; };
//                                    ^~~ type of an object that performs visitation

```

The `Visitor` type is a function object that accepts invocation with objects of types derived from `A` (cf. string comparison in the naïve solution).


```c++
struct Visitor
{
  void operator()(B& b) { /* action for B */ }
  void operator()(C& c) { /* action for C */ }

  // the else-path (unsupported object) is a compile error,
  // instead of a runtime assert as in the naïve solution
};
```

It is not always possible to make `Visitable` a superclass of all the types (cf. third-party libraries), thus we introduce it in the middle of a inheritance tree. That forces us to use `dynamic_cast` to cross cast from received polymorphic handle to `A` (returned by `provide` function) to `Visitable`.


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

Virtual member function `visit` is used to access actual object pointed by polymorphic handle. Having done the dispatch through that function we execute its body. Since all the types in its body are already calculated by the compiler and put in the binary, the passed `v` visitor function object is called against `*this` object of the derived type (here it is either `B` or `C`). The visitor's function call operators are bound early too.


# The limitations

The above presented visitor pattern implementation has a notable drawback: it is not composable. There is no way to alter visitor actions on-the-fly. That may be solved by turning Visitor function call operators being `virtual`, and then by using derived types that provide alternative actions' implementations. Another idea is to lift the `visit` member function to a member function template that accepts any visitor type (i.e. either generate multiple virtual member functions per every visitor type, or strip `virtual` keyword and turn `visit` into member function template). The former solution is the classic OOP approach to visitor pattern that introduces double dispatch (firstly through `visit`, secondly through virtual function call operator defined by the `Visitor`). The latter is the [Alexandrescu's](http://loki-lib.sourceforge.net/html/a00685.html) visitor and [can be realised through "static polymorphism" aka CRTP](https://stackoverflow.com/questions/7876168/using-the-visitor-pattern-with-template-derived-classes).

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
* extending support to new types in the inheritance hierarchy (subtypes of `A`) is cumbersome,
* all the actions for all the supported visitable types must be specified (even if not desired),
* actions for the supported visitable types must be specified in strictly defined order,
* application of the visitor to an unsupported type generates hard-to-grasp compile errors.

First two bullets apply to alternative visitor implementations too.

# The solution

One of the goals that will improve the overall user experience of the `Visitor` is the reduction of amount of work required to add support for new visitable types. It would be great to have a single place in code where the change has to be done, like a set of types for which `Visitor` is allowed to work, and it would be great to have the rest of the `Visitor` contents is automatically generated by the compiler. Let's define the single point of extension as:

```c++
using visitable_types = std::tuple<B, C>;  // tuple works here as a "type list" of UNDECORATED types
```

In order to support additional visitable type, user has to tweak the `Visitor` class constructor, add dedicated function call operator, and extend the class members with a polymorphic function wrapper that will keep the action passed by the user in the constructor. All that code has to be generated by the compiler having `visitable_types` "type list" as a sole input. To achieve that, we will write a couple of meta-programs (i.e. program that operates on types and constant expressions).

## The constructor

We are required to work for any number of different types of actions, we will define constructor as a variadic template that stores all the actions in the `std::tuple` of actions. That is, instead of multiple members that keep the passed actions, we are going to use a heterogeneous iterable container (`std::tuple`) of actions, where action can be accessed by specifying the action type.

```c++
using action_types =
  std::tuple<
    std::function<void ( B &)>  // see actOn* members from the previous example
  , std::function<void ( C &)>
  >;
```

Careful reader will notice immediately, that action type is parametrised by visitable type only, and it can be lifted to:

```c++
template<class T> 
using action_type = std::function<void ( T &)>;
//                                       ^~~ any type from visitable_types can be put here
```

Having defined parametric `action_type` as type constructor that produces action type for any passed `T`. Let's generate the `action_types` type with a meta-program called `make_action_types` that simply iterates over the `visitable_types` and applies subsequent visitable type to `action_type` type constructor.

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

And now, we can define all the actions we are going to use as a `Visitor` object member `actions` of the generated type `action_types`.

The `action_type` data type is a parametrically polymorphic, so that it can be fixed with any type `T`. That's not acceptable for us, because we want to have a set of possible `T` types limited to those stored in the `visitable_types` list of types. We will perform such a compile-time check in the constructor body. The constructor itself takes variadic number of values of unspecified types (denoted as `Fs...`). Those values must model [`Callable`](http://en.cppreference.com/w/cpp/concept/Callable) concept to be accepted by `std::function`. 

```c++
template<class... Fs>
Visitor(Fs... fs)
{
    static_assert(are_actions_allowed<visitable_types, Fs...>::value
                , "Unexpected action passed");

// ...
```

Meta-program `are_actions_allowed<AllowedVisitableTypes, PassedActionType...>` implements the following algorithm:
* for each `F` type in `PassedActionType...` types it determines `F`'s first argument (called later `arg0_type`) by inspecting `F::operator()`, and then...
* it looks for`arg0_type` in the `visitable_types` and returns search result (boolean) to the user.

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

where `is_within` returns `true` if `T` is in type list `U`, otherwise `false`

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

We are done with user-input validation in the constructor. It is time to store the passed actions. In order to do that we need to find a storage inside the `actions` member based on the passed action type. Basically, we want to do this:

```c++
template<class... Fs>  // types Fs... are deduced from the passed arguments
Visitor(Fs... fs)
{
  static_assert(are_actions_allowed<visitable_types, Fs...>::value
              , "Unexpected action passed");

  fill_in(actions, std::forward<Fs>(fs)...);
}
```

`fill_in` function inspects the passed actions in order, and based on their type it accesses using `std::get` the `actions` tuple in order to save the action. Unfortunately, if user passes a lambda as an action, action type does not match the one generated by `action_type` type constructor. It is due to fact, that `action_type` is a polymorphic wrapper for a `Callable` that is convertible to the given function signature.

```c++
template<class T> 
using action_type = std::function<void ( T &)>;
//                                ^^^^^^^^^^^
```

We have to inspect the type of the passed in the constructor action in order to generate a type that will be used in indexing the `actions` tuple. Here comes an example for an action `f` of type `F`, where type `F` is one of the types in the `Fs...` types.

```c++
std::get< action_type<arg0_type<F>> >(actions) = f;
//        ^^^^^^^^^^^^^^^^^^^^^^^^^              ^~~ of type F convertible to generated action type
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

The `fill_in` function is insensitive to the order of passed actions and its number, what mitigates the drawbacks we noticed in the original solution.

## The applicator

By lifting the function call operator to a template that takes any type `T` from `visitable_types` we are able to generate required code on-the-fly.

```c++
template<class T>
void operator()(T& t)
{
  std::get<action_type<T>>(actions)(t);
}
```

Unfortunately, if there is no such an action for type `T`, compiler generates quite a lot of error messages. We can control that by enabling above presented applicator only for `T` types that are found in the `visitable_types`:

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
*and* by providing a fallback function call operator which impossible to be used by the user:

```c++
void operator()(...) = delete;  // passed type is not in visitable types!
```

If the passed `T` is not found within `visitable_types`, compiler will choose *deleted* fallback `operator()` that accepts everything. That will cause compile time error with the message that is clean and short:

```
error: use of deleted function 'void Visitor::operator()(...)'
note: declared here
     void operator()(...) = delete;  // passed type is not in visitable types!
          ^~~~~~~~
```

It may happen that some of the actions are not specified by the user, even if there is an entry in the `visitable_types` that allows such action to be specified. Sometimes it may be not desirable to specify all the actions for all the visitable types. In such cases, an action is optional and if `Visitor` is applied against visitable type that has no specified action, no action shall be taken. Here is the solution to that case:

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
