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

#include "AbstractManager.h"

#include <algorithm>
#include <sstream>

#ifndef _WIN32
#ifndef CORRADE_TARGET_NACL_NEWLIB
#include <dlfcn.h>
#endif
#else
#include <windows.h>
#undef interface
#define dlsym GetProcAddress
#define dlerror GetLastError
#define dlclose FreeLibrary
#endif

#include "Utility/Assert.h"
#include "Utility/Directory.h"
#include "Utility/Configuration.h"
#include "PluginManager/AbstractPlugin.h"

#include "corradePluginManagerConfigure.h"

using namespace Corrade::Utility;

namespace Corrade { namespace PluginManager {

const int AbstractManager::Version = CORRADE_PLUGIN_VERSION;

std::map<std::string, AbstractManager::Plugin*>* AbstractManager::plugins() {
    static std::map<std::string, Plugin*>* const _plugins = new std::map<std::string, Plugin*>();

    /* If there are unprocessed static plugins for this manager, add them */
    if(staticPlugins()) {
        for(auto it = staticPlugins()->begin(); it != staticPlugins()->end(); ++it) {
            StaticPlugin* const staticPlugin = *it;

            /* Load static plugin metadata */
            Resource r("CorradeStaticPlugin_" + staticPlugin->plugin);
            std::istringstream metadata(r.get(staticPlugin->plugin + ".conf"));

            /* Insert plugin to list */
            CORRADE_INTERNAL_ASSERT_OUTPUT(_plugins->insert(std::make_pair(staticPlugin->plugin, new Plugin(metadata, staticPlugin))).second);
        }

        /** @todo Assert dependencies of static plugins */

        /* Delete the array to mark them as processed */
        delete staticPlugins();
        staticPlugins() = nullptr;
    }

    return _plugins;
}

std::vector<AbstractManager::StaticPlugin*>*& AbstractManager::staticPlugins() {
    static std::vector<StaticPlugin*>* _staticPlugins = new std::vector<StaticPlugin*>();

    return _staticPlugins;
}

#ifndef DOXYGEN_GENERATING_OUTPUT
void AbstractManager::importStaticPlugin(const std::string& plugin, int _version, const std::string& interface, Instancer instancer, void(*initializer)(), void(*finalizer)()) {
    CORRADE_ASSERT(_version == Version,
        "PluginManager: wrong version of static plugin" << plugin + ", got" << _version << "but expected" << Version, );
    CORRADE_ASSERT(staticPlugins(),
        "PluginManager: too late to import static plugin" << plugin, );

    staticPlugins()->push_back(new StaticPlugin{plugin, interface, instancer, initializer, finalizer});
}
#endif

#ifndef CORRADE_TARGET_NACL_NEWLIB
AbstractManager::AbstractManager(std::string pluginDirectory) {
    setPluginDirectory(std::move(pluginDirectory));
}
#else
AbstractManager::AbstractManager(std::string) {}
#endif

AbstractManager::~AbstractManager() {
    /* Unload all plugins associated with this plugin manager */
    #ifndef CORRADE_TARGET_NACL_NEWLIB
    std::vector<std::map<std::string, Plugin*>::iterator> removed;
    #endif
    for(auto it = plugins()->begin(); it != plugins()->end(); ++it) {

        /* Plugin doesn't belong to this manager */
        if(it->second->manager != this) continue;

        /**
         * @bug When two plugins depend on each other and the base is unloaded
         *      first, it fails (but it shouldn't)
         */

        #ifndef CORRADE_TARGET_NACL_NEWLIB
        /* Unload the plugin */
        LoadState loadState = unload(it->first);
        CORRADE_ASSERT(loadState & (LoadState::Static|LoadState::NotLoaded|LoadState::WrongMetadataFile),
            "PluginManager: cannot unload plugin" << it->first << "on manager destruction:" << loadState, );

        /* Schedule it for deletion, if it is not static, otherwise just
           disconnect this manager from the plugin and finalize it, so another
           manager can take over it in the future. */
        if(loadState == LoadState::Static) {
        #endif
            it->second->manager = nullptr;
            it->second->staticPlugin->finalizer();
        #ifndef CORRADE_TARGET_NACL_NEWLIB
        } else
            removed.push_back(it);
        #endif
    }

    #ifndef CORRADE_TARGET_NACL_NEWLIB
    /* Remove the plugins from global container */
    for(auto it = removed.cbegin(); it != removed.cend(); ++it) {
        delete (*it)->second;
        plugins()->erase(*it);
    }
    #endif
}

#ifndef CORRADE_TARGET_NACL_NEWLIB
std::string AbstractManager::pluginDirectory() const {
    return _pluginDirectory;
}

void AbstractManager::setPluginDirectory(std::string directory) {
    _pluginDirectory = std::move(directory);

    /* Remove all unloaded plugins from the container */
    auto it = plugins()->begin();
    while(it != plugins()->end()) {
        if(it->second->manager == this && it->second->loadState & (LoadState::NotLoaded|LoadState::WrongMetadataFile)) {
            delete it->second;

            #ifndef CORRADE_GCC44_COMPATIBILITY
            it = plugins()->erase(it);
            #else
            /* GCC 4.4 returns void from map::erase(), but other iterators
               aren't invalidated, so it's safe */
            auto erase = it;
            ++it;
            plugins()->erase(erase);
            #endif

        } else ++it;
    }

    /* Find plugin files in the directory */
    static const std::size_t suffixSize = std::strlen(PLUGIN_FILENAME_SUFFIX);
    const std::vector<std::string> d = Directory::list(_pluginDirectory,
        Directory::Flag::SkipDirectories|Directory::Flag::SkipDotAndDotDot);
    for(auto it = d.cbegin(); it != d.cend(); ++it) {
        const std::string& filename = *it;
        /* File doesn't have module suffix, continue to next */
        const std::size_t end = filename.length()-suffixSize;
        if(filename.substr(end) != PLUGIN_FILENAME_SUFFIX)
            continue;

        /* Dig plugin name from filename */
        const std::string name = filename.substr(0, end);

        /* Skip the plugin if it is among loaded */
        if(plugins()->find(name) != plugins()->end()) continue;

        /* Insert plugin to list */
        plugins()->insert({name, new Plugin(Directory::join(_pluginDirectory, name + ".conf"), this)});
    }
}

void AbstractManager::reloadPluginDirectory() {
    setPluginDirectory(pluginDirectory());
}
#endif

std::vector<std::string> AbstractManager::pluginList() const {
    std::vector<std::string> names;
    for(auto i = plugins()->cbegin(); i != plugins()->cend(); ++i) {

        /* Plugin doesn't belong to this manager */
        if(i->second->manager != this) continue;

        names.push_back(i->first);
    }
    return names;
}

const PluginMetadata* AbstractManager::metadata(const std::string& plugin) const {
    auto foundPlugin = plugins()->find(plugin);

    /* Given plugin doesn't exist or doesn't belong to this manager, nothing to do */
    if(foundPlugin == plugins()->end() || foundPlugin->second->manager != this)
        return nullptr;

    return &foundPlugin->second->metadata;
}

LoadState AbstractManager::loadState(const std::string& plugin) const {
    auto foundPlugin = plugins()->find(plugin);

    /* Given plugin doesn't exist or doesn't belong to this manager, nothing to do */
    if(foundPlugin == plugins()->end() || foundPlugin->second->manager != this)
        return LoadState::NotFound;

    return foundPlugin->second->loadState;
}

LoadState AbstractManager::load(const std::string& plugin) {
    auto foundPlugin = plugins()->find(plugin);

    /* Given plugin doesn't exist or doesn't belong to this manager, nothing to do */
    if(foundPlugin == plugins()->end() || foundPlugin->second->manager != this)
        return LoadState::NotFound;

    Plugin& pluginObject = *foundPlugin->second;

    #ifdef CORRADE_TARGET_NACL_NEWLIB
    return pluginObject.loadState;
    #else
    /* Plugin is not ready to load */
    if(pluginObject.loadState != LoadState::NotLoaded)
        return pluginObject.loadState;

    /* Vector of found dependencies. If everything goes well, this plugin will
       be added to each dependency usedBy list. */
    std::vector<std::pair<std::string, Plugin*>> dependencies;

    /* Load dependencies and remember their names for later */
    for(auto it = pluginObject.metadata.depends().cbegin(); it != pluginObject.metadata.depends().cend(); ++it) {
        /* Find manager which is associated to this plugin and load the plugin
           with it */
        auto foundDependency = plugins()->find(*it);

        if(foundDependency == plugins()->end() || !foundDependency->second->manager || !(foundDependency->second->manager->load(*it) & LoadState::Loaded))
            return LoadState::UnresolvedDependency;

        dependencies.push_back(*foundDependency);
    }

    const std::string filename = Directory::join(_pluginDirectory, plugin + PLUGIN_FILENAME_SUFFIX);

    /* Open plugin file, make symbols available for next libs (which depends on this) */
    #ifndef _WIN32
    void* module = dlopen(filename.c_str(), RTLD_NOW|RTLD_GLOBAL);
    #else
    HMODULE module = LoadLibraryA(filename.c_str());
    #endif
    if(!module) {
        Error() << "PluginManager: cannot open plugin file"
                << '"' + filename + "\":" << dlerror();
        return LoadState::LoadFailed;
    }

    /* Check plugin version */
    #ifdef __GNUC__ /* http://www.mr-edd.co.uk/blog/supressing_gcc_warnings */
    __extension__
    #endif
    int (*_version)(void) = reinterpret_cast<int(*)()>(dlsym(module, "pluginVersion"));
    if(_version == nullptr) {
        Error() << "PluginManager: cannot get version of plugin" << '\'' + plugin + "':" << dlerror();
        dlclose(module);
        return LoadState::LoadFailed;
    }
    if(_version() != Version) {
        Error() << "PluginManager: wrong plugin version, expected" << Version << "but got" << _version();
        dlclose(module);
        return LoadState::WrongPluginVersion;
    }

    /* Check interface string */
    #ifdef __GNUC__ /* http://www.mr-edd.co.uk/blog/supressing_gcc_warnings */
    __extension__
    #endif
    const char* (*interface)() = reinterpret_cast<const char* (*)()>(dlsym(module, "pluginInterface"));
    if(interface == nullptr) {
        Error() << "PluginManager: cannot get interface string of plugin" << '\'' + plugin + "':" << dlerror();
        dlclose(module);
        return LoadState::LoadFailed;
    }
    if(interface() != pluginInterface()) {
        Error() << "PluginManager: wrong interface version, expected" << '\'' + pluginInterface() + '\'' << "but got" << '\'' + std::string(interface()) + '\'';
        dlclose(module);
        return LoadState::WrongInterfaceVersion;
    }

    /* Load plugin instancer */
    #ifdef __GNUC__ /* http://www.mr-edd.co.uk/blog/supressing_gcc_warnings */
    __extension__
    #endif
    Instancer instancer = reinterpret_cast<Instancer>(dlsym(module, "pluginInstancer"));
    if(instancer == nullptr) {
        Error() << "PluginManager: cannot get instancer of plugin" << '\'' + plugin + "':" << dlerror();
        dlclose(module);
        return LoadState::LoadFailed;
    }

    /* Initialize plugin */
    #ifdef __GNUC__ /* http://www.mr-edd.co.uk/blog/supressing_gcc_warnings */
    __extension__
    #endif
    void(*initializer)() = reinterpret_cast<void(*)()>(dlsym(module, "pluginInitializer"));
    if(initializer == nullptr) {
        Error() << "PluginManager: cannot get initializer of plugin" << '\'' + plugin + "':" << dlerror();
        dlclose(module);
        return LoadState::LoadFailed;
    }
    initializer();

    /* Everything is okay, add this plugin to usedBy list of each dependency */
    for(auto it = dependencies.cbegin(); it != dependencies.cend(); ++it) {
        /* If the plugin is not static with no associated manager, use its
           manager for adding this plugin */
        if(it->second->manager)
            it->second->manager->addUsedBy(it->first, plugin);

        /* Otherwise add this plugin manually */
        else it->second->metadata._usedBy.push_back(plugin);
    }

    /* Update plugin object, set state to loaded */
    pluginObject.loadState = LoadState::Loaded;
    pluginObject.module = module;
    pluginObject.instancer = instancer;
    return LoadState::Loaded;
    #endif
}

LoadState AbstractManager::unload(const std::string& plugin) {
    auto foundPlugin = plugins()->find(plugin);

    /* Given plugin doesn't exist or doesn't belong to this manager, nothing to do */
    if(foundPlugin == plugins()->end() || foundPlugin->second->manager != this)
        return LoadState::NotFound;

    Plugin& pluginObject = *foundPlugin->second;

    #ifdef CORRADE_TARGET_NACL_NEWLIB
    return pluginObject.loadState;
    #else
    /* Plugin is not ready to unload, nothing to do */
    if(pluginObject.loadState != LoadState::Loaded)
        return pluginObject.loadState;

    /* Plugin is used by another plugin, don't unload */
    if(!pluginObject.metadata.usedBy().empty())
        return LoadState::Required;

    /* Plugin has active instances */
    auto foundInstance = instances.find(plugin);
    if(foundInstance != instances.end()) {
        /* Check if all instances can be safely deleted */
        for(auto it = foundInstance->second.cbegin(); it != foundInstance->second.cend(); ++it)
            if(!(*it)->canBeDeleted())
                return LoadState::Used;

        /* If they can be, delete them. They remove itself from instances
           list on destruction, thus going backwards */
        for(std::size_t i = foundInstance->second.size(); i != 0; --i)
            delete foundInstance->second[i-1];
    }

    /* Remove this plugin from "used by" list of dependencies */
    for(auto it = pluginObject.metadata.depends().cbegin(); it != pluginObject.metadata.depends().cend(); ++it) {
        auto mit = plugins()->find(*it);

        if(mit != plugins()->end()) {
            /* If the plugin is not static with no associated manager, use
               its manager for removing this plugin */
            if(mit->second->manager)
                mit->second->manager->removeUsedBy(mit->first, plugin);

            /* Otherwise remove this plugin manually */
            else for(auto it = mit->second->metadata._usedBy.begin(); it != mit->second->metadata._usedBy.end(); ++it) {
                if(*it == plugin) {
                    mit->second->metadata._usedBy.erase(it);
                    break;
                }
            }
        }
    }

    /* Finalize plugin */
    #ifdef __GNUC__ /* http://www.mr-edd.co.uk/blog/supressing_gcc_warnings */
    __extension__
    #endif
    void(*finalizer)() = reinterpret_cast<void(*)()>(dlsym(pluginObject.module, "pluginFinalizer"));
    if(finalizer == nullptr) {
        Error() << "PluginManager: cannot get finalizer of plugin" << '\'' + plugin + "':" << dlerror();
        /* Not fatal, continue with unloading */
    }
    finalizer();

    /* Close the module */
    #ifndef _WIN32
    if(dlclose(pluginObject.module) != 0) {
    #else
    if(!FreeLibrary(pluginObject.module)) {
    #endif
        Error() << "PluginManager: cannot unload plugin" << '\'' + plugin + "':" << dlerror();
        pluginObject.loadState = LoadState::NotLoaded;
        return LoadState::UnloadFailed;
    }

    /* Update plugin object, set state to not loaded */
    pluginObject.loadState = LoadState::NotLoaded;
    pluginObject.module = nullptr;
    pluginObject.instancer = nullptr;
    return LoadState::NotLoaded;
    #endif
}

void AbstractManager::registerInstance(std::string plugin, AbstractPlugin* instance, const Configuration** configuration, const PluginMetadata** metadata) {
    /** @todo assert proper interface */
    auto foundPlugin = plugins()->find(plugin);

    /* Given plugin doesn't exist or doesn't belong to this manager, nothing to do */
    if(foundPlugin == plugins()->end() || foundPlugin->second->manager != this)
        return;

    auto foundInstance = instances.find(plugin);

    if(foundInstance == instances.end())
        foundInstance = instances.insert({std::move(plugin), {}}).first;

    foundInstance->second.push_back(instance);

    *configuration = &foundPlugin->second->configuration;
    *metadata = &foundPlugin->second->metadata;
}

void AbstractManager::unregisterInstance(const std::string& plugin, AbstractPlugin* instance) {
    auto foundPlugin = plugins()->find(plugin);

    /* Given plugin doesn't exist or doesn't belong to this manager, nothing to do */
    if(foundPlugin == plugins()->end() || foundPlugin->second->manager != this)
        return;

    auto foundInstance = instances.find(plugin);
    if(foundInstance == instances.end()) return;
    std::vector<AbstractPlugin*>& _instances = foundInstance->second;

    auto pos = std::find(_instances.begin(), _instances.end(), instance);
    if(pos == _instances.end()) return;

    _instances.erase(pos);

    if(_instances.empty()) instances.erase(plugin);
}

void AbstractManager::addUsedBy(const std::string& plugin, std::string usedBy) {
    auto foundPlugin = plugins()->find(plugin);

    /* Given plugin doesn't exist, nothing to do */
    if(foundPlugin == plugins()->end()) return;

    foundPlugin->second->metadata._usedBy.push_back(std::move(usedBy));
}

void AbstractManager::removeUsedBy(const std::string& plugin, const std::string& usedBy) {
    auto foundPlugin = plugins()->find(plugin);

    /* Given plugin doesn't exist, nothing to do */
    if(foundPlugin == plugins()->end()) return;

    for(auto it = foundPlugin->second->metadata._usedBy.begin(); it != foundPlugin->second->metadata._usedBy.end(); ++it) {
        if(*it == usedBy) {
            foundPlugin->second->metadata._usedBy.erase(it);
            return;
        }
    }
}

void* AbstractManager::instanceInternal(const std::string& plugin) {
    auto foundPlugin = plugins()->find(plugin);

    /* Plugin with given name doesn't exist or isn't successfully loaded */
    #ifndef CORRADE_TARGET_NACL_NEWLIB
    if(foundPlugin == plugins()->end() || !(foundPlugin->second->loadState & LoadState::Loaded))
    #else
    if(foundPlugin == plugins()->end())
    #endif
        return nullptr;

    return foundPlugin->second->instancer(this, plugin);
}

#ifndef CORRADE_TARGET_NACL_NEWLIB
AbstractManager::Plugin::Plugin(const std::string& _metadata, AbstractManager* _manager): configuration(_metadata, Utility::Configuration::Flag::ReadOnly), metadata(configuration), manager(_manager), instancer(nullptr), module(nullptr) {
    loadState = configuration.isValid() ? LoadState::NotLoaded : LoadState::WrongMetadataFile;
}
#endif

AbstractManager::Plugin::Plugin(std::istream& _metadata, StaticPlugin* staticPlugin): loadState(LoadState::Static), configuration(_metadata, Utility::Configuration::Flag::ReadOnly), metadata(configuration), manager(nullptr), instancer(staticPlugin->instancer), staticPlugin(staticPlugin) {}

AbstractManager::Plugin::~Plugin() {
    #ifndef CORRADE_TARGET_NACL_NEWLIB
    if(loadState == LoadState::Static)
    #endif
        delete staticPlugin;
}

} namespace Utility {

#ifndef DOXYGEN_GENERATING_OUTPUT
Debug operator<<(Debug debug, PluginManager::LoadState value) {
    switch(value) {
        #define ls(state) case PluginManager::LoadState::state: return debug << "PluginManager::LoadState::" #state;
        ls(NotFound)
        #ifndef CORRADE_TARGET_NACL_NEWLIB
        ls(WrongPluginVersion)
        ls(WrongInterfaceVersion)
        ls(WrongMetadataFile)
        ls(UnresolvedDependency)
        ls(LoadFailed)
        ls(Loaded)
        ls(NotLoaded)
        ls(UnloadFailed)
        ls(Required)
        #endif
        ls(Static)
        #ifndef CORRADE_TARGET_NACL_NEWLIB
        ls(Used)
        #endif
        #undef ls
    }

    return debug << "PluginManager::LoadState::(invalid)";
}
#endif

}}
