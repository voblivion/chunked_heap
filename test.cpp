/*
Even though the use case of chunked_heap is not the same as other std containers,
here is a small comparison of different containers for the following test :
1. add n elements
2. add 1 more element and remember its position (or the iterator pointing to it)
3. add n more elements
4. remove the element at stored position while keeping the order of elements
5. add n more elements

NOTE : yes, std::vector seems about 3x faster, but remember the purpose of
chunked_heap is to keep valid pointers to elements in any case.
std::vector cannot achieve that on erase (even using erase-remove idiom).
A more comparable case would be with the std::list and here the chunked_heap
is about 2x faster (thanks to better cache use).
*/
#include <iostream>
#include <deque>
#include <list>
#include <chrono>
#include <unordered_map>
#include "chunked_heap.h"

struct Foo {
    int m_k;
    double m_d;
    double m_d2;
    Foo(int k) : m_k(k), m_d(3.5) {}
};

void main() {
    unsigned n = 1000000;

    {
        std::chrono::high_resolution_clock::time_point t0 = std::chrono::high_resolution_clock::now();
        vob::chunked_heap<Foo, vob::planned_chunk_sizer<3*1000000, 8>> c;
        for (unsigned k = 0; k < n; ++k) {
            c.emplace(k);
        }
        auto it = c.emplace(42);
        for (unsigned k = n; k < 2 * n; ++k) {
            c.emplace(k);
        }
        c.erase(it);
        for (unsigned k = 2 * n; k < 3 * n; ++k) {
            c.emplace(k);
        }
        for (auto it = c.begin(); it != c.end(); ++it) {
            ++(it->m_k);
        }
        std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> d0 = std::chrono::duration_cast<std::chrono::duration<double >> (t1 - t0);
        std::cout << d0.count() << " for chunked_heap" << std::endl;
    }
    
    {
        std::chrono::high_resolution_clock::time_point t0 = std::chrono::high_resolution_clock::now();
        std::vector<Foo> c;
        c.reserve(3 * 1000000);
        for (unsigned k = 0; k < n; ++k) {
            c.emplace_back(k);
        }
        c.emplace_back(42);
        for (unsigned k = n; k < 2*n; ++k) {
            c.emplace_back(k);
        }
        auto it = c.begin() + n;
        auto jt = c.end() - 1;
        std::swap(it, jt);
        c.erase(c.end() - 1);
        for (unsigned k = 2*n; k < 3*n; ++k) {
            c.emplace_back(k);
        }
        for (auto it = c.begin(); it != c.end(); ++it) {
            ++(it->m_k);
        }
        std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> d0 = std::chrono::duration_cast<std::chrono::duration<double >> (t1 - t0);
        std::cout << d0.count() << " for vector" << std::endl;
    }

    {
        std::chrono::high_resolution_clock::time_point t0 = std::chrono::high_resolution_clock::now();
        std::unordered_map<unsigned, Foo> c;
        for (unsigned k = 0; k < n; ++k) {
            c.emplace(std::make_pair(k, Foo(k)));
        }
        c.emplace(std::make_pair(-1, Foo(-1)));
        for (unsigned k = n; k < 2 * n; ++k) {
            c.emplace(std::make_pair(k, Foo(k)));
        }
        c.erase(-1);
        for (unsigned k = 2 * n; k < 3 * n; ++k) {
            c.emplace(std::make_pair(k, Foo(k)));
        }
        for (auto it = c.begin(); it != c.end(); ++it) {
            ++(it->second.m_k);
        }
        std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> d0 = std::chrono::duration_cast<std::chrono::duration<double>>(t1 - t0);
        std::cout << d0.count() << " for unordered_map" << std::endl;
    }

    {
        std::chrono::high_resolution_clock::time_point t0 = std::chrono::high_resolution_clock::now();
        std::list<Foo> c;
        for (unsigned k = 0; k < n; ++k) {
            c.emplace_back(k);
        }
        c.emplace_back(42);
        auto it = c.end();
        --it;
        for (unsigned k = n; k < 2 * n; ++k) {
            c.emplace_back(k);
        }
        c.erase(it);
        for (unsigned k = 2 * n; k < 3 * n; ++k) {
            c.emplace_back(k);
        }
        for (auto it = c.begin(); it != c.end(); ++it) {
            ++(it->m_k);
        }
        std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> d0 = std::chrono::duration_cast<std::chrono::duration<double >> (t1 - t0);
        std::cout << d0.count() << " for list" << std::endl;
    }

    {
        std::chrono::high_resolution_clock::time_point t0 = std::chrono::high_resolution_clock::now();
        std::deque<Foo> c;
        for (unsigned k = 0; k < n; ++k) {
            c.emplace_back(k);
        }
        c.emplace_back(42);
        auto it = c.end();
        --it;
        for (unsigned k = n; k < 2 * n; ++k) {
            c.emplace_back(k);
        }
        c.erase(it);
        for (unsigned k = 2 * n; k < 3 * n; ++k) {
            c.emplace_back(k);
        }
        for (auto it = c.begin(); it != c.end(); ++it) {
            ++(it->m_k);
        }
        std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> d0 = std::chrono::duration_cast<std::chrono::duration<double >> (t1 - t0);
        std::cout << d0.count() << " for deque" << std::endl;
    }

    system("pause");
}
