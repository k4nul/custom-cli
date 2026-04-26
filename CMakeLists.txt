cmake_minimum_required(VERSION 3.18)

project(CLIStarter VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

set(CLI_STARTER_BINARY_NAME "cli-starter" CACHE STRING "Output executable name")
set(CLI_STARTER_DISPLAY_NAME "CLI Starter" CACHE STRING "Human-friendly project name")
set(CLI_STARTER_CONFIG_FILE "cli-starter.json" CACHE STRING "Default configuration filename")
set(CLI_STARTER_PROMPT_LABEL "starter" CACHE STRING "Default interactive prompt label")
option(CLI_STARTER_BUILD_TESTS "Build the starter test target" ${BUILD_TESTING})

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/project_config.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/starter/project_config.hpp
    @ONLY
)

add_custom_target(
    validate_management_docs
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/validate_management_docs.cmake
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Validating management docs"
)

add_custom_target(
    validate_project_completion
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/validate_project_completion.cmake
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Validating project completion state"
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

if(MSVC)
    target_compile_options(starter_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(starter_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(${CLI_STARTER_BINARY_NAME} src/main.cpp)
target_link_libraries(${CLI_STARTER_BINARY_NAME} PRIVATE starter_core)
set_target_properties(${CLI_STARTER_BINARY_NAME} PROPERTIES OUTPUT_NAME "${CLI_STARTER_BINARY_NAME}")

if(CLI_STARTER_BUILD_TESTS)
    add_executable(starter_tests tests/config_tests.cpp)
    target_link_libraries(starter_tests PRIVATE starter_core)
    target_compile_features(starter_tests PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(starter_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(starter_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME starter_tests COMMAND starter_tests)
endif()

if(BUILD_TESTING)
    add_test(
        NAME management_docs_schema
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/validate_management_docs.cmake
    )
    add_test(
        NAME project_completion_state
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/validate_project_completion.cmake
    )
endif()
