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
set(xorg_PACKAGE_FOLDER_DEBUG "${CMAKE_CURRENT_LIST_DIR}/../tdm_deploy/xorg")
set(xorg_BUILD_MODULES_PATHS_DEBUG )


set(xorg_INCLUDE_DIRS_DEBUG )
set(xorg_RES_DIRS_DEBUG )
set(xorg_DEFINITIONS_DEBUG )
set(xorg_SHARED_LINK_FLAGS_DEBUG )
set(xorg_EXE_LINK_FLAGS_DEBUG )
set(xorg_OBJECTS_DEBUG )
set(xorg_COMPILE_DEFINITIONS_DEBUG )
set(xorg_COMPILE_OPTIONS_C_DEBUG )
set(xorg_COMPILE_OPTIONS_CXX_DEBUG )
set(xorg_LIB_DIRS_DEBUG )
set(xorg_BIN_DIRS_DEBUG )
set(xorg_LIBRARY_TYPE_DEBUG SHARED)
set(xorg_IS_HOST_WINDOWS_DEBUG 0)
set(xorg_LIBS_DEBUG )
set(xorg_SYSTEM_LIBS_DEBUG )
set(xorg_FRAMEWORK_DIRS_DEBUG )
set(xorg_FRAMEWORKS_DEBUG )
set(xorg_BUILD_DIRS_DEBUG )
set(xorg_NO_SONAME_MODE_DEBUG FALSE)


# COMPOUND VARIABLES
set(xorg_COMPILE_OPTIONS_DEBUG
    "$<$<COMPILE_LANGUAGE:CXX>:${xorg_COMPILE_OPTIONS_CXX_DEBUG}>"
    "$<$<COMPILE_LANGUAGE:C>:${xorg_COMPILE_OPTIONS_C_DEBUG}>")
set(xorg_LINKER_FLAGS_DEBUG
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>:${xorg_SHARED_LINK_FLAGS_DEBUG}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,MODULE_LIBRARY>:${xorg_SHARED_LINK_FLAGS_DEBUG}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:${xorg_EXE_LINK_FLAGS_DEBUG}>")


set(xorg_COMPONENTS_DEBUG )