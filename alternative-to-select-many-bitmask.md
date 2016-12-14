
# Alternative to select-many bitmask

Suppose we have an interface that returns some value depending on combination of other values, and we would like get resource of some type `R` that is common for Alice and Bob. Here is our interface:

```c++
R query(std::uint32_t bitmask);
```

First question arises quickly: what to put into `bitmask`? There are plenty of values of type `uint32_t`. Usually, we look into documentation and examples for a naming convention of constants, e.g.:

```c++
// ... more bitmasks here
static const std::uint32_t GET_BITMASK_ALICE = 0x10000000;
// ... and here
static const std::uint32_t GET_BITMASK_BOB = 0x800000;
// ... and here
```

and then combine them to select many properties at once:

```c++
auto r = query(GET_BITMASK_ALICE | GET_BITMASK_BOB);
```

Presented above approach is hard to extend and easy to break, moreover relies on naming conventions that compiler simply does not take into account. Our interface is broken by design. Let's try to fix it.

## Classify

Enumeration makes it easy to classify bitmask values under a common type and get rid of naming conventions:

```c++
enum class E : std::uint32_t
{
// ...
  , Alice = 0x10000000
// ...
  , Bob   = 0x800000
// ...
};
```

which makes possible to mark the range of possible values:

```c++
R query(std::underlying_type_t<E> bitmask);

std::underlying_type_t<E> operator| (E lhs, E rhs)
{
    assert(is_power_of_two(lhs) && is_power_of_two(rhs));

    return   static_cast<std::underlying_type_t<E>>(lhs)
           | static_cast<std::underlying_type_t<E>>(rhs);
}
```

We do little to the types (we cannot mix enumerations of different underlying type), thus caller side looks just a little bit better than before (old way of invocation is still possible):

```c++
auto r = query(E::Alice | E::Bob);
```

## Hide

Through explicit `bitmask` exposed in the interface we shift our selection algorithm to the user, thus making user responsible for combining a **valid** mask. Such a mask is later on split into a sequence of values in the body of `query` using another bit mask operations. That's pretty much of bit twiddling. Can we just pass explicitly a sequence of values, so that we eliminate mask combining and recovering?

### Cul-de-sac

This is rather bad idea:

```c++
R query(std::vector<E>&&);
```
We will (probably) allocate small buffer on the heap and then release it quickly, thus increase memory fragmentation.

## Proxy without a type

C++11 introduces handy proxy of non-deducible type to array of items of unspecified storage that we can use here:

```c++
R query(std::initializer_list<E>);
```

All the mask combination (if still needed) is shifted to the internals of `query`:

```c++
auto r = query({E::Alice, E::Bob});
```

which gives us much cleaner interface.

## Lift

With `std::initializer_list<E>` we still require memory buffer to store passed sequence of items. Ideally, our select many can be done during compilation time. We would like to make it possible to use interface as follows:

```c++
auto r = query<Alice, Bob>();
```

which leads to variadic template with some bounds on parameters that disables `query` if any of passed types is not derived (no runtime polymorphism intended!) from type `S`:


```c++
struct S {};  // out "base class" that classifies types

template<class... Ts>
auto query()
  -> std::enable_if_t<
       std::conjunction_v<std::is_base_of<S, Ts>...>
     , R
     >;
```

where

```c++
struct Alice : S, std::integral_type<std::uint32_t, 0x10000000> {};
struct Bob   : S, std::integral_type<std:uint32_t, 0x800000> {};
```

We have added stronger typing to our application by use of loosely-coupled unique types classified by custom tag (here it is empty base class `S`). We can easily extend the set of available "enumerations", they can even carry the same value (which is not possible with `enum`). To add another set of "enumerations" we create new tag type and types that derive from it. Compiler detects misuse for us, no runtime is involved. The only precondition is that we need to know what we want to select beforehand, at the compilation time.

#### About this document

December 14, 2016 &mdash; Krzysztof Ostrowski

