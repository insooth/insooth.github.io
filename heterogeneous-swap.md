 # **WIP:** Heterogeneous swap

A colleague of mine asked me a question about the most effective way to transform a `string` into a `vector<int8_t>`, a sequence of [ASCII codes](http://www.asciitable.com/), where each one character in the input `string` gets its integer ASCII code assigned.

That can be easily [solved with a `std::transform`](https://gist.github.com/insooth/30fa720d0d18eafc733880bef3d01acc) and an explicit cast. It can be solved even easier by simply picking the proper types at software design phase (which is an obvious rule, but not commonly enforced).

In fact, we do a cast to make compiler happy rather than to change the underlying binary representation. Since `string::value_type` and `int8_t` are identical in memory (both occupy a byte), and both `vector` and `string` are (roughly) _equivalent_ to `T*` and some metadata (if <acronym title="Small Buffer Optimisation">SBO</acronym> is not considered), we may be tempted to do a swap:

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

Side note. Revision C++14 of the language brings [`T std::exchange(T&, U&&)`](https://en.cppreference.com/w/cpp/utility/exchange) that implements a heteregenous "replace with" algorithm. Unfortunately, there is a type constraint put on objects of type `T` that states there shall be possible to move-assign objects of type `U` to `T` objects, i.e. `T` shall define a move-assignment operator that takes a `U` object. In other words, definition of `T` type assumes both the existence and the properties of `U` type, and explicitly exposes mechanics to _cooperate_ with the internals of that type, which all is not in scope of our case.

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

While name equivalence is desired for the compiler, `swap` implementation is interested in the actual structure of the type to be able to actually swap the things. Sadly, C++ language does not offer built-in constructs to perform a structural equivalence test on types, i.e. compare the types recursively, rather than the values they describe. Consider the following:

```c++
// A and B are equivalent under structural equivalence
// A and B are distinct   under name       equivalence
class A { int i; };
class B { int i; };
```

Having that, `std::swap` constraints its both operands to be of a single (i.e. homogeneous) `T` type, that in turn, guarantees structural equivalence for free. On the other hand, heterogeneous `swap` requires an additional _predicate_ that judges whether two types are structurally equivalent, (simplified) C++20 snippet follows:

```c++
template<class T, class U> void swap(T&, U&) requires StructurallyEquivalent<T, U>;
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

Side note. Structural equivalence is very useful in software source code analysis, aka "code reading"; as well as in the software prototyping (my advice here is to use Haskell(-like) language directly). Every piece of code has its functional equivalent expressed in plain structural equivalent and a set of actions with attributes (like `async`, `generated`, etc.). Set of structural equivalents can be limited to an absolute minimum, e.g. to a _list_ construct as it was done in Lisp; or sequences `[a]`, tuples `(t, u, ...)`, alternatives `x|y|...`, and combined types like maps `[(k1, v1), (k2, v2), ...]`.

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

Type constructor [`std::vector`](https://en.cppreference.com/w/cpp/container/vector) (for `T` other than `bool`) meets the requirements of `AllocatorAwareContainer`, `SequenceContainer`, `ContiguousContainer`, `Container`, and [`ReversibleContainer`](https://en.cppreference.com/w/cpp/named_req/ReversibleContainer); where `ReversibleContainer` must additionally satisify `LegacyRandomAccessIterator` (also required by `ContiguousContainer`) _or_ [`LegacyBidirectionalIterator`](https://en.cppreference.com/w/cpp/named_req/BidirectionalIterator) (which is a subset of `LegacyRandomAccessIterator`).

It is easy to notice that `std::vector` meets all the requirements of `std::string` if it satisfies `LegacyRandomAccessIterator` (in `ReversibleContainer`). From the concept-based point of view, `std::vector<T>` is a subset of `std::string` for some `T`.

Side note. It is worth to note that while both the following concepts must be satisified for `std::vector`, `ContiguousContainer` requires `LegacyRandomAccessIterator` solely, but `ReversibleContainer` is free to choose its weaker form (i.e. `LegacyBidirectionalIterator`). 


## AllocatorAwareContainer and direct memory access

Once we establish a structural equivalence for a pair of types, we may try to _swap_ their memory representations. To do that we need to define a custom allocator, its properties to fulfill ["allocator completeness requirements"](https://en.cppreference.com/w/cpp/named_req/Allocator#Allocator_completeness_requirements) (via optional specialisation of [`std::allocator_traits`](https://en.cppreference.com/w/cpp/memory/allocator_traits)), and actual behaviour during swap operation. Since we want to transfer the current state from one abstraction to the another, the designed allocator shall be stateful.

Notice that by performing a heterogeneous `swap` we, in fact, simulate `move` in contexts where it is actually not supported by design at all (like `vector<char> v = move(str)` where type of `str` is `string`).

Even if [`pmr`](https://en.cppreference.com/w/cpp/memory/polymorphic_allocator) offers dynamic selection of an allocator, we cannot use it. Consider this note from the reference (emphasis is mine):
> `polymorphic_allocator` **does not propagate on** container copy assignment, move assignment, or **swap**. As a result, move assignment of a `polymorphic_allocator`-using container can throw, and swapping two `polymorphic_allocator`-using containers whose allocators do not compare equal results in undefined behavior. 

We want to move the state while swapping the data type `T` allocated using `Alloc<T>`. To do so we need to inform `allocator_traits` it is acutally possible through `propagate_on_container_swap` member type.

```c++
template<class T, class BackingT>
struct ByteSeqAlloc
{
  using allocator_type = ByteSeqAlloc;
  using propagate_on_container_swap = std::true_type;
  using value_type = T;
  
  static_assert(std::is_convertible_v<T, BackingT>
             && std::is_convertible_v<BackingT, T>);
  
  // state [--
  BackingT* last    = nullptr;  // last allocated ptr
  std::size_t lastN = 0;        // last passed n
  bool reuse        = false;    // shall next allocate reuse `last`?
  // --] state end

  [[nodiscard]] T* allocate(std::size_t n)
  {
    // ... 
    
    if (auto p = static_cast<T*>(reuse ? last : std::malloc(n * sizeof(U))))
    {
      last = reinterpret_cast<BackingT*>(p);

      return reuse
              ? (reuse = false, reinterpret_cast<T*>(last))
              : (lastN = n, p);
    }
  }
  
  // ...
};

std::pair<U, T> swap(T& t, U& t)
{
  auto aT = t.get_allocator();
  auto aU = u.get_allocator();
  
  aT.reuse = true;
  aU.reuse = true;

  // inspect lastN and last to construct
  // copies of T and U with swapped data
  // (allocate shall return prepared memory)
}
```
```
using BackingT = std::common_type_t<char, std::uint8_t>;

std::basic_string<
    char
  , std::char_traits<char>
  , ByteSeqAlloc<char, BackingT>
  > s = {'a', 'b', ...};

std::vector<
    std::uint8_t
  , ByteSeqAlloc<std::uint8_t, BackingT>
  > v = {97, 98, ...};
  
auto [v1, s1] = swap(s, v);
```

Unfortunately, that's a the dead end rather than a proper solution. Memory management issues come into play, e.g. what if the original items that hold the pointers to the allocated memory try to reclaim it. SBO optimisation in `std::string` completely bypasses the allocator, i.e. this solution may work only for large enough strings. 

## Alternative

Helpful may be `span` from GSL and C++20:

```c++
std::span<std::uint8_t> v{s.data(), s.length()};
```

#### About this document

November 16, 2019; January 18, 2020 &mdash; Krzysztof Ostrowski

[LICENSE](https://github.com/insooth/insooth.github.io/blob/master/LICENSE)
