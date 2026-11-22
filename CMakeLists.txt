cmake_minimum_required(VERSION 3.22)
project(InstitutoInnova VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
include(FetchContent)
find_package(Threads REQUIRED)

FetchContent_Declare(
    crow_headers
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG v1.3.4
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR headers-only
)
FetchContent_Declare(
    asio_headers
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-30-2
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR headers-only
)
FetchContent_MakeAvailable(crow_headers asio_headers)

add_library(innova_modelo src/modelo.cpp)
target_include_directories(innova_modelo PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_executable(innova src/servidor.cpp)
target_include_directories(innova SYSTEM PRIVATE
    "${crow_headers_SOURCE_DIR}/include"
    "${asio_headers_SOURCE_DIR}/asio/include"
)
target_compile_definitions(innova PRIVATE ASIO_STANDALONE ASIO_NO_DEPRECATED)
target_link_libraries(innova PRIVATE innova_modelo Threads::Threads)

if(WIN32)
    target_compile_definitions(innova PRIVATE _WIN32_WINNT=0x0601 NOMINMAX)
    target_link_libraries(innova PRIVATE ws2_32 mswsock)
endif()

if(BUILD_TESTING)
    add_executable(modelo_test tests/modelo_test.cpp)
    target_link_libraries(modelo_test PRIVATE innova_modelo)
    add_test(NAME modelo COMMAND modelo_test)

    find_program(NODE_EXECUTABLE NAMES node nodejs)
    if(NODE_EXECUTABLE)
        execute_process(COMMAND "${NODE_EXECUTABLE}" --version
                        OUTPUT_VARIABLE NODE_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(NODE_VERSION MATCHES "^v([0-9]+)" AND CMAKE_MATCH_1 GREATER_EQUAL 18)
            add_test(NAME api COMMAND "${NODE_EXECUTABLE}"
                     "${CMAKE_CURRENT_SOURCE_DIR}/tests/api_test.mjs" "$<TARGET_FILE:innova>")
            set_tests_properties(api PROPERTIES TIMEOUT 60)
        else()
            message(STATUS "Las pruebas de API requieren Node.js 18 o posterior.")
        endif()
    else()
        message(STATUS "Node.js no encontrado: las pruebas de API no se ejecutarán.")
    endif()
endif()

if(MSVC)
    target_compile_options(innova_modelo PRIVATE /W4 /utf-8 /permissive-)
    target_compile_options(innova PRIVATE /W4 /utf-8 /permissive-)
    if(BUILD_TESTING)
        target_compile_options(modelo_test PRIVATE /W4 /utf-8 /permissive-)
    endif()
else()
    target_compile_options(innova_modelo PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(innova PRIVATE -Wall -Wextra -Wpedantic)
    if(BUILD_TESTING)
        target_compile_options(modelo_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()
