########### AGGREGATED COMPONENTS AND DEPENDENCIES FOR THE MULTI CONFIG #####################
#############################################################################################

set(xorg_COMPONENT_NAMES "")
if(DEFINED xorg_FIND_DEPENDENCY_NAMES)
  list(APPEND xorg_FIND_DEPENDENCY_NAMES )
  list(REMOVE_DUPLICATES xorg_FIND_DEPENDENCY_NAMES)
else()
  set(xorg_FIND_DEPENDENCY_NAMES )
endif()

########### VARIABLES #######################################################################
#############################################################################################
set(xorg_PACKAGE_FOLDER_RELEASE "${CMAKE_CURRENT_LIST_DIR}/../tdm_deploy/xorg")
set(xorg_BUILD_MODULES_PATHS_RELEASE )


set(xorg_INCLUDE_DIRS_RELEASE )
set(xorg_RES_DIRS_RELEASE )
set(xorg_DEFINITIONS_RELEASE )
set(xorg_SHARED_LINK_FLAGS_RELEASE )
set(xorg_EXE_LINK_FLAGS_RELEASE )
set(xorg_OBJECTS_RELEASE )
set(xorg_COMPILE_DEFINITIONS_RELEASE )
set(xorg_COMPILE_OPTIONS_C_RELEASE )
set(xorg_COMPILE_OPTIONS_CXX_RELEASE )
set(xorg_LIB_DIRS_RELEASE )
set(xorg_BIN_DIRS_RELEASE )
set(xorg_LIBRARY_TYPE_RELEASE SHARED)
set(xorg_IS_HOST_WINDOWS_RELEASE 0)
set(xorg_LIBS_RELEASE )
set(xorg_SYSTEM_LIBS_RELEASE )
set(xorg_FRAMEWORK_DIRS_RELEASE )
set(xorg_FRAMEWORKS_RELEASE )
set(xorg_BUILD_DIRS_RELEASE )
set(xorg_NO_SONAME_MODE_RELEASE FALSE)


# COMPOUND VARIABLES
set(xorg_COMPILE_OPTIONS_RELEASE
    "$<$<COMPILE_LANGUAGE:CXX>:${xorg_COMPILE_OPTIONS_CXX_RELEASE}>"
    "$<$<COMPILE_LANGUAGE:C>:${xorg_COMPILE_OPTIONS_C_RELEASE}>")
set(xorg_LINKER_FLAGS_RELEASE
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>:${xorg_SHARED_LINK_FLAGS_RELEASE}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,MODULE_LIBRARY>:${xorg_SHARED_LINK_FLAGS_RELEASE}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:${xorg_EXE_LINK_FLAGS_RELEASE}>")


set(xorg_COMPONENTS_RELEASE )