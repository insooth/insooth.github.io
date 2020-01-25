
// see LICENSE on insooth.github.io

#include <iostream>

#include <cstring>  // strlen
#include <array>
#include <bitset>
#include <iomanip>  // boolalpha
#include <utility>  // pair, forward


template<class T, class Tag = void, class Logger = void>
struct run_log;


// --- interface

/** @{ Tagged. */
template<class Tag, class T>
constexpr decltype(auto) make_log(T&& arg)
{
    return run_log<T, Tag, void>
                {}
                (std::forward<T>(arg));
}

template<class Tag, class T, class Logger>
constexpr decltype(auto) make_log(T&& arg, Logger&& logger)
{
    return run_log<T, Tag, Logger>
                {std::forward<Logger>(logger)}
                (std::forward<T>(arg));
}
/** @} */

/** @{ Optional tagged. */
template<class T, class Tag = void>
constexpr decltype(auto) make_log(T&& arg)
{
    return run_log<T, Tag, void>
                {}
                (std::forward<T>(arg));
}

template<class T, class Logger, class Tag = void>
constexpr decltype(auto) make_log(T&& arg, Logger&& logger)
{
    return run_log<T, Tag, Logger>
                {std::forward<Logger>(logger)}
                (std::forward<T>(arg));
}
/** @} */

// --- customisation points

// default logger, untagged
template<class Tag>
struct run_log<std::pair<std::bitset<8>, bool>, Tag, void>
{
    decltype(auto) operator()(const std::pair<std::bitset<8>, bool>& arg) const
    {
        return run_log<
                    std::pair<std::bitset<8>, bool>
                  , Tag
                  , decltype(std::cout)
                  >
                  {std::cout}
                  (arg);
    }
};

// custom logger, untagged
template<class Tag, class Logger>
struct run_log<std::pair<std::bitset<8>, bool>, Tag, Logger>
{
    run_log(Logger& l) : logger{l} {}
    
    decltype(auto) operator()(const std::pair<std::bitset<8>, bool>& arg) const
    {
        std::array<char, 8 + 7 + 11> str = { R"("errors":[])" };

        const std::size_t total = arg.first.size();
        const std::size_t max   = str.size();

        std::size_t j = std::strlen(str.data()) - 1;  // -1: for \0

        for (std::size_t i = 0; (i < total) && (j < max); ++i)
        {
            if (arg.first.test(i))
            {
                str[j++] = i + '0';
                str[j++] = ',';
            }
        }

        str[--j] = ']';
        str[++j] = '\0';

        logger << str.data();

        return arg.second;
    }

    Logger& logger;  // short-lived, not expected to be copied/reassigned
};

// default logger, tagged
template<>
struct run_log<std::pair<std::bitset<8>, bool>, std::true_type, void>
{
    decltype(auto) operator()(const std::pair<std::bitset<8>, bool>& arg) const
    {
        std::cout << "run_log with tag\n";
        
        return run_log<
                    std::pair<std::bitset<8>, bool>
                  , std::true_type
                  , decltype(std::cout)
                  >
                  {std::cout}
                  (arg);
    }
};

std::pair<std::bitset<8>, bool> foo() { return std::make_pair(0b110, true); }

int main()
{
    bool r1 = make_log(foo());
    std::cout << '\n' << std::boolalpha << r1 << '\n';

    bool r2 = make_log<std::true_type>(foo());
    std::cout << '\n' << std::boolalpha << r2 << '\n';

    bool r3 = make_log(foo(), std::cout);
    std::cout << '\n' << std::boolalpha << r3 << '\n';
    
    return 0;
}

/*
g++ -std=c++2a -O2 -Wall -pedantic -pthread main.cpp && ./a.out
"errors":[1,2]
true
run_log with tag
"errors":[1,2]
true
"errors":[1,2]
true
*/
