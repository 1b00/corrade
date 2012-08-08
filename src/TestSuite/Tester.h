#ifndef Corrade_TestSuite_Tester_h
#define Corrade_TestSuite_Tester_h
/*
    Copyright © 2007, 2008, 2009, 2010, 2011, 2012
              Vladimír Vondruš <mosra@centrum.cz>

    This file is part of Corrade.

    Corrade is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License version 3
    only, as published by the Free Software Foundation.

    Corrade is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License version 3 for more details.
*/

/** @file
 * @brief Class Corrade::TestSuite::Tester, macros CORRADE_TEST_MAIN(), CORRADE_VERIFY(), CORRADE_COMPARE(), CORRADE_COMPARE_AS().
 */

#include <functional>
#include <iostream>

#include "Utility/Debug.h"
#include "Compare.h"

namespace Corrade { namespace TestSuite {

/**
@brief Base class for unit tests

See @ref unit-testing for introduction.

@see CORRADE_TEST_MAIN(), CORRADE_VERIFY(), CORRADE_COMPARE(), CORRADE_COMPARE_AS()
*/
template<class Derived> class Tester {
    public:
        /** @brief Pointer to test case function */
        typedef void (Derived::*TestCase)();

        inline constexpr Tester(): logOutput(nullptr), errorOutput(nullptr), testCaseLine(0), expectedFailure(nullptr) {}

        /**
         * @brief Execute the tester
         * @param logOutput     Output stream for log messages
         * @param errorOutput   Output stream for error messages
         * @return Non-zero if there are no test cases, if any test case fails
         *      or doesn't contain any checking macros, zero otherwise.
         */
        int exec(std::ostream* logOutput = &std::cout, std::ostream* errorOutput = &std::cerr) {
            this->logOutput = logOutput;
            this->errorOutput = errorOutput;

            /* Fail when we have nothing to test */
            if(testCases.empty()) {
                Utility::Error(errorOutput) << "In" << testName << "weren't found any test cases!";
                return 2;
            }

            Utility::Debug(logOutput) << "Starting" << testName << "with" << testCases.size() << "test cases...";

            unsigned int errorCount = 0,
                noCheckCount = 0;

            for(typename std::vector<std::function<void(Derived&)>>::const_iterator i = testCases.begin(); i != testCases.end(); ++i) {
                try {
                    testCaseName.clear();
                    (*i)(*static_cast<Derived*>(this));
                } catch(Exception e) {
                    ++errorCount;
                    continue;
                }

                /* No testing macros called, don't print function name to output */
                if(testCaseName.empty()) {
                    ++noCheckCount;
                    continue;
                }

                Utility::Debug d(logOutput);
                d << (expectedFailure ? " XFAIL:" : "    OK:") << testCaseName;
                if(expectedFailure) d << "\n       " << expectedFailure->message();
            }

            Utility::Debug d(logOutput);
            d << "Finished" << testName << "with" << errorCount << "errors.";
            if(noCheckCount)
                d << noCheckCount << "test cases didn't contain any checks!";

            return errorCount != 0 || noCheckCount != 0;
        }

        /**
         * @brief Add test cases
         *
         * Adds one or more test cases to be executed when calling exec().
         */
        template<class ...T> void addTests(TestCase first, T... next) {
            testCases.push_back(std::mem_fn(first));

            addTests(next...);
        }

        #ifndef DOXYGEN_GENERATING_OUTPUT
        /* Compare two identical types without explicit type specification */
        template<class T> inline void compare(const std::string& actual, const T& actualValue, const std::string& expected, const T& expectedValue) {
            return compare<T, T, T>(actual, actualValue, expected, expectedValue);
        }

        /* Compare two different types without explicit type specification while
           type of `actual` is convertible to type of `expected`, thus it is
           converted */
        template<class T, class U> inline typename std::enable_if<std::is_convertible<T, U>::value, void>::type compare(const std::string& actual, const T& actualValue, const std::string& expected, const U& expectedValue) {
            return compare<U, T, U>(actual, actualValue, expected, expectedValue);
        }

        /* Compare two different types without explicit type specification while
           type of `actual` is NOT convertible to type of `expected`, thus type
           of `expected` is converted to type of `actual` */
        template<class T, class U> inline typename std::enable_if<!std::is_convertible<T, U>::value, void>::type compare(const std::string& actual, const T& actualValue, const std::string& expected, const U& expectedValue) {
            return compare<T, T, U>(actual, actualValue, expected, expectedValue);
        }

        /* Compare two different types with explicit type specification */
        template<class T, class U, class V> void compare(const std::string& actual, const U& actualValue, const std::string& expected, const V& expectedValue) {
            Compare<T> compare;

            /* If the comparison succeeded or the failure is expected, done */
            bool equal = compare(actualValue, expectedValue);
            if(!expectedFailure) {
                if(equal) return;
            } else if(!equal) {
                Utility::Debug(logOutput) << " XFAIL:" << testCaseName << "at" << testFilename << "on line" << testCaseLine << "\n       " << expectedFailure->message() << actual << "and" << expected << "are not equal.";
                return;
            }

            /* Otherwise print message to error output and throw exception */
            Utility::Error e(errorOutput);
            e << (expectedFailure ? " XPASS:" : "  FAIL:") << testCaseName << "at" << testFilename << "on line" << testCaseLine << "\n       ";
            if(!expectedFailure) compare.printErrorMessage(e, actual, expected);
            else e << actual << "and" << expected << "are not expected to be equal.";
            throw Exception();
        }

        void verify(const std::string& expression, bool expressionValue) {
            /* If the expression is true or the failure is expected, done */
            if(!expectedFailure) {
                if(expressionValue) return;
            } else if(!expressionValue) {
                Utility::Debug(logOutput) << " XFAIL:" << testCaseName << "at" << testFilename << "on line" << testCaseLine << "\n       " << expectedFailure->message() << "Expression" << expression << "failed.";
                return;
            }

            /* Otherwise print message to error output and throw exception */
            Utility::Error e(errorOutput);
            e << (expectedFailure ? " XPASS:" : "  FAIL:") << testCaseName << "at" << testFilename << "on line" << testCaseLine << "\n        Expression" << expression;
            if(!expectedFailure) e << "failed.";
            else e << "was expected to fail.";
            throw Exception();
        }

        inline void registerTest(const std::string& filename, const std::string& name) {
            testFilename = filename;
            testName = name;
        }

    protected:
        class ExpectedFailure {
            public:
                inline ExpectedFailure(Tester* instance, const std::string& message): instance(instance), _message(message) {
                    instance->expectedFailure = this;
                }

                inline ~ExpectedFailure() { instance->expectedFailure = nullptr; }

                inline std::string message() const {
                    return _message;
                }

            private:
                Tester* instance;
                std::string _message;
        };

        inline void registerTestCase(const std::string& name, int line) {
            if(testCaseName.empty()) testCaseName = name + "()";
            this->testCaseLine = line;
        }
        #endif

    private:
        class Exception {};

        void addTests() {} /* Terminator function for addTests() */

        std::ostream *logOutput, *errorOutput;
        std::vector<std::function<void(Derived&)>> testCases;
        std::string testFilename, testName, testCaseName, expectFailMessage;
        size_t testCaseLine;
        ExpectedFailure* expectedFailure;
};

/** @hideinitializer
@brief Create `main()` function for given Tester subclass
*/
#define CORRADE_TEST_MAIN(Class)                                            \
    int main(int, char**) {                                                 \
        Class t;                                                            \
        t.registerTest(__FILE__, #Class);                                   \
        return t.exec();                                                    \
    }

#ifndef DOXYGEN_GENERATING_OUTPUT
#define _CORRADE_REGISTER_TEST_CASE()                                       \
    registerTestCase(__func__, __LINE__);
#endif

/** @hideinitializer
@brief Verify an expression in Tester subclass

If the expression is not true, the expression is printed and execution of given
test case is terminated. Example usage:
@code
string s("hello");
CORRADE_VERIFY(!s.empty());
@endcode

@see CORRADE_COMPARE(), CORRADE_COMPARE_AS()
*/
#define CORRADE_VERIFY(expression)                                          \
    do {                                                                    \
        _CORRADE_REGISTER_TEST_CASE();                                      \
        verify(#expression, expression);                                    \
    } while(false)

/** @hideinitializer
@brief Compare two values in Tester subclass

If the values are not the same, they are printed for comparison and execution
of given test case is terminated. Example usage:
@code
int a = 5 + 3;
CORRADE_COMPARE(a, 8);
@endcode

@see CORRADE_VERIFY(), CORRADE_COMPARE_AS()
*/
#define CORRADE_COMPARE(actual, expected)                                   \
    do {                                                                    \
        _CORRADE_REGISTER_TEST_CASE();                                      \
        compare(#actual, actual, #expected, expected);                      \
    } while(false)

/** @hideinitializer
@brief Compare two values in Tester subclass with explicitly specified type

If the values are not the same, they are printed for comparison and execution
of given test case is terminated. Example usage:
@code
CORRADE_COMPARE_AS(sin(0.0f), 0.0f, float);
@endcode
See also @ref Corrade::TestSuite::Compare "Compare" class documentation for
example of more involved comparisons.

@see CORRADE_VERIFY(), CORRADE_COMPARE()
*/
#define CORRADE_COMPARE_AS(actual, expected, Type)                          \
    do {                                                                    \
        _CORRADE_REGISTER_TEST_CASE();                                      \
        compare<Type>(#actual, actual, #expected, expected);                \
    } while(false)

/** @hideinitializer
@brief Expect failure in all following checks in the same scope
@param message Message which will be printed into output as indication of
    expected failure

Expects failure in all following CORRADE_VERIFY(), CORRADE_COMPARE() and
CORRADE_COMPARE_AS() checks in the same scope. In most cases it will be until
the end of the function, but you can limit the scope by placing relevant
checks in a separate block:
@code
{
    CORRADE_EXPECT_FAIL("Not implemented");
    CORRADE_VERIFY(isFutureClear());
}

int i = 6*7;
CORRADE_COMPARE(i, 42);
@endcode
If any of the following checks passes, an error will be printed to output.
*/
#define CORRADE_EXPECT_FAIL(message)                                        \
    ExpectedFailure expectedFailure##__LINE__(this, message)

}}

#endif
