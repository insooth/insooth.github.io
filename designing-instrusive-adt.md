
# Designing intrusive data types

Designing for resource-constrained systems requires more attention that usual. We need to pose "what if" question much often, ask for edge conditions and handle _all_ the cases we can anticipate. We need to understand control flow and deal with it transparently; know the resources, their owners and lifetime. We need to do multitude of things and do them with care, and at the same time keep our design _simple_, it may be complex but never complicated.

We can help ourselves at the very begining and start with design that is [testable](https://github.com/insooth/insooth.github.io/blob/master/testable-design.md "Start with testable design right now"). We can start even better and separate data from the algorithms that operate on it, as it is done in C++ STL and in the most of  functional laguages. We can do even more and move part of responsibilities directly to the caller.

Let's have a look at the selected thoughts that build idea of _intrusive_ design which applies well to the resource-constrained systems.

NOTE: This article is neither about [building intrusive list or tree](http://www.codefarms.com/publications/intrusiv/Intrusive.pdf "Intrusive Data Structures  - Jiri Soukup"), Linux kernel [lists](https://isis.poly.edu/kulesh/stuff/src/klist/ "Linux Kernel Linked List Explained"), nor [Boost](http://www.boost.org/doc/libs/1_61_0/doc/html/intrusive.html "Boost.Intrusive") or [BSD](http://www.freebsd.org/cgi/man.cgi?queue "FreeBSD Library Functions Manual - QUEUE(3)") intrusive containers.

## Data and control plane

Widely used communication protocols that are used to exchange data between endpoints share common idea of dedicated components. Those components include i.a. _data plane_ that defines the raw data being transported; and _control plane_ that establishes, keeps alive and terminates communication. Typically, _data plane_ describes way to treat raw bits of data through predefined algorithms, so that it actually _does_ some controlling of the raw data being transported.

Split of data and algorithms that operate on it applies in multiple places. One of them is STL containers and `<algorithms>` together with `<functional>`, although it is not perfect (cf. `std::map` and `std::find` which is not `map`-aware, [UFCS](https://isocpp.org/files/papers/N4165.pdf "Herb Sutter: Unified Call Syntax") may fix that). Intrusive design benefits from the described split.

### Memory utilisation

Most of the data we operate on is fixed in the memory, i.e. it is preallocated. We can easily allocate huge buffer with `mmap` upon system start and take chunks out of it later on, but in such case we need to deal with fragmentation and all the other issues already solved in kernel memory management and `malloc`'s internals (which are designed intrusively).

What we want to have, are the pools of identical objects (rather than raw memory) and predefined ways to compose such objects. By objects we mean structures with known data layout, i.e. they never define hidden members introduced by `virtual` keyword, or we can predict offsets and sizes of those members. We never allocate (on heap, stack, etc.) _additional_ memory: we can take an object from the pool, or put back object to the pool. That implies following:
* we know address ranges and addresses of all the objects (i.e. memory) used in our system &mdash; that gives additional debugging possibilities,
* we link object through pointers which are always valid addresses (memory is prealloacted and never freed during runtime),
* we do not use subtype polymorphism, thus we eliminate indirect calls through `vtable`s &mdash; we strive to be cache-friendly (we can even force data alignment),
* fatal error is raised if we run out of "free" objects in the pool &mdash; we need to carrefully calculate pool sizes and give fallback possibilities (force some resources to be freed, c.f. [Linux kernel out-of-memory handling](http://linux-mm.org/OOM_Killer "OOM Killer") or calls dropping in case of emergency calls).

As an example we model forward traversal list `[head, next]`:

```c++
auto head = pool_of<item>::occupy();
auto next = pool_of<item>::occupy();
head.link(next);
```

where

```c++
struct item
{
    // some ctors, etc.

    void link(const item* const element) { /* ... */ }

 private:
    // some data here
    atomic<item*> next{nullptr}; 
};
```

There is no extra memory allocation for the "link" that connects nodes, i.e. item in the list contains pointer to the next item. Pointer assignment can be done by use of non-blocking algorithms. To attach same node to more than one list, we need to duplicate "link" member `next` as many times as we need. That means, we need to anticipate during system design the maximum number of node reuse. Reuse count  will apply for all the nodes of given type, so actual value shall be defined with care.

It is an arbitrary decision whether list should be circular or not. Typically `head` should store size of the list, and that implies head node to be of different type. Linking already linked item is a fatal error in resource management. Same applies to releasing already released item.

We can lift result type of function `pool_of<T>::occupy` into `Maybe`-like monad seemlessly by wrapping result type into a `item`-compatible barebone. If the `head` is `Nothing` then no matter what is under `next`, `head.link(...)` has no effect. Special care shall be taken if releasing `next` while `head` is `Nothing`. That can be done seamlessly or explicitly:

```c++
head.link_or_release(next);
```

### State persistency

Algorithms that operate on data in intrusive design do not create that data generally. The data to be processed must be already _prepared_. That includes all the user-specific parts (_payload_) and meta-data which is required by algorithms and abstract data structures in which data is organised. Example for the latter is the `next` member in the intrusive linked list.

In case of stateful system we would like to store additional data that is utilised by the algorithms internally. That may be reference counter, availability bit, etc. The hard part is to define beforehand the data which is going to be stored persistently in objects. Size of such data must be minimal yet fully satisfying given algorithm.

## Resources

Resources are not just about the memory. Consider a mutex which is (in fact) piece of allocated memory available at certain address, but it has much more complex semantics that regular allocated memory. What does it mean to copy a mutex? What does it mean to destroy a mutex? When to reclaim the allocated memory? hat is why there is no one simple solution to garbage collecting engine.

Dealing with _regular_ resources is a base that can be reused and extented to resources with extra semantics (system resources typically).

### Multiple users

Once allocated memory needs to be freed eventually, but the hard part is the timepoint such action shall eventually occur. In the simplest case, there is only one user of memory, its _owner_, who allocates it and deallocates it. In case of multiple users and shared resource, we can force the _last user_ of the memory to reclaim it. How to determine such last user? Most common way to do that is reference counting.

We are not going to allocate reference counter per piece of shared memory. We just embed the counter into the memory which sharing is _anticiapted_.

```c++
struct resource
{
    std::atomic<size_t> counter{0};
    // ... some data
};
```

Some other solution for single resource and multiple users include feeding each user with a copy of the resource and update the original resource once modified (an implementation of [RCU](https://en.wikipedia.org/wiki/Read-copy-update "Read-copy-update")).

### Non-contiguous resources


## Rich with types


#### About this document

June 19,29,30;August 10 2016 &mdash; Krzysztof Ostrowski
