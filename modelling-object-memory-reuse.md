
# Modelling reuse of object memory

We would like to minimise memory fragmentation by reusing already allocated memory once it is no longer owned by the application. For fixed-size data it is easily achievable with a pool of fixed-size objects. Reusing varying-size objects needs more attention.

NOTE: We are not going to model pool allocator. We want to model management of object that already provide allocators. We want to minimise memory fragmentation without writing specialised allocators.

Object pool is typically built from sequence of objects and a data structure that points to the next object ready for reuse. Object can be in one of the states:
* not allocated and not in use (unborn),
* already allocated and in use (occupied),
* already allocated and not in use (free),
* objects is destructed and memory released -- happens in pool destructor (dead).

```
                           +--------------release--------------+
                           |                                   |
                           |                                   V
    unborn --allocate--> free --use--> occupied --release--> dead
                           ^              |
                           |              |
                           +----leave-----+
```

To track the state of the object pointed by `T*` (which can be equal to `nullptr` or valid pointer to the allocated memory) we use `size_t` value that indicates usage count of the object. We define a pair `(object, usage)` of type `std::pair<T*, size_t>`, where its valid values and corresponding states are:
* `(nullptr, 0)` → unborn,
* `(p, 0)` where `p != nullptr` → free,
* `(p, n)` where `p != nullptr` and `n > 0` → occupied,
* otherwise → dead.

We would like to be able to fetch first pair that is either in state free (most preffered) or unborn. That requires some ordering relation for `std::pair<T*, size_t>` and data structure that will provide fast access to the pair we would like to use/reuse (move into state free or occupied). We can use max heap (namely `std::make_heap` and `std::pop_heap`) to access the first free or unborn item.

There must be an upper limit `max` of objects managed in the pool, i.e. pool cannot allocate until out of system memory. To achieve that we will use `std::array<pair<T*, size_t>, max>` which will fit well into max heap (we need to define custom comparator).

We are going to use varying-size objects, thus we need to classify our objects somehow. Classification algorithm depends on the objects' (data) properties.

Let's model for specialised `Eigen::Matrix<double, Dynamic, Dynamic>` objects, where matrix dimensions `a` and `b` will vary. We want to provide object pool for matrices of different size, thus we want to map `(a, b)` into allocated memory for a matrix of that size. Pairs `(a, b)` compose intervals that point to `std::array<std::pair<T*, size_t>, max>`, we can use `std::map` with custom comparator to build... *interval tree* in fact. We will produce constrained interval tree, where 
* both endpoints are included, i.e. we have `[a, b]` intervals only,
* intervals never overlap (no split, merge, etc. happen), even if we insert `[3, 3]` having `[4, 4]` already in the tree.

Having intervals trees involved we can easily search for an already exisiting pool of `(a', b')` that is close enough to the dimensions `(a, b)` we are looking for and use it instead of allocating new tree node.

We want to make object state transitions thread-safe and model exclusive ownership (then releasing memory is equal to moving back object to the pool). Those can be achieved with:
* `std::shared_ptr<std::pair<std::mutex, std::array<std::pair<T*, size_t>, max>>>` and a custom deleter that will free sequence of not-null `T*` objects,
* `std::unique_ptr<T, D>` where `T` is a memory from the pool and `D` is a custom deleter that moves back object to the pool.

We can model our pool data as:

```c++
using P = std::map<
   std::tuple<size_t, size_t>  // lexicographical comparator out-of-the-box
 , std::shared_ptr<
    std::pair<
        std::mutex
      , std::array<std::pair<T*, size_t>, max>
      >
    >
 >;
```

and provide following interface (`construct` has access to `P` object):

```c++
template<class... As>
std::pair<E, std::optional<std::unique_ptr<T, D>>> construct(const K& k, As&&... args);
```

where `D` stores information to which pool put back the returned `T`, at least `std::tuple<size_t, size_t>` (here aliased into `K`) to navigate in the three. Search for `T*` in tree mapped type is linear, so that `max` value should be kept small. Moving from state free (or unborn which requires memory allocation for `T` object) to occupied is done by dedicated function that locks stored `mutex` and modifies the `array` of objects.

