include_directories(..)
include_directories(../..)
include_directories(../../../utils)

add_executable(test_tem test_tem.cpp)
target_link_libraries(test_tem ebus ebuscontrib)
add_test(tem test_tem)
