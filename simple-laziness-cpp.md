
# Take benefit from simple laziness

In the C++ world _lazy evaluation_ is usually linked to templates and their property of separation of definition and actual instantiation. Given that we can for instance delay binding of a symbol:

```c++
void register(int);
void deregister(int);

template<class Tag>
struct delayed_registrator
{
    delayed_registrator() { register(&id); }
    ~delayed_registrator() { deregister(&id); }
};
```

where symbol `id` is not avaiable at the point of definition, but it is avaiable at the point of instantiation:

```c++
const int id = 1234;
struct device_tag {};

delayed_registrator<device_tag> registered;
```

Functions in `register` and `deregister` must be there &mdash; name lookup for non-template dependent entities is done at the point of definition, so that at least forward declarations of `register` and `deregister` must be available _before_ definition of `delayed_registrator`. Type is templated over `Tag` which does the "delay effect" we wanted to achieve.

Besides the above, we have old plain short circuiting we inherited from `C` language: logical operators like `&&` (`and`), `||` (`or`) and ternary operator `?:`. They can be used as constructions to lazy execute (expressions must be valid C++) some of the expressions. Note, "not executed" is not the same as "not evaluated" (like [`decltype`, `noexcept`, `sizeof`, `typeid` or `requires`](http://en.cppreference.com/w/cpp/language/expressions#Unevaluated_expressions "Expressions")). With short circuiting we want to delay or skip execution of a costly operation.

## Ordering with `and`

Simple ordering by the increasing processing cost can save a lot of runtime:

```c++
const bool can_progress
    = invariant                 // constant cost
      && has_environment()      // medium cost: verifies in-memory structures
      && check_network_nodes(); // huge cost: does blocking IO
```

This technique recalls Linux kernels [`likely` and `unlikely` macros](http://kernelnewbies.org/FAQ/LikelyUnlikely "FAQ/LikelyUnlikely") used in `if-else` conditionals that give possibility to expliticly favour one of the execution branches.

## Pattern matching with `or`

Let's consider serialisation engine that provides family of functions of prototype:

```c++
template<class V>
bool serialise(const S& from, variant<int, string, C>& into);
```

that return `true` if we can serialise `from` value into value of type `V` which is one of the types under `variant`. User provides value of type `S` through:

```c++
bool serialize_any(const S& from);
```

We need to check inside the body of `serialize_any` whether we can serialise into one of the supported, do the serialisation and return with status. One can think about cascade of `if-elseif-else` block or `switch`, but we will use short circuiting to do kind of pattern matching over set of varianted types:

```c++
bool serialise_any(const S& from, variant<int, string, C>& into)
{
    return 
           serialise<int>(from, into)     // smallest cost
        || serialize<string>(from, into)  // bigger cost: must deserialize sequence of chars
        || serialize<C>(from, into);      // huge cost: custom data type
}
```

Function `serialise_any` will stop execution at first `true` returned from subsequent `serialise` call. This is classic understanding of pattern matching, where all the cases are tested in order.

# Construction with `?:`

In some cases we cannot construct value with default constructor (and then update its state), then we use ternary operator:

```c++
const T t{ exists ? "right here" : reset_value()};
//                  ^~~ cheap      ^~~ costly
```

where `exists` of `true` is more likely than of `false`. We can put above to the extremum and play with comma operator:

```c++
void fill(V&&);

bool fill_valid(V&& v)
{
    return v.valid() ? (fill(v), true) : false;
}
```

Please note that we do not use negated boolean logic, that is we always check for `true` not for `false` in the condition. That's good for readbility.

#### About this document

June 13, 2016 &mdash; Krzysztof Ostrowski

