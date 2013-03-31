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

#include "Resource.h"

#include <sstream>
#include <tuple>
#include <vector>

#include "Debug.h"

namespace Corrade { namespace Utility {

std::map<std::string, std::map<std::string, Resource::ResourceData>>& Resource::resources() {
    static std::map<std::string, std::map<std::string, Resource::ResourceData>> resources;
    return resources;
}

void Resource::registerData(const char* group, unsigned int count, const unsigned char* positions, const unsigned char* filenames, const unsigned char* data) {
    if(resources().find(group) == resources().end()) resources().insert(std::make_pair(group, std::map<std::string, ResourceData>()));

    /* Cast to type which can be eaten by std::string constructor */
    const char* _positions = reinterpret_cast<const char*>(positions);
    const char* _filenames = reinterpret_cast<const char*>(filenames);

    unsigned int size = sizeof(unsigned int);
    unsigned int oldFilenamePosition = 0, oldDataPosition = 0;

    /* Every 2*sizeof(unsigned int) is one data */
    for(unsigned int i = 0; i != count*2*size; i=i+2*size) {
        unsigned int filenamePosition = numberFromString<unsigned int>(std::string(_positions+i, size));
        unsigned int dataPosition = numberFromString<unsigned int>(std::string(_positions+i+size, size));

        ResourceData res;
        res.data = data;
        res.position = oldDataPosition;
        res.size = dataPosition-oldDataPosition;

        std::string filename = std::string(_filenames+oldFilenamePosition, filenamePosition-oldFilenamePosition);
        resources()[group].insert(std::make_pair(filename, res));

        oldFilenamePosition = filenamePosition;
        oldDataPosition = dataPosition;
    }
}

void Resource::unregisterData(const char* group, const unsigned char* data) {
    if(resources().find(group) == resources().end()) return;

    /* Positions which to remove */
    std::vector<std::string> positions;

    for(auto it = resources()[group].begin(); it != resources()[group].end(); ++it) {
        if(it->second.data == data)
            positions.push_back(it->first);
    }

    /** @todo wtf? this doesn't crash?? */
    for(auto it = positions.cbegin(); it != positions.cend(); ++it)
        resources()[group].erase(*it);

    if(resources()[group].empty()) resources().erase(group);
}

std::string Resource::compile(const std::string& name, const std::map<std::string, std::string>& files) const {
    std::string positions, filenames, data;
    unsigned int filenamesLen = 0, dataLen = 0;

    /* Convert data to hexacodes */
    for(auto it = files.cbegin(); it != files.cend(); ++it) {
        filenamesLen += it->first.size();
        dataLen += it->second.size();

        positions += hexcode(numberToString(filenamesLen));
        positions += hexcode(numberToString(dataLen));

        filenames += hexcode(it->first, it->first);
        data += hexcode(it->second, it->first);
    }

    /* Remove last comma from data */
    positions = positions.substr(0, positions.size()-2);
    filenames = filenames.substr(0, filenames.size()-2);
    data = data.substr(0, data.size()-2);

    /* Resource count */
    std::ostringstream count;
    count << files.size();

    /* Return C++ file. The functions have forward declarations to avoid warning
       about functions which don't have corresponding declarations (enabled by
       -Wmissing-declarations in GCC) */
    return "/* Compiled resource file. DO NOT EDIT! */\n\n"
        "#include \"Utility/utilities.h\"\n"
        "#include \"Utility/Resource.h\"\n\n"
        "static const unsigned char resourcePositions[] = {\n" +
        positions + "\n};\n\n"
        "static const unsigned char resourceFilenames[] = {\n" +
        filenames + "\n};\n\n"
        "static const unsigned char resourceData[] = {\n" +
        data +      "\n};\n\n"
        "int resourceInitializer_" + name + "();\n"
        "int resourceInitializer_" + name + "() {\n"
        "    Corrade::Utility::Resource::registerData(\"" + group + "\", " + count.str() + ", resourcePositions, resourceFilenames, resourceData);\n"
        "    return 1;\n"
        "} AUTOMATIC_INITIALIZER(resourceInitializer_" + name + ")\n\n"
        "int resourceFinalizer_" + name + "();\n"
        "int resourceFinalizer_" + name + "() {\n"
        "    Corrade::Utility::Resource::unregisterData(\"" + group + "\", resourceData);\n"
        "    return 1;\n"
        "} AUTOMATIC_FINALIZER(resourceFinalizer_" + name + ")\n";
}

std::string Resource::compile(const std::string& name, const std::string& filename, const std::string& data) const {
    std::map<std::string, std::string> files;
    files.insert(std::make_pair(filename, data));
    return compile(name, files);
}

std::tuple<const unsigned char*, unsigned int> Resource::getRaw(const std::string& filename) const {
    /* If the group/filename doesn't exist, return empty string */
    if(resources().find(group) == resources().end()) {
        Error() << "Resource: group" << '\'' + group + '\'' << "was not found";
        return {};
    } else if(resources()[group].find(filename) == resources()[group].end()) {
        Error() << "Resource: file" << '\'' + filename + '\'' << "was not found in group" << '\'' + group + '\'';
        return {};
    }

    const ResourceData& r = resources()[group][filename];
    return std::make_tuple(r.data+r.position, r.size);
}

std::string Resource::get(const std::string& filename) const {
    const unsigned char* data;
    unsigned int size;
    std::tie(data, size) = getRaw(filename);
    return data ? std::string(reinterpret_cast<const char*>(data), size) : std::string();
}

std::string Resource::hexcode(const std::string& data, const std::string& comment) const {
    /* Add comment, if set */
    std::string output = "    ";
    if(!comment.empty()) output = "\n    /* " + comment + " */\n" + output;

    int row_len = 4;
    for(unsigned int i = 0; i != data.size(); ++i) {

        /* Every row is indented by four spaces and is max 80 characters long */
        if(row_len > 74) {
            output += "\n    ";
            row_len = 4;
        }

        /* Convert char to hex */
        std::ostringstream converter;
        converter << std::hex;
        converter << static_cast<unsigned int>(static_cast<unsigned char>(data[i]));

        /* Append to output */
        output += "0x" + converter.str() + ",";
        row_len += 3+converter.str().size();
    }

    return output + '\n';
}

template<class T> std::string Resource::numberToString(const T& number) {
    return std::string(reinterpret_cast<const char*>(&number), sizeof(T));
}

template<class T> T Resource::numberFromString(const std::string& number) {
    return *reinterpret_cast<const T*>(number.c_str());
}

}}
