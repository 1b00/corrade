/*
    This file is part of Corrade.

    Copyright © 2007, 2008, 2009, 2010, 2011, 2012, 2013
              Vladimír Vondruš <mosra@centrum.cz>

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#include <cctype>

#include "TestSuite/Tester.h"
#include "TestSuite/Compare/Container.h"
#include "Utility/String.h"

namespace Corrade { namespace Utility { namespace Test {

class StringTest: public TestSuite::Tester {
    public:
        StringTest();

        void trim();
        void split();
        void lowercase();
        void uppercase();
        void whitespace();
};

StringTest::StringTest() {
    addTests<StringTest>({&StringTest::trim,
              &StringTest::split,
              &StringTest::lowercase,
              &StringTest::uppercase,
              &StringTest::whitespace});
}

void StringTest::trim() {
    /* Spaces at the end */
    CORRADE_COMPARE(String::ltrim("abc  "), "abc  ");
    CORRADE_COMPARE(String::rtrim("abc  "), "abc");

    /* Spaces at the beginning */
    CORRADE_COMPARE(String::ltrim("  abc"), "abc");
    CORRADE_COMPARE(String::rtrim("  abc"), "  abc");

    /* Spaces on both beginning and end */
    CORRADE_COMPARE(String::trim("  abc  "), "abc");

    /* No spaces */
    CORRADE_COMPARE(String::trim("abc"), "abc");

    /* All spaces */
    CORRADE_COMPARE(String::trim("\t\r\n\f\v "), "");

    /* Special characters */
    CORRADE_COMPARE(String::trim("ouya", "aeiyou"), "");
}

void StringTest::split() {
    /* No delimiters */
    CORRADE_COMPARE_AS(String::split("abcdef", '/'),
        std::vector<std::string>{"abcdef"}, TestSuite::Compare::Container);

    /* Common case */
    CORRADE_COMPARE_AS(String::split("ab/c/def", '/'),
        (std::vector<std::string>{"ab", "c", "def"}), TestSuite::Compare::Container);

    /* Empty parts */
    CORRADE_COMPARE_AS(String::split("ab//c/def//", '/'),
        (std::vector<std::string>{"ab", "", "c", "def", "", ""}), TestSuite::Compare::Container);

    /* Skip empty parts */
    CORRADE_COMPARE_AS(String::split("ab//c/def//", '/', false),
        (std::vector<std::string>{"ab", "c", "def"}), TestSuite::Compare::Container);
}

void StringTest::lowercase() {
    /* Lowecase */
    CORRADE_COMPARE(String::lowercase("hello"), "hello");

    /* Uppercase */
    CORRADE_COMPARE(String::lowercase("QWERTZUIOP"), "qwertzuiop");

    /* Special chars */
    CORRADE_COMPARE(String::lowercase(".,?- \"!/(98765%"), ".,?- \"!/(98765%");

    /* UTF-8 */
    CORRADE_EXPECT_FAIL("UTF-8 lowercasing is not supported.");
    CORRADE_COMPARE(String::lowercase("ĚŠČŘŽÝÁÍÉÚŮĎŤŇ"), "ěščřžýáíéúůďťň");
}

void StringTest::uppercase() {
    /* Lowecase */
    CORRADE_COMPARE(String::uppercase("hello"), "HELLO");

    /* Uppercase */
    CORRADE_COMPARE(String::uppercase("QWERTZUIOP"), "QWERTZUIOP");

    /* Special chars */
    CORRADE_COMPARE(String::uppercase(".,?- \"!/(98765%"), ".,?- \"!/(98765%");

    /* UTF-8 */
    CORRADE_EXPECT_FAIL("UTF-8 uppercasing is not supported.");
    CORRADE_COMPARE(String::uppercase("ěščřžýáíéúůďťň"), "ĚŠČŘŽÝÁÍÉÚŮĎŤŇ");
}

void StringTest::whitespace() {
    for(auto it = String::Whitespace.begin(); it != String::Whitespace.end(); ++it)
        CORRADE_VERIFY(std::isspace(*it));
}

}}}

CORRADE_TEST_MAIN(Corrade::Utility::Test::StringTest)
