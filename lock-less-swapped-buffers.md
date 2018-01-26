
# Lock less with swapped buffers

NOTE: Intention of this article is NOT to provide the reader with the yet another implementation of the producer-consumer queue, but to highlight the importance of understanding the cost of synchronisation and to demonstrate an example cost minimisation technique. Use [Boost.Lockfree](http://www.boost.org/doc/libs/1_66_0/doc/html/lockfree.html) or an already exisiting well-tested code if looking for an ready-to-use implementation of the producer-consumer's problem solution.

----

In the classic [producer-consumer problem](https://en.wikipedia.org/wiki/Producer%E2%80%93consumer_problem) two threads of execution (i.e. the producer and the consumer) modify concurrently a shared resource, in this case it is a buffer for the items produced. It is highly possible that the producer would push new items to the buffer during active remove operation started by the consumer. Such race condition leads to unpredictable state of the shared buffer resource.


We can get rid of the unpleasant race condition by applying synchronisation mechanisms (like `std::mutex`) to the shared resource. That is, both producer and consumer shall lock the resource before attempting to modify it. That works perfectly, but has at least one great disadvantage: it completely freezes the producer thread for the time of items' consumption. If the consumer processes items sequentially by applying a custom procedure of unknown/varying complexity, all the items producer generates in the meantime are lost irrevocably.

Proposed here solution keeps the shared resource locking, but unblocks the producer execution for the time of items' consumption. 

## Starting point

In the following code snippet, producer thread calls `push_back` in its context to append an `Item` to the fixed-size buffer, while consumer thread calls `consume` with custom action `F` that is applied to each item in the buffer. Buffer's contents are removed by the consumer once last item is processed by `F` action.

```c++
class Mailbox
{

 public:

  using Item   = int;  // for exposition only
  using Buffer = std::vector<Item>;  // or boost::circular_buffer

  static constexpr Buffer::size_type capacity = 10;  // arbitrary chosen


  Item& push_back(Item m)           // -- accessed by the producer thread
  {
    if (buffer.size() >= capacity)
    {
      throw std::overflow_error{"buffer full"};
    }

    {                                               //
      std::lock_guard<std::mutex> lock{modifying};  //
                                                    //
      buffer.push_back(std::move(m));               //
    }                                               //

    return buffer.back();
  }

  template<class F>
  //  requires Callable<F, Item&, void>
  void consume(F f)                 // -- accessed by the consumer thread
  {
    {
      std::lock_guard<std::mutex> lock{modifying};

      std::for_each(std::begin(buffer), std::end(buffer), std::move(f));  // unknown cost
//    ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
      buffer.clear();
    }
  }


  Mailbox() { buffer.reserve(capacity); }

 private:

  Buffer     buffer;     // shared resource
  std::mutex modifying;  // lock for the shared resource

};
```

The producer is not able to execute `push_back` until the consumer executing `consume` member function releases the `modifying` lock.

## An auxiliary buffer

By introducing an extra buffer for the consumer the time period in which shared buffer is locked can be minimised (cf. [double buffering](https://en.wikipedia.org/wiki/Multiple_buffering)).

```c++
class Mailbox
{
...
  template<class F>
  //  requires Callable<F, Item&, void>
  void consume(F f)                 // -- accessed by the consumer
  {
    {
      assert(auxiliary.empty());

      std::lock_guard<std::mutex> lock{modifying};

      using std::swap;

      swap(buffer, auxiliary);  // known cost
//    ^^^^^^^^^^^^^^^^^^^^^^^^
    }
    
    std::for_each(std::begin(auxiliary), std::end(auxiliary), std::move(f));

    auxiliary.clear();
  }


  Mailbox()
  {
    buffer.reserve(capacity);
    auxiliary.reserve(capacity);
  }

 private:

  Buffer     buffer;     // shared resource ("front buffer")
  Buffer     auxiliary;  // non-shared resource ("back buffer")
  std::mutex modifying;  // lock for the shared resource

};
```

Since `swap` on the buffers will lead to `move` (in fact a [reassignment of pointers](https://gcc.gnu.org/onlinedocs/gcc-7.2.0/libstdc++/api/a06912.html#a97d8ff35af22b6787d9aa7c60b2ba3ff)), the producer thread hangs on access to the shared resource for a relatively short period of time. That's significant gain in overall performance of the solution and its reliability.


#### About this document

January 20, 2018 &mdash; Krzysztof Ostrowski


