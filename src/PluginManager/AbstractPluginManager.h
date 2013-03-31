#ifndef Corrade_PluginManager_AbstractPluginManager_h
#define Corrade_PluginManager_AbstractPluginManager_h
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
 * @brief Class Corrade::PluginManager::AbstractPluginManager
 */

#include <vector>
#include <string>
#include <map>

#ifdef _WIN32
#include <windows.h>
#undef interface
#endif

#include "Containers/EnumSet.h"
#include "Utility/Resource.h"
#include "Utility/Debug.h"
#include "PluginMetadata.h"

namespace Corrade { namespace PluginManager {

class AbstractPlugin;

/** @relates AbstractPluginManager
@brief Plugin load state

@see LoadStates, AbstractPluginManager::loadState(),
    AbstractPluginManager::load(), AbstractPluginManager::unload(),
    AbstractPluginManager::reload().
*/
enum class LoadState: unsigned short {
    /**
     * The plugin cannot be found. Returned by AbstractPluginManager::loadState(),
     * AbstractPluginManager::load() and AbstractPluginManager::reload().
     */
    NotFound = 1 << 0,

    /**
     * The plugin is build with different version of plugin manager and cannot
     * be loaded. Returned by AbstractPluginManager::load() and
     * AbstractPluginManager::reload().
     */
    WrongPluginVersion = 1 << 1,

    /**
     * The plugin uses different interface than the interface used by plugin
     * manager and cannot be loaded. Returned by AbstractPluginManager::load()
     * and AbstractPluginManager::reload().
     */
    WrongInterfaceVersion = 1 << 2,

    /**
     * The plugin doesn't have any metadata file or the metadata file contains
     * errors. Returned by AbstractPluginManager::load() and
     * AbstractPluginManager::reload().
     */
    WrongMetadataFile = 1 << 3,

    /**
     * The plugin depends on another plugin, which cannot be loaded (e.g. not
     * found or wrong version). Returned by AbstractPluginManager::load() and
     * AbstractPluginManager::reload().
     */
    UnresolvedDependency = 1 << 4,

    /**
     * The plugin failed to load for other reason (e.g. linking failure).
     * Returned by AbstractPluginManager::load() and
     * AbstractPluginManager::reload().
     */
    LoadFailed = 1 << 5,

    /**
     * The plugin is successfully loaded. Returned by
     * AbstractPluginManager::loadState(), AbstractPluginManager::load() and
     * AbstractPluginManager::reload().
     */
    Loaded = 1 << 6,

    /**
     * The plugin is not loaded. Plugin can be unloaded only if is dynamic and
     * is not required by any other plugin. Returned by
     * AbstractPluginManager::loadState(), AbstractPluginManager::load() and
     * AbstractPluginManager::reload().
     */
    NotLoaded = 1 << 7,

    /**
     * The plugin failed to unload. Returned by AbstractPluginManager::unload()
     * and AbstractPluginManager::reload().
     */
    UnloadFailed = 1 << 8,

    /**
     * The plugin cannot be unloaded because another plugin is depending on it.
     * Unload that plugin first and try again. Returned by
     * AbstractPluginManager::unload() and AbstractPluginManager::reload().
     */
    Required = 1 << 9,

    /**
     * The plugin is static. Returned by AbstractPluginManager::loadState(),
     * AbstractPluginManager::load(), AbstractPluginManager::reload() and
     * AbstractPluginManager::unload().
     */
    Static = 1 << 10,

    /**
     * The plugin has active instance and cannot be unloaded. Destroy all
     * instances and try again. Returned by AbstractPluginManager::unload()
     * and AbstractPluginManager::reload().
     */
    Used = 1 << 11
};

/** @relates AbstractPluginManager
@brief Plugin load states

Useful when checking whether LoadState in in given set of values, for example:
@code
if(loadState & (LoadState::Loaded|LoadState::Static)) {
    // ...
}
@endcode
@see AbstractPluginManager::loadState(), AbstractPluginManager::load(),
    AbstractPluginManager::unload(), AbstractPluginManager::reload().
*/
typedef Containers::EnumSet<LoadState, unsigned short> LoadStates;

CORRADE_ENUMSET_OPERATORS(LoadStates)

/**
 * @brief Non-templated base class of PluginManager
 *
 * Base abstract class for all PluginManager templated classes. See also
 * @ref plugin-management.
 */
class CORRADE_PLUGINMANAGER_EXPORT AbstractPluginManager {
    friend class AbstractPlugin;

