
#include <iostream>

struct C
{
    int i = 200;
};

struct A
{
    A() = default;
    explicit A(int _i) : i(_i) {}

    A(const C& c) : i(c.i) {}
    
    int i = 0;
};

void foo(A a)
{
    std::cout << a.i << "\n";
}

struct B
{
    operator A () { return A{100}; }
};

int main()
{
    foo(A{1234});
    foo(B{});  // implicit conversion operator
    foo(C{});  // implicit conversion construtor
    
    return 0;
}
