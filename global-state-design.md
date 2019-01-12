
# Design with a global state

State is inevitable. Essential in practice, a side-effect, is a change applied to a shared _global_ state. Ruminations about side-effect-free programming boil down to an isolation of a state, and its careful management by means of dedicated abstractions for [modelling computations](https://en.wikipedia.org/wiki/Monad_(functional_programming)#See_also). Popular stateless designs require passing a state in subject along with an action that is supposed to affect it. That brings us a design with an explicitly exposed the previously hidden global state. With the state up front, we make it easily trackable and visible in the data-flow diagrams. No more bad surprises after refactoring, at least in theory.

Recently, I came across C++ code that seemed to be a well-designed piece of software, but in fact it was a mess. There was a `class` with some data and several member functions that were transforming another data. All was fine at first glance, but there was a hidden shared global state too. Consider the following C code where every function depends on and modifies the contents of `status` variable:

```c
static struct Status status;

struct Data* receive(const struct Source*);
int alter(struct Data*);
int transmit(struct Sink*, struct Data\*);
```

The dependency between functions and `status` are invisible to the reader. Rough translation to C++ might look like:


```c++
class Brick
{

 public:

  Data receive(const Source&)
  bool alter(Data&&);
  bool transmit(Sink&, Data&&);

 private:

  Status status;
};
```

A hidden global state is still a global state, even it is encapsulated. Bad practice remains Curcial dependencies are invisible to the reader. Member functions' names and signatures lie.

Possible solution include simply mentioning the dependencies in the member function signatures:

```c++
class Brick;

class Status
{
  friend class Brick;
// ...
};

class Brick
{

 public:

  std::pair<Data, Status> receive(const Source&);
  std::pair<Data, Status> alter(Data&&, Status&&);
  Status transmit(Sink&, Data&&, Status&&);

};
```

And that's it. State is managed explicitly. `Brick` becomes stateless.


#### About this document

January 12, 2019 &mdash; Krzysztof Ostrowski

[LICENSE](https://github.com/insooth/insooth.github.io/blob/master/LICENSE)

