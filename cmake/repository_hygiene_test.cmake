cmake_minimum_required(VERSION 3.18)

if(NOT DEFINED CLI_STARTER_SOURCE_DIR)
    message(FATAL_ERROR "CLI_STARTER_SOURCE_DIR is required")
endif()

find_program(GIT_EXECUTABLE git)
if(NOT GIT_EXECUTABLE)
    message(STATUS "Skipping repository hygiene check because git was not found.")
    return()
endif()

execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${CLI_STARTER_SOURCE_DIR}" rev-parse --is-inside-work-tree
    RESULT_VARIABLE git_result
    OUTPUT_VARIABLE git_stdout
    ERROR_VARIABLE git_stderr
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
)

if(NOT git_result EQUAL 0 OR NOT git_stdout STREQUAL "true")
    message(STATUS "Skipping repository hygiene check outside a git worktree.")
    return()
endif()

execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${CLI_STARTER_SOURCE_DIR}" ls-files "build-local-*" ".sandbox-user/*"
    RESULT_VARIABLE hygiene_result
    OUTPUT_VARIABLE tracked_artifacts
    ERROR_VARIABLE hygiene_stderr
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
)

if(NOT hygiene_result EQUAL 0)
    message(FATAL_ERROR "Failed to inspect tracked local artifacts:\n${hygiene_stderr}")
endif()

if(NOT tracked_artifacts STREQUAL "")
    string(REPLACE "\n" ";" tracked_artifact_list "${tracked_artifacts}")
endif()

set(present_artifacts)
foreach(artifact_path IN LISTS tracked_artifact_list)
    if(EXISTS "${CLI_STARTER_SOURCE_DIR}/${artifact_path}")
        list(APPEND present_artifacts "${artifact_path}")
    endif()
endforeach()

if(present_artifacts)
    list(JOIN present_artifacts "\n" present_artifacts_text)
    message(FATAL_ERROR
        "Tracked generated local artifacts are present in the repository checkout but should remain ignored:\n"
        "${present_artifacts_text}\n"
        "Remove these paths from the repository and validate from a fresh build/ tree.")
endif()
