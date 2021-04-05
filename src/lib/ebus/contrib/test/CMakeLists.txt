include_directories(..)
include_directories(../..)
include_directories(../../../utils)

add_executable(test_contrib test_tem.cpp)
target_link_libraries(test_contrib ebus ebuscontrib pthread)
add_test(contrib test_contrib)

include(CTest)