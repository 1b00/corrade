#ifndef Corrade_Utility_Resource_h
#define Corrade_Utility_Resource_h
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

/** @file
 * @brief Class Corrade::Utility::Resource
 */

#include <map>
#include <string>
#include <tuple>

#include "utilities.h"

namespace Corrade { namespace Utility {

/**
@brief Data resource management

This class provides support for compiled-in data resources - both compiling
and reading. Resources can be differentiated into more groups, every resource
in given group has unique filename.

See @ref resource-management for brief introduction and example usage.
Standalone resource compiler executable is implemented in @ref rc.cpp.

@todo Ad-hoc resources
@todo Test data unregistering
@todo Test empty files
 */
class CORRADE_UTILITY_EXPORT Resource {
    public:
        /**
         * @brief Register data resource
         * @param group         Group name
         * @param count         File count
         * @param positions     Positions of filenames and data in binary blobs
         * @param filenames     Pointer to binary blob with filenames
         * @param data          Pointer to binary blob with file data
         *
         * This function is used internally for automatic data resource
         * registering, no need to use it directly.
         */
        static void registerData(const char* group, unsigned int count, const unsigned char* positions, const unsigned char* filenames, const unsigned char* data);

        /**
         * @brief Unregister data resource
         * @param group         Group name
         * @param data          Pointer to binary blob with file data
         *
         * This function is used internally for automatic data resource
         * unregistering, no need to use it directly.
         */
        static void unregisterData(const char* group, const unsigned char* data);

        /**
         * @brief Constructor
         * @param _group        Group name for getting data or compiling new
         *      resources.
         */
        inline explicit Resource(const std::string& _group): group(_group) {}

        /**
         * @brief Compile data resource file
         * @param name          %Resource name (see RESOURCE_INITIALIZE())
         * @param files         Map with files, first item of pair is filename,
         *      second is file data.
         *
         * Produces C++ file with hexadecimally represented file data. The file
         * then must be compiled directly to executable or library.
         */
        std::string compile(const std::string& name, const std::map<std::string, std::string>& files) const;

        /**
         * @brief Compile data resource file
         * @param name          %Resource name (see RESOURCE_INITIALIZE())
         * @param filename      Filename
         * @param data          File data
         *
         * Convenience function for compiling resource with only one file.
         */
        std::string compile(const std::string& name, const std::string& filename, const std::string& data) const;

        /**
         * @brief Get pointer to raw resource data
         * @param filename      Filename
         *
         * Returns data of given group and filename as pair of pointer and
         * size. If not found, the pointer is `nullptr` and size is `0`.
         */
        std::tuple<const unsigned char*, unsigned int> getRaw(const std::string& filename) const;

        /**
         * @brief Get data resource
         * @param filename      Filename
         * @return Data of given group (specified in constructor) and filename.
         *      Returns empty string if nothing was found.
         */
        std::string get(const std::string& filename) const;

    private:
        struct CORRADE_UTILITY_LOCAL ResourceData {
            unsigned int position;
            unsigned int size;
            const unsigned char* data;
        };

        /* Accessed through function to overcome "static initialization order
           fiasco" which I think currently fails only in static build */
        CORRADE_UTILITY_LOCAL static std::map<std::string, std::map<std::string, ResourceData>>& resources();

        std::string group;

        CORRADE_UTILITY_LOCAL std::string hexcode(const std::string& data, const std::string& comment = std::string()) const;

        /** @todo Move to utilities.h? */
        template<class T> static std::string numberToString(const T& number);
        template<class T> static T numberFromString(const std::string& number);
};

/**
@brief Initialize resource

If a resource is compiled into dynamic library or directly into executable, it
will be initialized automatically thanks to AUTOMATIC_INITIALIZER() macros.
However, if the resource is compiled into static library, it must be explicitly
initialized via this macro, e.g. at the beginning of main(). You can also wrap
these macro calls into another function (which will then be compiled into
dynamic library or main executable) and use AUTOMATIC_INITIALIZER() macro for
automatic call.

@attention This macro should be called outside of any namespace. If you are
    running into linker errors with `resourceInitializer_*`, this could be the
    problem. If you are in a namespace and cannot call this macro from main(),
    try this:
@code
static void initialize() {
    RESOURCE_INITIALIZE(res)
}

namespace Foo {
    void bar() {
        initialize();

        //...
    }
}
@endcode
*/
#define RESOURCE_INITIALIZE(name)                                             \
    extern int resourceInitializer_##name();                                  \
    resourceInitializer_##name();

/**
@brief Cleanup resource

Cleans up previously (even automatically) initialized resource.

@attention This macro should be called outside of any namespace. See
    RESOURCE_INITIALIZE() documentation for more information.
*/
#define RESOURCE_CLEANUP(name)                                                \
    extern int resourceFinalizer_##name();                                    \
    resourceFinalizer_##name();

}}

#endif
