cmake_minimum_required(VERSION 3.0.2)
project(sensor_driver)

## 编译选项：启用警告
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

## 查找 catkin 和依赖包
find_package(catkin REQUIRED COMPONENTS
  roscpp
  sensor_msgs
  std_msgs
  tf2
  serial
)

## 声明 catkin 的相关依赖
catkin_package(
  CATKIN_DEPENDS roscpp sensor_msgs std_msgs tf2 serial
)

## 生成可执行文件
add_executable(sensor_driver_node src/sensor_driver_node.cpp)

## 指定包含目录
target_include_directories(sensor_driver_node PUBLIC
  ${catkin_INCLUDE_DIRS}
)

## 链接依赖
target_link_libraries(sensor_driver_node
  ${catkin_LIBRARIES}
)

## 指定目标的编译标准
set_target_properties(sensor_driver_node PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

## 安装目标文件
install(TARGETS sensor_driver_node
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

## 安装 launch 文件
install(DIRECTORY launch/
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}/launch
)