    public:
        /** @brief Plugin version */
        static const int Version;

        #ifndef DOXYGEN_GENERATING_OUTPUT
        typedef void* (*Instancer)(AbstractPluginManager*, const std::string&);
        static void importStaticPlugin(const std::string& plugin, int _version, const std::string& interface, Instancer instancer, void(*initializer)(), void(*finalizer)());
        #endif

        /**
         * @brief Constructor
         * @param pluginDirectory   Directory where plugins will be searched. No
         *      recursive processing is done.
         *
         * First goes through list of static plugins and finds ones that use
         * the same interface as this PluginManager instance. Then gets list of
         * all dynamic plugins in given directory.
         * @note Dependencies of static plugins are skipped, as static plugins
         *      should have all dependencies present. Also, dynamic plugins
         *      with the same name as another static plugin are skipped.
         * @see pluginList()
         */
        explicit AbstractPluginManager(std::string pluginDirectory);

        AbstractPluginManager(const AbstractPluginManager&) = delete;
        AbstractPluginManager(AbstractPluginManager&&) = delete;

        /**
         * @brief Destructor
         *
         * Destroys all plugin instances and unload all plugins.
         */
        virtual ~AbstractPluginManager();

        AbstractPluginManager& operator=(const AbstractPluginManager&) = delete;
        AbstractPluginManager& operator=(AbstractPluginManager&&) = delete;

        /** @brief Plugin directory */
        std::string pluginDirectory() const;

        /**
         * @brief Set another plugin directory
         *
         * Keeps loaded plugins untouched, removes unloaded plugins which are
         * not existing anymore and adds newly found plugins.
         */
        void setPluginDirectory(std::string directory);

        /**
         * @brief Reload plugin directory
         *
         * Convenience equivalent to `setPluginDirectory(pluginDirectory())`.
         */
        void reloadPluginDirectory();

        /** @brief List of all available plugin names */
        std::vector<std::string> pluginList() const;

        /**
         * @brief Plugin metadata
         *
         * Returns pointer to plugin metadata or `nullptr`, if given plugin is
         * not found.
         */
        const PluginMetadata* metadata(const std::string& plugin) const;

        /**
         * @brief Load state of a plugin
         *
         * Returns @ref LoadState "LoadState::Loaded" if the plugin is loaded or
         * @ref LoadState "LoadState::NotLoaded" if not. For static plugins
         * returns always @ref LoadState "LoadState::Static". On failure returns
         * @ref LoadState "LoadState::NotFound" or @ref LoadState "LoadState::WrongMetadataFile".
         * @see load(), unload(), reload()
         */
        LoadState loadState(const std::string& plugin) const;

        /**
         * @brief Load a plugin
         *
         * Returns @ref LoadState "LoadState::Loaded" if the plugin is already
         * loaded or if loading succeeded. For static plugins returns always
         * @ref LoadState "LoadState::Static". On failure returns
         * @ref LoadState "LoadState::NotFound", @ref LoadState "LoadState::WrongPluginVersion",
         * @ref LoadState "LoadState::WrongInterfaceVersion", @ref LoadState "LoadState::UnresolvedDependency"
         * or @ref LoadState "LoadState::LoadFailed".
         *
         * If the plugin has any dependencies, they are recursively processed
         * before loading given plugin.
         *
         * @see unload(), reload(), loadState()
         */
        virtual LoadState load(const std::string& plugin);

        /**
         * @brief Unload a plugin
         *
         * Returns @ref LoadState "LoadState::NotLoaded" if the plugin is not
         * loaded or unloading succeeded. For static plugins always returns
         * @ref LoadState "LoadState::Static". On failure returns
         * @ref LoadState "LoadState::UnloadFailed", @ref LoadState "LoadState::Required"
         * or @ref LoadState "LoadState::Used".
         *
         * @see load(), reload(), loadState()
         */
        virtual LoadState unload(const std::string& plugin);

