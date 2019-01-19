
#include <iostream>


class A
{
    int m = 999;
    void foo() { std::cout << "foo: private!\n"; }
    void bar() { std::cout << "bar: private!\n"; }
};


template<class R, class C, class... Ts>
struct MemFun
{
    using type = R (C::*)(Ts...);
};

template<class T, class C>
struct Mem
{
    using type = T (C::*);
};


template<class Tag, class T>
struct Proxy
{
    static typename T::type value;
};

template<class Tag, class T>
typename T::type Proxy<Tag, T>::value;


template<class Tag, class T, typename T::type P>
class MakeProxy
{
    struct X { X() { Proxy<Tag, T>::value = P; } };

    static X dummy;
};

template<class Tag, class T, typename T::type P>
typename MakeProxy<Tag, T, P>::X MakeProxy<Tag, T, P>::dummy;

template<class T>
const auto access_v = Proxy<T, T>::value;




// --------------------------------------------------


struct AFoo : MemFun<void, A> {};
struct ABar : MemFun<void, A> {};
struct AM   : Mem<int, A>     {};


template class MakeProxy<AFoo, AFoo, &A::foo>;
template class MakeProxy<ABar, ABar, &A::bar>;
template class MakeProxy<AM,   AM,   &A::m>;



int main()
{
    A a;
    
    (a.* access_v<AFoo>)();  // calls A::foo
    (a.* access_v<ABar>)();  // calls A::bar
    
    std::cout << (a.* access_v<AM>) << "\n";
}

