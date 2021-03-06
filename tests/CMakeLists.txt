# Set output dir
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests)

# Add executables
add_executable(basics ./basics.cpp)
add_executable(direct_sync_cpu ./direct_sync_cpu.cpp)
add_executable(supervisorsynth_cpu ./supervisorsynth_cpu.cpp)
add_executable(sync_supervisorsynth_cpu ./sync_supervisorsynth_cpu.cpp)
add_executable(superproxy ./superproxy.cpp)
add_executable(ct2 ./ct2.cpp)
add_executable(fsm ./fsm.cpp)
add_executable(fulllazy_ct5 ./fulllazy_ct5.cpp)
# add_executable(lazy_fsm ./lazy_fsm.cpp)
add_executable(projection_cpu ./projection_cpu.cpp)

if(CLDES_OPENCL_ENABLED)
    add_executable(basics_gpu ./basics_gpu.cpp)
endif(CLDES_OPENCL_ENABLED)

# Link libraries
if(CLDES_OPENMP_ENABLED)
    target_link_libraries(basics OpenMP::OpenMP_CXX)
    target_link_libraries(direct_sync_cpu OpenMP::OpenMP_CXX)
    target_link_libraries(supervisorsynth_cpu OpenMP::OpenMP_CXX)
    target_link_libraries(sync_supervisorsynth_cpu OpenMP::OpenMP_CXX)
    target_link_libraries(superproxy OpenMP::OpenMP_CXX)
    target_link_libraries(ct2 OpenMP::OpenMP_CXX)
    target_link_libraries(fsm OpenMP::OpenMP_CXX)
    target_link_libraries(fulllazy_ct5 OpenMP::OpenMP_CXX)
    # target_link_libraries(lazy_fsm OpenMP::OpenMP_CXX)
    target_link_libraries(projection_cpu OpenMP::OpenMP_CXX)
endif(CLDES_OPENMP_ENABLED)
if(CLDES_OPENCL_ENABLED)
    if(CLDES_OPENMP_ENABLED)
        target_link_libraries(basics_gpu OpenMP::OpenMP_CXX OpenCL::OpenCL)
    else()
        target_link_libraries(basics_gpu OpenCL::OpenCL)
    endif(CLDES_OPENMP_ENABLED)
endif(CLDES_OPENCL_ENABLED)
