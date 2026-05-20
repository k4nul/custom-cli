cmake_minimum_required(VERSION 3.18)

if(NOT DEFINED CLI_STARTER_EXECUTABLE)
    message(FATAL_ERROR "CLI_STARTER_EXECUTABLE is required")
endif()

if(NOT EXISTS "${CLI_STARTER_EXECUTABLE}")
    message(FATAL_ERROR "CLI executable does not exist: ${CLI_STARTER_EXECUTABLE}")
endif()

if(NOT DEFINED CLI_STARTER_SMOKE_DIR)
    message(FATAL_ERROR "CLI_STARTER_SMOKE_DIR is required")
endif()

file(REMOVE_RECURSE "${CLI_STARTER_SMOKE_DIR}")
file(MAKE_DIRECTORY "${CLI_STARTER_SMOKE_DIR}")

set(SMOKE_CONFIG_PATH "${CLI_STARTER_SMOKE_DIR}/config/local.json")

function(run_cli_smoke_case case_name expected_stdout_regex)
    execute_process(
        COMMAND "${CLI_STARTER_EXECUTABLE}" ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
    )

    if(NOT result EQUAL 0)
        message(FATAL_ERROR
            "Smoke case '${case_name}' failed with exit code ${result}\n"
            "stdout:\n${stdout}\n"
            "stderr:\n${stderr}")
    endif()

    if(NOT stdout MATCHES "${expected_stdout_regex}")
        message(FATAL_ERROR
            "Smoke case '${case_name}' stdout did not match /${expected_stdout_regex}/\n"
            "stdout:\n${stdout}")
    endif()

    if(NOT stderr STREQUAL "")
        message(FATAL_ERROR
            "Smoke case '${case_name}' wrote to stderr\n"
            "stderr:\n${stderr}")
    endif()
endfunction()

function(run_cli_failure_smoke_case case_name)
    cmake_parse_arguments(
        FAILURE_SMOKE
        ""
        ""
        "COMMAND;STDERR_MATCHES"
        ${ARGN}
    )

    if(NOT FAILURE_SMOKE_COMMAND)
        message(FATAL_ERROR "Failure smoke case '${case_name}' requires COMMAND arguments")
    endif()

    if(NOT FAILURE_SMOKE_STDERR_MATCHES)
        message(FATAL_ERROR "Failure smoke case '${case_name}' requires STDERR_MATCHES patterns")
    endif()

    execute_process(
        COMMAND "${CLI_STARTER_EXECUTABLE}" ${FAILURE_SMOKE_COMMAND}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
    )

    if(result EQUAL 0)
        message(FATAL_ERROR
            "Failure smoke case '${case_name}' unexpectedly succeeded\n"
            "stdout:\n${stdout}\n"
            "stderr:\n${stderr}")
    endif()

    if(NOT stdout STREQUAL "")
        message(FATAL_ERROR
            "Failure smoke case '${case_name}' wrote to stdout\n"
            "stdout:\n${stdout}")
    endif()

    foreach(expected_stderr_regex IN LISTS FAILURE_SMOKE_STDERR_MATCHES)
        if(NOT stderr MATCHES "${expected_stderr_regex}")
            message(FATAL_ERROR
                "Failure smoke case '${case_name}' stderr did not match /${expected_stderr_regex}/\n"
                "stderr:\n${stderr}")
        endif()
    endforeach()
endfunction()

run_cli_smoke_case("version" "CLI Starter 0\\.1\\.0" --version)
run_cli_smoke_case("about" "neutral CLI starter" about)
run_cli_smoke_case("doctor" "Starter layout looks healthy\\." doctor)
run_cli_smoke_case("config init" "Wrote config template to .*config/local\\.json"
    --config "${SMOKE_CONFIG_PATH}" config init)
run_cli_smoke_case("config show" "Source: disk"
    --config "${SMOKE_CONFIG_PATH}" config show)
run_cli_smoke_case("hello" "Hello, Ada\\." hello --name Ada)
run_cli_smoke_case("echo numbered" "1\\. one" echo --numbered one two)

run_cli_failure_smoke_case("unknown command"
    STDERR_MATCHES "missing-command" "Run with --help"
    COMMAND missing-command)
run_cli_failure_smoke_case("missing echo text"
    STDERR_MATCHES "text is required" "Run with --help"
    COMMAND echo)
run_cli_failure_smoke_case("unknown hello option"
    STDERR_MATCHES "--unknown" "Run with --help"
    COMMAND hello --unknown)
run_cli_failure_smoke_case("missing config subcommand"
    STDERR_MATCHES "A subcommand is required" "Run with --help"
    COMMAND config)

set(BAD_CONFIG_PATH "${CLI_STARTER_SMOKE_DIR}/config/bad.json")
file(WRITE "${BAD_CONFIG_PATH}" "{\"default_name\":")
run_cli_failure_smoke_case("malformed config show"
    STDERR_MATCHES "error:" "parse error"
    COMMAND --config "${BAD_CONFIG_PATH}" config show)

set(WRONG_TYPE_CONFIG_PATH "${CLI_STARTER_SMOKE_DIR}/config/wrong-type.json")
file(WRITE "${WRONG_TYPE_CONFIG_PATH}" "{\"default_name\":42}")
run_cli_failure_smoke_case("wrong-type config hello"
    STDERR_MATCHES "error:" "type must be string"
    COMMAND --config "${WRONG_TYPE_CONFIG_PATH}" hello)
