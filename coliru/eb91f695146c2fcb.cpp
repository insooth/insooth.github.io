
// see LICENSE on insooth.github.io

#include <iostream>  // cout

#include <cstddef>  // size_t, uint8_t
#include <bitset>  // bitset
#include <limits>  // numeric_limits
#include <stdexcept>  // out_of_range
#include <string> // to_string, string
#include <tuple> // tuple, make_tuple
#include <type_traits>  // underlying_type, is_unsigned
#include <utility>  // pair, make_pair, move
#include <vector>  // vector


// for exposition only
struct Logger
{
    decltype(std::cout)& error = std::cout;
};

using Start = int;
using End = int;
using Attributes = int;
using InternalRange = int;





namespace converters
{
    
enum class Error : std::uint8_t // up to eight different types of errors
{
    INVALID_START
  , INVALID_START_OR_END
  , INVALID_ATTRIBUTE_COUNT
};

std::string to_string(Error which)  // NOTE the result type
{
    switch (which)
    {
    case Error::INVALID_START:           return "INVALID_START";           break;
    case Error::INVALID_START_OR_END:    return "INVALID_START_OR_END";    break;
    case Error::INVALID_ATTRIBUTE_COUNT: return "INVALID_ATTRIBUTE_COUNT"; break;
    default:                             return "???";                     break;
    }

    throw std::out_of_range{"Impossible path taken"};  // refactoring failed!
}


// ------------------------------------------------------------------------


using ConverterErrors =
    std::bitset<
        std::numeric_limits<
            std::underlying_type_t<Error>
          >::digits
      >;

ConverterErrors& operator|= (ConverterErrors& errors, Error which)  // throws out_of_range
{
   static_assert(std::is_unsigned_v<std::underlying_type_t<Error>>, "No negative bit indices");

   return errors.set(static_cast<std::underlying_type_t<Error>>(which));
}

std::string to_string(ConverterErrors errors)  // throws bad_alloc, out_of_range, length_error, 
{
    std::string stringified;

    stringified.reserve(255);  // some estimated

    for (std::size_t index = 0; index < errors.size(); ++index)  // no iterator interface in bitset
    {
        if (errors.test(index))
        {
            stringified += to_string(static_cast<Error>(index))
                           + "(" + std::to_string(index) + ") ";
        }
    }

    return stringified ;
}


// ------------------------------------------------------------------------


[[nodiscard]] std::pair<std::vector<::InternalRange>, ConverterErrors>
toContinuousRanges(const std::vector<std::tuple<::Start, ::End, ::Attributes>>& ranges)
{
    ConverterErrors errors;  // models value semantics

    std::vector<InternalRange> converted;  // for exposition only

// ...
    errors |= Error::INVALID_START;
// ...
    errors |= Error::INVALID_START_OR_END;
// ...
    errors |= Error::INVALID_ATTRIBUTE_COUNT;
// ...

    return std::make_pair(std::move(converted), errors);
}

}

int main()
{
    Logger logger;
    
    std::vector ranges = {std::make_tuple(Start{1}, End{2}, Attributes{3})};
    
    auto rangeAndErrors = converters::toContinuousRanges(ranges);

    if (rangeAndErrors.second.none())  // success -- "zero" cost
    {
        // do your job with "d"
    }
    else  // presumably huge I/O cost due to logging (think about distributed loggers)
    {
        logger.error << "Conversion failed with: "
                     << converters::to_string(rangeAndErrors.second);
    
        // handle error
    }
    

    // C++17:
    if (auto [d, e] = converters::toContinuousRanges(ranges); e.none())
    {
        // do your job
    }
    else
    {
        logger.error << "\nConversion failed with: " << converters::to_string(e);
    
        // handle error
    }
    
    return 0;
}

