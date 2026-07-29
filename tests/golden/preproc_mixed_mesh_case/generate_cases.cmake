if(NOT DEFINED PREPROC OR NOT DEFINED CASE_A OR NOT DEFINED CASE_B)
    message(FATAL_ERROR "PREPROC, CASE_A, and CASE_B are required")
endif()

foreach(case_path IN ITEMS "${CASE_A}" "${CASE_B}")
    execute_process(
        COMMAND "${PREPROC}" generate
            --profile mixed-minimal
            --output "${case_path}"
            --force
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR
            "scau_preproc failed for ${case_path} (exit ${result})\nstdout: ${output}\nstderr: ${error}")
    endif()
    if(NOT EXISTS "${case_path}")
        message(FATAL_ERROR "scau_preproc reported success but did not create ${case_path}")
    endif()
endforeach()
