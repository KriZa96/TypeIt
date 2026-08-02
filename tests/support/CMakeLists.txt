# Fakes and helpers shared by every layer's tests. Header-only today, so it is
# an INTERFACE target; the first .cpp turns it into a STATIC one.
#
# It lives under tests/ rather than libs/ because it is not shipped: nothing in
# the application links it, and the install rules never see it.
add_library(typeit_test_support INTERFACE)
add_library(typeit::test_support ALIAS typeit_test_support)

target_include_directories(typeit_test_support INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(typeit_test_support INTERFACE typeit::core)
