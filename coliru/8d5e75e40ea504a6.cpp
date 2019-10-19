
// see LICENSE on insooth.github.io

#include <cstddef>  // size_t
#include <tuple>  // tuple, tuple_element, tuple_size
#include <type_traits>  // disjunction
#include <utility>  // {make_,}index_sequence


#include <iostream>



template<class Tuple, template<class> class Predicate>
class find_in_if
{
    template<
        class T  // actual type
      , template<class> class P  // predicate
      , std::size_t I  // index within a tuple
      >
    struct box
    {
        using                        type  = T;
        static constexpr std::size_t index = I;
        static constexpr bool        value = P<T>::value;  // disjunction uses this
    };
  
    template<class, class = void>
    struct find_in_if_impl;
    
    template<std::size_t... Is, class _>
    struct find_in_if_impl<std::index_sequence<Is...>, _>
    {
        using type =
            std::disjunction<
                box<std::tuple_element_t<Is, Tuple>, Predicate, Is>...
              , box<struct not_found, Predicate, std::tuple_size_v<Tuple>>  // fallback
            >;
    };
    
 public:

    using type =
        typename find_in_if_impl<
            std::make_index_sequence<std::tuple_size_v<Tuple>>
        >::type;
};

template<class Tuple, template<class> class Predicate>
using find_in_if_t = typename find_in_if<Tuple, Predicate>::type;






// for exposition only -- partially applied is_same, in general can be any predicate

template<class T> using is_int = std::is_same<T, int>;
template<class T> using is_bool = std::is_same<T, bool>;
template<class T> using is_float = std::is_same<T, float>;
template<class T> using is_long = std::is_same<T, long>;


int main()
{
    using foundI = find_in_if_t<std::tuple<int, bool, float>, is_int>;
    using foundB = find_in_if_t<std::tuple<int, bool, float>, is_bool>;
    using foundF = find_in_if_t<std::tuple<int, bool, float>, is_float>;
    using foundL = find_in_if_t<std::tuple<int, bool, float>, is_long>;
    
    std::cout << "{ " << std::boolalpha << is_int<foundI::type>::value
                      << ", " << foundI::index
                      << ", " << foundI::value << " }\n";

    std::cout << "{ " << std::boolalpha << is_bool<foundB::type>::value
                      << ", " << foundB::index
                      << ", " << foundB::value << " }\n";

    std::cout << "{ " << std::boolalpha << is_float<foundF::type>::value
                      << ", " << foundF::index
                      << ", " << foundF::value << " }\n";

    std::cout << "{ " << std::boolalpha << is_long<foundL::type>::value
                      << ", " << foundL::index
                      << ", " << foundL::value << " }\n";
    
    return 0;
}


/*
g++ -std=c++17 -O2 -Wall -pedantic -pthread main.cpp && ./a.out

{ true, 0, true }
{ true, 1, true }
{ true, 2, true }
{ false, 3, false }
*/
