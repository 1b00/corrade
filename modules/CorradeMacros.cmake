
# (the blank line is here so CMake doesn't generate documentation from it)

# Set variable for current and also parent scope, if parent scope exists.
#  set_parent_scope(name value)
# Workaround for ugly CMake bug.
macro(set_parent_scope name)
    if("${ARGN}" STREQUAL "")
        set(${name} "")
    else()
        set(${name} ${ARGN})
    endif()

    # Set to parent scope only if parent exists
    if(NOT ${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
        if("${ARGN}" STREQUAL "")
            # CMake bug: nothing is set in parent scope
            set(${name} "" PARENT_SCOPE)
        else()
            set(${name} ${${name}} PARENT_SCOPE)
        endif()
    endif()
endmacro()

function(corrade_add_test2 test_name)
    # Get DLL and path lists
    foreach(arg ${ARGN})
        if(${arg} STREQUAL LIBRARIES)
            set(__DOING_LIBRARIES ON)
        else()
            if(__DOING_LIBRARIES)
                set(libraries ${libraries} ${arg})
            else()
                set(sources ${sources} ${arg})
            endif()
        endif()
    endforeach()

    add_executable(${test_name} ${sources})
    target_link_libraries(${test_name} ${CORRADE_TESTSUITE_LIBRARIES} ${libraries})
    add_test(${test_name} ${test_name})
endfunction()

if(QT4_FOUND)
function(corrade_add_test test_name moc_header source_file)
    foreach(library ${ARGN})
        set(libraries ${library} ${libraries})
    endforeach()

    qt4_wrap_cpp(${test_name}_MOC ${moc_header})
    add_executable(${test_name} ${source_file} ${${test_name}_MOC})
    target_link_libraries(${test_name} ${libraries} ${QT_QTCORE_LIBRARY} ${QT_QTTEST_LIBRARY})
    add_test(${test_name} ${test_name})
endfunction()
else()
function(corrade_add_test)
    message(FATAL_ERROR "Qt4 is required for corrade_add_test(). Be sure you call "
            "find_package(Qt4) before find_package(Corrade)")
endfunction()
endif()

function(corrade_add_multifile_test test_name moc_headers_variable source_files_variable)
    foreach(library ${ARGN})
        set(libraries ${library} ${libraries})
    endforeach()

    qt4_wrap_cpp(${test_name}_MOC ${${moc_headers_variable}})
    add_executable(${test_name} ${${source_files_variable}} ${${test_name}_MOC})
    target_link_libraries(${test_name} ${libraries} ${QT_QTCORE_LIBRARY} ${QT_QTTEST_LIBRARY})
    add_test(${test_name} ${test_name})
endfunction()

function(corrade_add_resource name group_name)
    set(IS_ALIAS OFF)
    foreach(argument ${ARGN})

        # Next argument is alias
        if(${argument} STREQUAL "ALIAS")
            set(IS_ALIAS ON)

        # This argument is alias
        elseif(IS_ALIAS)
            set(arguments ${arguments} -a ${argument})
            set(IS_ALIAS OFF)

        # Filename
        else()
            set(arguments ${arguments} ${argument})
            set(dependencies ${dependencies} ${argument})
        endif()
    endforeach()

    # Run command
    set(out resource_${name}.cpp)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${out}
        COMMAND ${CORRADE_RC_EXECUTABLE} ${name} ${group_name} ${arguments} > ${CMAKE_CURRENT_BINARY_DIR}/${out}
        DEPENDS ${CORRADE_RC_EXECUTABLE} ${dependencies}
        COMMENT "Compiling data resource file ${out}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    # Save output filename
    set(${name} ${CMAKE_CURRENT_BINARY_DIR}/${out} PARENT_SCOPE)
endfunction()

function(corrade_add_plugin plugin_name install_dir metadata_file)
    if(WIN32)
        add_library(${plugin_name} SHARED ${ARGN})
    else()
        add_library(${plugin_name} MODULE ${ARGN})
    endif()

    # Plugins doesn't have any prefix (e.g. 'lib' on Linux)
    set_target_properties(${plugin_name} PROPERTIES
        PREFIX ""
        COMPILE_FLAGS -DCORRADE_DYNAMIC_PLUGIN)

    if(${install_dir} STREQUAL ${CMAKE_CURRENT_BINARY_DIR})
        add_custom_command(
            OUTPUT ${plugin_name}.conf
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${metadata_file} ${CMAKE_CURRENT_BINARY_DIR}/${plugin_name}.conf
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${metadata_file})
        add_custom_target(${plugin_name}-metadata ALL DEPENDS ${plugin_name}.conf)
    else()
        install(TARGETS ${plugin_name} DESTINATION "${install_dir}")
        install(FILES ${metadata_file} DESTINATION "${install_dir}" RENAME "${plugin_name}.conf")
    endif()
endfunction()

macro(corrade_add_static_plugin static_plugins_variable plugin_name metadata_file)
    foreach(source ${ARGN})
        set(sources ${sources} ${source})
    endforeach()

    corrade_add_resource(${plugin_name} plugins ${metadata_file} ALIAS "${plugin_name}.conf")
    add_library(${plugin_name} STATIC ${sources} ${${plugin_name}})

    set_target_properties(${plugin_name} PROPERTIES COMPILE_FLAGS "-DCORRADE_STATIC_PLUGIN ${CMAKE_SHARED_LIBRARY_CXX_FLAGS}")

    # Unset sources array (it's a macro, thus variables stay between calls)
    unset(sources)

    set_parent_scope(${static_plugins_variable} ${${static_plugins_variable}} ${plugin_name})
endmacro()

function(corrade_bundle_dlls library_install_dir)
    # Get DLL and path lists
    foreach(arg ${ARGN})
        if(${arg} STREQUAL PATHS)
            set(__DOING_PATHS ON)
        else()
            if(__DOING_PATHS)
                set(paths ${paths} ${arg})
            else()
                set(dlls ${dlls} ${arg})
            endif()
        endif()
    endforeach()

    # Find and install all DLLs
    foreach(dll ${dlls})
        # Separate filename from path
        get_filename_component(path ${dll} PATH)
        get_filename_component(filename ${dll} NAME)

        # Add current DLL's path to search paths
        foreach(_path ${paths})
            set(${dll}_paths ${${dll}_paths} ${_path}/${path})
        endforeach()

        find_file(${dll}_found ${filename} PATHS ${${dll}_paths}
            NO_DEFAULT_PATH)

        if(${dll}_found)
            install(FILES ${${dll}_found} DESTINATION ${library_install_dir}/${path})
        else()
            message(FATAL_ERROR "DLL ${dll} needed for bundle not found!")
        endif()
    endforeach()
endfunction()
