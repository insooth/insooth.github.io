
# Handling asynchronous inputs, in a bizarre way

I was triggered to write this article by a software design I've discovered during a read of quite a large undocumented code base of the system-wide module. Generally speaking, the module was driven by four asynchronous inputs, and had a single asynchronous output. The mechanics to handle the module inputs introduced by the author were inconsistent. Even if at first glance all those four approaches seemed equivalent, they were different in details that, after several days of analysis, turned out to be fundamental. To put aside the existence of four different designs to handle the module's input, the overall software architecture was convoluted enough to signal code smells.

# Status quo

The common part of the introduced designs included either a member or a base class of the `Module` that, during module's `init` phase, registered an action that was executed _asynchronously_ in a _separate_ thread of execution upon new message arrival. The action itself was just a call forward to the `Module` member function. The essence of that bizarre design (let's call it _Module-Adapter-Observer_) is presented below.

```c++
struct Message { /* ... */ };

struct IObserver
{
    virtual void onMsg(Message) const;
};

struct Adapter
{
    bool init(const IObserver& o) const
    {
        return Input::handle([o](Message m) { o.onMsg(std::move(m)); });
//             ^~~ register a handler that is executed upon Message arrival
    }
};

class Module : public IObserver
{

 public:

    bool init() { return adapter.init(*this); }

    // exposed for testability: one can inject prepared messages
    void newMsg(Message m) { /* actual work */ }

 private:

    void onMsg(Message m) const override { newMsg(std::move(m)); }

    std::unique_ptr<Adapter> adapter = std::make_unique<Adapter>();
};


// Module m; assert(m.init());
```

The _Module-Adapter-Observer_ approach has couple of significant flaws, for instance:

(1) Functionality defined outside of `Module`'s thread of execution may access dangling reference to `Module` object through `IObserver::onMsg`. That will result in a crash while accessing `Module::onMsg` originated in an unrelated thread. Pass-by-reference does not help here at all, because lifetimes of `Input` and `Module` are not correlated enough.

(2) Unclear information flow that is extremely hard to follow and compose with. In fact, a circular dependency on input port was introduced incidentally. If it were a white-boarded design (e.g. very useful [complete composite structure diagram](https://www.sparxsystems.com/resources/tutorials/uml2/composite-diagram.html)) up front it wouldn't be a case, consider:

```


                                            m :Message
                                            .-------.
                                            |       | 
                                            |       v
+-------------+                        +----|---[newMsg]-------+
|             |       m :Message       |    |                  |
|   Input   [sensor]-------------->[onMsg]--'                  |
|             |                        |         Module        |
+-------------+                        |                       |
                                       |                       |
                                       +-----------------------+
```

The re-route of the `m` is forced by `Module::newMsg` due to testability reasons, I suppose. However, from the logical point of view, `Module` consumes `m` twice.



## Challenge the status quo

From the functional point of view, we can insert a broker between `Input` and `Module` components that will solve lifetimes' issues. Imagine an additional component `Broker` that is composed of a message queue, where `Input` just writes to it, and `Modules` just reads from it. That's a single producer, single consumer design ([example solution](https://github.com/insooth/insooth.github.io/blob/master/lock-less-swapped-buffers.md)).

## Middleman

`Broker` introduces a shared resource, not necessarily synchronised if the _read_ and _write_ threads are separated by design well enough. If the write thread pushes new messages to a circular buffer of a fixed size (i.e. we can refer to each element in the buffer by its index), effectively modifying it without a lock; and the read thread inspects that buffer, either as a reaction to an event or periodically, in order to make a copy of new messages to its thread of execution; then we can bring an additional information in a form of a (conceptual) sequence of indices into `Broker`. Only the read thread modifies the sequence of indices, while the write thread uses the information stored there to reuse elements in the circular buffer. Reuse shall happen for all the pointed indices (all or nothing strategy). Completion is indicated by setting an atomic flag. That flag is a lock-free synchronisation point between read and write threads.

`Broker` may come with unacceptable message processing latencies. Horizontal scaling may help here, i.e. assignment of dedicated circular buffers per group of messages, and aligning the priorities for processing them with in the read thread.

## Simplyfing

Heavily-OOP solution written in C++ typically does not help the code readers. Introduction of base classes, interfaces and concepts (soon) must be preceded by proper analysis, it shall not be an ad-hoc arbitrary decision. It is not unusual that the reader would like to work with a code that follows intuition, and common sense that puts every thing in its right place.

Let's try to model the general (for our four inputs) asynchronous input handler. To follow TDD style, we will make `Module` testable first by providing `newMsg` interface.

```
// STL misses curring of metafunctions
template<class T>
struct is_t
{
    template<class U>
    using apply = std::is_same<T, U>;
};


template<class T, class M>
concept Input
  = requires(T t, M m)
  {
    { t.inject(m) } -> bool
  };



struct Odometry { /* ... */ };
struct Camera   { /* ... */ };
struct GNSS     { /* ... */ };
struct Lidar    { /* ... */ };


struct Module
{
    using inputs_type = std::tuple<Odometry, Camera, GNSS, Lidar>;

    //! Injects message M to reception buffer of input T.
    template<class T, class M>
      requires Input<T, M>
    constexpr auto newMsg(M msg) const
    {
       static_assert(find_in_if_t<inputs_type, is_t<T>::template apply>::value
                   , "Input not supported");
                   
       return std::get<T>(inputs).inject(std::move(msg));
    }

    
    inputs_type inputs;
};

// Module m; m.newMsg<
```



#### About this document

November 10, 2019 &mdash; Krzysztof Ostrowski

[LICENSE](https://github.com/insooth/insooth.github.io/blob/master/LICENSE)
