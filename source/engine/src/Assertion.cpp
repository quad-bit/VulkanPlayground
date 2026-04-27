#include "Assertion.h"
#include <iostream>

void Common::AssertWithMsg(const char* expr_str, bool expr, const char* file, int line, const char* msg)
{
    if (!expr)
    {
        std::cerr << "\nAssert failed:\t" << msg << "\n"
            << "Expected:\t" << expr_str << "\n"
            << "Source:\t\t" << file << ", line " << line << "\n";
        abort();
    }
}

void Common::Assert(const char* expr_str, bool expr, const char* file, int line)
{
    if (!expr)
    {
        std::cerr << "Expected:\t" << expr_str << "\n"
            << "Source:\t\t" << file << ", line " << line << "\n";
        abort();
    }
}
