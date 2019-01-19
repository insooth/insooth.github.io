
// see LICENSE on insooth.github.io

#include <iostream>

#include <algorithm>
#include <utility>
#include <functional>
#include <memory>
#include <variant>

// LEAKS memory -- DO NOT USE!
struct A
{
    explicit A(int& v) : value{v} {}
    explicit A(std::unique_ptr<int> v) : value{std::move(v)} {}
    
    union V
    {
        explicit V(int& v) : ref{v} {}
        explicit V(std::unique_ptr<int> v) : own{std::move(v)} {}
        
        ~V()
        {
            // crash if following present, otherwise memory leak:
            // if (own) own.~unique_ptr<int>();
        }
        
        std::unique_ptr<int>        own;
        std::reference_wrapper<int> ref;
    } value;
};

// Uses runtime switch -- equivalent to a hand-coded one.
struct B
{
    std::variant<std::unique_ptr<int>, std::reference_wrapper<int>> value;
};

// Dependency chain
struct C
{
    explicit C(int& v) : own{nullptr}, ref{v} {}
    explicit C(std::unique_ptr<int> v) : own{std::move(v)}, ref{*own} {}
    
    // cannot copy (unique_ptr) disable that
    // may move only -- ref-to-own dependecy is not broken
    
    std::unique_ptr<int>        own;
    std::reference_wrapper<int> ref;
};

// DO NOT USE -- use B!
struct D
{
    int* value;  // DOES NOT WORK for arrays!
    bool owned;
    
    explicit D(int& v) : value{&v}, owned{false} {}
    D() : value{new int{200}}, owned{true} {}
    
    explicit D(const D& d)
    {
        if (&d != this)
        {
            if (d.owned) { value = new int{*d.value}; } else { value = d.value; }
            owned = d.owned;
        }
    }
    
    explicit D(D&& d)
    {
        if (&d != this)
        {
            value = d.value; owned = d.owned; d.value = nullptr; d.owned = false;
        }
    }
    
    D& operator=(const D& d)
    {
        if (&d != this)
        {
            if (d.owned) { value = new int{*d.value}; } else { value = d.value; }
            owned = d.owned;
        }
        
        return *this;
    }
    
    D& operator=(D&& d)
    {
        if (&d != this)
        {
            value = d.value; owned = d.owned; d.value = nullptr; d.owned = false;
        }
        
        return *this;
    }
    
    ~D() { if (owned && (nullptr != value)) { delete value; } else { value = nullptr; } }
};

int main()
{
    int i = 100;
    A a{i};
    std::cout << a.value.ref.get() << '\n';
    std::cout << a.value.own.get() << '\n';
    std::cout << *a.value.own << '\n';
    
//    A a1{a};  -- implicitly deleted
    
    A aa{std::make_unique<int>(200)};
    std::cout << "---\n" << aa.value.ref.get() << '\n';
    std::cout << aa.value.own.get() << '\n';
    std::cout << *aa.value.own << '\n';
    
//    A aa1{aa}; -- implicitly deleted
    
    B b0;  // our variant is DefaultConstructible because unique_ptr is
    
    B b{i};
    std::cout << "---\n" << (std::holds_alternative<std::unique_ptr<int>>(b.value)
        ? *std::get<std::unique_ptr<int>>(b.value) : std::get<std::reference_wrapper<int>>(b.value).get())
        << '\n';

//    B b1{b};  -- implicitly deleted

    B bb{std::make_unique<int>(200)};
    std::cout << "---\n" << (std::holds_alternative<std::unique_ptr<int>>(bb.value)
        ? *std::get<std::unique_ptr<int>>(bb.value) : std::get<std::reference_wrapper<int>>(bb.value).get())
        << '\n';
    
//    B bb1{bb};  -- implicitly deleted
    
    C c{i};
    std::cout << "---\n" << c.ref.get() << '\n';
    std::cout << c.own.get() << '\n';
//    std::cout << *c.own << '\n';

    C cc{std::make_unique<int>(200)};
    std::cout << "---\n" << cc.ref.get() << '\n';
    std::cout << cc.own.get() << '\n';
    std::cout << *cc.own << '\n';
    
//    C cc1{c}; -- implicitly deleted
    
    C cc2{std::move(c)};
    std::cout << "---\n" << cc2.ref.get() << '\n';
    std::cout << cc2.own.get() << '\n';
//    std::cout << *cc2.own << '\n';
    
    C cc3{std::move(cc)};
    std::cout << "---\n" << cc3.ref.get() << '\n';
    std::cout << cc3.own.get() << '\n';
    std::cout << *cc3.own << '\n';
    
    cc3 = std::move(cc2);
    std::cout << "---\n" << cc3.ref.get() << '\n';
    std::cout << cc3.own.get() << '\n';
//    std::cout << *cc3.own << '\n';
    
    D d{i};
    std::cout << "---\n" << *d.value << '\n';
    
    D dd{};
    std::cout << "---\n" << *dd.value << '\n';
    
    D dd1{d};
    std::cout << "---\n" << *dd1.value << '\n';
    
    D dd2{std::move(d)};
    std::cout << "---\n" << *dd2.value << '\n';
    
    D dd3{i};
    dd3 = dd;
    std::cout << "---\n" << *dd3.value << '\n';
    
    D dd4{i};
    dd4 = std::move(dd3);
    std::cout << "---\n" << *dd4.value << '\n';
    
    return 0;
}

