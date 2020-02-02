
// see LICENSE on insooth.github.io

#include <iostream>
#include <string>

#include <cstddef>  // size_t, uint16_t
#include <stdexcept>  // out_of_range
#include <type_traits>  // is_const, conditional, remove_const


template<class T, std::size_t Step = 1>
struct view_as
{
  view_as() = default;
  
  template<class U>
  view_as(U* p, std::size_t t)
    :
      data{reinterpret_cast<char*>(const_cast<std::remove_const_t<U>*>(p))}
    , total{t}
    , current{0}
  {}
  
  T& operator()() { return *next(*this); }

  const T& operator()() const { return *next(*this); }

 private:
 
  template<class U, std::size_t S>
  static auto next(view_as<U, S>& v)
    -> std::conditional_t<
           std::is_const_v<decltype(v)>
         , const U*
         , U*
         >
  {
    if (v.data && (v.current < v.total))
    { 
      U* u = reinterpret_cast<U*>(v.data + (v.current * sizeof(U)));
      v.current += S * sizeof(U);
      
      return u;
    }
    else { throw std::out_of_range{"add iterator iface"}; };
  }
 
  char*       data    = nullptr;
  std::size_t total   = 0;
  std::size_t current = 0;
};

int main()
{
    const std::string s = "abcdefghi";
    view_as<std::int16_t, 2> v{s.data(), s.length()};
    
    for (auto x : s)
    {
        std::cout << x << " -- " << int{v()} << '\n';
    }
}

/*
http://coliru.stacked-crooked.com/a/08eec2a03ad34417

g++ -std=c++17 -O2 -Wall -pedantic -pthread main.cpp && ./a.out
terminate called after throwing an instance of 'std::out_of_range'
  what():  add iterator iface
a -- 25185
b -- 105
c -- 3200
d -- bash: line 7: 23065 Aborted                 (core dumped) ./a.out
*/
