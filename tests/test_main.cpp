#include "framework.h"

int main() {
    int failed_cases = 0;
    for (const auto& c : tf::registry()) {
        const int before = tf::failures();
        c.fn();
        const int added = tf::failures() - before;
        std::printf("%s  %s\n", added == 0 ? "ok  " : "FAIL", c.name);
        if (added != 0) ++failed_cases;
    }
    std::printf("\n%zu cases, %d failed\n", tf::registry().size(), failed_cases);
    return failed_cases == 0 ? 0 : 1;
}
