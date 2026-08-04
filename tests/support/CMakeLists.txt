# Fakes and helpers shared by every layer's tests.
#
# It lives under tests/ rather than libs/ because it is not shipped: nothing in
# the application links it, and the install rules never see it.
add_library(typeit_test_support STATIC src/LogBuilder.cpp src/TempEnv.cpp)
add_library(typeit::test_support ALIAS typeit_test_support)

target_include_directories(typeit_test_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(typeit_test_support PUBLIC typeit::core)

typeit_set_warnings(typeit_test_support)
typeit_enable_coverage(typeit_test_support)
typeit_enable_sanitizers(typeit_test_support)

# The allocation counter replaces global operator new and delete, which is a
# whole-binary decision and not one a test target should inherit by accident.
# Kept out of test_support so that linking it is a choice: a binary that never
# counts allocations keeps the sanitizer's own allocator, and with it ASan's
# new/delete mismatch detection.
add_library(typeit_allocation_counter STATIC src/AllocationCounter.cpp)
add_library(typeit::allocation_counter ALIAS typeit_allocation_counter)

target_include_directories(typeit_allocation_counter PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

typeit_set_warnings(typeit_allocation_counter)
typeit_enable_coverage(typeit_allocation_counter)
typeit_enable_sanitizers(typeit_allocation_counter)
