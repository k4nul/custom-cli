cmake_minimum_required(VERSION 3.18)

project(CLIStarter VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

set(CLI_STARTER_BINARY_NAME "cli-starter" CACHE STRING "Output executable name")
set(CLI_STARTER_DISPLAY_NAME "CLI Starter" CACHE STRING "Human-friendly project name")
set(CLI_STARTER_CONFIG_FILE "cli-starter.json" CACHE STRING "Default configuration filename")
set(CLI_STARTER_PROMPT_LABEL "starter" CACHE STRING "Default interactive prompt label")
option(CLI_STARTER_BUILD_TESTS "Build the starter test target" ${BUILD_TESTING})

function(cli_starter_enable_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/project_config.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/starter/project_config.hpp
    @ONLY
)

add_library(starter_core
    src/app/application.cpp
    src/commands/about_command.cpp
    src/commands/config_command.cpp
    src/commands/doctor_command.cpp
    src/commands/echo_command.cpp
    src/commands/hello_command.cpp
    src/commands/register_commands.cpp
    src/core/completion.cpp
    src/core/config.cpp
    src/core/project_info.cpp
    src/core/shell_line_reader.cpp
    src/core/tokenize.cpp
)

target_compile_features(starter_core PUBLIC cxx_std_17)
target_include_directories(starter_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cli11
        ${CMAKE_CURRENT_BINARY_DIR}/generated
)

cli_starter_enable_warnings(starter_core)

add_executable(${CLI_STARTER_BINARY_NAME} src/main.cpp)
target_link_libraries(${CLI_STARTER_BINARY_NAME} PRIVATE starter_core)
set_target_properties(${CLI_STARTER_BINARY_NAME} PROPERTIES OUTPUT_NAME "${CLI_STARTER_BINARY_NAME}")
cli_starter_enable_warnings(${CLI_STARTER_BINARY_NAME})

if(CLI_STARTER_BUILD_TESTS)
    add_executable(starter_tests tests/config_tests.cpp)
    target_link_libraries(starter_tests PRIVATE starter_core)
    target_compile_features(starter_tests PRIVATE cxx_std_17)
    cli_starter_enable_warnings(starter_tests)

    add_test(NAME starter_tests COMMAND starter_tests)
    add_test(
        NAME cli_starter_smoke
        COMMAND ${CMAKE_COMMAND}
            -DCLI_STARTER_EXECUTABLE=$<TARGET_FILE:${CLI_STARTER_BINARY_NAME}>
            -DCLI_STARTER_SMOKE_DIR=${CMAKE_CURRENT_BINARY_DIR}/ctest-smoke
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/cli_smoke_test.cmake
    )
    set_tests_properties(cli_starter_smoke PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()
