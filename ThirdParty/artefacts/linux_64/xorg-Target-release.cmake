# Avoid multiple calls to find_package to append duplicated properties to the targets
include_guard()########### VARIABLES #######################################################################
#############################################################################################
set(xorg_FRAMEWORKS_FOUND_RELEASE "") # Will be filled later
conan_find_apple_frameworks(xorg_FRAMEWORKS_FOUND_RELEASE "${xorg_FRAMEWORKS_RELEASE}" "${xorg_FRAMEWORK_DIRS_RELEASE}")

set(xorg_LIBRARIES_TARGETS "") # Will be filled later


######## Create an interface target to contain all the dependencies (frameworks, system and conan deps)
if(NOT TARGET xorg_DEPS_TARGET)
    add_library(xorg_DEPS_TARGET INTERFACE IMPORTED)
endif()

set_property(TARGET xorg_DEPS_TARGET
             APPEND PROPERTY INTERFACE_LINK_LIBRARIES
             $<$<CONFIG:Release>:${xorg_FRAMEWORKS_FOUND_RELEASE}>
             $<$<CONFIG:Release>:${xorg_SYSTEM_LIBS_RELEASE}>
             $<$<CONFIG:Release>:>)

####### Find the libraries declared in cpp_info.libs, create an IMPORTED target for each one and link the
####### xorg_DEPS_TARGET to all of them
conan_package_library_targets("${xorg_LIBS_RELEASE}"    # libraries
                              "${xorg_LIB_DIRS_RELEASE}" # package_libdir
                              "${xorg_BIN_DIRS_RELEASE}" # package_bindir
                              "${xorg_LIBRARY_TYPE_RELEASE}"
                              "${xorg_IS_HOST_WINDOWS_RELEASE}"
                              xorg_DEPS_TARGET
                              xorg_LIBRARIES_TARGETS  # out_libraries_targets
                              "_RELEASE"
                              "xorg"    # package_name
                              "${xorg_NO_SONAME_MODE_RELEASE}")  # soname

# FIXME: What is the result of this for multi-config? All configs adding themselves to path?
set(CMAKE_MODULE_PATH ${xorg_BUILD_DIRS_RELEASE} ${CMAKE_MODULE_PATH})

########## GLOBAL TARGET PROPERTIES Release ########################################
    set_property(TARGET xorg::xorg
                 APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                 $<$<CONFIG:Release>:${xorg_OBJECTS_RELEASE}>
                 $<$<CONFIG:Release>:${xorg_LIBRARIES_TARGETS}>
                 )

    if("${xorg_LIBS_RELEASE}" STREQUAL "")
        # If the package is not declaring any "cpp_info.libs" the package deps, system libs,
        # frameworks etc are not linked to the imported targets and we need to do it to the
        # global target
        set_property(TARGET xorg::xorg
                     APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                     xorg_DEPS_TARGET)
    endif()

    set_property(TARGET xorg::xorg
                 APPEND PROPERTY INTERFACE_LINK_OPTIONS
                 $<$<CONFIG:Release>:${xorg_LINKER_FLAGS_RELEASE}>)
    set_property(TARGET xorg::xorg
                 APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                 $<$<CONFIG:Release>:${xorg_INCLUDE_DIRS_RELEASE}>)
    # Necessary to find LINK shared libraries in Linux
    set_property(TARGET xorg::xorg
                 APPEND PROPERTY INTERFACE_LINK_DIRECTORIES
                 $<$<CONFIG:Release>:${xorg_LIB_DIRS_RELEASE}>)
    set_property(TARGET xorg::xorg
                 APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS
                 $<$<CONFIG:Release>:${xorg_COMPILE_DEFINITIONS_RELEASE}>)
    set_property(TARGET xorg::xorg
                 APPEND PROPERTY INTERFACE_COMPILE_OPTIONS
                 $<$<CONFIG:Release>:${xorg_COMPILE_OPTIONS_RELEASE}>)

########## For the modules (FindXXX)
set(xorg_LIBRARIES_RELEASE xorg::xorg)
