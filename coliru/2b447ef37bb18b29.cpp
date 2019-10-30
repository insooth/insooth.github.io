
// see LICENSE on insooth.github.io


#include <cstddef>  // size_t
#include <tuple> // tuple, tuple_size, get, tuple_element
#include <type_traits>  // true_type, false_type, is_same, common_type
#include <utility> // index_sequence, forward

#include <iostream>
#include <string>




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




// ------------------------------------


// box<Tag, F> is a type constructor
// we want to partially apply
template<class Tag, class F>
struct box : F
{
    using F::operator();
        
    using type     = F;
    using tag_type = Tag;
};

// TODO: deduction guide possible?
template<class Tag, class F>
constexpr auto boxify(F f)
{
    return box<Tag, F>{f};
}

// Partially apply tag_matcher than looks for box<Tag, F>.
template<class Tag>
class matcher
{
    template<class T, class B>
    struct tag_matcher;
    
    template<class T, class U, class F>
    struct tag_matcher<T, box<U, F>> : std::false_type {};
    
    template<class T, class F>
    struct tag_matcher<T, box<T, F>> : std::true_type {};

 public:

    template<class Box>
    using apply = tag_matcher<Tag, Box>;
};

// Get F from box<Tag, F> stored in Fs tuple.
template<class Tag, class Fs>
constexpr auto unbox(Fs&& fs)
{
    using items = std::remove_reference_t<Fs>;
    using found = find_in_if_t<items, matcher<Tag>::template apply>;

    if constexpr (found::index < std::tuple_size_v<items>)
    {
        return std::get<found::index>(std::forward<Fs>(fs));
    }
    else
    {
        return [](auto&&...) -> typename found::type {};  // not_found
    }
}


template<class T, class U>
struct is_equiv : std::false_type {};

template<class R, template<class> class C1, class C2, class... Ts, class... As>
struct is_equiv<R (C1<Ts...>::*)(As...), R (C2::*)(As...)> : std::true_type {};

template<class T>
struct drop_const : std::common_type<T> {};

template<class R, class C, class... As>
struct drop_const<R (C::*)(As...) const> : std::common_type<R (C::*)(As...)> {};

template<class Tag, class F>
using is_delegate = 
    is_equiv<typename Tag::type
           , typename drop_const<decltype(&F::operator())>::type
           >;



// --------------------



template<class Injected>
struct Testable
{
    
    void foo() { obj.foo(); }
    
    int bar(std::string s)
    {
        obj.foo();
        
        return obj.bar(s);
    }
    
    Injected obj;
};


struct Foo : std::common_type<void (Testable<int>::*)()> {};
struct Bar : std::common_type<int (Testable<int>::*)(std::string)> {};


template<class... Fs>
struct M
{
    M(Fs... fs) : fs{fs...}
    {
        static_assert(std::conjunction_v<
            is_delegate<typename Fs::tag_type, typename Fs::type>...
            >, "Tag sig must match sig of mocked mem fn");
    }
    
    // iface
    void foo() { unbox<Foo>(fs)(); }
    int bar(std::string s) { return unbox<Bar>(fs)(s); }
    
    std::tuple<Fs...> fs;
};


int main()
{
    using namespace std::literals;
    
    M m
    {
        boxify<Foo>([] { std::cout << "foo" << std::endl; })
      , boxify<Bar>([](std::string s) -> int { std::cout << "bar " << s << std::endl; return 0; })
    };
    
    Testable<decltype(m)> t{m};

    t.foo();
    t.bar("xxx"s);
    
    return 0;
}


/*
http://coliru.stacked-crooked.com/a/2b447ef37bb18b29

g++ -std=c++17 -O2 -Wall -pedantic -pthread main.cpp && ./a.out
foo
foo
bar xxx
*/
