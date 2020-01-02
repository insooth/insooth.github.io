 # **WIP:** Heterogeneous swap

A colleague of mine asked me a question about the most effective way to transform a `string` into a `vector<int8_t>`, a sequence of [ASCII codes](http://www.asciitable.com/), where each one character in the input `string` gets its integer ASCII code assigned.

That can be easily [solved with a `std::transform`](https://gist.github.com/insooth/30fa720d0d18eafc733880bef3d01acc) and an explicit cast. 

In fact, we do the cast to make compiler happy rather than to change the underlying binary representation. Since `string::value_type` and `int8_t` are identical in memory (both occupy a byte), and both `vector` and `string` are (roughly) _equivalent_ to `T*` and some metadata (if <acronym title="Small Buffer Optimisation">SBO</acronym> is not considered), we may be tempted to do a swap:

```
string s = "Hello world";

vector<int8_t> v;
assert(v.empty());

swap(s, v);  // <<<

assert(s.empty());
```

It would be great if the above worked well, but it didn't. STL's [`swap` is defined](https://en.cppreference.com/w/cpp/algorithm/swap) for homogeneous types (here: `T` type), i.e. (simplified)

```c++
template<class T> void swap(T&, T&);
//             ^            ^   ^
```

While `std::swap` cannot be defined for two different types, and neither `vector` constructor takes `string` as an argument to steal its contents (cf. `move`), nor `string` constructor does that for `vector`, we have to look for another solution. This article is an attempt to use structural equivalence and custom allocators as an enabler of heterogeneous swap.

Side note. Revision C++14 of the language brings [`T std::exchange(T&, U&&)`](https://en.cppreference.com/w/cpp/utility/exchange) that implements a heteregenous "replace with" algorithm.

## Equivalence

If we look a little bit closer at the swap behaviour we will realise that it works because there is an implicit _structural_ equivalence relation established between its operands. That is, `swap(T, U)` will do what's expected if `T` and `U` data types are equivalent. Type checker in the C++ language implements type equivalence in a form of name equivalence (strong (new types) &mdash; `class`, `enum class`, and loose (type aliases) &mdash; `typedef`, `enum`), consider the following:

```c++
// E and F are distinct under strong name equivalence
enum class E { foo };
enum class F { foo };

// int and X::bad are equivalent under loose name equivalence
enum X { bad };
int horribly = bad;
```

While name equivalence is desired for the compiler, `swap` implementation is interested in the actual structure of the type to be able to actually swap the things. Sadly, C++ language does not offer built-in constructs to perform the types' structural equivalence, i.e. compare the types recursively, not the values they describe. Consider the following:

```c++
// A and B are equivalent under structural equivalence
// A and B are distinct   under name       equivalence
class A { int i; };
class B { int i; };
```

Having that, `std::swap` constraints its both operands to be of a single (i.e. homogeneous) `T` type, that in turn, guarantees structural equivalence for free. On the other hand, heterogeneous `swap` requires an additional _predicate_ that judges whether two types are structurally equivalent, (simplified) C++20 snippet follows:

```c++
template<class, class U> void swap(T&, U&) requires StructurallyEquivalent<T, U>;
```

Where `StructurallyEquivalent` concept is a predicate that ensures `T` and `U` are structurally equivalent:

```c++
template<class T, class U>
concept StructurallyEquivalent
  = Same<T, U>  // homogeneous, std::swap case
 || requires    // heterogeneous
    {
      // magic here
    };
```

Heterogeneous case requires a dedicated construct to run the actual structural equivalence check on `T` and `U` types.

---

Side note. Structural equivalence is very useful in software source code analysis, aka "code reading". Every piece of code has its functional equivalent expressed in plain structural equivalent and a set of actions with attributes (like `async`, `generated`, etc.). Set of structural equivalents can be limited to an absolute minimum, e.g. to a _list_ construct as it was done in Lisp; or sequences `[a]`, tuples `(t, u, ...)`, alternatives `x|y|...`, and combined types like maps `[(k1, v1), (k2, v2), ...]`.

## Concepts

`std::swap`, which is parametrised by type `T`, imposes following requirements on its parameter:
* `MoveAssignable` and `MoveConstructible`; predicates: [`std::is_move_constructible_v`](https://en.cppreference.com/w/cpp/types/is_move_constructible) and [`std::is_move_assignable_v`](https://en.cppreference.com/w/cpp/types/is_move_assignable).
* `Swappable` for `[T]`; predicate: [`std::is_swappable_v`](https://en.cppreference.com/w/cpp/types/is_swappable).

Interestingly, `Swappable` can be realised by means of a `std` trait parametrised by two (possibly) distinct types, which may be used during definition of a `StructurallyEquivalent` concept:

```
template<class T, class U> struct is_swappable_with;
```

Looking further, `std::basic_string<char>` (aka `std::string`) meets the following requirements:
* [`AllocatorAwareContainer`](https://en.cppreference.com/w/cpp/named_req/AllocatorAwareContainer); customisation point: [`std::allocator_traits<A>`](https://en.cppreference.com/w/cpp/named_req/AllocatorAwareContainer).
* [`SequenceContainer`](https://en.cppreference.com/w/cpp/named_req/SequenceContainer), and [`ContiguousContainer`](https://en.cppreference.com/w/cpp/named_req/ContiguousContainer) which is a conjunction of [`Container`](https://en.cppreference.com/w/cpp/named_req/Container), [`LegacyRandomAccessIterator`](https://en.cppreference.com/w/cpp/named_req/RandomAccessIterator), and [`LegacyContiguousIterator`](https://en.cppreference.com/w/cpp/named_req/ContiguousIterator) for iterator's nested member type.

While `std::vector` (for `T` other than `bool`) meets the requirements of `AllocatorAwareContainer`, `SequenceContainer`, `ContiguousContainer`, `Container`, and [`ReversibleContainer`](https://en.cppreference.com/w/cpp/named_req/ReversibleContainer) which must meet `LegacyRandomAccessIterator` or [`LegacyBidirectionalIterator`](https://en.cppreference.com/w/cpp/named_req/BidirectionalIterator) (which is a subset of `LegacyRandomAccessIterator`).

It is easy to notice that `std::vector` meets all the requirements of `std::string` if it satisfies `LegacyRandomAccessIterator` (in `ReversibleContainer`). From the concept-based point of view, `std::vector<T>` is a subset of `std::string` for some `T`.


## AllocatorAwareContainer and direct memory access

> polymorphic_allocator does not propagate on container copy assignment, move assignment, or swap. As a result, move assignment of a polymorphic_allocator-using container can throw, and swapping two polymorphic_allocator-using containers whose allocators do not compare equal results in undefined behavior. 

> std::allocator_traits<allocator_type>::propagate_on_container_swap

Custom static allocator

#### About this document

November 16, 2019 &mdash; Krzysztof Ostrowski

[LICENSE](https://github.com/insooth/insooth.github.io/blob/master/LICENSE)
