cmake_minimum_required(VERSION 3.13)

set(INCLUDE_DIR ${CMAKE_SYSROOT}/usr/include)

add_executable(wl-compositor ${CMAKE_CURRENT_SOURCE_DIR}/wl_compositor.cpp)
target_compile_definitions(wl-compositor PRIVATE WLR_USE_UNSTABLE)
target_link_libraries(wl-compositor PRIVATE "libwlroots-0.19.so" "libEGL.so" "libGLESv2.so" "libwayland-server.so" "libgbm.so" "libdrm.so")
target_include_directories(wl-compositor PRIVATE 
                           ${CMAKE_CURRENT_SOURCE_DIR}/wl_protocols
                           ${INCLUDE_DIR}/wlroots-0.19 
                           ${INCLUDE_DIR}/pixman-1 
                           ${INCLUDE_DIR}/libdrm 
                           ${INCLUDE_DIR}/xkbcommon
                           ${INCLUDE_DIR}/wayland-protocols)

#install(TARGETS wl-compositor DESTINATION bin)
