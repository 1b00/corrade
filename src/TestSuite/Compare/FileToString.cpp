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

#include "FileToString.h"

#include <fstream>

namespace Corrade { namespace TestSuite {

bool Comparator<Compare::FileToString>::operator()(const std::string& filename, const std::string& expectedContents) {
    this->filename = filename;

    std::ifstream in(filename);

    if(!in.good())
        return false;

    in.seekg(0, std::ios::end);
    actualContents.reserve(in.tellg());
    in.seekg(0, std::ios::beg);

    actualContents.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    this->expectedContents = expectedContents;
    state = State::Success;

    return actualContents == expectedContents;
}

void Comparator<Compare::FileToString>::printErrorMessage(Utility::Error& e, const std::string& actual, const std::string& expected) const {
    if(state != State::Success) {
        e << "File" << actual << '(' + filename + ')' << "cannot be read.";
        return;
    }

    e << "Files" << actual << "and" << expected << "have different";
    if(actualContents.size() != expectedContents.size())
        e << "size, actual" << actualContents.size() << "but" << expectedContents.size() << "expected.";
    else
        e << "contents.";

    for(std::size_t i = 0, end = std::max(actualContents.size(), expectedContents.size()); i != end; ++i) {
        if(actualContents.size() > i && expectedContents.size() > i && actualContents[i] == expectedContents[i]) continue;

        if(actualContents.size() <= i)
            e << "Expected has character" << expectedContents[i];
        else if(expectedContents.size() <= i)
            e << "Actual has character" << actualContents[i];
        else
            e << "Actual character" << actualContents[i] << "but" << expectedContents[i] << "expected";

        e << "on position" << i;
        e.setFlag(Utility::Debug::SpaceAfterEachValue, false);
        e << '.';
        e.setFlag(Utility::Debug::SpaceAfterEachValue, true);

        break;
    }

}

}}
