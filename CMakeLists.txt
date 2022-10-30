cmake_minimum_required(VERSION 3.19)
project(http_server)

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/thirdparty/jsoncpp/include)
include_directories(${PROJECT_SOURCE_DIR}/thirdparty/curl/include)
include_directories(${PROJECT_SOURCE_DIR}/thirdparty/googletest/include)
include_directories(${PROJECT_SOURCE_DIR}/thirdparty/easylogging++/include)


link_directories(${PROJECT_SOURCE_DIR}/thirdparty/jsoncpp/lib)
link_directories(${PROJECT_SOURCE_DIR}/thirdparty/curl/lib)
link_directories(${PROJECT_SOURCE_DIR}/thirdparty/googletest/lib)
link_directories(${PROJECT_SOURCE_DIR}/thirdparty/easylogging++/lib)

set(CMAKE_CXX_STANDARD 11)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

aux_source_directory(${PROJECT_SOURCE_DIR}/src/ SRC_LIST)
add_executable(http_server ${SRC_LIST})
target_link_libraries(http_server jsoncpp curl pthread easyloggingpp)

aux_source_directory(${PROJECT_SOURCE_DIR}/test/ TEST_LIST)
add_executable(http_server_test ${TEST_LIST})
target_link_libraries(http_server_test gtest pthread easyloggingpp)