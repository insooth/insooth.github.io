# Customisation points to support interfaces

Definition of the interfaces by means of which entities communicate in the designed system is one of the most crucial tasks in the system and software design. In fact, it is one of the first things to do, if the mentioned entities are identified. Interfaces are modelled as actions with certain properties. Interfaces' relations are described by their primary interactions, usually visualised with [sequence diagrams](https://www.uml-diagrams.org/sequence-diagrams.html) (hand-drawn or [text-based](http://plantuml.com/sequence-diagram)).

In the typical design targeted C++ language actions are functions (no matter whether they free or bound to a class or an object), and their properties may include concurrency aspects (sync/async), resource ownership/management, etc. &mdash; properties are likely to be expressed with library abstractions (e.g. `std::future`, `std::unique_ptr`/`std::shared_ptr`).

Definition of actions is not a trivial task, and if done wrong, we may not end up in bags of setters and getters (known as [Anemic Domain Model](https://martinfowler.com/bliki/AnemicDomainModel.html) anti-pattern). The resulting abstraction that embeds the defined actions does not have to be a set of `virtual` member functions wrapped into a `class` from which implementations exposed to the user derive. Actually, the blind `virtual`ity brought by such approach comes with more cons than pros. General guideline promoted in this article is to expose non-`virtual` interfaces to the user while providing a _customisation point_ in the form of minimal set of actions that can be combined with each other. 

## Code

TODO

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


#### About this document

July 7, 2018 &mdash; Krzysztof Ostrowski

[LICENSE](https://github.com/insooth/insooth.github.io/blob/master/LICENSE)
