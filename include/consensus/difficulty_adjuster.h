#pragma once

#include "block/block.h"

#include <cstdint>
#include <string>
#include <vector>

namespace blockchain
{

    /**
     * DifficultyAdjuster — Dynamic difficulty retargeting engine.
     *
     * Measures average block time over the last N blocks and adjusts difficulty
     * to converge on a target block interval. Prevents timestamp manipulation
     * by clamping the adjustment factor and validating monotonic timestamps.
     *
     * Algorithm:
     *   1. Take the last `windowSize` blocks
     *   2. Compute actualTimeSpan = timestamp(latest) - timestamp(oldest)
     *   3. Compute expectedTimeSpan = windowSize * targetBlockTimeSeconds
     *   4. ratio = expectedTimeSpan / actualTimeSpan
     *      - If ratio > maxAdjustmentFactor → clamp to maxAdjustmentFactor
     *      - If ratio < 1/maxAdjustmentFactor → clamp to 1/maxAdjustmentFactor
     *   5. newDifficulty = currentDifficulty * ratio (rounded)
     *   6. Clamp to [minDifficulty, maxDifficulty]
     *
     * Anti-manipulation:
     *   - Timestamps must be monotonically non-decreasing
     *   - Blocks with future timestamps (> 2 minutes ahead) are rejected
     *   - The clamp factor prevents extreme jumps (max 4x change per epoch)
     */
    class DifficultyAdjuster
    {
    public:
        /**
         * @param targetBlockTimeSec Desired seconds between blocks (default: 10)
         * @param windowSize         Number of blocks in the retarget window (default: 10)
         * @param maxAdjustmentFactor Max multiplier per retarget (default: 4.0)
         * @param minDifficulty       Floor difficulty (default: 1)
         * @param maxDifficulty       Ceiling difficulty (default: 8)
         */
        explicit DifficultyAdjuster(
            uint32_t targetBlockTimeSec = 10,
            uint32_t windowSize = 10,
            double maxAdjustmentFactor = 4.0,
            uint32_t minDifficulty = 1,
            uint32_t maxDifficulty = 8);

        /**
         * Calculate the next difficulty based on the current chain.
         * Only retargets when chain.size() is a multiple of windowSize.
         *
         * @param chain The current chain (must include the latest block)
         * @param currentDifficulty The current difficulty level
         * @return The new difficulty to use for the next block
         */
        uint32_t calculateNextDifficulty(
            const std::vector<Block> &chain,
            uint32_t currentDifficulty) const;

        /**
         * Validate that a block's timestamp is acceptable.
         * - Must be >= previous block timestamp (no time travel)
         * - Must not be > 2 minutes in the future
         *
         * @param block The block to validate
         * @param previousBlock The block before it
         * @return true if timestamp is valid
         */
        static bool validateBlockTimestamp(
            const Block &block,
            const Block &previousBlock);

        /**
         * Check if a retarget should happen at this chain height.
         */
        bool shouldRetarget(size_t chainLength) const;

        // Getters
        uint32_t getTargetBlockTime() const { return targetBlockTimeSec_; }
        uint32_t getWindowSize() const { return windowSize_; }

    private:
        /**
         * Parse an ISO 8601 timestamp string to Unix epoch seconds.
         */
        static int64_t parseTimestamp(const std::string &isoTimestamp);

        uint32_t targetBlockTimeSec_;
        uint32_t windowSize_;
        double maxAdjustmentFactor_;
        uint32_t minDifficulty_;
        uint32_t maxDifficulty_;
    };

} // namespace blockchain
