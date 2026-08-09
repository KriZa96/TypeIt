// The numbers a race is run by (GAMEPLAY section 3.5).
//
// Separate from `RaceConfig`, which is the shape of the configuration file.
// This is the *effective* set: presets resolved, overrides applied, values
// validated. The controller reads only this, so it can be handed a set nobody
// wrote down and tested against it (TI-122); turning a config file into one is
// TI-123's job.
//
// The defaults are the Standard preset, because a default that is not one of
// the presets is a fourth difficulty nobody chose.
#ifndef TYPEIT_CORE_RACE_RACEPARAMS_H
#define TYPEIT_CORE_RACE_RACEPARAMS_H

#include <cstddef>

#include "typeit/core/util/Units.h"

namespace typeit::core {

    struct RaceParams {
        /// `k_up`: how fast the target speed climbs, in WPM per second, when
        /// the typist is comfortably ahead and accurate.
        double ramp_up = 0.60;

        /// `k_down`: how fast it backs off. Larger than `ramp_up` in every
        /// shipped preset, so a stumble buys breathing room rather than
        /// starting a death spiral — recovery is meant to be possible.
        double ramp_down = 1.50;

        /// `A_min`: below this rolling accuracy, no amount of lead earns any
        /// speed. The single term that makes the mode teach typing rather than
        /// teach mashing.
        Accuracy min_accuracy{0.92};

        /// The lead at which climbing begins, and the lead below which the
        /// speed backs off. Between them is the dead band.
        double lead_comfort = 25.0;
        double lead_danger = 8.0;

        /// How much lead beyond `lead_comfort` counts as fully comfortable.
        /// The ramp is proportional up to this and flat after it, so a typist
        /// two hundred graphemes ahead does not accelerate away.
        double lead_scale = 30.0;

        /// The band the target speed is held in whatever the ramp says.
        ///
        /// `min_speed` is below the adaptive-start floor of 20 on purpose: a
        /// race that backs off has to be able to reach a speed the typist can
        /// actually hold, and stopping at the floor they started from would
        /// mean the mode cannot help somebody having a bad day.
        Wpm min_speed{10.0};
        /// Well above any human sustained rate. It exists so that a bug in the
        /// ramp cannot run the pacer off into arithmetic nobody can read, not
        /// because anybody will reach it.
        Wpm max_speed{300.0};

        /// How long `lead <= 0` has to last before the pacer catches the
        /// typist. A single fumbled keystroke must not end a run; a sustained
        /// inability to keep up must.
        Millis grace{300};

        /// Being caught costs a life rather than the run, until there are none
        /// left.
        std::size_t lives = 1;

        /// What being caught costs in speed, as a fraction.
        double catch_penalty = 0.10;

        /// The window rolling accuracy is measured over, in graphemes.
        std::size_t accuracy_window = 50;

        /// How long a speed must be held to count as sustained, which is what
        /// a personal best and the next race's starting speed are taken from —
        /// **not** the peak instantaneous value, which any lucky burst
        /// inflates (GAMEPLAY section 3.4).
        Millis sustain_window{10'000};

        /// `α`: the fraction of the sustained best the next race starts at.
        double start_factor = 0.85;
    };

}  // namespace typeit::core

#endif  // TYPEIT_CORE_RACE_RACEPARAMS_H
