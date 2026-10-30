#pragma once

#include <cmath>
#include <cstdio>
#include <vector>

namespace tf {

using Fn = void (*)();
struct Case {
    const char* name;
    Fn fn;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> r;
    return r;
}
inline int& failures() {
    static int f = 0;
    return f;
}

struct Registrar {
    Registrar(const char* name, Fn fn) { registry().push_back({name, fn}); }
};

} // namespace tf

#define TF_CAT_(a, b) a##b
#define TF_CAT(a, b) TF_CAT_(a, b)

#define TEST_CASE(name)                                                    \
    static void TF_CAT(tf_test_, __LINE__)();                              \
    static ::tf::Registrar TF_CAT(tf_reg_, __LINE__)(                      \
        name, &TF_CAT(tf_test_, __LINE__));                                \
    static void TF_CAT(tf_test_, __LINE__)()

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);  \
            ++::tf::failures();                                            \
        }                                                                  \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                              \
    do {                                                                   \
        const double tf_a = static_cast<double>(a);                        \
        const double tf_b = static_cast<double>(b);                        \
        const double tf_e = static_cast<double>(eps);                      \
        if (std::fabs(tf_a - tf_b) > tf_e) {                               \
            std::printf("  FAIL %s:%d  |%g - %g| > %g\n",                  \
                        __FILE__, __LINE__, tf_a, tf_b, tf_e);             \
            ++::tf::failures();                                            \
        }                                                                  \
    } while (0)
