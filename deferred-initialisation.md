
# Deferred initialisation for classes that inherit constructors

*(or Automatic two-phase init)*

Constructor inheritance through using-declaration is a powerful technique that can be easily used to introduce pre-defined behaviour to custom types. As an example, consider a [generic visitor design pattern implementation](https://github.com/insooth/insooth.github.io/blob/master/visitor-pattern.md) that is parametrised by the names of visitable types, and an unique tag that identifies given visitor template instance. Such an instance can be "mixed-in" into a custom type `D` by simply deriving from it... and inheriting its constructors to make it behave as it would be a `Visitor` template instance itself:

```c++
struct D
  : Visitor<
        A         // the tag -- *ANY* type can be passed here
      , X, Y, Z   // "list" of visitable types
      >
{
    using Visitor::Visitor;  // behave as a Visitor type
};
```

Note that, one may be tempted to write:

```c++
using D = Visitor<class TagD, X, Y, Z>;
```

which does the same thing, except `D` is an alias here, and not a unique type. In such a case, any compilation messages will point to `Visitor<class TagD, X, Y, Z>` type, instead of referencing a more readable user-defined type `D`. Worth to note is that the `D` type "behaves" exactly as an alias to `Visitor<...>` while it is seen as a distinct being by the language type system. That resembles strongly-typed typedefs.

What if there is a need to run an "additional" initialisation code as it would be run in the `D` constructor if one were there? Bjarne Stroustrup suggests [member-initializer](http://www.stroustrup.com/C++11FAQ.html#inheriting)s that do half of a work we would like to do here. While direct member initialisation sets required initial values for all the members (except bit field members), it does not offer a direct way to execute custom code that makes use of those members, the custom code that typically resides in the constructor's body. This article presents a solution that enables injection of a custom code to be executed once all the non-static members are initialised.


## The gist

[C++ ensures](http://en.cppreference.com/w/cpp/language/initializer_list#Initialization_order) that direct base classes of some class `A` are initialised before the non-static data members get initialised in order of their declaration in the class definition. We can use those properties and reference selected members at the very end of the class definition:

```c++
struct A
{
    std::string s = "Hello";
    std::string x = s + " World!";
//                  ^^^^^^^^^^^^^
};
```

Since `+` resolves to `std::string::operator+`, we can try to put any action that initialises the member, for example:

```c++
struct A
{
    std::string s = "Hello";
    std::string x = s + " World!";

    int dummy = (email(x), 0);
//      ^^^^^^^^^^^^^^^^^^^^^
};
```

The [built-in comma operator]((http://en.cppreference.com/w/cpp/language/operator_other#Built-in_comma_operator)) [guarantees sequencing](http://en.cppreference.com/w/cpp/language/eval_order), which means that `email(x)` will be evaluated before assignment of `0` to `dummy` happens. Combining that with the knowledge on order of initialisation we can transform following code:

```c++
struct A
{
    A()
     : b{/* ... */}
    {
        auto x = bar(b.foo());
        C::qux(this, x);
        // ...
    }

    T bar(auto) { /* ... */ }

    B b;
};
```

into

```c++
struct A
{
    T bar(auto) { /* ... */ }

    B b = {/* ... */};
//      ^^^^^^^^^^^^^

    void init()
    {
        auto x = bar(b.foo());
        C::qux(this, x);
        // ...
    }

    int dummy = (init(), 0);
//  ^^^
};
```

that delegates the required work from the constructor of the class `A` to its member function `init`. Possible pitfall is the lack of the "re-init" on copy and copy-assignment (move and move-assignment are not considered &mdash; object is already initialised). That can be solved with a custom type that replaces the `int` type used in the example above.

## A prototype

Let's take into consideration a class `D` that inherits constructors from its base class `B`. Since `D` requires additional work to be done inside its original constructor, and inherited constructors cannot directly call that constructor, we pass an action of a fixed signature, a deferred initialiser, to the inherited constructor, and store it as a member of the base class.

```c++
struct B
{
    template<class F>  // F must be convertible to type of action
    B(F&& f)
      : action{std::forward<F>(f)}
//      ^~~ F must be convertible to type of action, checked by the compiler
    {}

    std::function<void ( D& )> action;
//                ^~~ fixed signature
};
```

Use of the saved action of type `F` is straightforward:

```c++
struct D : B
{
    using B::B;

    int initialised = (action(*this), 0);
};
```

Above presented prototype needs some refinements in the following areas:
* Type of the `initialised` member shall properly handle copying, assigning and moving of objects of type `D`.
* There should be a protection against double initialisation provided.
* Derived type `D` should be able to optionally override (or compose with) the passed initialisation action.
* An interface that performs the initialisation only once per object shall be exposed by the base class, and inherited by the derived ones.
* Solution shall be reusable for any type `D`, and for any action signature.

## Initialiser type

We need a class that implements [the rule of five](http://en.cppreference.com/w/cpp/language/rule_of_three) and has access to an object of the class `B` that stores the initialisation `action`. For example:

```c++
class W
{

 public:

    W(B&)
      : base{&base}
    {
        init();
    }

    W(const W& rhs)
      : base(rhs.base)
    {
        if (&rhs != this) { init(); }
//          ^~~ prevents from double-init
    }

    W& operator=(const W& rhs)
    {
        if (&rhs != this)
        {
            base = rhs.base;
            init();
        }
    }

    W(W&& rhs)            { base = rhs.base; }
    W& operator=(W&& rhs) { base = rhs.base; }

    W()  = default; // leaves object in the partially-formed state
    ~W() = default; // no de-init

 private:

    B* const base = nullptr;


    void init()
    {
        assert(nullptr != base);

        base->action( * static_cast<D*>(base));
//                      ^^^^^^^^^^^^^^^^^^^^^
    }
};
```

Even that `W` takes a reference to `B` (i.e. a base class of `D` which objects we want to initialise), we store it in a member of a raw pointer type rather than as an instance of `std::reference_wrapper` template. That's due to fact, that reference wrappers are not [`DefaultConstructible`](http://en.cppreference.com/w/cpp/concept/DefaultConstructible), what in turn, will implicitly mark all the default constructors as `delete`d (including those that do not take arguments &mdash; the nullary ones) for all the bases of `D` class.

The check against self-assignment guarantees that we won't initialise ourselves again. Move operations do not de-initialise already initialised object.

We change the type of `initialised` from `int` to `W`, so that `D` becomes:

```c++
struct D : B
{
    using B::B;

    W initialised{*this};  // init action is executed here!
//                 ^~~ points to derived object!
};
```

Unfortunately, any user of the `D` type can easily run `B::action` multiple times. To protect against that we will make the `action` member private, and call it through a public interface `prepare` that ensures initialisation happens only once.

```c++
class B
{

    friend class W;

 public:

    template<class F>
    B(F&& f)
      : action{std::forward<F>(f)}
    {}

    void prepare()
    {
        std::call_once(prepared, [this] { action( * static_cast<D*>(this)); });
//                                        ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    }

 private:

    std::function<void ( D& )> action;
    std::once_flag             prepared;
};
```

## Customisation

It may happen that some of the derived types would like to override, or compose with, the passed initialisation `action`. To enable that, `B` class will define a private `initialise` interface that is able to take an object of any [`Callable`](http://en.cppreference.com/w/cpp/concept/Callable) type convertible to the required `action` type. Default implementation of the `initialise` member function in the `B` class will simply call that function against the `action`. User may "override" `initialise` in the derived type `D` by simply specifying it again as a *public* member function. No `virtual` member functions are in use. So called "static polymorphism" is realised solely through the use of the [CRTP](https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern) idiom.


```c++
class B
{

    friend class W;

 public:

    template<class F>
    B(F&& f)
      : action{std::forward<F>(f)}
    {}

    void prepare()
    {
        std::call_once(prepared, [this] { static_cast<D*>(this)->initialise(action); });
//                                         ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    }

 private:

    template<class F>
    void initialise(F&& f)  // fallback initialise
    {
        f( * static_cast<D*>(this));
//      ^~~ compare with the original body of the prepare member function
    }

    std::function<void ( D& )> action;
    std::once_flag             prepared;
};
```

As a last step, we need to change `W::init` body to actually call the `initialise` member function instead of executing directly the `B::action`.

```c++
    void init()
    {
        assert(nullptr != base);

        static_cast<D*>(base)->initialise( base->action );
//      ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    }
```

A bit of explanation of what exactly CRTP brings here may be needed. Basically, the `B::action` member is, instead of being called *directly* from `W::init` member function, called through a construction that involves:

* a down-cast to the most derived type `D` (this is safe as long as `D` derives from `B` &mdash; see how to assure that in the link to the full example included at the end of this article),
* an application of a selected `D::initialise` member function against the initialisation `action` stored as a `B` class object member.

If `D` defines *public* `initialise` member function, it will be executed, otherwise the `initialise` *inherited* from `B` will be executed.

`B::initialise` is intentionally a private member function. There shall be no possibility to explicitly call `B::initialise` from code other than trusted `W::init` and `B::prepare` member functions. That protects `D` objects from being double initialised.


## Lifting and helpers

Proposed solution can be lifted into a reusable abstraction. An example of a working code is available [here](https://github.com/insooth/CANdevStudio/blob/backend-constructor/src/common/withexplicitinit.h).




#### About this document

September 8, 2017 &mdash; Krzysztof Ostrowski

[LICENSE](https://github.com/insooth/insooth.github.io/blob/master/LICENSE)
