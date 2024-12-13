cmake_minimum_required(VERSION 3.8)
project(streaming_server)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
# find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf REQUIRED)

set(GRPC_LIBRARIES grpc++ grpc)
set(GRPC_INCLUDE_DIRS /usr/include)
set(GRPC_LIBRARY_DIRS /usr/lib/x86_64-linux-gnu)

include_directories(${GRPC_INCLUDE_DIRS})
link_directories(${GRPC_LIBRARY_DIRS})

find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin REQUIRED)
if(NOT GRPC_CPP_PLUGIN)
  message(FATAL_ERROR "gRPC C++ plugin (grpc_cpp_plugin) not found. Install grpc-dev and protobuf-compiler-grpc.")
endif()

set(PROTO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/proto/pointcloud_stream.proto)


set(ALL_PROTO_SRCS)
set(ALL_PROTO_HDRS)
foreach(ABS_FIL ${PROTO_FILES})
  file(RELATIVE_PATH REL_FIL ${PROJECT_SOURCE_DIR} ${ABS_FIL})
  get_filename_component(DIR ${REL_FIL} DIRECTORY)
  get_filename_component(FIL_WE ${REL_FIL} NAME_WE)

  list(APPEND ALL_PROTO_SRCS "${PROJECT_BINARY_DIR}/${DIR}/${FIL_WE}.pb.cc")
  list(APPEND ALL_PROTO_HDRS "${PROJECT_BINARY_DIR}/${DIR}/${FIL_WE}.pb.h")
  list(APPEND ALL_PROTO_SRCS "${PROJECT_BINARY_DIR}/${DIR}/${FIL_WE}.grpc.pb.cc")
  list(APPEND ALL_PROTO_HDRS "${PROJECT_BINARY_DIR}/${DIR}/${FIL_WE}.grpc.pb.h")

  add_custom_command(
    OUTPUT "${PROJECT_BINARY_DIR}/${DIR}/${FIL_WE}.pb.cc"
           "${PROJECT_BINARY_DIR}/${DIR}/${FIL_WE}.pb.h"
           "${PROJECT_BINARY_DIR}/${DIR}/${FIL_WE}.grpc.pb.cc"
           "${PROJECT_BINARY_DIR}/${DIR}/${FIL_WE}.grpc.pb.h"
    COMMAND  ${PROTOBUF_PROTOC_EXECUTABLE}
    ARGS --cpp_out  ${PROJECT_BINARY_DIR} -I
      ${PROJECT_SOURCE_DIR} ${ABS_FIL}
      --grpc_out=${PROJECT_BINARY_DIR}
      --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
    DEPENDS ${ABS_FIL}
    COMMENT "Running C++ protocol buffer compiler on ${ABS_FIL}"
    VERBATIM
  )
endforeach()
set_source_files_properties(${ALL_PROTO_SRCS} ${ALL_PROTO_HDRS} PROPERTIES GENERATED TRUE)



# Add library for the gRPC service
add_library(pointcloud_stream_proto
    ${ALL_PROTO_HDRS}
    ${ALL_PROTO_SRCS}
)
target_include_directories(pointcloud_stream_proto PUBLIC ${Protobuf_INCLUDE_DIRS} ${CMAKE_BINARY_DIR})
target_link_libraries(pointcloud_stream_proto PUBLIC ${GRPC_LIBRARIES} protobuf::libprotobuf)



add_executable(streaming_server_node src/streaming_server_node.cpp)
target_include_directories(streaming_server_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  )
target_compile_features(streaming_server_node PUBLIC c_std_99 cxx_std_17)  # Require C99 and C++17
target_link_libraries(streaming_server_node pointcloud_stream_proto ${GRPC_LIBRARIES} protobuf::libprotobuf)
ament_target_dependencies(streaming_server_node rclcpp sensor_msgs)

add_executable(streaming_client_node src/streaming_client_node.cpp)
target_link_libraries(streaming_client_node pointcloud_stream_proto ${GRPC_LIBRARIES} protobuf::libprotobuf)
ament_target_dependencies(streaming_client_node rclcpp sensor_msgs)

# Install targets
install(TARGETS
    streaming_server_node
    streaming_client_node
    pointcloud_stream_proto
    DESTINATION lib/${PROJECT_NAME}
)

# Install Proto Files
install(FILES
    ${PROTO_FILES}
    DESTINATION share/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
