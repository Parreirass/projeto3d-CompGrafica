cmake_minimum_required(VERSION 3.12)

project(Projeto3D)

add_executable(Projeto3D main.cpp)
#add_executable(Vectors "vector.cpp")
#add_executable(Matriz "matriz.cpp")

#target_include_directories(Matriz PRIVATE dependencias/glm)

#target_include_directories(Vectors PRIVATE dependencias/glm)

target_include_directories(Projeto3D PRIVATE dependencias/glm
											dependencias/stb
											dependencias/glfw/include
											dependencias/glew/include)

target_link_directories(Projeto3D PRIVATE dependencias/glfw/lib-mingw-w64
										 dependencias/glew/lib/Release/x64)

target_link_libraries(Projeto3D PRIVATE glfw3.lib
									   glew32.lib
									   opengl32.lib)

add_custom_command(TARGET Projeto3D POST_BUILD
				   COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/dependencias/glew/bin/Release/x64/glew32.dll" "${CMAKE_BINARY_DIR}")

add_custom_command(TARGET Projeto3D POST_BUILD
				   COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/dependencias/glfw/lib-mingw-w64/glfw3.dll" "${CMAKE_BINARY_DIR}")
