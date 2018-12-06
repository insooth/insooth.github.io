
# Log less, log once

It is almost unbelievable how it is easy to commit a sin against simplicity. Consider a funcionality that converts some data type into an another one data type. All the information required to do a conversion is included in the original data type. How would you report errors if [`optional`-way were banned](https://github.com/insooth/insooth.github.io/blob/master/data-type-converters.md)?

Following snippet comes from the production code base. We have a converter member function `getContinuousRanges` that filters the received vector of tuples, and converts the selected data into custom `InternalRange` values. All the data types consumed and produced by the converter model C++ aggregates concept (which is an equivalent to C `struct`s) of relatively small size in bytes. 

```c++
class Conversions
{
 public:
    vector<InternalRange> getContinuousRanges(const vector<tuple<Start, End, Attributes>>& ranges) const;
 private:
    mutable Logger logger{GlobalLogger::get()};  // <-- spot this
};
```

Member function `Conversions::getContinuousRanges` does the actual conversion, and reports the errors encountered during that process via the logger functionality.

```c++
vector<InternalRange>
Conversions::getContinuousRanges(const vector<tuple<Start, End, Attributes>>& ranges) const
{
// ...
    logger.error << "Invalid start";
// ...
    logger.error << "Invalid start or end";
// ...
    logger.error << "Invalid number of attributes ";
// ...

    return converted;
}
```

Even if all seems to be fine with the presented code snippet, it shall not pass the code review. Following issues can be identified immediately:
* unnecessary shared mutable state (`Logger`),
* no feedback information to the caller if the conversion fails,
* conversion functionality is not testable unless log messages are captured and parsed,
* cannot use (conceptually) _pure_ converter without an object of type `Conversions`,
* converter naming is misleading (we "filter and convert" rather than "get").

## The fix

Following code snippet fixes the issues introduced with the original design. New design brings significant benefits:
* converters are stateless (and with easy-to-estimate performance),
* errors are explicitly reported to the caller,
* straightforward testability,
* conceptually pure function is implemented as a pure one,
* set of converters is easily extensible even from a different header file (`namespace` models an "open class"),
* naming matches the behaviour.

```c++
namespace converters
{

[[nodiscard]] pair<vector<InternalRange>, ConverterErrors>
toContinuousRanges(const vector<tuple<Start, End, Attributes>>& ranges)
{
    ConverterErrors errors;  // models value semantics

// ...
    errors |= Error::INVALID_START;
// ...
    errors |= Error::INVALID_START_OR_END;
// ...
    errors |= Error::INVALID_ATTRIBUTE_COUNT;
// ...

    return make_pair(std::move(converted), errors);
}

}
```

## Required boilerplate

Everything comes with a cost. No exception here.

Having a definition of the possible `Error` values, that are in fact bits set in the `ConverterErrors` bitset:

```c++
enum class Error : std::uint8_t  // up to eight different types of errors
{
    INVALID_START
  , INVALID_START_OR_END
  , INVALID_ATTRIBUTE_COUNT
};
```

We choose a type of the `ConverterErrors` to be a `std::bitset` of number of bits equal to the number of available distinct error values:

```c++
using ConverterErrors =
    std::bitset<
        std::numeric_limits<
            std::underlying_type_t<Error>
          >::digits
      >;
```

We need to tell the `ConverterErrors` how to combine with `Error` values by means of `|`:

```c++
ConverterErrors& operator|= (ConverterErrors& errors, Error which)  // throws out_of_range
{
   static_assert(std::is_unsigned_v<std::underlying_type_t<Error>>, "No negative bit indices");

   return errors.set(static_cast<std::underlying_type_t<Error>>(which));
}
```

## Caller side

Caller calls the conversion function, checks whether _none_ of errors was reported, and it either does its happy-path job, or reports the errors to the logger, then handles the erroneous case. Error logging is done once, and whether it is done actually or not it up to the caller. A converter does a conversion, solely, dot.

```c++
if (auto [d, e] = converters::toContinuousRanges(ranges); e.none())   // success -- "zero" cost
{
    // do your job with "d"
}
else  // presumably huge I/O cost due to logging (think about distributed loggers)
{
    logger.error << "Conversion failed with: " << converters::to_string(e);

    // handle error
}
```

The stringification of `ConverterErrors` is implemented fairly easy. (Note the indentation that helps the reader.):

```c++
std::string to_string(Error which)
{
    switch (which)
    {
    case Error::INVALID_START:           return "INVALID_START";           break;
    case Error::INVALID_START_OR_END:    return "INVALID_START_OR_END";    break;
    case Error::INVALID_ATTRIBUTE_COUNT: return "INVALID_ATTRIBUTE_COUNT"; break;
    default:                             return "???";                     break;
    }

    throw std::out_of_range{"Impossible path taken"};  // refactoring failed!
}

std::string to_string(ConverterErrors errors)  // throws bad_alloc, out_of_range, length_error, 
{
    std::string stringified;

    stringified.reserve(255);  // some estimated

    for (std::size_t index = 0; index < errors.size(); ++index)  // no iterator interface in bitset
    {
        if (errors.test(index))
        {
            stringified +=   to_string(static_cast<Error>(index))
                           + "(" + std::to_string(index) + ") ";
        }
    }

    return stringified ;
}
```

## Live code


Code available on [Coliru](http://coliru.stacked-crooked.com/a/eb91f695146c2fcb).


#### About this document

December 5, 2018 &mdash; Krzysztof Ostrowski

[LICENSE](https://github.com/insooth/insooth.github.io/blob/master/LICENSE)
