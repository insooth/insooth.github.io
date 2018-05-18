# Writing data type converters

Converter can be understood as a function that encapsulates given algorithm that transforms a type into an another type. Simplest converters in the C++ world of classes are modelled by _conversion_ constructors and operators. They work pretty well as long as the designer of a class knows all the types to which its abstraction can be converted. If it comes to integrate a third-party library that exposes a series of types _almost_ equal to the types used by us, and the source code of the third-party library _must not_ be modified, we end up producing tons of out-of-an-abstraction converters.

Aside from picking the right name for the conversion function (here many fail), through choosing the adequate signature for such function (multiple fails here too), the converter idea itself is definitely worth a closer look. 


Wrong | Better
---   | ---
`void convertAppleToOrange(const Apple&, Orange&)` | `Orange convert(const Apple&)`
`Orange* /* allocates memory */ convertAppleToNewOrange(const Apple&)` | `std::unique_ptr<Orange> convert(const Apple&)`
`/* using Int = int; */` `EnumA convert(Int)` and `EnumB convert(Int)`  | `convert<EnumA>(Int)` and `convert<EnumB>(Int)` that are instances of `std::enable_if_t<any_v<T, EnumA, EnumB>, T> convert(Int)` where `template<class T, class... Ts> class any;`
`B convert(A&)` | `B convert(const A&)` or (even better if in generic context) `B convert(A)`
repeated `for (const auto& item : items) others.push_back(convert(item))` | generalise over all [`SequenceContainer`](http://en.cppreference.com/w/cpp/concept/SequenceContainer)s: `SequenceContainer convert(const SequenceContainer&) { /* map convert(...) over input */ }`
## How does it work

