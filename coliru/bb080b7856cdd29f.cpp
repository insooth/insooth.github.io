
#include <iostream>
#include <string>
#include <vector>

#include <type_traits> // remove_reference
#include <tuple>
#include <functional>
#include <utility>

struct X {};
struct Y {};
struct Z {};
struct W {};

class A
{
    
public:


    using visitable_types = std::tuple<X, Y, Z>;  // DO NOT PASS REFERENCES
    

private:


    template<class... T>
    struct are_bare_types
      : std::true_type
    {};

    template<class T, class... Ts>
    struct are_bare_types<std::tuple<T, Ts...>>
    {
        static const bool value = 
            ! std::is_reference<T>::value
                && are_bare_types<Ts...>::value;
    };
    
    template<class T>
    struct are_bare_types<std::tuple<T>>
    {
        static const bool value =
            ! std::is_reference<T>::value;
    };
    


    template<class T, class U>
    struct is_within
      : std::false_type
    {};
    
    template<class T, class... Ts>
    struct is_within<T, std::tuple<T, Ts...>>
      : std::true_type
    {};

    template<class T, class U, class... Ts>
    struct is_within<T, std::tuple<U, Ts...>>
      : is_within<T, std::tuple<Ts...>>
    {};
    
    
    

    template<class F>
    struct lambda_traits
      : lambda_traits<decltype(&std::remove_reference_t<F>::operator())>
    {};
    
    template<class R, class T, class... As>  // mutable lambda
    struct lambda_traits<R (T::*)(As...)>
      : lambda_traits<R (T::*)(As...) const>
    {};
    
    template<class R, class T, class... As>
    struct lambda_traits<R (T::*)(As...) const>
    {
        static constexpr auto arity = sizeof...(As);
        
        using result_type = R;
        
        template<unsigned I>
        using arg_type = std::tuple_element_t<I, std::tuple<As...>>;
    };



    template<class F>
    using arg0_type = typename lambda_traits<F>::template arg_type<0>;
    



    template<class T, class F, class... Fs>
    struct are_actions_allowed
    {
        static const bool value =
           is_within<std::remove_reference_t<arg0_type<F>>, T>::value
            && are_actions_allowed<T, Fs...>::value;
    };

    template<class T, class F>
    struct are_actions_allowed<T, F>
    {
        static const bool value =
            is_within<std::remove_reference_t<arg0_type<F>>, T>::value;
    };
    
    
    
public:

    static_assert(are_bare_types<visitable_types>::value
                , "Visitable types cannot be references");


    template<class T>
    auto operator()(T& t)
        -> std::enable_if_t<is_within<T, visitable_types>::value, void>
    {
        if (std::get<action_type<T>>(actions))  // action specified?
        {
            std::get<action_type<T>>(actions)(t);
        }
        /** else: noop -- action not specified */
    }
    
    

    
    template<class... Fs>
    A(Fs&&... fs)
    {
        static_assert(are_actions_allowed<visitable_types, Fs...>::value
                    , "Unexpected action passed");
        
        fill_in(actions, std::forward<Fs>(fs)...);
    }



private:

    void operator()(...) = delete;  // passed type is not in visitable types!





    template<class T, class F>
    static void assign_action(T&& t, F&& f)
    {
        std::get<action_type<arg0_type<F>>>(std::forward<T>(t)) = std::forward<F>(f);
    }

    template<class T, class... Fs>
    static void fill_in(T&& t, Fs&&... fs)
    {
        int dummy[sizeof...(Fs)] = { (assign_action(std::forward<T>(t), std::forward<Fs>(fs)), 0)... };
        // cannot paste assign_action directly here ^~~ (not allowed by param packs expansion) 

        (void) dummy;
    }


    
    template<class T>
    using action_type = std::function<void (T&)>;    
    
    template<class Ts>
    struct make_action_types;
    
    template<class... Ts>
    struct make_action_types<std::tuple<Ts...>>
    {
        using type = std::tuple<action_type<Ts>...>;
    };    




    using action_types = typename make_action_types<visitable_types>::type;
    
    action_types actions;
};



int main()
{
    A a{[](X&) { std::cout << "X\n"; }, [](Y&) { std::cout << "Y\n"; }, [](Z&) { std::cout << "Z\n"; }};
    //A a{[](X&) { std::cout << "X\n"; }, [](Z&) { std::cout << "Z\n"; }, [](Y&) { std::cout << "Y\n"; }};
    //A a{[](X&) { std::cout << "X\n"; }, [](Y&) { std::cout << "Y\n"; }};
    //A a{[](X&) { std::cout << "X\n"; }, [](Y&) { std::cout << "Y\n"; }, [](W&) { std::cout << "W\n"; }};
    //A a{};
    X x;
    Y y;
    Z z;
    W w;
    a(x);
    a(y);
    a(z);
    //a(w);

    return 0;
}
