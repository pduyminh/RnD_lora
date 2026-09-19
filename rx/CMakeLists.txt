cmake_minimum_required(VERSION 3.16)

# Đăng ký thư mục common làm component dùng chung
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../common")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(omni_rx)
