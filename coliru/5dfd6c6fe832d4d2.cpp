#include <iostream>
#include <string>
#include <vector>

#include <iterator> // being, end
#include <tuple>
#include <algorithm>
#include <utility> // pair
#include <functional> // ref

using R = std::pair<unsigned, unsigned>;
using E = std::tuple<R, int, std::pair<bool, bool>>;
//                                     left, right

constexpr E t[] =
{
    {{0, 0}, 1, {true, true}}
  , {{1, 100}, 2, {true, false}}
  , {{100, 200}, 3, {true, true}}
};

struct C
{
    bool operator() (unsigned x, const E& e)
    {
        const bool m = lessL(x, e);
        
        if (!p && !m && !lessR(e, x)) p = &e;
        
        return m;
    }
    
    bool operator() (const E& e, unsigned x)
    {
        const bool m = lessR(e, x);
        
        if (!p && !m && !lessL(x, e)) p = &e;
        
        return m;
    }

    bool lessL (unsigned x, const E& e)
    {
        const auto& r = std::get<0>(e);
        const auto& c = std::get<2>(e);
    
        return c.first ? (x < r.first) : (x <= r.first);
    }
    
    bool lessR (const E& e, unsigned x)
    {
        const auto& r = std::get<0>(e);
        const auto& c = std::get<2>(e);
    
        return c.second ? (x > r.second) : (x >= r.second);
    }
    
    const E* p = nullptr;
};

int main()
{
    for (unsigned v : {0, 1, 2, 99, 100, 101, 102, 150, 199, 200, 201})
    {
        C c;
        const bool f = std::binary_search(std::begin(t), std::end(t), v, std::ref(c));
        
        std::cout << std::boolalpha <<  f << " " << c.p << "\n";
        
        if (c.p)
        {
            const auto& p = *(c.p);
            std::cout << v << " -> " << std::get<0>(p).first << " " << std::get<0>(p).second << "\n";
        }
        else std::cout << "failed: " << v << "\n" ;
    }
    
    return 0;
}
