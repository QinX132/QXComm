########################### cmake version need ##############################
cmake_minimum_required(VERSION 3.0)
############################## project name #################################
project(QXServer VERSION 1.0.0)
############################### c++ standard ################################
set(CMAKE_CXX_STANDARD 14)
########################## project directories ##############################
set(BUILD_ROOT .)
set(UTILS_ROOT ${BUILD_ROOT}/../../utils)
set(THIRD_PARTY_ROOT ${BUILD_ROOT}/../../third_party)
set(SC_SHARE_ROOT ${BUILD_ROOT}/../../SCShare)
######################### global set ########################################
## find static lib first
set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)
################################ src files ##################################
set(SOURCES
	QXServerMain.cpp
	QXServerWorker.cpp
	QXServerMsgBussiness.cpp
)
add_executable(${PROJECT_NAME} ${SOURCES})
############################# include dir ###################################
include_directories(
    ${BUILD_ROOT}/include
    ${UTILS_ROOT}/include
    ${THIRD_PARTY_ROOT}/libevent/include
    ${THIRD_PARTY_ROOT}/libevent/build/include
	${THIRD_PARTY_ROOT}/json/include
    ${SC_SHARE_ROOT}/include
	${PROTOBUF_INCLUDE_DIRS}
)
############################# cc option ###################################
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Werror)
endif()
if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
endif()
target_compile_options(${PROJECT_NAME} PRIVATE -g -O0)
set(CMAKE_VERBOSE_MAKEFILE ON)
############################ lib options ####################################
## set all static include libc/libc++
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
## set link dir
find_library(EVENT_LIBRARY NAMES event PATHS ${THIRD_PARTY_ROOT}/libevent/build/lib NO_DEFAULT_PATH)
find_library(GMSSL_LIBRARY NAMES gmssl PATHS ${THIRD_PARTY_ROOT}/GmSSL/build/bin NO_DEFAULT_PATH)
find_library(QXUTILS_LIBRARY NAMES QXUtils PATHS ${UTILS_ROOT}/build/lib NO_DEFAULT_PATH)
find_library(QXSCSHARE_LIBRARY NAMES QXSCShare PATHS ${SC_SHARE_ROOT}/build/lib NO_DEFAULT_PATH)
## link 
target_link_libraries(${PROJECT_NAME} PRIVATE 
    ${QXSCSHARE_LIBRARY} 
	${QXUTILS_LIBRARY} 
	${EVENT_LIBRARY} 
	${GMSSL_LIBRARY}
	${PROTOBUF_LIBRARIES}
	Threads::Threads 
)
