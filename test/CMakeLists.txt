macro(add_fcl_test test_name)
  add_executable(${ARGV})
  target_link_libraries(${test_name}
    fcl
    ${Boost_SYSTEM_LIBRARY_RELATIVE_PATHS}
    ${Boost_THREAD_LIBRARY_RELATIVE_PATHS}
    ${Boost_DATE_TIME_LIBRARY_RELATIVE_PATHS}
    ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY_RELATIVE_PATHS})
	  
  add_test(${test_name} ${EXECUTABLE_OUTPUT_PATH}/${test_name})
endmacro(add_fcl_test)

# configure location of resources
file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/fcl_resources" TEST_RESOURCES_DIR)
if(WIN32)
    # Correct directory separator for Windows
    string(REPLACE "\\" "\\\\" TEST_RESOURCES_DIR ${TEST_RESOURCES_DIR})
endif(WIN32)
configure_file("${TEST_RESOURCES_DIR}/config.h.in" "${TEST_RESOURCES_DIR}/config.h")

include_directories(.)

add_fcl_test(test_fcl_collision test_fcl_collision.cpp test_fcl_utility.cpp)
add_fcl_test(test_fcl_continuous_collision test_fcl_continuous_collision.cpp test_fcl_utility.cpp)
add_fcl_test(test_fcl_distance test_fcl_distance.cpp test_fcl_utility.cpp)
add_fcl_test(test_fcl_geometric_shapes test_fcl_geometric_shapes.cpp test_fcl_utility.cpp)
add_fcl_test(test_fcl_broadphase test_fcl_broadphase.cpp test_fcl_utility.cpp)
add_fcl_test(test_fcl_shape_mesh_consistency test_fcl_shape_mesh_consistency.cpp test_fcl_utility.cpp)
add_fcl_test(test_fcl_frontlist test_fcl_frontlist.cpp test_fcl_utility.cpp)
add_fcl_test(test_fcl_math test_fcl_math.cpp test_fcl_utility.cpp)
add_fcl_test(test_fcl_articulated_model test_fcl_articulated_model.cpp)
add_fcl_test(test_fcl_articulated_collision test_fcl_articulated_collision.cpp test_fcl_utility.cpp)
add_fcl_test(test_fcl_interpolation test_fcl_interpolation.cpp boost_auto_param_test_case.hpp)
add_fcl_test(test_fcl_broadphase_continues test_fcl_broadphase_continues.cpp test_fcl_utility.cpp)
add_fcl_test(test_fcl_conservative_advancement test_fcl_conservative_advancement.cpp test_fcl_utility.cpp)

if (FCL_HAVE_OCTOMAP)
  add_fcl_test(test_fcl_octomap test_fcl_octomap.cpp test_fcl_utility.cpp)
endif()
