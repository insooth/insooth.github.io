
# Access tuple-like container by type to return an index

A colleague of mine asked me a very interesting question that led to some code and this article. Here is the question:

> How would you look for an item in a tuple by the item's type, and return the item's index within that tuple?

It is easy to realise that we are going to deal with an iteration of a tuple. Such an iteration task can be done in a recursive way (as presented [here with `is_within`](https://github.com/insooth/insooth.github.io/blob/master/visitor-pattern.md)), or by means of variadic templates. The latter technique seems to be a better idea due to absence of an explicit recursion.

It turns out that the toughest task is to be able to convey an additional information just between the iteration steps: the current index of the element that is being processed. This article presents an approach to that task that is using lazy fold expression implemented as [`std::disjunction`](https://en.cppreference.com/w/cpp/types/disjunction), and an idea of type embellishment that lets us introduce an implicit context in which current element is embedded.

## Iteration

Let's recall `is_within` from [_The role of the visitor pattern_](https://github.com/insooth/insooth.github.io/blob/master/visitor-pattern.md), a metafunction that returns `true` if type `T` is found inside the tuple `U`. Here we use variadic templates to pattern match over the types `Us...` contained in the tuple `U`, and the progress is guaranteed by means of inheritance (explicit recursion), stop condition is trivial.

```c++
template<class T, class U>
//                      ^~~ tuple to be pattern-matched
struct is_within
  : std::false_type
//       ^~~ result (failure: T not found)
{};

template<class T, class... Us>
struct is_within<T, std::tuple<T, Us...>>
//               ^             ^
//               `~~~~~~~~~~~~~`~~~ stop condition
  : std::true_type
//       ^~~ result (success: T found)
{};

template<class T, class U, class... Us>
struct is_within<T, std::tuple<U, Us...>> : is_within<T, std::tuple<Us...>>
//                                        ^~~~ inheritance makes recursive call
{};
```

Generally, `is_within` does lazy disjunction (logical `OR`) of the elements in the tuple `U` with a predicate `is_same_v<T, Us>` applied to each one `Us` (indexed from 0) element from `U`.

```
   is_same_v<T, tuple_element_t<0, U>>
|| is_same_v<T, tuple_element_t<1, U>>
|| is_same_v<T, tuple_element_t<2, U>>
...
|| is_same_v<T, tuple_element_t<tuple_size_v<U>, U>>
```

C++17 brings [`std::disjunction`](https://en.cppreference.com/w/cpp/types/disjunction) and [fold expression](https://en.cppreference.com/w/cpp/language/fold) `(... || Us)`. While the former stops instantiation of templates upon first match (_short-circuits_), the latter instantiates every `Us` which is expensive and can cause a hard error when instantiation gives malformed type.

To be able to use disjunction over predicate-applied types we have to generate a sequence of indices by which given tuple will be accessed. This is done with [`std::make_index_sequence`](https://en.cppreference.com/w/cpp/utility/integer_sequence) that generates a tuple called `std::index_sequence` parametrised by the indices. That is, `std::make_index_sequence<5>` produces a type `std::index_sequence<0, 1, 2, 3, 4>`. We will pattern patch over the produced types to extract the index and apply it as a first argument to [`std::tuple_element`](https://en.cppreference.com/w/cpp/utility/tuple/tuple_element).

```c++
template<class Tuple, template<class> class Predicate>
class find_in_if
{
    template<class, class = void>
//                  ^~~ nested classes may not be fully specialised
    struct find_in_if_impl;
    
    template<std::size_t... Is, class _>
    struct find_in_if_impl<std::index_sequence<Is...>, _>
//                         ^~~ extract indices
    {
        // does short-circuiting:
        using type =
            std::disjunction<
                Predicate<std::tuple_element_t<Is, Tuple>>...
//                                                        ^~~ same for the rest
            >;

        // ***DOES NOT short-circuiting***:
        static constexpr bool value =
            (... || Predicate<std::tuple_element_t<Is, Tuple>>::value);
    };
    
 public:

    using type =
        typename find_in_if_impl<
            std::make_index_sequence<std::tuple_size_v<Tuple>>
//          ^~~ generate an index sequence
        >::type;
};

// template<class T> using is_int = std::is_same<T, int>;  -- partially applied
// using foundI = find_in_if<std::tuple<int, bool, float>, is_int>;
// foundI::value  -- true if found, otherwise false
```

Note that, `std::disjunction` can only be applied to types usable as base classes (recursion is done through inheritance), and to those that expose `value` member convertible to `bool`. Metafunctions from `<type_traits>` header, e.g. partially applied `std::is_same`, are good candidates for parameters to `std::disjunction`.

## Embellishment

Current version of `find_in_if` acts as a predicate solely, we receive yes-no answer from it. To be able to retain some information (like indices) and return it to the user with the search result, every type from the input `Tuple` will be embedded into `box` type that meets requirements put by `std::disjunction` (see previous paragraph).

```c++
template<
    class T                  // actual type
  , template<class> class P  // predicate
  , std::size_t I            // index within a tuple
  >
struct box
{
    using                        type  = T;
    static constexpr std::size_t index = I;
    static constexpr bool        value = P<T>::value;  // disjunction uses this
};
```

A contrived unique `box` type will be used as a fallback type, a type returned if disjunction were produced `false`.

```c++
template<std::size_t... Is, class _>
struct find_in_if_impl<std::index_sequence<Is...>, _>
{
    using type =
        std::disjunction<
            box<std::tuple_element_t<Is, Tuple>, Predicate, Is>...
          , box<struct not_found, Predicate, std::tuple_size_v<Tuple>>  // -- fallback
        >;
// ...
};
```

Result type `box` is flexible enough to carry over additional information if needed. Moreover, `find_in_if` is general enough to accept any predicate, and any type that understands [`tuple_element`](https://en.cppreference.com/w/cpp/utility/tuple/tuple_element) and [`tuple_size`](https://en.cppreference.com/w/cpp/utility/tuple/tuple_size) metafunctions in place of a `Tuple`.

## Full example

```c++
template<class Tuple, template<class> class Predicate>
class find_in_if
{
    template<
        class T  // actual type
      , template<class> class P  // predicate
      , std::size_t I  // index within a tuple
      >
    struct box
    {
        using                        type  = T;
        static constexpr std::size_t index = I;
        static constexpr bool        value = P<T>::value;  // disjunction uses this
    };
  
    template<class, class = void>
    struct find_in_if_impl;
    
    template<std::size_t... Is, class _>
    struct find_in_if_impl<std::index_sequence<Is...>, _>
    {
        using type =
            std::disjunction<
                box<std::tuple_element_t<Is, Tuple>, Predicate, Is>...
              , box<struct not_found, Predicate, std::tuple_size_v<Tuple>>  // fallback
            >;
    };
    
 public:

    using type =
        typename find_in_if_impl<
            std::make_index_sequence<std::tuple_size_v<Tuple>>
        >::type;
};

template<class Tuple, template<class> class Predicate>
using find_in_if_t = typename find_in_if<Tuple, Predicate>::type;
```

Live code is available on [Coliru](http://coliru.stacked-crooked.com/a/8d5e75e40ea504a6).

#### About this document

October 19, 2019 &mdash; Krzysztof Ostrowski

[LICENSE](https://github.com/insooth/insooth.github.io/blob/master/LICENSE)
