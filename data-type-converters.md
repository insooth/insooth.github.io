# Writing data type converters

Converter can be understood as a function that encapsulates given algorithm that transforms a type into an another type. Simplest converters in the C++ world of classes are modelled by _conversion_ constructors and operators. They work pretty well as long as the designer of a class knows all the types to which its abstraction can be converted. If it comes to integrate a third-party library that exposes a series of types _almost_ equal to the types used by us, and the source code of the third-party library _must not_ be modified, we end up producing tons of out-of-an-abstraction converters.

Aside from picking the right name for the conversion function (here many fail), through choosing the adequate signature for such function (multiple fails here too), the converter idea itself is definitely worth a closer look. 


Wrong | Better
---   | ---
`void convertAppleToOrange(const Apple&, Orange&)` | `Orange convert(const Apple&)`
`Orange* /* allocates memory */ convertAppleToNewOrange(const Apple&)` | `std::unique_ptr<Orange> convert(const Apple&)`
`B convert(A&)` | `B convert(const A&)` or (even better if in generic context) `B convert(A)`
`/* using Int = int; */` `EnumA convert(Int)` and `EnumB convert(Int)`  | `convert<EnumA>(Int)` and `convert<EnumB>(Int)` that are instances of `std::enable_if_t<any_v<T, EnumA, EnumB>, T> convert(Int)` where `template<class T, class... Ts> class any;`
repeated `for (const auto& item : items) others.push_back(convert(item))` | generalise over all the [`SequenceContainer`](http://en.cppreference.com/w/cpp/concept/SequenceContainer) models: `SequenceContainer convert(const SequenceContainer&) { /* map convert(...) over input */ }`

## How does it work

It is relatively easy to write converters between simple abstractions like enumerations and [fundamental types](http://en.cppreference.com/w/cpp/language/types). Since we are interested in conversions of _data values_, reference/pointer/function type and type qualifiers (like `const`) transformations are not discussed in this article. What's left in our scope are array types and classes. Having in mind that an array (or any other sequence of data, like `std::vector` or `std::map`) is just a way to organise values of some type, we can easily generalise the conversion to a mapping of a item-dedicated (fine-grained) `convert` function over the original sequence. Things are getting complicated for nested types and custom `class`es. Things are getting even more complicated for _sum_ types like `std::optional` or `std::variant` (just think about the data access).

Let's forget about the headache-provoking details for a while and approach the problem using top-down strategy. Question is: what does it mean _to convert_ a data type? We definitely need to inspect the data type structure, i.e. _unwrap_ its abstraction or just some layers of it. Conversion comprises (roughly) three steps:
* querying the original data type to select the interesting part,
* recursively traversing the selected part and applying the fine-grained conversions,
* assembling the converted bricks to generate the data of the desired type.

The middle part can be done in parallel, providing that there is no inter-data dependencies that prevent from that. If done in parallel, the last step can be understood as a synchronisation point for asynchronous conversion tasks.

To generalise even further, we may think of a conversion as a pair of actions, where first is a "getter" applied to the original data, and the second is the "setter" applied to the data returned by the "getter". The "setter" in the example changes the received data type. That's the idea of [Profunctor Optics](https://arxiv.org/abs/1703.10857) in fact &mdash; in our case &mdash; combined with a _state_ which is used to build up the result data.

#### About this document

May xx, 2018 &mdash; Krzysztof Ostrowski

[LICENSE](https://github.com/insooth/insooth.github.io/blob/master/LICENSE)


