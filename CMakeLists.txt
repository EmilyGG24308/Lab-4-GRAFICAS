cmake_minimum_required(VERSION 3.16)
project(Lab4Graficas CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED CONFIG)
find_package(glm REQUIRED CONFIG)

add_executable(lab4 src/main.cpp src/obj_loader.cpp)
target_link_libraries(lab4 PRIVATE SDL2::SDL2main SDL2::SDL2 glm::glm)

if(MSVC)
  target_compile_options(lab4 PRIVATE /W4)
else()
  target_compile_options(lab4 PRIVATE -Wall -Wextra)
endif()

# MinGW: enlazar el runtime de C++ estáticamente y copiar SDL2.dll junto al
# ejecutable, para poder ejecutarlo fuera de la terminal de MSYS2.
if(MINGW)
  target_link_options(lab4 PRIVATE -static-libgcc -static-libstdc++
    -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive,-Bdynamic)
endif()
if(WIN32)
  add_custom_command(TARGET lab4 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:lab4>)
endif()
