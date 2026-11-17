if(NOT DEFINED TEST_EXECUTABLE OR NOT DEFINED TEST_LOG)
    message(FATAL_ERROR "Browser window test executable and log are required.")
endif()

# A crashed WebEngine renderer can outlive the test while its core is handled.
# Its inherited output descriptors must not keep CTest's capture pipe open.
string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef suffix)
set(log_path "${TEST_LOG}.${suffix}")
execute_process(
    COMMAND "${TEST_EXECUTABLE}"
    OUTPUT_FILE "${log_path}"
    ERROR_FILE "${log_path}"
    RESULT_VARIABLE result
)
file(READ "${log_path}" output)
file(REMOVE "${log_path}")
message("${output}")
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "Browser window test failed: ${result}")
endif()
