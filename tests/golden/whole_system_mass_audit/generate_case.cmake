if(NOT DEFINED PREPROC OR NOT DEFINED CASE_PATH)
    message(FATAL_ERROR "PREPROC and CASE_PATH are required")
endif()

execute_process(
    COMMAND "${PREPROC}" generate
        --profile mixed-minimal
        --output "${CASE_PATH}"
        --force
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "scau_preproc failed for ${CASE_PATH} (exit ${result})\nstdout: ${output}\nstderr: ${error}")
endif()
if(NOT EXISTS "${CASE_PATH}")
    message(FATAL_ERROR "scau_preproc reported success but did not create ${CASE_PATH}")
endif()
