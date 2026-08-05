// Keys, as a person writes them and as a terminal sends them.
//
// The rule throughout: a binding that cannot be understood is a message, never
// a key that silently does nothing. Somebody who mistypes `ctlr-q` should be
// told, not left pressing it.

#include <ftxui/component/event.hpp>
#include <gtest/gtest.h>
#include <map>
#include <string>
#include <string_view>

#include "Keymap.h"
#include "typeit/core/util/Result.h"

namespace typeit::tui {
    namespace {

        Binding parsed(std::string_view text) {
            const core::Result<Binding> binding = parse_binding(text);
            EXPECT_TRUE(binding) << "could not parse \"" << text << "\"";
            return binding.value_or(Binding{});
        }

        // --- Parsing ------------------------------------------------------------

        TEST(KeyBindingParserTest, TheDocumentedFormsAllParse) {
            EXPECT_EQ(parsed("ctrl-q"), (Binding{.key = "q", .ctrl = true, .alt = false}));
            EXPECT_EQ(parsed("escape"), (Binding{.key = "escape", .ctrl = false, .alt = false}));
            EXPECT_EQ(parsed("f1"), (Binding{.key = "f1", .ctrl = false, .alt = false}));
            EXPECT_EQ(parsed("alt-x"), (Binding{.key = "x", .ctrl = false, .alt = true}));
            EXPECT_EQ(parsed("?"), (Binding{.key = "?", .ctrl = false, .alt = false}));
            EXPECT_EQ(parsed("ctrl-,"), (Binding{.key = ",", .ctrl = true, .alt = false}));
        }

        TEST(KeyBindingParserTest, EveryFormRoundTripsThroughItsCanonicalSpelling) {
            // What lets a settings screen write back what it read without
            // rewriting somebody's whole file.
            for (const std::string_view spelled: {"ctrl-q", "escape", "f1", "alt-x", "?", "ctrl-,", "ctrl-alt-z"}) {
                const std::string canonical = to_string(parsed(spelled));
                EXPECT_EQ(parsed(canonical), parsed(spelled)) << spelled;
                EXPECT_EQ(to_string(parsed(canonical)), canonical) << spelled;
            }
        }

        TEST(KeyBindingParserTest, ParsingIsCaseInsensitiveAndToleratesWhitespace) {
            // A configuration file is written by a person.
            EXPECT_EQ(parsed("Ctrl-Q"), parsed("ctrl-q"));
            EXPECT_EQ(parsed("CTRL-Q"), parsed("ctrl-q"));
            EXPECT_EQ(parsed("  ctrl-q  "), parsed("ctrl-q"));
            EXPECT_EQ(parsed("F1"), parsed("f1"));
        }

        TEST(KeyBindingParserTest, ModifiersCombineInEitherOrder) {
            EXPECT_EQ(parsed("ctrl-alt-z"), parsed("alt-ctrl-z"));
            EXPECT_EQ(to_string(parsed("alt-ctrl-z")), "ctrl-alt-z") << "and canonicalise to one order";
        }

        TEST(KeyBindingParserTest, AnUnparseableBindingNamesTheOffendingString) {
            for (const std::string_view bad: {"", "ctrl-", "ctlr-q", "f13", "ctrl-nonsense", "  "}) {
                const core::Result<Binding> binding = parse_binding(bad);

                ASSERT_FALSE(binding) << "accepted \"" << bad << "\"";
                EXPECT_EQ(binding.error().code, core::ErrorCode::InvalidKeyBinding);
                EXPECT_EQ(binding.error().context, bad) << "and says which one";
            }
        }

        TEST(KeyBindingParserTest, EveryFunctionKeyToTwelveParsesAndNoFurther) {
            for (int number = 1; number <= 12; ++number) {
                const std::string key = "f" + std::to_string(number);
                EXPECT_TRUE(parse_binding(key)) << key;
            }
            EXPECT_FALSE(parse_binding("f0"));
            EXPECT_FALSE(parse_binding("f13"));
        }

        // --- Delivery -------------------------------------------------------------

        TEST(KeymapTest, ABindingIsMatchedAgainstTheEventATerminalSends) {
            EXPECT_TRUE(matches(parsed("escape"), ftxui::Event::Escape));
            EXPECT_TRUE(matches(parsed("f1"), ftxui::Event::F1));
            EXPECT_TRUE(matches(parsed("?"), ftxui::Event::Character('?')));
            EXPECT_TRUE(matches(parsed("ctrl-q"), ftxui::Event::Special(std::string(1, '\x11'))));

            EXPECT_FALSE(matches(parsed("escape"), ftxui::Event::Return));
            EXPECT_FALSE(matches(parsed("ctrl-q"), ftxui::Event::Character('q')));
        }

        TEST(KeymapTest, BindingsATerminalCannotSendAreFlaggedRatherThanIgnored) {
            // `ctrl-,` is the documented example — GAMEPLAY §9 gives Settings
            // an F2 fallback precisely because of it.
            EXPECT_FALSE(is_deliverable(parsed("ctrl-,")));
            EXPECT_TRUE(is_deliverable(parsed("ctrl-q")));
            EXPECT_TRUE(is_deliverable(parsed("f2")));
            EXPECT_TRUE(is_deliverable(parsed("escape")));
        }

