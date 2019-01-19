
#include <utility> // pair, move, forward
#include <cassert>

struct A
{
    A() = delete;
    explicit A(int _i) : i(_i) {}
    int i = 0;
};

template<class T>
std::pair<bool, T> foo(int x, T&& t= T{})
{
    t.i = x;
    return {true, std::forward<T>(t)};
}

int main()
{
    A a{300};

    assert(300 == a.i);
    
    foo<A>(100, std::move(a));
    
    assert(100 == a.i);
    
    
    // error: use of deleted function 'A::A()'
//    foo<A>(100);

    return 0;
}