    #ifdef DOXYGEN_GENERATING_OUTPUT
    private:
    #else
    protected:
    #endif
        struct StaticPlugin {
            std::string plugin;
            std::string interface;
            Instancer instancer;
            void(*initializer)();
            void(*finalizer)();
        };

        struct Plugin {
            LoadState loadState;
            const Utility::Configuration configuration;
            PluginMetadata metadata;

            /* If set to nullptr, the plugin has not any associated plugin
               manager and cannot be loaded. */
            AbstractPluginManager* manager;

            Instancer instancer;

            union {
                /* For static plugins */
                StaticPlugin* staticPlugin;

                /* For dynamic plugins */
                #ifndef _WIN32
                void* module;
                #else
                HMODULE module;
                #endif
            };

            /* Constructor for dynamic plugins */
            explicit Plugin(const std::string& _metadata, AbstractPluginManager* _manager);

            /* Constructor for static plugins */
            explicit Plugin(std::istream& _metadata, StaticPlugin* staticPlugin);

            ~Plugin();
        };

        std::string _pluginDirectory;

        /* Defined in PluginManager */
        virtual std::string pluginInterface() const = 0;

        /* Global storage of static, unloaded and loaded plugins. The map is
           accessible via function, not directly, because we need to fill it
           with data from staticPlugins() before first use. */
        static std::map<std::string, Plugin*>* plugins();

        /* Because the plugin manager must be noticed about adding the plugin to
           "used by" list, it must be done through this function. */
        virtual void addUsedBy(const std::string& plugin, std::string usedBy);

        /* Because the plugin manager must be noticed about removing the plugin
           from "used by" list, it must be done through this function. */
        virtual void removeUsedBy(const std::string& plugin, const std::string& usedBy);

        void* instanceInternal(const std::string& plugin);

    private:
        /* Temporary storage of all information needed to import static plugins.
           They are imported to plugins() map on first call to plugins(),
           because at that time it is safe to assume that all static resources
           (plugin configuration files) are already registered. After that, the
           storage is deleted and set to `nullptr` to indicate that static
           plugins have been already processed.

           The vector is accessible via function, not directly, because we don't
           know initialization order of static members and thus the vector could
           be uninitalized when accessed from PLUGIN_REGISTER(). */
        CORRADE_PLUGINMANAGER_LOCAL static std::vector<StaticPlugin*>*& staticPlugins();

        std::map<std::string, std::vector<AbstractPlugin*> > instances;

        CORRADE_PLUGINMANAGER_LOCAL void registerInstance(std::string plugin, AbstractPlugin* instance, const Utility::Configuration** configuration, const PluginMetadata** metadata);
        CORRADE_PLUGINMANAGER_LOCAL void unregisterInstance(const std::string& plugin, AbstractPlugin* instance);
};

/** @hideinitializer
@brief Import static plugin
@param name      Static plugin name (defined with PLUGIN_REGISTER())

If static plugins are compiled into dynamic library or directly into the
executable, they should be automatically loaded at startup thanks to
AUTOMATIC_INITALIZER() and AUTOMATIC_FINALIZER() macros.

If static plugins are compiled into static library, they are not
automatically loaded at startup, so you need to load them explicitly by
calling PLUGIN_IMPORT() at the beggining of main() function. You can also
wrap these macro calls into another function (which will then be compiled
into dynamic library or main executable) and use AUTOMATIC_INITIALIZER()
macro for automatic call.
@attention This macro should be called outside of any namespace. If you are
    running into linker errors with `pluginImporter_*`, this could be the
    problem. See RESOURCE_INITIALIZE() documentation for more information.
 */
#define PLUGIN_IMPORT(name)                                                 \
    extern int pluginImporter_##name();                                     \
    pluginImporter_##name();                                                \
    RESOURCE_INITIALIZE(name)

} namespace Utility {

/** @debugoperator{Corrade::PluginManager::AbstractPluginManager} */
Debug CORRADE_PLUGINMANAGER_EXPORT operator<<(Debug debug, PluginManager::LoadState value);

}}

#endif
