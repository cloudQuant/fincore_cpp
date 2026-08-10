include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(googletest)

add_executable(fincore_tests
    unit/build_contract_test.cpp
    unit/dataframe_test.cpp)
target_link_libraries(fincore_tests PRIVATE
    fincore::fincore
    GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(fincore_tests)
