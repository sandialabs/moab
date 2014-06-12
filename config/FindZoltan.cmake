#
# Find Zoltan include directories and libraries
#
# Zoltan_INCLUDES            - list of include paths to find netcdf.h
# Zoltan_LIBRARIES           - list of libraries to link against when using Zoltan
# Zoltan_FOUND               - Do not attempt to use Zoltan if "no", "0", or undefined.

set (Zoltan_DIR "" CACHE PATH "Path to search for Zoltan header and library files" )
set (Zoltan_FOUND NO CACHE INTERNAL "Found Zoltan components successfully." )

find_path( Zoltan_INCLUDE_DIR zoltan.h
  ${Zoltan_DIR}
  ${Zoltan_DIR}/include
  /usr/local/include
  /usr/include
)

find_library( Zoltan_LIBRARY
  NAMES zoltan
  HINTS ${Zoltan_DIR}
  ${Zoltan_DIR}/lib64
  ${Zoltan_DIR}/lib
  /usr/local/lib64
  /usr/lib64
  /usr/lib64/zoltan
  /usr/local/lib
  /usr/lib
  /usr/lib/zoltan
)


macro (Zoltan_GET_VARIABLE makefile name var)
  set (${var} "NOTFOUND" CACHE INTERNAL "Cleared" FORCE)
  execute_process (COMMAND ${CMAKE_BUILD_TOOL} -f ${${makefile}} show VARIABLE=${name}
    OUTPUT_VARIABLE ${var}
    RESULT_VARIABLE zoltan_return)
endmacro (Zoltan_GET_VARIABLE)

macro (Zoltan_GET_ALL_VARIABLES)
  if (NOT zoltan_config_current)
    # A temporary makefile to probe this Zoltan components's configuration
    # The current inspection is based on Zoltan-3.6 installation
    set (zoltan_config_makefile "${CMAKE_CURRENT_BINARY_DIR}/Makefile.zoltan")
    file (WRITE ${zoltan_config_makefile}
      "## This file was autogenerated by FindZoltan.cmake
include ${Zoltan_INCLUDE_DIR}/Makefile.export.zoltan
include ${Zoltan_INCLUDE_DIR}/Makefile.export.zoltan.macros
show :
	-@echo -n \${\${VARIABLE}}")
    Zoltan_GET_VARIABLE (zoltan_config_makefile ZOLTAN_CPPFLAGS    zoltan_extra_cppflags)
    Zoltan_GET_VARIABLE (zoltan_config_makefile ZOLTAN_EXTRA_LIBS  zoltan_extra_libs)
    Zoltan_GET_VARIABLE (zoltan_config_makefile ZOLTAN_LDFLAGS     zoltan_ldflags)
    
    file (REMOVE ${zoltan_config_makefile})
    SET(tmp_incs "-I${Zoltan_INCLUDE_DIR} ${zoltan_extra_cppflags}")
    resolve_includes(Zoltan_INCLUDES ${tmp_incs})
    SET(tmp_libs "${Zoltan_LIBRARY} ${zoltan_ldflags} ${zoltan_extra_libs}")
    resolve_libraries (Zoltan_LIBRARIES "${tmp_libs}")
  endif ()
endmacro (Zoltan_GET_ALL_VARIABLES)


IF (NOT Zoltan_FOUND)
  if ( Zoltan_INCLUDE_DIR AND Zoltan_LIBRARY )
    set( Zoltan_FOUND YES )
    if(EXISTS ${Zoltan_INCLUDE_DIR}/Makefile.export.zoltan)
      include (ResolveCompilerPaths)
      Zoltan_GET_ALL_VARIABLES()
    else(EXISTS ${Zoltan_INCLUDE_DIR}/Makefile.export.zoltan)
      SET(Zoltan_INCLUDES ${Zoltan_INCLUDE_DIR})
      SET(Zoltan_LIBRARIES ${Zoltan_LIBRARY})
    endif(EXISTS ${Zoltan_INCLUDE_DIR}/Makefile.export.zoltan)
    message (STATUS "---   Zoltan Configuration ::")
    message (STATUS "        INCLUDES  : ${Zoltan_INCLUDES}")
    message (STATUS "        LIBRARIES : ${Zoltan_LIBRARIES}")
  else ( Zoltan_INCLUDE_DIR AND Zoltan_LIBRARY )
    set( Zoltan_FOUND NO )
    message("finding Zoltan failed, please try to set the var Zoltan_DIR")
  endif ( Zoltan_INCLUDE_DIR AND Zoltan_LIBRARY )
ENDIF (NOT Zoltan_FOUND)

mark_as_advanced(
  Zoltan_DIR
  Zoltan_INCLUDES
  Zoltan_LIBRARIES
)

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (Zoltan "Zoltan not found, check environment variables Zoltan_DIR"
  Zoltan_DIR Zoltan_INCLUDES Zoltan_LIBRARIES)

