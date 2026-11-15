// Minimal test harness (no dependencies).
#pragma once
#include <cstdio>
#include <cmath>
#include <vector>
#include <functional>
#include <string>

struct TestCase { const char* name; std::function<void()> fn; };
inline std::vector<TestCase>& tests() { static std::vector<TestCase> t; return t; }
inline int& failures() { static int f = 0; return f; }
struct Reg { Reg(const char* n, std::function<void()> f) { tests().push_back({n, f}); } };
#define CAT2(a, b) a##b
#define CAT(a, b) CAT2(a, b)
#define TEST(name) static void CAT(t_, __LINE__)(); static Reg CAT(r_, __LINE__)(name, CAT(t_, __LINE__)); static void CAT(t_, __LINE__)()
#define CHECK(c) do { if (!(c)) { failures()++; std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define CHECK_NEAR(a, b, tol) do { double _a = (a), _b = (b); if (!(std::fabs(_a - _b) <= (tol))) { failures()++; std::printf("  FAIL %s:%d: %s = %g, expected %g +/- %g\n", __FILE__, __LINE__, #a, _a, _b, (double)(tol)); } } while (0)
