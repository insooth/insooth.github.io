
# When type inference fails

C++11 re-introduces `auto` keyword that enables basic type inference. Using `auto` not only improves code readability. Consider:

```c++
static std::shared_ptr<VeryLongInterfaceType<Something>> s_commonInterfaceProvider(std::make_shared<VeryLongInterfaceType<Something>>());
```

versus

```c++
static auto s_commonInterfaceProvider = std::make_shared<VeryLongInterfaceType<Something>>();
//          ^~~ name is here            ^~~ interface    ^~~ resource
```

which is far more readable and does not contain repeated type information.

Use of type inference puts impact on what is possible to be done with certain value, i.e. on its interface or concept it models, rather than on its concrete type. Unfortunately, `auto` does type inference locally, thus is not such powerful as we might expect. Here follows some examples of `auto`-inference failures.

## A case of range

Let's define such a sequence integer indexes. This will not work with C++14:

```c++
auto indexes {1, 2, 3, 4, 5};
//          ^~~ error: direct-list-initialization of 'auto' requires exactly one element 
```

And this works fine:

```c++
auto indexes = {1, 2, 3, 4, 5};
```

Type inferred for `indexes` is neither `std::vector` nor `std::array`, it is `std::initializer_list<int>`. That's very useful in expressions like:

```c++
enum class item { a = 100, b, c, d };

for (auto i : { item::a, item::b, item::c, item::d }) {}
//        ^~~ type is item

for (auto i : {"x", "y", "z"}) {}
//        ^~~ type is const char*

```

Note that, unlike for references, pointer is inferred. Constness is added implicitly. To achieve similar effect for references `decltype(auto)` shall be used.

## A case of including

Some sources say that using forward declaration whenever possible is a must, others say that headers shall be self-contained (which implies that order of `#include`s does not matter). Let's consider an interface (file `interface.hpp`):

```c++
struct A;

A* foo();
```

and its usage that produces compilation error:

```c++
#include "interface.hpp"

auto something = foo();
something->bar();
// ^~~  error: invalid use of incomplete type 'struct A'
```

That's easy to be fixed in early project phase, but may not be so obvious if caused by late refactoring. Compiler reports an errors related to the type which is not visible in our code. Moral of this story is such that publicly exposed header files shall be self-contained.

Moreover, we deal implicitly with pointers (`auto` infers pointers correctly), and we can easily crash at dereferencing `something`. Someone can be tempted to fix that by returning reference instead of a pointer. That's really bad idea, consider:

```c++
auto something = foo();
something.foo();
//       ^~~ null pointers cannot happen (if no hackery done)
```

We solved dangerous crash possibility by doing copy of a value returned by `foo`. This is what `auto` for references does. In order to deduce a reference, we need to use:

```c++
decltype(auto) something = foo();
```

That requires us to remember that `foo` returns reference, which reduces type inference to type aliasing (where `auto` is just "abbreviated" `A`). That's rather disappointing.

There is way to assure not-null resource under `something` and straight usage of `auto` by changing `foo` result type to `std::reference_wrapper<A>` which is `TriviallyCopyable`, thus works well with type inference through plain `auto` that leads to copying of a value.

```c++
// std::reference_wrapper<A> foo();

auto something = foo();
something.get().foo();
//       ^~~ ugly unwrapping
```

We still need to look under the mask to know that we must unwrap the stored reference. This may be fixed by introducing overloading of `operator.` in future incarnation of C++.

## A case of folding
  
Let's take an example of folding a sequence of integers using an action that produces sequence of types `T`. Sequences of type `T` are produced by passing integer index `i` to function `get`.

Each time we access index `i`, we fetch sequence of `T` values through `get(i)`, and we append it to the sequence produced at previous step `i - 1`. Initial sequence of `T` is empty. Let's model that with `std::accumulate` (left fold):

```c++
auto indexes = {1, 2, 3, 4, 5};

auto out =
    std::accumulate(std::begin(indexes), std::end(indexes), {}
                  , [](auto&& result, auto index)
                    {
                        auto ts = get(index);  // (1)

                        const auto size = result.size();  // (2)
                        result.reserve(size + ts.size());

                        (void) std::move(std::begin(ts), std::end(ts), std::back_inserter(result)); // (3)

                        return std::move(result);  // (4)
                    });
```

`(1)` fetches sequence of items at given `index`, `(2)` does some memory usage assumptions, `(3)` appends `ts` to `result` (which is `{}` initially), `(4)` moves result to te caller (which will move it as `result` to action executed for the next index).

Note, that presented code assumes that certain types provide certain semantics, i.e. at least:
* `(2)` assumes that type of `ts` provides operation `size() -> a`,
* `(2)` assumes that type of `result` provides operations `reserve(a) -> b` and `size() -> a`,
* `(2)` assumes that values of type `a` can be added (operation `+`) and output of such operation is of type `a`, 
* `(3)` assumes that type of `result` provides append-like operation (usually `push_back` or `emplace_back`),
* `(3)` assumes that `ts` and `result` are containers that store values of equal types,
* `(3)` assumes that values in `ts` container must be at least `Copyable`
where `a` and `b` are some types.

Besides the above, we can notice that:
* types of `indexes` and `ts` must be iterable, i.e. `begin` and `end` operations can be applied over them,
* values of type of `result` must be `DefaultConstructible` with `{}`,
* `result` and `ts` must be at least `CopyConstructible` and `CopyAssignable` (`accumulate` requires that).

Those assumptions are in fact requirements on types that must be satisfied, otherwise we won't get code compiling.

Our solution seems to be conceptually correct, unfortunately it does not type check. Compiler (GCC 6.1.0 with `-std=c++14`) reports that it was not able to deduce template parameter `_Tp` in the following expression:

```
template<class _InputIterator, class _Tp, class _BinaryOperation>
_Tp std::accumulate(_InputIterator, _InputIterator, _Tp, _BinaryOperation)
```

`_Tp` stands here for a type of `result` which has to be the same as a type of `ts` due to fact that C++ does not support statically typed heterogeneous iterable containers (neither `tuple` nor single-value container `variant` are iterable). Type of `ts` is known from inspecting `get`. We need to help compiler moving forward by fixing `_Tp`, that is:

```
    std::accumulate(std::begin(indexes), std::end(indexes), R{}
//                                                          ^~~ here
```

where `R` is be described as `using R = decltype(get({}));`. That solves our type tetris.

Note, that neither fixing result type for `accumulate` nor fixing lambda argument/result type:
```
                  , [](R&& result, auto index)
//                     ^~~ here

                  , [](auto&& result, auto index) -> R
//                                                   ^~~ here
```
help compiler. Signature for `accumulate` expects `_BinaryOperation` that does not explicitly state type requirements (like `Callable<_BinaryOperation, _R, _I, R>` where `_R` is any form of decorated `R` (e.g. `R&&`), so that `_I`). Things may change once Concepts TS get merged into standard. 

## A note on `Copy`-concepts

Plain old copy is used as a fallback when move operation cannot be perfomed. That will imply that type that is `CopyConstructible` or `CopyAssignable` may be respectively `MoveConstructible` or `MoveAssignable` too. In other words, we may think that `Move` concepts are derived from `Copy` concepts, but that's not true. `Copy` concepts require `Move` concepts to be satisified, i.e. `Copy` concepts are derived from `Move` concepts. That seems to be counter-intuitive, because `Move`-semantics are by default optional and `Copy`-semantics are by default mandatory (but can be disabled).



#### About this document

November 5, 2016 &mdash; Krzysztof Ostrowski