        TEST(KeymapTest, AnUndeliverableBindingIsRecordedForTheDoctor) {
            const Keymap keymap = Keymap::from_config({{"settings", "ctrl-,"}});

            ASSERT_EQ(keymap.unreachable().size(), 1U);
            EXPECT_NE(keymap.unreachable().front().find("settings"), std::string::npos);
            EXPECT_EQ(keymap.binding(Action::Settings), parsed("ctrl-,"))
                    << "and kept, because a terminal that can send it should work";
        }

        // --- The map ---------------------------------------------------------------

        TEST(KeymapTest, EveryActionHasADefault) {
            const Keymap keymap;

            for (const Action action: kAllActions) {
                EXPECT_FALSE(keymap.binding(action).key.empty()) << to_string(action);
            }
        }

        TEST(KeymapTest, TheDefaultsAreTheOnesTheManualDocuments) {
            const Keymap keymap;

            EXPECT_EQ(to_string(keymap.binding(Action::QuitOrBack)), "escape");
            EXPECT_EQ(to_string(keymap.binding(Action::ForceQuit)), "ctrl-q");
            EXPECT_EQ(to_string(keymap.binding(Action::Restart)), "ctrl-r");
            EXPECT_EQ(to_string(keymap.binding(Action::NewText)), "ctrl-n");
            EXPECT_EQ(to_string(keymap.binding(Action::History)), "ctrl-h");
            EXPECT_EQ(to_string(keymap.binding(Action::TextLibrary)), "ctrl-l");
            EXPECT_EQ(to_string(keymap.binding(Action::Help)), "f1");
        }

        TEST(KeymapTest, ControlTIsNotADefaultAnywhere) {
            // It is SIGINFO on BSD, a tab key in several emulators, and the
            // common tmux prefix. This test's only job is to keep it that way.
            const Keymap keymap;

            for (const Action action: kAllActions) {
                EXPECT_NE(to_string(keymap.binding(action)), "ctrl-t") << to_string(action);
            }
        }

        TEST(KeymapTest, SettingsShipsWithTheDeliverableFallback) {
            // UX asks for `ctrl-,`, which most terminals cannot send. Shipping
            // it as the default would be shipping a key nobody can press.
            const Keymap keymap;

            EXPECT_TRUE(is_deliverable(keymap.binding(Action::Settings)));
            EXPECT_EQ(to_string(keymap.binding(Action::Settings)), "f2");
        }

        TEST(KeymapTest, ConfigurationOverridesADefault) {
            const Keymap keymap = Keymap::from_config({{"force_quit", "ctrl-x"}});

            EXPECT_EQ(to_string(keymap.binding(Action::ForceQuit)), "ctrl-x");
            EXPECT_EQ(to_string(keymap.binding(Action::Help)), "f1") << "and leaves the rest alone";
            EXPECT_TRUE(keymap.warnings().empty());
        }

        TEST(KeymapTest, AnUnparseableBindingWarnsAndTheDefaultStands) {
            // A keymap that refused to load would leave the program with no
            // keys at all, which is worse than one wrong key.
            const Keymap keymap = Keymap::from_config({{"force_quit", "ctlr-x"}});

            EXPECT_EQ(to_string(keymap.binding(Action::ForceQuit)), "ctrl-q");
            ASSERT_EQ(keymap.warnings().size(), 1U);
            EXPECT_NE(keymap.warnings().front().find("ctlr-x"), std::string::npos) << "and names what was written";
        }

        TEST(KeymapTest, AnActionNobodyRecognisesWarns) {
            const Keymap keymap = Keymap::from_config({{"teleport", "ctrl-x"}});

            ASSERT_EQ(keymap.warnings().size(), 1U);
            EXPECT_NE(keymap.warnings().front().find("teleport"), std::string::npos);
        }

        TEST(KeymapTest, ADuplicateBindingIsRefusedNamingBoth) {
            // "That key is taken" is no use without saying by what.
            const Keymap keymap = Keymap::from_config({{"force_quit", "ctrl-x"}, {"restart", "ctrl-x"}});

            ASSERT_FALSE(keymap.warnings().empty());
            const std::string& warning = keymap.warnings().front();
            EXPECT_NE(warning.find("ctrl-x"), std::string::npos) << warning;
            EXPECT_NE(warning.find("force_quit"), std::string::npos) << warning;
        }

        TEST(KeymapTest, AKeyPressIsResolvedToItsAction) {
            const Keymap keymap;

            EXPECT_EQ(keymap.action_for(ftxui::Event::F1), Action::Help);
            EXPECT_EQ(keymap.action_for(ftxui::Event::Special(std::string(1, '\x11'))), Action::ForceQuit);
            EXPECT_FALSE(keymap.action_for(ftxui::Event::Character('z')).has_value());
        }

        TEST(KeymapTest, EveryActionHasAConfigurationName) {
            for (const Action action: kAllActions) {
                const std::string_view name = to_string(action);
                EXPECT_NE(name, "unknown") << static_cast<int>(action);

                Action found{};
                EXPECT_TRUE(action_from(name, found)) << name;
                EXPECT_EQ(found, action) << name;
            }
        }

    }  // namespace
}  // namespace typeit::tui
