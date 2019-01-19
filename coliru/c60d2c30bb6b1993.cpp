
// see LICENSE on insooth.github.io

#include <iostream>
#include <ostream>  // ostream

#include <chrono>  // milliseconds, hours
#include <iomanip>  // boolalpha
#include <type_traits>  // common_type
#include <variant>  // bad_variant_access
#include <vector>
#include <new>  // bad_alloc

using T = std::vector<std::chrono::milliseconds>;  // for exposition only
using U = std::vector<std::chrono::hours>;  // for exposition only

std::ostream& operator<< (std::ostream& o, std::chrono::milliseconds m)
{
    return o << m.count() << "ms";
}

std::ostream& operator<< (std::ostream& o, std::chrono::hours m)
{
    return o << m.count() << "h";
}





template<class T>
using identity_type = std::common_type<T>;


int main()
{

    auto foo = [](std::variant<T, U>& c, auto v)
    {
        try
        {
            std::get< T >(c).push_back(v);
            //        ^~~ fixed here, but may be U depending on v
        }
        catch (std::bad_variant_access&) { /* ... */ }
        catch (std::bad_alloc&)          { /* ... */ }
        
        return c;
    };
    
    auto bar = [](std::variant<T, U>& c, auto v, auto w)
    {
        using W = typename decltype(w)::type;  // extracts type 
        
        try
        {
            std::get<W>(c).push_back(v);
        }
        catch (std::bad_variant_access&) { /* ... */ }
        catch (std::bad_alloc&)          { /* ... */ }
        
        return c;
    };
    
    using namespace std::literals::chrono_literals;
    
    auto c1 = std::variant<T, U>{T{}};
    std::cout << std::get<T>(foo(c1, T::value_type{1})).front() << '\n';
    
    auto c2 = std::variant<T, U>{U{}};
//    std::cout << std::get<U>(foo(c2, U::value_type{2})).front() << '\n';

    auto c3 = std::variant<T, U>{T{}};
    std::cout << std::get<T>(bar(c3, 3ms,  identity_type<T>{} )).front() << '\n';
    
    auto c4 = std::variant<T, U>{U{}};
    std::cout << std::get<U>(bar(c4, 4h,  identity_type<U>{} )).front() << '\n';
    
    auto c5 = std::variant<T, U>{U{}};
    auto w = identity_type<U>{};
    std::cout << ' ' << sizeof(w) << std::boolalpha << ' ' << std::is_empty_v<decltype(w)> << '\n';
//    std::cout << std::get<U>(bar(c5, 4s,  w )).front() << '\n';
    std::cout << std::get<U>(bar(c5, 4h,  w )).front() << '\n';

    return 0;
}


