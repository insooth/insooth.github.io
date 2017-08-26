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
//  | B |<-------------| Visitable |
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
* application of the visitor to unsupported type generates hard-to-grasp compile errors.

First two bullets apply to alternative visitor implementations too.

# The solution

TBD

http://coliru.stacked-crooked.com/a/153c7369448409e9

#### About this document

August 27, 2017 &mdash; Krzysztof Ostrowski
