#pragma once

#include <mutex>

#include "moilcali_algorithm.h"

namespace ComputeOps {

// MoilCali's noise-cleaning switch is process-wide state, set from a checkbox in
// the client. A node answering several clients must not inherit whatever the last
// request happened to leave behind, so every op sets it from its own params and
// restores it afterwards.
//
// The lock is the part that matters: without it two concurrent requests interleave
// their set and restore, and one gets computed under the other's setting. That
// failure is silent and intermittent -- the numbers are plausible, just not the
// ones the operator's checkbox asked for.
// `raw` rides along for the same reason and under the same lock: it is the same
// kind of process-wide switch, set per request and restored after. It defaults to
// false so every existing caller keeps exactly the behaviour it has.
class CleaningScope {
public:
    explicit CleaningScope(bool on, bool raw = false)
        : lock_(mutex()),
          prev_(MoilCali::noise_cleaning_enabled()),
          prevRaw_(MoilCali::raw_nodes_enabled()) {
        MoilCali::set_noise_cleaning(on);
        MoilCali::set_raw_nodes(raw);
    }
    ~CleaningScope() {
        MoilCali::set_noise_cleaning(prev_);
        MoilCali::set_raw_nodes(prevRaw_);
    }

    CleaningScope(const CleaningScope &) = delete;
    CleaningScope &operator=(const CleaningScope &) = delete;

private:
    static std::mutex &mutex() {
        static std::mutex m;
        return m;
    }
    std::unique_lock<std::mutex> lock_;
    bool prev_;
    bool prevRaw_;
};

}  // namespace ComputeOps
