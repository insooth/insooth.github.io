# Customisation points to support interfaces

Definition of the interfaces by means of which entities communicate in the designed system is one of the most crucial tasks in the system and software design. In fact, it is one of the first things to do, if the mentioned entities are identified. Interfaces are modelled as actions with certain properties. Interfaces' relations are described by their primary interactions, usually visualised with [sequence diagrams](https://www.uml-diagrams.org/sequence-diagrams.html) (hand-drawn or [text-based](http://plantuml.com/sequence-diagram)).

In the typical design targeted C++ language actions are functions (no matter whether they free or bound to a class or an object), and their properties may include concurrency aspects (sync/async), resource ownership/management, etc. &mdash; properties are likely to be expressed with library abstractions (e.g. `std::future`, `std::unique_ptr`/`std::shared_ptr`).

Definition of actions is not a trivial task, and if done wrong, we may not end up in bags of setters and getters (known as [Anemic Domain Model](https://martinfowler.com/bliki/AnemicDomainModel.html) anti-pattern). The resulting abstraction that embeds the defined actions does not have to be a set of `virtual` member functions wrapped into a `class` from which implementations exposed to the user derive. Actually, the blind `virtual`ity brought by such approach comes with more cons than pros. General guideline promoted in this article is to expose non-`virtual` interfaces to the user while providing a _customisation point_ in the form of minimal set of actions that can be combined with each other. 

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

TODO


#### About this document

July 7, 2018 &mdash; Krzysztof Ostrowski

[LICENSE](https://github.com/insooth/insooth.github.io/blob/master/LICENSE)
