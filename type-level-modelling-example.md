
# Type-level modelling example

Let's assume we want to model a piece of software that transforms relatively small fixed-size data fetched from the layer below upon request from layer above. Transformation involves memory allocation for the transformed data. We would like to pass allocated memory to the layer. We don't want to share data between layers, so that we want to model exclusive ownership. We would like to make it possible to pass user-defined algorithm that augments the allocated data, but don't let the user to modify the data explicitly. We want to have memory management automatic. We don't want to suffer from memory fragmentation. We want to achieve type-level safety as much as possible. We don't like to throw exceptions.

Let's reprase:

... *software* ... *transforms* *relatively small fixed-size data* ... *request from layer above* ... *memory allocation for the transformed data* ... *pass allocated memory* ... *don't want to share data* ... *exclusive ownership* ... *pass user-defined algorithm that augments* ... *allocated data* ... *don't let the user to modify the data explicitly* ... *have memory management automatic* ... *don't want* ... *memory fragmentation* ... *type-level safety* ... *don't like to throw exceptions*.

## Modelling

The data we pass to the layer above is of type `T` (transformed). The data we receive is of type `S` (source). Following can be observed:

* *software* --> some modularisation (at least dedicated class `A`) will be required,
* *transforms* --> there must exist at least one function `S -> T`,
* *relatively small fixed-size data* and *don't want* ... *memory fragmentation* --> `boost::object_pool` fits here well,
* *have memory management automatic* and *don't want to share data* ... *exclusive ownership* --> `std::unique_ptr` does this,
* *pass user-defined algorithm that augments* ... *allocated data* --> user passes function of type `T& -> E` where `E` is a type that indicates augumentation operation result,
* *allocated data* --> our layer will care about memory management,
* *don't let the user to modify the data explicitly*  --> make it impoossible to modify/release memory outside our layer,
* *type-level safety* --> trigger compilation error on contract violation where possible,
* *don't like to throw exceptions* --> `optional` and a model of `Either` (like `std::pair`) to carry errors will be helpful.

### Data types

We have distilled following data types:
* `boost::object_pool<T>` to avoid memory fragmentation,
* `std::unique_ptr<T, D>` to manage objects allocated within object pool (`D` is a custom deleter that will move object back to pool),
* `std::optional<std::unique_ptr<T, D>>` to wrap allocated resource or signal lack of it,
* `std::pair<E, std::optional<std::unique_ptr<T, D>>>` to carry status of type `E` along with (possibily) valid resource.

### Interface

We need to figure out how layer above will call us, i.e. we need to define our interface. Since data will be provided upon *request from layer above*, and we need to make it possible to *pass user-defined algorithm that auguments* ... *allocated data*, following minimal interface can be defined inside out layer's scope (let's use `struct A`):

```c++
struct A
{
    enum class E { no_error, error/*, ...*/ };

    std::pair<E, std::optional<std::unique_ptr<T, D>>> take();

    template<class F>
    E augument(std::unique_ptr<T,D>& v, F&& f);
};
```

Unfortunately, such an interface contains a bug that violates *don't let the user to modify the data explicitly* requirement. We are able to do: 

```c++
A a;
auto r = a.take();

assert(valid(r));
assert(boost::none != r.second);

*(r.second)->mutate();
```

or even cause double-free easily:

```c++
r.second.get_deleter()(r.second.get());
```

We want to get rid of such issues by using types. We want punish user with compilation error upon attempt to modify resource outside `A`.

## Refining interface

### Fixing memory management

We don't let the user (i.e. actions outside `A`) modify the contents under `unique_ptr`, how we can achieve that? We cannot simply put `const unique_ptr<T, D>`, because we want be able to `move` it to the user. We don't wan to play with `const &&` either. Half-solution is to mark managed resource `const`, i.e. `unique_ptr<const T, D>`. This will work but we need to refine our deleter `D`:

```c++
class D
{
 public:
    constexpr D(boost::object_pool<T>& p) : pool{p} {}

    // NOTE: this function can be called at any point
    void operator() (T* t)
    //               ^~~ we will have `const T*` here
    { if (nullptr != t) pool.destroy(/* will be const! */t); }

 private:
    boost::object_pool<T>& pool;
};
```

We want to limit possibility to call `D` to `A` actions only. We can simply do that by (`unique_ptr` adjusted too):

```c++
class D
{
    friend class std::unique_ptr<const T, D>; // only unique_ptr can run this deleter

    void operator() (const T* t)
    { if (nullptr != t) pool.destroy(const_cast<T*>(t)); }
    // const cast is safe since memory for T was initially non-const,
    // it was obtained through non-const pool.construct()

    boost::object_pool<T>& pool;

public:
    constexpr D(boost::object_pool<T>& p) : pool{p} {}
};
```

Now, following casuse compilation errors (`p` is of type `std::unique_ptr<const T, D>`):


```c++
// cannot mutate -- read-only view
p->mutate();

// ...cannot mutate even this way
p->get()->mutate();

// cannot release memory manually
p.get_deleter()(p.get());

// cannot copy, unique_ptr property
auto p2{p};
```

We gained certain type-level safety for our resource manager `A` that manages pool of `T` objects. Our `take` interface function evolved into:

```c++
struct A
{
  // calls A::pool.construct() and transfers ownership to user if no errors
  std::pair<E, std::optional<std::unique_ptr<const T, D>>> take();
};
```

### Fixing passing user-defined actions

Unfortunately `std::unique_ptr<const T, D>` makes it impossible to modify data of type `T` at the caller side. We need to user-defined pass algorithm to `take` to modify object of type `T&` before it gets wrapped into `unique_ptr`. Let's adjust `take`:

```c++
template<class F>
    requires Callable<F, T&, E>
std::pair<E, std::optional<std::unique_ptr<const T, D>>> take(F&& f);
```

and `augument` which can access read-only data directly at the caller side (without transfering ownership):

```c++
template<class F>
    requires Callable<F, const T&, E>
E augument(const std::optional<std::unique_ptr<const T, D>>& v, F&& f);
```

We have lifted `unique_ptr` into `optional` to make it easier to be used with `take` result type: `augument` will apply `f` to value under `optional` if it is meaningful (i.e. not `none`) ,and will return result of that application (of type `E`) to the caller.

What if user does not want to pre-process while calling `take`? We can define some "interface sugar":

```c++
std::pair<E, std::optional<std::unique_ptr<const T, D>>> take()
{ return take([](T&) { return E::no_error; }); }
```

that mimic "noop" action on the allocated data if it exists.

## And that's it!

We defined an interface that composes available abstractions and provides acceptable level of type-safety. We allocate resource using object pool, thus we avoid memory fragmentation. We control data mutation, by allowing it only in explicitly defined ways (here in `take`). Type system prevents user from mutating (including memory releasing) of the received data. We use `optional` and model of `Either` to signal errors to the user. Allocated memory ownership is exclusive, released memory moves back to the pool.

Note that we can reason about the interface by looking at its functions' signatures. That's definitely an example of a good interface!

#### About this document

October 24, 2016 &mdash; Krzysztof Ostrowski
