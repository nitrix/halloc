#ifndef TESTING_H
#define TESTING_H

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#define ADD_TEST(name, ...) \
    static bool name(void) { \
        __VA_ARGS__ \
        return true; \
    }

#define RUN_TEST(name) \
    printf("Testing %s ... ", #name); \
    bool result_ ## name = name(); \
    if (!result_ ## name) { \
        printf("FAIL\n"); \
        failed++; \
    } else { \
        passed++; \
        printf("OK\n"); \
    }

#define MAIN(...) \
    int main(void) { \
        int passed = 0; \
        int failed = 0; \
        __VA_ARGS__ \
        printf("Total tests: %d/%d\n", passed, passed + failed); \
        if (failed > 0) { \
            return EXIT_FAILURE; \
        } \
        return EXIT_SUCCESS; \
    }

#define MUST(cond, reason) \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s\n", reason); \
        return false; \
    }

#endif
