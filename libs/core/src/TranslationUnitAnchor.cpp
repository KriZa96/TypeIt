// A static library needs a translation unit, and TI-023 deliberately ships no
// domain code: TI-024 through TI-026 are header-only, so the first real source
// here arrives with the UTF-8 decoder in TI-027. Delete this file then.
namespace typeit::core {
    namespace {
        [[maybe_unused]] constexpr int kTranslationUnitAnchor = 0;
    }  // namespace
}  // namespace typeit::core
