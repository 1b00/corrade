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

#include "Configuration.h"

#include <fstream>

#include "Debug.h"
#include "String.h"

using namespace std;

namespace Corrade { namespace Utility {

Configuration::Configuration(const string& filename, Flags flags): ConfigurationGroup(this), _filename(filename), flags(static_cast<InternalFlag>(std::uint32_t(flags))) {
    /* Open file with requested flags */
    ifstream::openmode openmode = ifstream::in;
    if(this->flags & InternalFlag::Truncate) openmode |= ifstream::trunc;
    ifstream file(filename.c_str(), openmode);

    /* File doesn't exist yet */
    if(!file.is_open()) {
        /** @todo check better */

        /* It is error for readonly configurations */
        /** @todo Similar check for istream constructor (?) */
        if(this->flags & InternalFlag::ReadOnly) return;

        this->flags |= InternalFlag::IsValid;
        return;
    }

    parse(file);

    /* Close file */
    file.close();
}

Configuration::Configuration(istream& file, Flags flags): ConfigurationGroup(this), flags(static_cast<InternalFlag>(std::uint32_t(flags))) {
    parse(file);

    /* Set readonly flag, because the configuration cannot be saved */
    this->flags |= InternalFlag::ReadOnly;
}

void Configuration::parse(istream& file) {
    try {
        if(!file.good())
            throw string("Cannot open configuration file.");

        /* It looks like BOM */
        if(file.peek() == String::Bom[0]) {
            char* bom = new char[4];
            file.get(bom, 4);

            /* This is not a BOM, rewind back */
            if(bom != String::Bom) file.seekg(0);

            /* Or set flag */
            else flags |= InternalFlag::HasBom;

            delete[] bom;
        }

        /* Parse file */
        parse(file, this, "");

        /* Everything went fine */
        flags |= InternalFlag::IsValid;

    } catch(string e) { Error() << e; }
}

string Configuration::parse(istream& file, ConfigurationGroup* group, const string& fullPath) {
    string buffer;

    /* Parse file */
    while(file.good()) {
        getline(file, buffer);

        /* Windows EOL */
        if(buffer[buffer.size()-1] == '\r')
            flags |= InternalFlag::WindowsEol;

        /* Trim buffer */
        buffer = String::trim(buffer);

        /* Group header */
        if(buffer[0] == '[') {

            /* Check ending bracket */
            if(buffer[buffer.size()-1] != ']')
                throw string("Missing closing bracket for group header!");

            string nextGroup = String::trim(buffer.substr(1, buffer.size()-2));

            if(nextGroup.empty())
                throw string("Empty group name!");

            /* Next group is subgroup of current group, recursive call */
            while(!nextGroup.empty() && (fullPath == "" || nextGroup.substr(0, fullPath.size()) == fullPath)) {
                ConfigurationGroup::Group g;
                g.name = nextGroup.substr(fullPath.size());
                g.group = new ConfigurationGroup(configuration);
                nextGroup = parse(file, g.group, nextGroup+'/');

                /* If unique groups are set, check whether current group is unique */
                bool save = true;
                if(flags & InternalFlag::UniqueGroups) {
                    /** @todo Do this in logarithmic time */
                    for(vector<Group>::const_iterator it = group->_groups.begin(); it != group->_groups.end(); ++it)
                        if(it->name == g.name) {
                            save = false;
                            break;
                        }
                }
                if(save) group->_groups.push_back(g);
            }

            return nextGroup;

        /* Empty line */
        } else if(buffer.size() == 0) {
            if(flags & (InternalFlag::SkipComments|InternalFlag::ReadOnly)) continue;

            group->items.push_back(ConfigurationGroup::Item());

        /* Comment */
        } else if(buffer[0] == '#' || buffer[0] == ';') {
            if(flags & (InternalFlag::SkipComments|InternalFlag::ReadOnly)) continue;

            ConfigurationGroup::Item item;
            item.value = buffer;
            group->items.push_back(item);

        /* Key/value pair */
        } else {
            size_t splitter = buffer.find_first_of('=');
            if(splitter == string::npos)
                throw string("Key/value pair without '=' character!");

            ConfigurationGroup::Item item;
            item.key = String::trim(buffer.substr(0, splitter));
            item.value = String::trim(buffer.substr(splitter+1));

            /* Remove quotes, if present */
            /** @todo Check `"` characters better */
            if(item.value.size() != 0 && item.value[0] == '"') {
                if(item.value.size() < 2 || item.value[item.value.size()-1] != '"')
                    throw string("Missing closing quotes in value!");

                item.value = item.value.substr(1, item.value.size()-2);
            }

            /* If unique keys are set, check whether current key is unique */
            if(flags & InternalFlag::UniqueKeys) {
                bool contains = false;
                for(vector<ConfigurationGroup::Item>::const_iterator it = group->items.begin(); it != group->items.end(); ++it)
                    if(it->key == item.key) {
                        contains = true;
                        break;
                    }
                if(contains) continue;
            }

            group->items.push_back(item);
        }
    }

    /* Remove last empty line, if present (will be written automatically) */
    if(group->items.size() != 0 && group->items[group->items.size()-1].key == "" && group->items[group->items.size()-1].value == "")
        group->items.pop_back();

    /* This was the last group */
    return "";
}

bool Configuration::save() {
    /* File is readonly or invalid, don't save anything */
    if(flags & InternalFlag::ReadOnly || !(flags & InternalFlag::IsValid)) return false;

    ofstream file(_filename.c_str(), ofstream::out|ofstream::trunc|ofstream::binary);
    if(!file.good()) {
        /** @todo Error to stderr */
        return false;
    }

    /* BOM, if user explicitly wants that crap */
    if((flags & InternalFlag::PreserveBom) && (flags & InternalFlag::HasBom))
        file.write(String::Bom.c_str(), 3);

    /* EOL character */
    string eol;
    if(flags & (InternalFlag::ForceWindowsEol|InternalFlag::WindowsEol) && !(flags & InternalFlag::ForceUnixEol)) eol = "\r\n";
    else eol = "\n";

    string buffer;

    /** @todo Checking file.good() after every operation */
    /** @todo Backup file */

    /* Recursively save all groups */
    save(file, eol, this, "");

    file.close();

    return true;
}

void Configuration::save(std::ofstream& file, const std::string& eol, ConfigurationGroup* group, const std::string& fullPath) const {
    string buffer;

    /* Foreach all items in the group */
    for(vector<ConfigurationGroup::Item>::const_iterator it = group->items.begin(); it != group->items.end(); ++it) {
        /* Key/value pair */
        if(!it->key.empty()) {
            if(it->value.find_first_of(String::Whitespace) != string::npos)
                buffer = it->key + "=\"" + it->value + '"' + eol;
            else
                buffer = it->key + '=' + it->value + eol;
        }

        /* Comment / empty line */
        else buffer = it->value + eol;

        file.write(buffer.c_str(), buffer.size());
    }

    /* Recursively process all subgroups */
    for(vector<Group>::const_iterator git = group->_groups.begin(); git != group->_groups.end(); ++git) {
        /* Subgroup name */
        string name = git->name;
        if(!fullPath.empty()) name = fullPath + '/' + name;

        buffer = '[' + name + ']' + eol;
        file.write(buffer.c_str(), buffer.size());

        save(file, eol, git->group, name);
    }
}

}}
