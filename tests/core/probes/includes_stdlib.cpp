// The control. A negative test that cannot distinguish "the include is
// unreachable" from "the compiler never ran" proves nothing, so this builds
// under exactly the same setup and must succeed.
#include <string>

int uses_stdlib() { return static_cast<int>(std::string("typeit").size()); }
