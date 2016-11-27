
# Basic dependent typing in example

Using fundamental types to do "simple things simple" fits very well in the most of cases . Unfortunately, if we assume that our application uses only subset of values carried by given fundamental type, we are like to get warning from the compiler. We shall take that warning seriously.

Cosider following code:

```c++
int transfer()
{
  const unsigned d = offset();
  const int p      = position();

  return static_cast<int>(p + d);
  //                      ^~~ quite a lot of implicit conversions here!
}
```

Note that, minimum value of `unsigned` is `0`, but its maximum value is twice as big as it is for `int`. That implies possible overflow on addition and loss of data due to cast in `return`. However... we go with this assumption:


> Result will never overflow, since `p + d` will never be greater than maximum value of `p` (i.e. `int`).

How to inform type system about that? Example solutions follow.

# Runtime assertion

Putting a runtime assertion is what cautious people usually do. We want to assure following holds:

```
d + p <= INT_MAX
                                    ==> add types:
unsigned(d) + int(p) <= int(INT_MAX)
                                    ==> sort by types:
unsigned(d) <= int(INT_MAX) - int(p)
```

which gives:

```c++
assert(d <= std::numeric_limits<int>::max() - p);

/* -- testing:
 (d <= INT_MAX - INT_MAX) ==> (d == 0)
 (d <= INT_MAX - INT_MIN) ==> (d <= INT_MAX + INT_MAX) ==> (d <= UINT_MAX)
*/
```

Assertion shall be put before doing actual computations.

# Expand domain

Lift computations to common type and check before cast in `return` is another option. Type that represents values domain of `int` and `unsigned` completely is `std::int64_t` (from `<cstdint>`) provided that `int` is `std::in32_t` and `unsigned` is `std::uint32_t`.

Lifted `transfer` looks as follows:

```c++
int transfer()
{
  const unsigned d = offset();
  const int p      = position();

  return static_cast<int>(std::int64_t{p} + std::int64_t{d});
  //                 ^~~ (im)possible loss of data
}
```

and may generate false alarm on data loss (cast from `std::in64_t` to "smaller" `int`).

# Lift types

Presented techniques offer certain level of safety during runtime, but do not (mostly) affect type system. How we can model our assumptions on values explicitly through type system?

One can say, dependent typing will help us (i.e. types depend on the values, in our example we'll choose types for `p` and result depending on values of `d` and `p`). Such feature most likely does not exist in C++. There is `std::integral_constant`, though. 

Let's introduce custom types for `p` and `d`, and change result type accordingly. As a first step we will make use of type inference:

```c++
auto transfer()
{
  const auto d = offset();
  const auto p = position();

  return p + d;
}
/* -- without temporaries:
auto transfer() { return position() + offset(); }
*/
```

There is only one operation to be implemented, i.e. addition of `Position` and `Offset` (in this order) values. Such operation returns new `Position` value. We would like to disable `Position::operator+` for certain values of offset. In order to do that, we need to know current value of the position. We want to do all that during compile tine.

```c++
template<unsigned i>
constexpr auto operator+(Offset<i> d)
  -> std::enable_if_t<
        (i <= Position::max - Position::value)  // here's our assertion
      , Position<Position::value + i>
      >
{
  return Position<Position::value + i>{};
}
```

where

```c++
template<int p>
struct Position : std::integral_constant<int, p>
{
  static constexpr auto max = std::numeric_limits<int>::max();
  // operator+
};

template<unsigned d>
using Offset = std::integral_constant<unsigned, d>;
```

Let's test it:

```c++
auto p1 =   Position<std::numeric_limits<int>::max()>{}
          + Offset<0>{};  // OK
auto p2 =   Position<std::numeric_limits<int>::max()>{}
          + Offset<1>{};  // compile-time error
auto p3 =   Position<std::numeric_limits<int>::min()>{}
          + Offset<std::numeric_limits<unsigned>::max()>{};  // OK
```

However following does not work due to explicit addition:

```c++
auto p4 =   Position<std::numeric_limits<int>::min()>{}
          + Offset<std::numeric_limits<unsigned>::max() + 1>{};  // wrong result!
//                                                      ^~~ overflow!
```

# And that's it!

We've done basic dependent typing in C++ and lifted runtime assertion into compile-time check.


#### About this document

November 27, 2016 &mdash; Krzysztof Ostrowski
