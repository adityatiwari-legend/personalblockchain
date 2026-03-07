#pragma once

#include "block/block.h"

#include <string>
#include <vector>

namespace blockchain
{
    namespace consensus
    {

        /**
         * IConsensusEngine — Abstract interface for pluggable consensus.
         *
         * This is the extension point that allows swapping PoW for PoS, PBFT,
         * Raft, or any other consensus algorithm WITHOUT rewriting the
         * networking layer, blockchain core, or HTTP API.
         *
         * The Blockchain class delegates three operations to the engine:
         *   1. validateBlock()   — Is this block valid according to the consensus rules?
         *   2. proposeBlock()    — Finalize a block so it satisfies consensus (e.g., mine it)
         *   3. shouldAcceptChain() — Given a candidate chain, should we replace ours?
         *
         * To implement a new consensus:
         *   1. Create a class that inherits from IConsensusEngine
         *   2. Implement the three pure virtual methods
         *   3. Pass an instance to Blockchain's constructor
         *
         * This design intentionally separates consensus from persistence, networking,
         * and state. The consensus engine is stateless w.r.t. peer connections.
         */
        class IConsensusEngine
        {
        public:
            virtual ~IConsensusEngine() = default;

            /**
             * Get the name of this consensus engine (for logging / API).
             */
            virtual std::string name() const = 0;

            /**
             * Validate a single block against the consensus rules.
             *
             * For PoW: checks that hash meets difficulty target.
             * For PoS: checks validator signature and stake.
             *
             * @param block The block to validate
             * @param previousBlock The block before it (for context)
             * @return true if the block passes consensus validation
             */
            virtual bool validateBlock(const Block &block,
                                       const Block &previousBlock) const = 0;

            /**
             * Finalize (seal) a block so it satisfies the consensus rules.
             *
             * For PoW: this performs the mining loop (increment nonce until target met).
             * For PoS: this signs the block with the validator's key.
             *
             * @param block The block to seal (modified in-place)
             * @param context Arbitrary context string (e.g., miner address, validator key)
             */
            virtual void sealBlock(Block &block, const std::string &context) const = 0;

            /**
             * Determine whether a candidate chain should replace the current one.
             *
             * For PoW: longest valid chain wins (Nakamoto consensus).
             * For PoS: could use total stake weight, finality gadget, etc.
             *
             * @param currentChain The node's current chain
             * @param candidateChain The proposed replacement chain
             * @return true if candidateChain should replace currentChain
             */
            virtual bool shouldAcceptChain(
                const std::vector<Block> &currentChain,
                const std::vector<Block> &candidateChain) const = 0;

            /**
             * Calculate the difficulty for the next block (if applicable).
             * Consensus engines that don't use difficulty can return 0.
             *
             * @param chain The current chain
             * @param currentDifficulty The current difficulty setting
             * @return The difficulty for the next block
             */
            virtual uint32_t nextDifficulty(
                const std::vector<Block> &chain,
                uint32_t currentDifficulty) const = 0;
        };

    } // namespace consensus
} // namespace blockchain
