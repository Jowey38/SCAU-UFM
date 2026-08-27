if(NOT DEFINED PREPROC OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "PREPROC and OUTPUT are required")
endif()

file(REMOVE "${OUTPUT}" "${OUTPUT}.tmp")

execute_process(
    COMMAND "${PREPROC}" generate --profile unknown --output "${OUTPUT}"
    RESULT_VARIABLE unknown_result
)
if(unknown_result EQUAL 0)
    message(FATAL_ERROR "unknown profile unexpectedly succeeded")
endif()
if(EXISTS "${OUTPUT}" OR EXISTS "${OUTPUT}.tmp")
    message(FATAL_ERROR "failed generation left output or temporary file")
endif()

execute_process(
    COMMAND "${PREPROC}" generate --profile mixed-minimal --output "${OUTPUT}"
    RESULT_VARIABLE first_result
)
if(NOT first_result EQUAL 0 OR NOT EXISTS "${OUTPUT}")
    message(FATAL_ERROR "initial generation failed")
endif()

execute_process(
    COMMAND "${PREPROC}" generate --profile mixed-minimal --output "${OUTPUT}"
    RESULT_VARIABLE overwrite_result
)
if(overwrite_result EQUAL 0)
    message(FATAL_ERROR "generation overwrote an existing file without --force")
endif()

execute_process(
    COMMAND "${PREPROC}" generate --profile mixed-minimal --output "${OUTPUT}" --force
    RESULT_VARIABLE force_result
)
if(NOT force_result EQUAL 0 OR NOT EXISTS "${OUTPUT}")
    message(FATAL_ERROR "--force generation failed")
endif()
if(EXISTS "${OUTPUT}.tmp")
    message(FATAL_ERROR "successful generation left a temporary file")
endif()

execute_process(
    COMMAND "${PREPROC}" validate --input "${OUTPUT}"
    RESULT_VARIABLE validate_result
)
if(NOT validate_result EQUAL 0)
    message(FATAL_ERROR "validate rejected a freshly generated case")
endif()

execute_process(
    COMMAND "${PREPROC}" validate --input "${OUTPUT}.missing.nc"
    RESULT_VARIABLE validate_missing_result
)
if(validate_missing_result EQUAL 0)
    message(FATAL_ERROR "validate accepted a missing file")
endif()

execute_process(
    COMMAND "${PREPROC}" validate
    RESULT_VARIABLE validate_noargs_result
)
if(validate_noargs_result EQUAL 0)
    message(FATAL_ERROR "validate without --input unexpectedly succeeded")
endif()
