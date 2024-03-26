############################## project name #################################
project(QXUtils)
########################### cmake version need ##############################
cmake_minimum_required(VERSION 3.0)
########################## project directories ##############################
set(UTILS_ROOT .)
set(THIRD_PARTY_ROOT ${UTILS_ROOT}/../third_party)
################################ src files ##################################
set(SOURCES
    QXUtilsCommonUtil.c
    QXUtilsModuleCommon.c
    QXUtilsCmdLine.c
    QXUtilsModuleHealth.c
    QXUtilsLogIO.c
    QXUtilsMem.c
    QXUtilsMsg.c
    QXUtilsThreadPool.c
    QXUtilsTimer.c
    QXUtilsSafetyManager.c
)
############################# include dir ###################################
include_directories(
    ${UTILS_ROOT}/include
    ${THIRD_PARTY_ROOT}/libevent/include
	${THIRD_PARTY_ROOT}/libevent/build/include
    ${THIRD_PARTY_ROOT}/GmSSL/include
)
############################ target rules ###################################
# lib options
option(BUILD_SHARED_LIBS "Build shared library" OFF)
if (BUILD_SHARED_LIBS)
    add_library(${PROJECT_NAME} SHARED ${SOURCES})
else ()
    add_library(${PROJECT_NAME} STATIC ${SOURCES})
endif ()
target_compile_options(${PROJECT_NAME} PRIVATE -g -O0)
set_target_properties(${PROJECT_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
############################# cc option ###################################
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Werror)
endif()
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
endif()

########################### unittest options ###############################
option(BUILD_TESTS "Build unit tests" ON)
if (BUILD_TESTS)
	enable_testing()
    set(TEST_SOURCES
        UnitTest/UnitTestTPool.c
        UnitTest/UnitTestMsg.c
        UnitTest/UnitTestMem.c
        UnitTest/UnitTestHealth.c
        UnitTest/UnitTestTimer.c
        UnitTest/UnitTestCommonUtil.c
    )
	foreach(UnitTestSrc ${TEST_SOURCES})
		get_filename_component(testName ${UnitTestSrc} NAME_WE)
		add_executable(${testName} ${UnitTestSrc})
		add_test(NAME ${testName} COMMAND ${testName})
		target_link_directories(${testName} PRIVATE
        	${THIRD_PARTY_ROOT}/libevent/build/lib
    	)
		target_link_libraries(${testName} PRIVATE
        	${PROJECT_NAME}
        	event
        	pthread
    	)
	endforeach()
endif ()
