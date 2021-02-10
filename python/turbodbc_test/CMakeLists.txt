find_package(PythonInterp REQUIRED)

add_test(
    NAME turbodbc_integration_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND py.test --cov=turbodbc -s --junitxml=${CMAKE_BINARY_DIR}/turbodbc_python_test.xml --cov-report=xml:${CMAKE_BINARY_DIR}/python_cov.xml --cov-report=term-missing
)
set_tests_properties(
    turbodbc_integration_test
    PROPERTIES
    ENVIRONMENT PYTHONPATH=${CMAKE_BINARY_DIR}
)