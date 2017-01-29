if(HAVE_CONTRIB)
  set(test_LIBS ${test_LIBS} ebuscontrib)
endif(HAVE_CONTRIB)

include_directories(..)
include_directories(../../utils)

add_executable(test_filereader test_filereader.cpp)
target_link_libraries(test_filereader ebus)
add_test(filereader test_filereader)

add_executable(test_device test_device.cpp)
target_link_libraries(test_device ebus ${test_LIBS})
add_test(device test_device)

add_executable(test_symbol test_symbol.cpp)
target_link_libraries(test_symbol ebus)
add_test(symbol test_symbol)

add_executable(test_data test_data.cpp)
target_link_libraries(test_data ebus ${test_LIBS})
add_test(data test_data)

add_executable(test_message test_message.cpp)
target_link_libraries(test_message ebus ${test_LIBS})
add_test(message test_message)
