# Fakes and helpers shared by every layer's tests.
#
# It lives under tests/ rather than libs/ because it is not shipped: nothing in
# the application links it, and the install rules never see it.
add_library(typeit_test_support STATIC src/AllocationCounter.cpp)
add_library(typeit::test_support ALIAS typeit_test_support)

target_include_directories(typeit_test_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(typeit_test_support PUBLIC typeit::core)

typeit_set_warnings(typeit_test_support)
typeit_enable_sanitizers(typeit_test_support)
