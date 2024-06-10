file(GLOB_RECURSE HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_library(platform-acc-lenet
ethernet.cpp
display.cpp
${HEADERS})

target_include_directories(platform-acc-lenet PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(platform-acc-lenet systemc)

add_executable(riscv-vp-acc-lenet
        main.cpp)

target_link_libraries(riscv-vp-acc-lenet rv32 platform-acc-lenet platform-common gdb-mc ${Boost_LIBRARIES} systemc pthread)

INSTALL(TARGETS riscv-vp-acc-lenet RUNTIME DESTINATION bin)
