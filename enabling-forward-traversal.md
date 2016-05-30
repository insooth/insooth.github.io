
# Enabling forward traversal

Let's consider following example. We have legacy interface that constructs object of type `T` using information stored in object of type `U`:

```c++
bool build_from(T& into, const U& from);
```

Such a `build_from` can be deserialiser function, e.g. from JSON to C++ type, for example:

```c++
result_type deserialise(T& into, const json_node_type& from);
```

Ideally, we should return `into` rather than taking it as an input argument, i.e. return `optional<T>` or type that carries information about the errors too, like C++ counterpart of [`Either`](https://hackage.haskell.org/package/base-4.9.0.0/docs/Data-Either.html "Data.Either"). Unfortunately, we have to deal with legacy interafce, so that _existing contracts cannot be broken_, and our patch sets shall favour composition and extension over the modification.

With `deserialise` we know how to convert internal JSON representation into object of type `T`. That works fine for non-structured types. JSON offers associative containers (dictionaries, hashes, _structures_ as defined in [YAML](http://www.yaml.org/spec/1.2/spec.html#id2760395 "Structures")) and collections. Dictionaries map easily to product types, `struct` in C++, and for those we typically need custom deserialisation procedures. Collections may be seen as abstractions over structures, or &ndash; more functionally &ndash; as sequenced values of type `T` for collection of type `[T]`. Since we know how to deserialise objects of type `T`, we can deserialise `[T]` by simply mapping `deserialise` over a sequence of `T` objects, that is doing forward traversal. Unfortunately we have just one JSON node `from`, not a sequence of nodes. We need to enable forward traversal for the type `json_node_type` with minimal number of changes.

## Explicit type

We know that `std::vector<T>` models [`ForwardIterator`](http://en.cppreference.com/w/cpp/concept/ForwardIterator "C++ concepts: ForwardIterator") concept, so it fits well. We apply our knowledge to get:

```c++
result_type deserialise(T& into, const std::vector<json_node_type>& from);  // T -> [U] -> E
```

We have changed the type of the second argument and pass the `[]` "box" explicitly. Major change is in expanded the `deserialise` domain: passed `from` can now contain *no* objects of type `json_node_type` at all! We need to check for that in the body of function, that is:

```c++
result_type deserialise(T& into, const std::vector<json_node_type>& from)
{
    if (false == from.empty())
    {
        /* do something with from.front() */
        return result_type::success;
    }

    return result_type::no_data;
}
```

We have introduced non-explicit requirement that must be satisified by user, that's definitely huge drawback of the above approach.


## Extend, do not change

We definitely want to keep the old signature that gives us non-empty guarantee (we receive reference to the object), that is:

```c++
result_type deserialise(T& into, const json_node_type& from);
```

How to achieve that?

### What is a list?

Functional programming languages usually define abstract data structures recursively. For instance, list `[T]` is a value of type `T` prepended (_consed_) to a list of `[T]` which can be an empty list, binary tree is an empty tree, or a node and two trees (left and right). The length of the list is defined recursively too &mdash; it is 0 for empty list, and 1 + length of the list without first element (thus 1 there).

### Extending

Let's creatively apply our knowledge to `json_node_type` and embed _tail_ into `json_node_type`:

```c++
class json_node_type
{
 public: /* ctors, etc.*/
 
 size_type size_type size() const noexcept
 {
     return 1 + m_tail.size();
 }
 
 private:
    std::string data;
    std::vector<node> tail;
};
```

It is guaranteed that `deserialise` receives _at least_ one node in its second argument. Type of the argument does not change. That's exactly what we wanted to have. The last thing to do is making possible to iterate over such `json_node_type` object. To enable forward traversal we will use Boost.Iterator [iterator_facade](http://www.boost.org/doc/libs/1_61_0/libs/iterator/doc/iterator_facade.html#iterator-facade-requirements "iterator_facade Requirements") to generate [required code](http://www.boost.org/doc/libs/1_61_0/libs/iterator/doc/iterator_facade.html#tutorial-example "Iterator Facade Tutorial Example") for us. We design an iterator that is never singular &ndash; it is always bound to a collection and carries information about that collection.

Type `json_node_type` with enabled forward traversal looks as follows (see on [Coliru](http://coliru.stacked-crooked.com/a/ac7d3030256ffb0c "Coliru")):

```c++
class json_node_type
{
    template<class T>
    class iter
      : public boost::iterator_facade<iter<T>, T, boost::forward_traversal_tag>
    {
        template<class> friend class iter;  // required by equal, cctor

        struct enabler {};
        using size_type = typename T::size_type;

     public:
        explicit iter(T& _n, size_type _i = 0)  // never singular
          : n{_n}, i{_i}
        {}
        
        template<class U>
        iter(const iter<U>& other
          , std::enable_if_t<
              std::is_convertible<U, T>::value
            , enabler
            > = enabler{}
          )
          : n{other.n}, i{other.i}
        {}

     private:
        friend class boost::iterator_core_access;

        template<class U>
        bool equal(const iter<U>& other) const
        {
            return (this->i == other.i) && ( &(this->n) == &(other.n) );
        }
        
        void increment()
        {
            assert(i < n.size());

            ++i;
        }
        
        T& dereference() const
        {
            assert(i < n.size());
            
            return (i == 0) ? n : n.tail[i - 1];
        }
        
        T& n;
        size_type i;
    };

 public: /* ctors, etc. */

    using tail_type = std::vector<json_node_type>;
    using size_type = tail_type::size_type;
    using iterator = iter<json_node_type>;
    using const_iterator = iter<const json_node_type>;

    iterator begin() noexcept { return iterator{*this, 0}; }
    iterator end() noexcept { return iterator{*this, size()}; }
    
    const_iterator cbegin() const noexcept { return const_iterator{*this, 0}; }
    const_iterator cend() const noexcept { return const_iterator{*this, size()}; }

  private: /* data, etc. */
 
    tail_type tail;
};
```

We have enabled forward traversal and kept the `json_node_type` type name unchanged. We guarantee that at least one `json_node_type` exists, that is the one is passed as second argument into `deserialise` &mdash; there is no need to check against empty `vector` of nodes as in the first solution. Thus, we eliminated branching at the cost of storing [three pointers](https://gcc.gnu.org/onlinedocs/libstdc++/latest-doxygen/a01523_source.html#l00082 "GCC std::vector implementation") per `json_node_type`.

Presented design is ready for extensions &ndash; we can form multi-level collections like list of lists, meet requirements of [RadomAccessIterator](http://en.cppreference.com/w/cpp/concept/RandomAccessIterator "C++ concepts: RandomAccessIterator"), etc. easily.


#### About this document

May 30, 2016 &mdash; Krzysztof Ostrowski

