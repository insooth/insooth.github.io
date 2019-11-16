 # Heterogeneous swap -- WIP

A colleague of mine asked me a question about the most effective way to transform a `string` into a `vector<int8_t>`, sequence of [ASCII codes](http://www.asciitable.com/), where each character in the string gets its integer ASCII code.

That can be easily [solved with a `std::transform`](https://gist.github.com/insooth/30fa720d0d18eafc733880bef3d01acc) and a explicit cast. 

In fact, we cast to make the compiler happy rather than to change the underlying binary representation. Since `string::value_type` and `int8_t` are identical in memory (both occupy a byte), and both `vector` and `string` are (roughly) equivalent to `T*` and some metadata (if <acronym title="Small Buffer Optimisation">SBO</acronym> is not considered), we may be tempted to do a swap:

```
string s = "Hello world";

vector<int8_t> v;
assert(v.empty());

swap(s, v);  // <<<

assert(s.empty());
```

It would be great if the above worked well, but it didn't. STL's [`swap` is defined](https://en.cppreference.com/w/cpp/algorithm/swap) for homogeneous types (here: `T` type), i.e.

```c++
template<class T> void swap(T&, T&);
//             ^            ^   ^
```

While `std::swap` cannot be defined for two different types, and neither `vector` constructor takes `string` as an argument to steal its contents (cf. `move`), nor `string` constructor does that for `vector`, we have to look for another solution. This article is an attempt to use stateful allocators as an enabler of heterogenenous swap.

## Concepts

> std::basic_string satisfies the requirements of AllocatorAwareContainer, SequenceContainer and ContiguousContainer
> swap: T must meet the requirements of MoveAssignable and MoveConstructible. 
> std::vector (for T other than bool) meets the requirements of Container, AllocatorAwareContainer, SequenceContainer, ContiguousContainer and ReversibleContainer. 


## `pmr`

Custom allocator

#### About this document

November 16, 2019 &mdash; Krzysztof Ostrowski

[LICENSE](https://github.com/insooth/insooth.github.io/blob/master/LICENSE)
