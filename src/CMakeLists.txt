set(PRJ_EXECUTABLE webservice)
set(PRJ_SOURCE main.cpp)
add_executable(${PRJ_EXECUTABLE} ${PRJ_SOURCE})
target_link_libraries(${PRJ_EXECUTABLE} pistache)
