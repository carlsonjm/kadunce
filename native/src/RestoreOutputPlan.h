#pragma once

namespace Kadunce {
enum class RestoreResult { Completed, OutputLost, Aborted };

// Candidates are a deduplicated value snapshot, not a queue or resize retry.
// Only topology loss permits moving on; user takeover/closure aborts outright.
template<class Candidates, class Available, class Apply>
RestoreResult restoreOnSurvivingOutput(const Candidates &candidates, Available available, Apply apply)
{
    for (const auto &candidate : candidates) {
        if (!available(candidate)) continue;
        const auto result = apply(candidate);
        if (result != RestoreResult::OutputLost) return result;
    }
    return RestoreResult::OutputLost;
}
}
