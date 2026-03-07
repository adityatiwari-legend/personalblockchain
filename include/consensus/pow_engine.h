#pragma once

#include "consensus/consensus_engine.h"
#include "consensus/difficulty_adjuster.h"

#include <iostream>

namespace blockchain
{
    namespace consensus
    {

        /**
         * PoWEngine — Proof-of-Work consensus engine.
         *
         * This wraps the existing mining logic into the pluggable consensus interface.
         * It also integrates the DifficultyAdjuster for dynamic retargeting.
         *
         * Block sealing: increments nonce until hash has `difficulty` leading zeros.
         * Chain selection: longest valid chain wins (Nakamoto consensus).
         * Difficulty: retargets every `windowSize` blocks using DifficultyAdjuster.
         */
        class PoWEngine : public IConsensusEngine
        {
        public:
            /**
             * @param adjuster The dynamic difficulty adjuster instance
             */
            explicit PoWEngine(DifficultyAdjuster adjuster = DifficultyAdjuster())
                : adjuster_(std::move(adjuster))
            {
                std::cout << "[Consensus] PoW engine initialized. Target block time: "
                          << adjuster_.getTargetBlockTime() << "s, window: "
                          << adjuster_.getWindowSize() << " blocks." << std::endl;
            }

            std::string name() const override { return "Proof-of-Work"; }

            bool validateBlock(const Block &block,
                               const Block &previousBlock) const override
            {
                // 1. Verify hash integrity
                if (block.hash != block.calculateHash())
                {
                    std::cerr << "[PoW] Block #" << block.index << " hash mismatch" << std::endl;
                    return false;
                }

                // 2. Verify difficulty target
                std::string target(block.difficulty, '0');
                if (block.hash.substr(0, block.difficulty) != target)
                {
                    std::cerr << "[PoW] Block #" << block.index << " doesn't meet difficulty" << std::endl;
                    return false;
                }

                // 3. Verify hash link to previous block
                if (block.previousHash != previousBlock.hash)
                {
                    std::cerr << "[PoW] Block #" << block.index << " invalid previousHash" << std::endl;
                    return false;
                }

                // 4. Verify Merkle root
                if (block.merkleRoot != block.computeMerkleRoot())
                {
                    std::cerr << "[PoW] Block #" << block.index << " invalid Merkle root" << std::endl;
                    return false;
                }

                // 5. Verify timestamp (anti-manipulation)
                if (!DifficultyAdjuster::validateBlockTimestamp(block, previousBlock))
                {
                    return false;
                }

                // 6. Verify all transaction signatures
                if (!block.hasValidTransactions())
                {
                    std::cerr << "[PoW] Block #" << block.index << " has invalid transactions" << std::endl;
                    return false;
                }

                return true;
            }

            void sealBlock(Block &block, const std::string & /*context*/) const override
            {
                // The existing mineBlock() already implements the PoW nonce search
                block.mineBlock();
            }

            bool shouldAcceptChain(
                const std::vector<Block> &currentChain,
                const std::vector<Block> &candidateChain) const override
            {
                // Nakamoto consensus: longest valid chain wins
                return candidateChain.size() > currentChain.size();
            }

            uint32_t nextDifficulty(
                const std::vector<Block> &chain,
                uint32_t currentDifficulty) const override
            {
                return adjuster_.calculateNextDifficulty(chain, currentDifficulty);
            }

        private:
            DifficultyAdjuster adjuster_;
        };

    } // namespace consensus
} // namespace blockchain
