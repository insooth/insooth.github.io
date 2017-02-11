
# Tweaking `std::binary_search` for intervals

STL comes with useful bisection algorithm that checks presence of a value of type `T` inside a range of values of type `U`. Certain requirements must be satifisied by both types `T` and `U` and their values. This article briefly summarises internals of `std::binary_search`, our algorithm in question, and demonstrates its usage in searching across intervals with passing found value back to the user.

## Interface

STL's `binary_search` returns value of `bool` type, i.e. it effectively checks whether given `value` is inside the range. That makes sense if our range contains items of `value` type, including needle. It is not needed to return iterator to the item which value we already know. Moreover, it protects us from ourselves: bisection is not supposed to modify input collection, what might happen if iterator was returned.

`std::binary_search` has [two prototypes](http://en.cppreference.com/w/cpp/algorithm/binary_search):

```c++
template< class ForwardIt, class T >
bool binary_search( ForwardIt first, ForwardIt last, const T& value );

template< class ForwardIt, class T, class Compare >
bool binary_search( ForwardIt first, ForwardIt last, const T& value, Compare comp );
```

where the former calls latter with comparision function object that satisfies [`Compare`](http://en.cppreference.com/w/cpp/concept/Compare) concept effectively equal to [`std::less<>`](http://en.cppreference.com/w/cpp/utility/functional/less) (see note below).

### Range

Algorithm takes a range denoted by two iterators `[first, last)`. The only requirement put on the value type stored in such range, is that there must exist a way to compare it with passed `value` of some type `T`. On the other hand, values in the range must be at least partially ordered, i.e. we shall be able to decide whether given value is less than another (equality check is induced through `!(x < y) && !(y < x)`). In practice, we sort the range beforehand.

### Comparision function object

The `comp` passed to `binary_search` is a decision maker that drives the algorithm. It is a binary predicate that models [`Compare` concept](http://en.cppreference.com/w/cpp/concept/Compare) which establishes strict weak ordering relation (that is modelled by `operator<`) and induces equivalence relationship if `!comp(x, y) && !comp(y, x)` yields `true`.

There exist multiple `Compare` models and its users across STL headers, one of them is [`std::less<>`](http://en.cppreference.com/w/cpp/utility/functional/less) used for instance by [`std::map`](http://en.cppreference.com/w/cpp/container/map) to order its keys. It can be passed to `binary_search` iff range items are of type `T` or can be implicitly converted to that type. In other words, it deals with homogeneous types.

We are free to write custom model of `Compare` concept. That's not hard, but there's one caveat we should be aware of. Bisection algorithm actually finds given `value` once equivalence relationship is met, i.e. `!comp(value, y) && !comp(y, value)` yields `true`. That requires us to provide less-than operations for values of types `T` and `U` (item from range) types, as well as for `U` and `T`. In practice, that's function object with two functions: `bool operator() (T, U)` and `bool operator() (U, T)`. That enables us to deal with heterogeneous types, where needle is of different type that the type of an item in the range.

## Data

We would like to find a point that belongs to some interval. Interval is represented as a pair of values of the same type called endpoints and associated metadata. Metadata defines whether given endpoint is open (`(`, `)`) or closed (`[`, `]`). For example, `(0, 1]` is right-closed, left-open interval from `0` to `1`.

To be able to use `binary_search` we need to sort sequence of intervals (the range) beforehand. It is easy with vocabulary types [`pair`](http://en.cppreference.com/w/cpp/utility/pair) and [`tuple`](http://en.cppreference.com/w/cpp/utility/tuple) that have defined lexicographical comparison, and so `operator<` required by [`std::sort`](http://en.cppreference.com/w/cpp/algorithm/sort).

Our task is to be able to find an interval in a sequence of intervals to which given point belongs to. We want to return handle to found interval back to the user. To accomplish that task, we need to write a model of `Compare` with a state that will carry (optional) handle to one of the interval if it is found. Note, that we need to compare two different types using less-than relation: point against interval: `x < (a, b)`, and interval against point: `(a, b) < x`. Let's prototype those ordering algorithms.

## Ordering prototyping

Haskell is great for prototyping and will be used here. We have to deal with two cases as mentioned, but if we take into account open-close endpoint cases for interval start (`L`) and interval end (`R`) points, we have (1):


```
x         -- needle ordered before interval
L a b R   -- interval (a, b) or [a, b) or (a, b] or [a, b]
```

and (2)

```
L a b R   -- interval (a, b) or [a, b) or (a, b] or [a, b]
x         -- needle ordered after interval
```

cases. If our intervals are sorted in ascending order, cases (1) and (2) will produce `true` for respectively `x < L a b R` and `L a b R < x` only in certain conditions. Let's model open-close property with type `ParenProp` as an alternative:

```haskell
data ParenProp = Open | Closed
```

Since we can have [either](https://hackage.haskell.org/package/base-4.9.1.0/docs/Data-Either.html) left endpoint or right endpoint we model it as:

```haskell
type Paren = Either ParenProp ParenProp
```

Case (1) called `lessL` ("less from left") through endpoint analysis produces:

```haskell
-- | x < (a, b)
lessL :: Double -> (Double, Double) -> (Paren, Paren) -> Bool
lessL x (a, b) ps =
    case ps of
        (Left Open,   _) -> x <= a
        (Left Closed, _) -> x < a
        otherwise        -> False
```

and (2) called `lessR` ("less from right") similarly gives:

```haskell
-- | (a, b) < x
lessR :: (Double, Double) -> Double -> (Paren, Paren) -> Bool
lessR (a, b) x ps =
    case ps of
        (_, Right Open)   -> x >= b
        (_, Right Closed) -> x > b
        otherwise         -> False
```

We've just defined algorithms that will be translated into C++ and used in the `Compare` model passed to `binary_search`. Live code is [available on Codepad](http://codepad.org/TwASbkgn).

## Implementation

We define a sequence of intervals and accompanied metadata as type `E`:

```c++
using E = std::tuple<std::pair<unsigned, unsigned>, int, std::pair<bool, bool>>;
//                             b         b          v              L     R
```

where `v` is some value bound to the range; `L` equals `true` gives closed left endpoint, otherwise open; `R` does the same as `L` but for right endpoint.

`Compare` model is defined by type `C` of shape:

```c++
struct C
{
    bool operator() (unsigned x, const E& e)
    {
        const bool m = lessL(x, e);

        if (!p && !m && !lessR(e, x)) p = &e;

        return m;
    }

    bool operator() (const E& e, unsigned x)
    {
        const bool m = lessR(e, x);

        if (!p && !m && !lessL(x, e)) p = &e;

        return m;
    }

    const E* p = nullptr;  /** Found interval (if so). */
};
```

where `less*` are defined as:

```c++
bool lessL(unsigned x, const E& e)
{
    const auto& r = std::get<0>(e);
    const auto& c = std::get<2>(e);

    return c.first ? (x < r.first) : (x <= r.first);
}

bool lessR(const E& e, unsigned x)
{
    const auto& r = std::get<0>(e);
    const auto& c = std::get<2>(e);

    return c.second ? (x > r.second) : (x >= r.second);
}
```

We invoke `std::binary_search` with a reference to `C` object since we want are interested in its `p` member.

```c++
C c;
const bool found = std::binary_search(std::begin(e), std::end(e), v, std::ref(c));
```

Note that, if `e` (which is of type `E`) reallocates, `p` may point to rubbish. Thus, usage of plain array of `std::array` to store `E` objects is recommended.

Live code is [available on Coliru](http://coliru.stacked-crooked.com/a/5dfd6c6fe832d4d2).

#### About this document

January 27 February 11, 2017 &mdash; Krzysztof Ostrowski

