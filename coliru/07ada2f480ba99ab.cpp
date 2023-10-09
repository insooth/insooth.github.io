
#include <string>
#include <iostream>
#include <sstream>
#include <array>
#include <bitset>


#include <cassert>
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <span>


template<std::size_t N>
struct A
{
    constexpr A(const std::uint8_t* const d) : data{d, N} {}    
    std::span<const std::uint8_t, N> data;
    struct { std::size_t read = 0; std::size_t write = 0; } cursor;
};





// start, stop :: BitPosition
constexpr std::uint8_t select(std::uint8_t data, std::uint8_t start, std::uint8_t stop)
{
    assert(start < 8);
    assert(stop < 8);
    assert(stop >= start);
    
    return data & (0xFF >> (7 - stop)) & (0xFF << start);
}

// start, offset :: BitPosition
constexpr std::uint8_t shift(std::uint8_t data, std::uint8_t start, std::uint8_t offset = 0)
{
    assert(start < 8);
    assert(offset <= 8);
    
    return (data >> start) << offset;
}


// bit indices
// 7654321076543210

template<std::size_t N>
constexpr std::pair<std::uint8_t, std::size_t> next(std::span<const std::uint8_t, N> data, std::size_t cursor, std::uint8_t n)
{
    assert(n > 0);
    assert(n <= 8);
    assert((cursor + n) <= (8 * N));
    
    const struct Ctrl {
        using BitPosition = std::uint8_t;  // [0, 7]
        using BitPositionRange = std::pair<BitPosition, BitPosition>;
        using ByteIndex = std::uint8_t;  // [0, ...]
        using BitCount = std::uint8_t; // [0, 7]
        
        Ctrl(std::size_t cursor, std::size_t n) :
            pos{BitPosition(cursor % 8), BitPosition((cursor + n - 1) % 8)}  // n > 0
          , twoBytes{pos.second < pos.first}
          , byte{ByteIndex(cursor / 8), ByteIndex((cursor + n - 1) / 8)} // n > 0
          , count{std::min<BitCount>(BitCount{8} - BitCount{pos.first}, BitCount(n)), twoBytes ? (BitCount{pos.second} + 1) : 0}
          , range{{pos.first, (twoBytes ? BitPosition{7} : pos.second)}, {BitPosition{0}, twoBytes ? pos.second : 0}}
        {
std::cout << "cursor " << cursor << " next " << n << " byte " << std::to_string(byte.first) << " " << std::to_string(byte.second) << " two " << twoBytes << " pos " << std::to_string(pos.first) << " " << std::to_string(pos.second) << " count " << std::to_string(count.first) << " " << std::to_string(count.second) <<"  range " << std::to_string(range.first.first) << " " << std::to_string(range.first.second)<< " " << std::to_string(range.second.first) << " " << std::to_string(range.second.second);
            assert(n > 0);
            assert((count.first + count.second) == n);
            assert(count.second == std::max<BitCount>(n - count.first, BitCount{0}));
            assert(byte.first < N);
            assert(byte.second < N);
        }
        
        BitPositionRange pos;
        bool twoBytes;
        std::pair<ByteIndex, ByteIndex> byte;
        std::pair<BitCount, BitCount> count;
        std::pair<BitPositionRange, BitPositionRange> range;
    } ctrl{cursor, n};
    
    
    const std::uint8_t first = shift(select(data[ctrl.byte.first], ctrl.range.first.first, ctrl.range.first.second), ctrl.range.first.first);
    const std::uint8_t second = shift(select(data[ctrl.byte.second], ctrl.range.second.first, ctrl.range.second.second), ctrl.range.second.first, ctrl.count.first);

std::cout << " first " << std::bitset<8>(first).to_string() << " second " << std::bitset<8>(second).to_string() << std::endl; 
    return std::make_pair(ctrl.twoBytes ? first | second : first, cursor + n );;
}



template<std::size_t N, class T>
std::string doNext(T&& t) { return [&t] { auto r = next(t.data, t.cursor.read, std::uint8_t(N)); t.cursor.read = r.second; return std::bitset<8>{r.first}.to_string(); }(); } 


int main()
{
    std::array<std::uint8_t, 3> s = {0b11111101, 0b10001010, 0b11110111};
    //                               first    0

    A<s.size()> a{s.data()};
    std::cout << doNext<1>(a) << " 1 " << std::endl;
    std::cout << doNext<4>(a) << " 1110 " << std::endl;
    std::cout << doNext<7>(a) << " 1010111 " << std::endl;
    a.cursor.read = 0;
    std::cout << doNext<1>(a) << " 1 " << std::endl;
    std::cout << doNext<7>(a) << " 11111110 " << std::endl;
    std::cout << doNext<1>(a) << " 0 " << std::endl;
    std::cout << doNext<6>(a) << " 000101 " << std::endl;
    std::cout << doNext<1>(a) << " 1 " << std::endl;
    a.cursor.read = 0;
    std::cout << doNext<8>(a) << " 11111101 " << std::endl;
    std::cout << doNext<8>(a) << " 10001010 " << std::endl;
    std::cout << doNext<8>(a) << " 11110111 " << std::endl;;
    //std::cout << (doNext<8>(a), doNext<8>(a), doNext<8>(a), doNext<1>(a)) << std::endl;
    
    return 0;
}

/*

g++ -std=c++20 -O2 -Wall -pedantic -pthread main.cpp && ./a.out
cursor 0 next 1 byte 0 0 two 0 pos 0 0 count 1 0  range 0 0 0 0 first 00000001 second 00000010
00000001 1 
cursor 1 next 4 byte 0 0 two 0 pos 1 4 count 4 0  range 1 4 0 0 first 00001110 second 00010000
00001110 1110 
cursor 5 next 7 byte 0 1 two 1 pos 5 3 count 3 4  range 5 7 0 3 first 00000111 second 01010000
01010111 1010111 
cursor 0 next 1 byte 0 0 two 0 pos 0 0 count 1 0  range 0 0 0 0 first 00000001 second 00000010
00000001 1 
cursor 1 next 7 byte 0 0 two 0 pos 1 7 count 7 0  range 1 7 0 0 first 01111110 second 10000000
01111110 11111110 
cursor 8 next 1 byte 1 1 two 0 pos 0 0 count 1 0  range 0 0 0 0 first 00000000 second 00000000
00000000 0 
cursor 9 next 6 byte 1 1 two 0 pos 1 6 count 6 0  range 1 6 0 0 first 00000101 second 00000000
00000101 000101 
cursor 15 next 1 byte 1 1 two 0 pos 7 7 count 1 0  range 7 7 0 0 first 00000001 second 00000000
00000001 1 
cursor 0 next 8 byte 0 0 two 0 pos 0 7 count 8 0  range 0 7 0 0 first 11111101 second 00000000
11111101 11111101 
cursor 8 next 8 byte 1 1 two 0 pos 0 7 count 8 0  range 0 7 0 0 first 10001010 second 00000000
10001010 10001010 
cursor 16 next 8 byte 2 2 two 0 pos 0 7 count 8 0  range 0 7 0 0 first 11110111 second 00000000
11110111 11110111 

*/
