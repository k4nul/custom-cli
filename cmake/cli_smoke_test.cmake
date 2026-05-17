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

run_cli_smoke_case("version" "CLI Starter 0\\.1\\.0" --version)
run_cli_smoke_case("about" "neutral CLI starter" about)
run_cli_smoke_case("doctor" "Starter layout looks healthy\\." doctor)
run_cli_smoke_case("config init" "Wrote config template to .*config/local\\.json"
    --config "${SMOKE_CONFIG_PATH}" config init)
run_cli_smoke_case("config show" "Source: disk"
    --config "${SMOKE_CONFIG_PATH}" config show)
run_cli_smoke_case("hello" "Hello, Ada\\." hello --name Ada)
run_cli_smoke_case("echo numbered" "1\\. one" echo --numbered one two)
