# Customisation points to support interfaces

Definition of the interfaces by means of which entities communicate in the designed system is one of the most crucial tasks in the system and software design. In fact, it is one of the first things to do, if the mentioned entities are identified. Interfaces are modelled as actions with certain properties. Interfaces' relations are described by their primary interactions, usually visualised with [sequence diagrams](https://www.uml-diagrams.org/sequence-diagrams.html) (hand-drawn or [text-based](http://plantuml.com/sequence-diagram)).

In the typical design targeted C++ language actions are functions (no matter whether they free or bound to a class or an object), and their properties may include concurrency aspects (sync/async), resource ownership/management, etc. &mdash; properties are likely to be expressed with library abstractions (e.g. `std::future`, `std::unique_ptr`/`std::shared_ptr`).

Definition of actions is not a trivial task, and if done wrong, we may not end up in bags of setters and getters (known as [Anemic Domain Model](https://martinfowler.com/bliki/AnemicDomainModel.html) anti-pattern). The resulting abstraction that embeds the defined actions does not have to be a set of `virtual` member functions wrapped into a `class` from which implementations exposed to the user derive. Actually, the blind `virtual`ity brought by such approach comes with more cons than pros. General guideline promoted in this article is to expose non-`virtual` interfaces to the user while providing a _customisation point_ in the form of a minimal set of actions that can be combined with each other. 

## Customise

Used in this article definition of the customisation point differs from the STL one (cf. `std::hash`). We do not lift our type into a generic one, and then use the compiler to select the appropriate specialisation in the first place (like it is done for `std::hash`). Instead we specify a fixed type `Interface` with a constructor that takes, as one of its arguments, an object with custom implementation. That object shall derive from a type that is an instance of `Backend` template, a fixed type specialised for our interface, i.e. `Backend<Interface>`. Customisation point is *polymorphic* in the object-oriented sense, i.e. it is not a fixed part of a type, and can be specified during runtime. That's a big win, as it was with [PMR](https://en.cppreference.com/w/cpp/memory/polymorphic_allocator).

Following snippet visualises the idea of a customisation point. We may either use the default resource with implementation, or just reference it. In the former case we create and *own* the resource, while in the latter we are just the observer of an injected resource (it is a *shared state*, thus must be thread-safe). The injection possibility enables our type for testing, and reuse with replaced [_backend_](https://github.com/insooth/insooth.github.io/blob/master/blessed-split.md) (cf. dependency injection).

<table style="white-space: pre">
<tr>
<th>Before</th>
<th>After</th>
</tr>
<tr>
<td style="vertical-align: top">
<pre lang="cpp">
class Interface
{
public:
 virtual R foo(A);
};
<br>
class Implementation : public Interface
{
public:
 R foo(A) override;
//        ^^^^^^^^^
<br>
 virtual ~Implementation() = default;
//^^^^^^
};
</pre>
</td>
<td style="vertical-align: top">
<pre lang="cpp">
class Interface
{
 // magic here
<br>
 constexpr Backend&lt;Interface&gt;&amp; backend();
 constexpr const Backend&lt;Interface&gt;&amp; backend() const;
<br>
public:
<br>
 Interface();  // sets default backend
 Interface(Backend&lt;Interface&gt;&amp;);  // injects backend
<br>
 U bar(T);  // uses actions via backend()
};
<br>
// where
<br>
template&lt;class Tag&gt; class Backend;
<br>
template&lt;&gt;
class Backend&lt;Interface&gt;
{
public:
 virtual R foo(A);
};
</pre>
</td>
</tr>
</table>

There are even more benefits that come from the polymorphic customisation points. Since we [do not expose `virtual` member functions](http://www.gotw.ca/publications/mill18.htm) (but we build functionalities on them) we can employ extended type checks. We can expose templated interfaces, which is impossible with `virtual`s.

Note that `Backend` template may act here as an implementation of a factory pattern. That may be not desirable for some designs that do not want to use such centralised control of subtyping.

## Own or share


As noted already, we _use_ a backend resource either owned by us or not owned by us. That's reflected in the `Interface` constructors:

```cpp
Interface();                     // owns the default backend instance
Interface(Backend<Interface>&);  // *shares* the ownership of the injected backend
Interface(Backend<Interface>&&); // transfers ownership of the backend instance (owns)
```

Constructors imply a storage area for either the passed reference to the backend resource, or for the backend object itself (internally created or moved from the caller). Such data type for a resource of type `T` is equivalent to ([why not a raw reference type](https://github.com/insooth/insooth.github.io/blob/master/wrap-members-of-reference-type.md)):

```
variant<unique_ptr<T>, reference_wrapper<T>>
//      ^~~ owns       ^~~ shares
```

which is actually a valid C++ type.

If you think about `union` or a single `T*` instead of a `variant` there is bad news for you. Union's [destructor needs to be defined explicitly](https://en.cppreference.com/w/cpp/language/union) to manage memory under `unique_ptr`, so that you will either leak memory or crash your application. In the latter case of `T*`, an additional flag will be required to distinguish between referenced and owned resource, and an implementation of all the constructors, copy-assignment operators and the destructor too &mdash; that's quite a lot of code that does not have to be written at all to get the _almost_ the same functionality as that provided by `variant`. Almost, because `unique_ptr` effectively disables copying, while `T*` does not.

Even if the `variant`-based approach is the simplest one, it has one drawback related to the runtime dispatch required to determine the type of the value currently set. That's the equivalent to the boolean flag used in `T*`-based approach.

We can eliminate that runtime check by owning and sharing _simultaneously_. We either share and do not own, or own and share the owned resource.

```cpp
struct Storage
{
    explicit Storage(T& t)
      : own{nullptr}, ref{t}
    {}

    explicit Storage(std::unique_ptr<T> t)
      : own{std::move(t)}, ref{*own}
    {}

    std::unique_ptr<T>        own;
    std::reference_wrapper<T> ref;
};
```

If we use [`unique_ptr`](https://en.cppreference.com/w/cpp/memory/unique_ptr) we do not have to worry about the broken dependency of shared-to-owned because our type is already neither [`CopyConstructible`](https://en.cppreference.com/w/cpp/named_req/CopyConstructible) nor [`CopyAssignable`](https://en.cppreference.com/w/cpp/named_req/CopyAssignable).

`Storage` type is not [`DefaultConstructible`](https://en.cppreference.com/w/cpp/named_req/DefaultConstructible) because [`reference_wrapper`](https://en.cppreference.com/w/cpp/utility/functional/reference_wrapper/reference_wrapper) is not. That's the difference between `variant`-based approach, where the `unique_ptr` type used as a first alternative type in the instance of `std::variant` [guarantees it is `DefaultConstructible`](https://en.cppreference.com/w/cpp/utility/variant/variant), because `unique_ptr` itself is `DefaultConstructible` (i.e. swapped alternative types will disable that property).

Live code can be found [on Coliru](http://coliru.stacked-crooked.com/a/cb0bdc3cd9119742).


#### About this document

July 7, 2018 &mdash; Krzysztof Ostrowski

[LICENSE](https://github.com/insooth/insooth.github.io/blob/master/LICENSE)
