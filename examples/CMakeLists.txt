add_executable(my_web src/my_web.cc)

target_link_libraries(my_web
    web_server_lib
)

# 包含头文件目录
target_include_directories(my_web PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)