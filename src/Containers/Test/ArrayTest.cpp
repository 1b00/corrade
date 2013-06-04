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

#include "Containers/Array.h"
#include "TestSuite/Tester.h"

namespace Corrade { namespace Containers { namespace Test {

class ArrayTest: public TestSuite::Tester {
    public:
        explicit ArrayTest();

        void constructEmpty();
        void construct();
        void constructMove();
        void emptyCheck();
        void access();
        void rangeBasedFor();
};

typedef Containers::Array<int> Array;

ArrayTest::ArrayTest() {
    addTests({&ArrayTest::constructEmpty,
              &ArrayTest::construct,
              &ArrayTest::constructMove,
              &ArrayTest::emptyCheck,
              &ArrayTest::access,
              &ArrayTest::rangeBasedFor});
}

void ArrayTest::constructEmpty() {
    const Array a;
    CORRADE_VERIFY(a == nullptr);
    CORRADE_COMPARE(a.size(), 0);

    /* Zero-length should not call new */
    const Array b(0);
    CORRADE_VERIFY(b == nullptr);
    CORRADE_COMPARE(b.size(), 0);
}

void ArrayTest::construct() {
    const Array a(5);
    CORRADE_VERIFY(a != nullptr);
    CORRADE_COMPARE(a.size(), 5);
}

void ArrayTest::constructMove() {
    Array a(5);
    CORRADE_VERIFY(a);
    const int* const ptr = a;

    Array b(std::move(a));
    CORRADE_VERIFY(a == nullptr);
    CORRADE_VERIFY(b == ptr);
    CORRADE_COMPARE(a.size(), 0);
    CORRADE_COMPARE(b.size(), 5);

    Array c;
    c = std::move(b);
    CORRADE_VERIFY(b == nullptr);
    CORRADE_VERIFY(c == ptr);
    CORRADE_COMPARE(b.size(), 0);
    CORRADE_COMPARE(c.size(), 5);
}

void ArrayTest::emptyCheck() {
    Array a;
    CORRADE_VERIFY(!a);
    CORRADE_VERIFY(a.empty());

    Array b(5);
    CORRADE_VERIFY(b);
    CORRADE_VERIFY(!b.empty());
}

void ArrayTest::access() {
    Array a(7);
    for(std::size_t i = 0; i != 7; ++i)
        a[i] = i;

    CORRADE_COMPARE(*(a.begin()+2), 2);
    CORRADE_COMPARE(a[4], 4);
    CORRADE_COMPARE(a.end()-a.begin(), a.size());
}

void ArrayTest::rangeBasedFor() {
    #ifndef CORRADE_GCC45_COMPATIBILITY
    Array a(5);
    for(auto& i: a)
        i = 3;

    CORRADE_COMPARE(a[0], 3);
    CORRADE_COMPARE(a[1], 3);
    CORRADE_COMPARE(a[2], 3);
    CORRADE_COMPARE(a[3], 3);
    CORRADE_COMPARE(a[4], 3);
    #else
    CORRADE_SKIP("Range-based for is not available in GCC 4.5");
    #endif
}

}}}

CORRADE_TEST_MAIN(Corrade::Containers::Test::ArrayTest)
