
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








#### About this document

November 10, 2019 &mdash; Krzysztof Ostrowski

[LICENSE](https://github.com/insooth/insooth.github.io/blob/master/LICENSE)
