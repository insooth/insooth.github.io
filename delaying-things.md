
# **WIP:** Delaying things

Think about functions as they were structured data. Use a structure to delay function application, its output and input.

## η (eta)

Let's consider the following, somehow trivial, design exercise. We are looking for a solution to the problem of running a predefined action upon reception of a well-known bit of information. We may want to design a protocol stack, an event-based mircoservice infrastructure, or maybe we just wan to make interrupts working. Our "system" is a function _f_ that takes _c_ (a bit of information from the well-known set) and returns an action "G" (for simplicity, a unary one):

```
class A;
class B;

// f :: c -> (a -> b)
template<class C, class G>
  requires
       Regular<C>
    && Invocable<G, A>
    && Same<B, invoke_result_t<G, A>>
G f(C);
```

To be able to return an action "G" we need to access a container of actions (actions are predefined, thus we are not allowed to invent them on the fly). Further, to create a container of actions `[g]` we need to treat functions as data. That's a very important observation. In C language we "turn" functions into data by means of layer of indirection that uses the function addresses in memory, function pointers. Additionally, C++ language supports pointers to members and member functions which are usually modelled as a pair of in-object offset and an actual data/function pointer.

An action `g` stored in the container of actions `[g]` may be partially applied (i.e. some its arguments are already known and carried with the action, cf. `std::bind`). That means, its application is interrupted, it is _delayed_. Is the unapplied action _delayed_ too? Yes, it is! If it were not, we would be required to pass arguments to the action in the call to `f`. Function treated as data is equivalent to a delayed function, where the delay is a structure we peel off by looking inside of it. This can be modelled with an additional argument that is applied implicitly: we add a layer of an abstraction, we do _eta-expansion_.

```
// g  :: a -> b        -- original
// gg :: λ_ -> a -> b  -- delayed, "eta-expanded"

auto gg = [](auto&&... as) { return g(forward<decltype(as)>(as)...); };
// now we can treat gg as data
```

## More with λ (lambda)

| Delay output | Delay application | Delay input |
|---|---|---|
| Return an action (which is e.g. a lambda): `F foo() requires Invocable<F, ...>`. Generate data. | `auto bar() { return [](auto) { return foo(); }; }`. Add laziness to a strict language. | Take an action: `T foo(F) requires Invocable<F, ...>`. Consume a stream. |

+ Omega combinator.
+ Y combinator (untyped lambda calculus)
+ delay Y combinator in a strict language
+ delay application with Z combinator

## Towards Free

Interpret description of a program, carry state between the interpreted structures.

Free is a Monad; it wraps another structure and makes it possible to treated as a Monad (nested data structure is interpreted, hidden state with current result of the computation is carried by Free between the next interpretation steps of the nested structure); like recursion through Y combinator: the function passed to Y is not recursive itself.

#### About this document

May 28, 2019 &mdash; Krzysztof Ostrowski

[LICENSE](https://github.com/insooth/insooth.github.io/blob/master/LICENSE)
