# ======================================================================
#  otsdaq_mu2e_stm main build file
#
#  cd .../path/to/build/directory
#  source .../path/to/otsdaq-mu2e-stm/ups/setup_for_development
#  buildtool
# ======================================================================

cmake_minimum_required (VERSION 3.19 FATAL_ERROR)

find_package(cetmodules 3.16.00 REQUIRED)

project(otsdaq_mu2e_stm VERSION 3.00.00)

include(CetCMakeEnv)
cet_cmake_env()

cet_set_compiler_flags(DIAGS VIGILANT
 WERROR
 NO_UNDEFINED
 EXTRA_FLAGS -pedantic -Wno-unused-parameter
 )

#string(TOUPPER ${CMAKE_BUILD_TYPE} BTYPE_UC )
#if( ${BTYPE_UC} MATCHES "DEBUG" )
#  cet_add_compiler_flags(-fsanitize=address)
#endif()

#cet_report_compiler_flags()

# these are minimum required versions, not the actual product versions
find_package(otsdaq_mu2e 1.02.00 REQUIRED)


 # XDAQ Extra setup
 include_directories($ENV{XDAQ_INC} $ENV{XDAQ_INC}/linux)
link_directories($ENV{XDAQ_LIB})


include(BuildPlugins)

# Code
add_subdirectory(otsdaq-mu2e-stm)

# Useful scripts
#add_subdirectory(tools)

# Test Programs
#add_subdirectory(test)

# Documentation (to comment out if not needed DOXYGEN locally)
if( $ENV{OTS_DOXY} MATCHES "DOIT" )
	add_subdirectory(doc)
endif()

# Data
#FILE(COPY ConfigurationDataExamples DESTINATION Data)
#INSTALL(DIRECTORY ConfigurationDataExamples DESTINATION ${product}/${version}/Data)
#FILE(COPY XDAQConfigurations DESTINATION Data)
#INSTALL(DIRECTORY XDAQConfigurations DESTINATION ${product}/${version}/Data)
#FILE(COPY ServiceData DESTINATION Data)
#INSTALL(DIRECTORY ServiceData DESTINATION ${product}/${version}/Data)

cet_cmake_config()
