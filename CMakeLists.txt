cmake_minimum_required(VERSION 3.16)

# Chặn đứng việc tự động tải để tránh xung đột phiên bản
#set(ENV{IDF_COMPONENT_MANAGER} "0") 

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(smart_lock_s3)