
#include <iostream>
#include <functional> // function
#include <type_traits> // common_type

template<class T>
void foo(std::function<void (T)>&&)
{
}

template<class T>
using identity_t = std::common_type_t<T>;

template<class T>
void bar(identity_t<std::function<void (T)>>&&)
{
}

int main()
{
//    foo([](int){});  // 'main()::<lambda(int)>' is not derived from 'std::function<void(T)>'

    bar<int>([](int){});

    return 0;
}
