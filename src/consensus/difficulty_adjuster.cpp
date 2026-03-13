#include "consensus/difficulty_adjuster.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace blockchain
{

    DifficultyAdjuster::DifficultyAdjuster(
        uint32_t targetBlockTimeSec,
        uint32_t windowSize,
        double maxAdjustmentFactor,
        uint32_t minDifficulty,
        uint32_t maxDifficulty)
        : targetBlockTimeSec_(targetBlockTimeSec),
          windowSize_(windowSize),
          maxAdjustmentFactor_(maxAdjustmentFactor),
          minDifficulty_(minDifficulty),
          maxDifficulty_(maxDifficulty)
    {
        std::cout << "[Difficulty] Adjuster initialized: target=" << targetBlockTimeSec_
                  << "s, window=" << windowSize_
                  << ", maxFactor=" << maxAdjustmentFactor_
                  << ", range=[" << minDifficulty_ << "," << maxDifficulty_ << "]"
                  << std::endl;
    }

    uint32_t DifficultyAdjuster::calculateNextDifficulty(
        const std::vector<Block> &chain,
        uint32_t currentDifficulty) const
    {
        // Don't retarget if we don't have enough blocks
        if (chain.size() < windowSize_ + 1)
        {
            return currentDifficulty;
        }

        // Only retarget at windowSize boundaries
        if (!shouldRetarget(chain.size()))
        {
            return currentDifficulty;
        }

        // Get the window of blocks: from (chainSize - windowSize) to (chainSize - 1)
        size_t latestIdx = chain.size() - 1;
        size_t oldestIdx = chain.size() - 1 - windowSize_;

        int64_t latestTime = parseTimestamp(chain[latestIdx].timestamp);
        int64_t oldestTime = parseTimestamp(chain[oldestIdx].timestamp);

        // Actual time taken for the last windowSize blocks
        int64_t actualTimeSpan = latestTime - oldestTime;

        // Prevent division by zero or negative spans (timestamp manipulation)
        if (actualTimeSpan <= 0)
        {
            std::cerr << "[Difficulty] Invalid time span (" << actualTimeSpan
                      << "s). Keeping difficulty at " << currentDifficulty << std::endl;
            return currentDifficulty;
        }

        // Expected time for windowSize blocks
        int64_t expectedTimeSpan = static_cast<int64_t>(windowSize_) * targetBlockTimeSec_;

        // Calculate adjustment ratio
        // If blocks are too fast (actualTimeSpan < expected), ratio > 1 → increase difficulty
        // If blocks are too slow (actualTimeSpan > expected), ratio < 1 → decrease difficulty
        double ratio = static_cast<double>(expectedTimeSpan) / static_cast<double>(actualTimeSpan);

        // Clamp the adjustment factor to prevent extreme jumps
        double clampedRatio = std::max(1.0 / maxAdjustmentFactor_,
                                       std::min(maxAdjustmentFactor_, ratio));

        // Calculate new difficulty
        double newDiffExact = static_cast<double>(currentDifficulty) * clampedRatio;
        uint32_t newDifficulty = static_cast<uint32_t>(std::round(newDiffExact));

        // Clamp to [min, max]
        newDifficulty = std::max(minDifficulty_, std::min(maxDifficulty_, newDifficulty));

        std::cout << "[Difficulty] Retarget at height " << chain.size()
                  << ": actual=" << actualTimeSpan << "s"
                  << ", expected=" << expectedTimeSpan << "s"
                  << ", ratio=" << ratio
                  << ", clamped=" << clampedRatio
                  << ", old=" << currentDifficulty
                  << ", new=" << newDifficulty
                  << std::endl;

        return newDifficulty;
    }

    bool DifficultyAdjuster::validateBlockTimestamp(
        const Block &block,
        const Block &previousBlock)
    {
        int64_t blockTime = parseTimestamp(block.timestamp);
        int64_t prevTime = parseTimestamp(previousBlock.timestamp);

        // 1. Timestamp must be >= previous block (monotonic non-decreasing)
        if (blockTime < prevTime)
        {
            std::cerr << "[Difficulty] Block #" << block.index
                      << " timestamp is before previous block." << std::endl;
            return false;
        }

        // 2. Timestamp must not be > 2 minutes in the future
        auto now = std::chrono::system_clock::now();
        auto nowEpoch = std::chrono::duration_cast<std::chrono::seconds>(
                            now.time_since_epoch())
                            .count();

        if (blockTime > nowEpoch + 120)
        {
            std::cerr << "[Difficulty] Block #" << block.index
                      << " timestamp is too far in the future ("
                      << (blockTime - nowEpoch) << "s ahead)." << std::endl;
            return false;
        }

        return true;
    }

    bool DifficultyAdjuster::shouldRetarget(size_t chainLength) const
    {
        // Retarget when the number of non-genesis blocks is a multiple of windowSize
        // chainLength includes genesis, so retarget when (chainLength - 1) % windowSize == 0
        if (chainLength <= 1)
            return false;
        return ((chainLength - 1) % windowSize_) == 0;
    }

    int64_t DifficultyAdjuster::parseTimestamp(const std::string &isoTimestamp)
    {
        // Parse "YYYY-MM-DDTHH:MM:SSZ" format
        std::tm tm = {};
        std::istringstream ss(isoTimestamp);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

        if (ss.fail())
        {
            std::cerr << "[Difficulty] Failed to parse timestamp: " << isoTimestamp
                      << std::endl;
            return 0;
        }

        // Convert to epoch seconds (UTC)
#ifdef _WIN32
        return static_cast<int64_t>(_mkgmtime(&tm));
#else
        return static_cast<int64_t>(timegm(&tm));
#endif
    }

} // namespace blockchain
