/+
REQUIRED_ARGS: -HC -c -o- -wi -extern-std=c++20
PERMUTE_ARGS:
TEST_OUTPUT:
---
compilable/dtoh_invalid_identifiers.d(103): Warning: function `register` is a keyword in C++
compilable/dtoh_invalid_identifiers.d(103):        The generated C++ header will contain identifiers that are keywords in C++
compilable/dtoh_invalid_identifiers.d(105): Warning: namespace `const_cast` is a keyword in C++
compilable/dtoh_invalid_identifiers.d(116): Warning: function `and` is a special operator in C++
compilable/dtoh_invalid_identifiers.d(121): Warning: enum `mutable` is a keyword in C++
compilable/dtoh_invalid_identifiers.d(123): Warning: alias `char8_t` is a keyword in C++20
compilable/dtoh_invalid_identifiers.d(141): Warning: function `offsetof` is a default macro in C++
compilable/dtoh_invalid_identifiers.d(143): Warning: function `wchar_t` is a keyword in C++11
compilable/dtoh_invalid_identifiers.d(145): Warning: function `__attribute__` is a reserved identifier in C++
// Automatically generated by Digital Mars D Compiler

#pragma once

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>

#ifdef CUSTOM_D_ARRAY_TYPE
#define _d_dynamicArray CUSTOM_D_ARRAY_TYPE
#else
/// Represents a D [] array
template<typename T>
struct _d_dynamicArray final
{
    size_t length;
    T *ptr;

    _d_dynamicArray() : length(0), ptr(NULL) { }

    _d_dynamicArray(size_t length_in, T *ptr_in)
        : length(length_in), ptr(ptr_in) { }

    T& operator[](const size_t idx) {
        assert(idx < length);
        return ptr[idx];
    }

    const T& operator[](const size_t idx) const {
        assert(idx < length);
        return ptr[idx];
    }
};
#endif

extern void register(int32_t* ptr);

namespace const_cast
{
    extern void bar();

}
template <typename register_>
struct S final
{
    register_ x;
    S()
    {
    }
};

struct S2 final
{
    int32_t register_;
    void and();
    S2() :
        register_()
    {
    }
    S2(int32_t register_) :
        register_(register_)
        {}
};

extern void f(int32_t alignas_);

enum class mutable
{
    yes = 0,
    no = 1,
};

typedef S<char > char8_t;

class Base
{
public:
    virtual void foo();
};

template <typename typename_>
class Alias : public typename_
{
};

extern void user(Alias<Base* >* i);

template <typename typename_>
struct InvalidNames final
{
    typename_ register_;
    void foo(typename_ and_);
    InvalidNames()
    {
    }
};

extern void useInvalid(InvalidNames<int32_t > _param_0);

extern size_t offsetof();

extern void wchar_t();

extern void __attribute__();
---
+/
#line 100
extern(C++):

__gshared bool and;
void register(int* ptr) {}

extern(C++, const_cast)
void bar() {}

struct S(register)
{
    register x;
}

struct S2
{
    int register;
    void and() {}
}

void f(int alignas) {}

enum mutable { yes, no }

alias char8_t = S!char;

class Base {
    void foo() {}
}
class Alias(typename) : typename {}

void user(Alias!Base i) {}

struct InvalidNames(typename)
{
    typename register;

    void foo(typename and) {}
}

void useInvalid(InvalidNames!int) {}

size_t offsetof() { return 0; }

void wchar_t() {}

void __attribute__() {}
