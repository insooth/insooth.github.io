
# The Blessed Split

Well design system is built up from multiple layers, where each layer may be complex piece of software. Data is fed into such a system and it flows from bottom layers up to the upper ones being transformed at each step. This is how telecommunication stacks work, and it usually involves state machines behind the scenes. If `fₙ` represents action performed by `n`th layer and `x` is the initial input data, then having layers from `0` (bottom) to `n` (top), layered system can be optimistically simplified to `fₙ(f₁(f₀ x)...)` which is just a function composition of layers' actions with input data. That's quite simple, but may be too simple in some cases.

Fortunately, this text is not going to be about monads, in particular `Maybe` or `Either`, and nor about system design patterns. We will look into a single layer of the system to find out how to make it _safe_ without sacrificing its performance.

## Let's look under the mask

Single system layer typically contains state machine that once fed with data generates next step and executes specific actions. From the layer's user perspective, it is a black box with the exposed interface. Black box can be realised as code compiled into a library, a set of C++ headers, another system over the network, etc. Interface is usually a set of functions that, if well designed, carries additional information about itself like purity, thread-safety, computational complexity, arguments and result values' domains, exception safety guarantees and more.

Average user utilises provided interface with goodwill, i.e. user expects that possible misuse will be handled gracefully or even understood and corrected internally. What is meant by user as a goodwill, not necessarily is the same for the interface author. There are several ways to protect against interface misuse, one of them is _type-rich programming_. Having strictly defined types that represent arguments and result values' domains misuse happens less often. As an example:

```c++
int make_resource(char*);
```

against types encoding values' domains:

```c++
using resource_id = uint8_t;
using resource_name = string_view;  // we don't want to copy input data

optional<resource_id> make_resource(resource_name);
```

That is common approach in functional languages like Haskell, where you enter type signature in [Hoogle](https://www.haskell.org/hoogle/) to find already implemented functionality. Strictly _statically_ typed design is easier to reason about and maintain. Happily, that applies to C++ too -- read more from [Stroustrup, Sutter et al.](https://github.com/isocpp/CppCoreGuidelines/blob/master/CppCoreGuidelines.md#in0-dont-panic)

Domains represented by fine-grained types may lead to interface functions that have well described arguments values' domain (or simply _domain_) and result values domain (_co-domain_). That helps a lot. For instance `make_resource`'s _co-domain_ is quite well specified, it is either valid `resource_id` or _nothing_. At the other extreme, its arguments' domain is a sequence of `char`s. Such a sequence can contain almost everything, including out-of-bounds values (in our case: non-printable), disallowed combinations or even an empty sequence. At least one is guaranteed: we never pass the bounds of the pointed sequence using [string_view](http://en.cppreference.com/w/cpp/experimental/basic_string_view). The rest is up to us, we cope with it internally.

## Handle misuses

It is not easy to design a single function that works _for all_ the values from the domain, i.e. making it a total function. Things are getting troublesome if function has side effects, or even more literally: works differently depending on the global context that is not passed to it through arguments, and is not expressed in its signature.

It is nothing wrong if we treat all the input as _tainted_ and potentially wrong. In most of the cases, it saves us tears during debugging and can protect us from hard to detect bugs, or even [heisenbugs](https://en.wikipedia.org/wiki/Heisenbug). All the tainted input must be validated, rejected if invalid or corrected and accepted if possible. We can benefit from GLS's [not_null](https://github.com/Microsoft/GSL/blob/master/include/gsl.h#L77) to deal with raw pointers, favour views over raw `T*`, or even provide custom abstraction that does some preliminary validation.

Let's get back to the `make_resource` and pose some questions regarding its input arguments:

* What if resource name is empty?
* What if resource name is not a C-string?
* What is maximum length of the resource name?
* What is allowed character set for resource name?

Some general questions include calling conditions:

* When a call to this function makes sense?
* What are the side effects introduced by this function?

These and similar questions are required to set up function [_contract_](https://en.wikipedia.org/wiki/Design_by_contract). Future incarnations of C++ language [may include](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4415.pdf) formal way of contract definition. Let's realise the bullets written above in the code:

```c++
optional<resource_id> make_resource(resource_name s)
{
  using T = resource_traits;

  // checks system state; log error and `return {};` if not in wrong state

  if (true == s.empty())
  { /* log error and `return {};` */ }

  if ('\0' != s.back())
  { /* log error and `return {};` */ }

  if (s.size() > T::max_length)
  { /* log error and `return {};` */ }

  if (string_view::npos != s.find_first_not_of(T::allowed_chars()))
  { /* log error and `return {};` */ }

  // do the stuff we were requested to do
  // do the real work here

  // check the side effects; log errors and return if needed

  return (true == success) ? {id} : {};
}
```

Validation and following error handling with (possible) input data correction take most of the function's body space. What we can do about that?

## Split it out

The idea of division of frontend and backend is quite old. Let's approach that idea. Frontend is supposed to be user-friendly interface, so that it helps the user in cases of misuse. Backend is more restrictive and immidiately stigmates user in case of misuse. We can apply that _blessed split_ to our interface:

```c++

namespace backend
{

optional<resource_id> make_resource(resource_name s)
{
  // assert on system state
  assert(false == s.empty());
  assert('\0' == s.back());
  assert(s.size() <= resource_traits::max_length);
  assert(string_view::npos == s.find_first_not_of(resource_traits::allowed_chars()));
  
  // do the real work here

  // check the side effects with assert
}

}

optional<resource_id> make_resource(resource_name s)
{
  // -- see implementation in previous the listing

  // do the stuff we were requested to do
  const auto result = backend::make_resource(s);

  // check the side effects; log errors and return if needed

  return result;
}
```

We have split the original implementation into frontend part, that does all the required input validation, serves the user with the log messages, assures pre/post conditions in user-friendly way; and backend that requires correct input data and system state.

Backend just fails fast in case of an erroneous input or system state, and if it happens, frontend has failed first. Having this, backend can operate at its maximum performance and is _unsafe_ if [`NDEBUG`](http://en.cppreference.com/w/cpp/error/assert) is set.

There is no requirement of one-to-one relation between frontend interface functions and backend functions, i.e. single interface function can compose and bind results of multiple backend functions. This leads to easily reconfigurable frontends, and in turn, flexible interfaces.

Type-rich designs that utilise blessed split tend to be easier to use and maintain. Quality is enforced by use (or even _overuse_) of types during compilation time, and with contracts during runtime. Testing of frontend is straight-forward, backend requires a little bit different techniques. Due to fact that it expects valid state and input, which implies that execution will not move forward otherwise, we can verify the underlying algorithm only. Configurable assertions from [BDE](https://github.com/bloomberg/bde/blob/master/groups/bsl/bsls/bsls_assert.h) may help in testing the assertions itself.

#### About this document

May 6, 2016 -- Krzysztof Ostrowski
