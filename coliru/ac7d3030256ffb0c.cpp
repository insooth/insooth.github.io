
#include <iostream>

#include <algorithm> // for_each
#include <string>
#include <vector>
#include <cassert> // assert
#include <cstdint> // size_t
#include <type_traits> // enable_if, is_convertible
#include <boost/iterator/iterator_facade.hpp>
#include <boost/iterator/iterator_categories.hpp>

class json_node_type
{
    template<class T>
    class iter
      : public boost::iterator_facade<iter<T>, T, boost::forward_traversal_tag>
    {
        struct enabler {};
        using size_type = typename T::size_type;

     public:
        explicit iter(T& _n, size_type _i = 0)
          : n{_n}, i{_i}
        {}
        
        template<class U>
        iter(const iter<U>& other
          , std::enable_if_t<std::is_convertible<U, T>::value, enabler> = enabler{})
          : n{other.n}, i{other.i}
        {}

     private:
        friend class boost::iterator_core_access;

        template<class> friend class iter;

        template<class U>
        bool equal(const iter<U>& other) const
        {
            return (this->i == other.i) && (&(this->n) == &(other.n));
        }
        
        void increment()
        {
            assert(i < n.size());
            ++i;
        }
        
        T& dereference() const
        {
            assert(i < n.size());
            
            return (i == 0) ? n : n.tail[i - 1];
        }
        
        T& n;
        size_type i;
    };

 public:    

    using tail_type = std::vector<json_node_type>;
    using size_type = tail_type::size_type;
    using iterator = iter<json_node_type>;
    using const_iterator = iter<const json_node_type>;

    size_type size() const noexcept { return 1 + tail.size(); }

    iterator begin() noexcept { return iterator{*this, 0}; }
    const_iterator begin() const noexcept { return const_iterator{*this, 0}; }
        
    iterator end() noexcept { return iterator{*this, size()}; }
    const_iterator end() const noexcept { return const_iterator{*this, size()}; }
    
    const_iterator cbegin() const noexcept { return const_iterator{*this, 0}; }
    const_iterator cend() const noexcept { return const_iterator{*this, size()}; }

    void push_back(json_node_type&& n) { tail.push_back(std::forward<json_node_type>(n)); }
    void push_back(const json_node_type& n) { tail.push_back(n); }

    /** NOTE: Resizes into @c size equal to @c n+1 value. */
    void resize(size_type n) { tail.resize(n); }

 private:
    std::string data;
    tail_type tail;
};


int main()
{
    std::vector<int> v;
    v.resize(3);

    (void) std::for_each(std::begin(v), std::end(v)
                       , [](const auto&) { std::cout << " -  " << std::endl; });
    
    json_node_type n;
    n.resize(2);
    (void) std::for_each(std::cbegin(n), std::cend(n)
                       , [](const auto&) { std::cout << " * " << std::endl; });
    
    
    assert(n.size() == v.size());

    // equal models zip
    (void) std::equal(std::cbegin(n), std::cend(n), std::begin(v)
                    , [](const auto&, const auto&)
                      { std::cout << " + " << std::endl; return true; });

    return 0;
}
