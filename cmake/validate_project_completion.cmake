set(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(COMPLETION_SCRIPT "${REPO_ROOT}/tests/check_project_completion.py")

find_program(PYTHON3_EXECUTABLE NAMES python3)
find_program(PYTHON_EXECUTABLE NAMES python)
find_program(PY_LAUNCHER_EXECUTABLE NAMES py)

if(PYTHON3_EXECUTABLE)
    set(PYTHON_COMMAND "${PYTHON3_EXECUTABLE}")
elseif(PYTHON_EXECUTABLE)
    set(PYTHON_COMMAND "${PYTHON_EXECUTABLE}")
elseif(PY_LAUNCHER_EXECUTABLE)
    set(PYTHON_COMMAND "${PY_LAUNCHER_EXECUTABLE}" "-3")
else()
    message(FATAL_ERROR "Could not find a Python 3 interpreter. Tried python3, python, and py -3.")
endif()

execute_process(
    COMMAND ${PYTHON_COMMAND} "${COMPLETION_SCRIPT}"
    WORKING_DIRECTORY "${REPO_ROOT}"
    RESULT_VARIABLE COMPLETION_EXIT_CODE
)

if(NOT COMPLETION_EXIT_CODE EQUAL 0)
    message(FATAL_ERROR "Project completion validation failed with exit code ${COMPLETION_EXIT_CODE}.")
endif()
