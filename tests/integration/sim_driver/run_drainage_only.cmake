# Execute the exported synthetic package with real SWMM and no D-Flow runtime.
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
file(COPY "${CASE_DIR}/swmm/model.inp" DESTINATION "${OUTPUT_DIR}")
file(READ "${CASE_DIR}/simdriver/run.conf" config)
string(REPLACE "engine_mode = mock" "engine_mode = real" config "${config}")
string(REPLACE "stcf_case_path = mesh/case.stcf.nc"
    "stcf_case_path = ${CASE_DIR}/mesh/case.stcf.nc" config "${config}")
string(REPLACE "swmm_inp_path = swmm/model.inp"
    "swmm_inp_path = ${OUTPUT_DIR}/model.inp" config "${config}")
string(REPLACE "output_summary_path = run_summary.json"
    "output_summary_path = ${OUTPUT_DIR}/run_summary.json" config "${config}")
string(APPEND config "\nenable_cvc_spatial_phi_t_correction = true\n")
file(WRITE "${OUTPUT_DIR}/run.conf" "${config}")
# A stale summary must never turn a failed invocation into success.
file(REMOVE "${OUTPUT_DIR}/run_summary.json")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "SCAU_DFLOWFM_LIBRARY=${OUTPUT_DIR}/deliberately_missing_dflowfm.dll"
        "${SIM}" "${OUTPUT_DIR}/run.conf"
    WORKING_DIRECTORY "${OUTPUT_DIR}"
    RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "drainage-only real run failed (${result}): ${stdout}\n${stderr}")
endif()
file(READ "${OUTPUT_DIR}/run_summary.json" summary)
string(JSON outcome GET "${summary}" outcome)
string(JSON committed GET "${summary}" committed_epochs)
if(NOT outcome STREQUAL "completed" OR NOT committed EQUAL 3)
    message(FATAL_ERROR "unexpected drainage-only result: ${summary}")
endif()
message(STATUS "real surface+SWMM completed three epochs without a D-Flow runtime")
