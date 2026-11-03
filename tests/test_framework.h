#ifndef SQLGUARD_TEST_FRAMEWORK_H
#define SQLGUARD_TEST_FRAMEWORK_H

#include <iostream>
#include <string>
#include <vector>

namespace sqlguard_test {

    using TestFunction = void (*)();

    struct TestCase {
        TestFunction function;
        const char* name;
    };

    inline std::vector<TestCase>& registry()
    {
        static std::vector<TestCase> tests;
        return tests;
    }

    inline int& failureCount()
    {
        static int failures = 0;
        return failures;
    }

    class Registrar {
    public:
        Registrar(TestFunction function, const char* name)
        {
            registry().push_back(TestCase { function, name });
        }
    };

    inline void fail(const char* file, int line, const char* expression)
    {
        ++failureCount();
        std::cerr << file << ':' << line << ": check failed: " << expression << '\n';
    }

    inline int runAll()
    {
        int passed = 0;
        for(const TestCase& test : registry()) {
            const int before = failureCount();
            test.function();
            if(failureCount() == before) {
                ++passed;
                std::cout << "[PASS] " << test.name << '\n';
            }
            else {
                std::cout << "[FAIL] " << test.name << '\n';
            }
        }
        const int failed = static_cast<int>(registry().size()) - passed;
        std::cout << "\n" << passed << " passed, " << failed << " failed, " << registry().size() << " total\n";
        return failed == 0 ? 0 : 1;
    }

} // namespace sqlguard_test

#define SG_TEST(testName)                                                              \
    static void testName();                                                            \
    static const ::sqlguard_test::Registrar registrar_##testName(testName, #testName); \
    static void testName()

#define SG_CHECK(expression)                                        \
    do {                                                            \
        if(!(expression)) {                                         \
            ::sqlguard_test::fail(__FILE__, __LINE__, #expression); \
        }                                                           \
    } while(false)

#define SG_REQUIRE(expression)                                      \
    do {                                                            \
        if(!(expression)) {                                         \
            ::sqlguard_test::fail(__FILE__, __LINE__, #expression); \
            return;                                                 \
        }                                                           \
    } while(false)

#define SG_CHECK_EQ(actual, expected)                                            \
    do {                                                                         \
        const auto& actualValue = (actual);                                      \
        const auto& expectedValue = (expected);                                  \
        if(!(actualValue == expectedValue)) {                                    \
            ::sqlguard_test::fail(__FILE__, __LINE__, #actual " == " #expected); \
            std::cerr << "  actual:   " << actualValue << '\n';                  \
            std::cerr << "  expected: " << expectedValue << '\n';                \
        }                                                                        \
    } while(false)

#endif
