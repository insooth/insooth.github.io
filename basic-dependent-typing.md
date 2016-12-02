
# Basic dependent typing

Using fundamental types to do "simple things simple" fits very well in the most of cases . Unfortunately, if we assume that our application uses only a subset of values carried by given fundamental type, we are likely to get warning from the compiler. We shall take that warning seriously.

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

Note that, the minimum value of `unsigned` is `0`, but its maximum value is twice as big as it is for `int`. That implies possible overflow on addition and loss of data due to cast in `return`. However, we ship code with this assumption in mind:


> Result will never overflow, since `p + d` will never be greater than maximum value of `p` (i.e. `int`).

How to inform type system about that? Example solutions follow.

# Runtime assertion

Putting a runtime assertion is what cautious people usually do. It has nothing to do with type system and disappears if `-DNDEBUG` is passed to compiler. We want to assure following holds:

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

Lift computations to common type and check before cast in `return` is another option. Type that represents values' domain of `int` and `unsigned` fully is `std::int64_t` (from `<cstdint>`) provided that `int` is `std::int32_t` and `unsigned` is `std::uint32_t`.

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

