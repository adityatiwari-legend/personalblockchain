#pragma once

#include "consensus/consensus_engine.h"

#include <iostream>
#include <stdexcept>

namespace blockchain
{
    namespace consensus
    {

        /**
         * PoSEngine — Proof-of-Stake consensus engine (SKELETON / INTERFACE ONLY).
         *
         * This is a structural placeholder that defines the contract for a future
         * Proof-of-Stake implementation. It compiles, links, and can be passed to
         * the Blockchain constructor — but its seal/validate methods throw
         * std::runtime_error since the staking logic is not yet implemented.
         *
         * To complete PoS, you would need:
         *   1. A stake registry mapping validator pubkeys to staked amounts
         *   2. A deterministic validator selection function (e.g., weighted random by stake)
         *   3. Block sealing via validator ECDSA signature instead of nonce mining
         *   4. Slashing conditions for double-signing or equivocation
         *   5. Epoch-based finality (optional: Casper FFG, Tendermint, etc.)
         *
         * This skeleton exists so the consensus interface can be validated at compile
         * time without the full staking implementation. It also serves as documentation
         * for the contract a PoS engine must fulfill.
         */
        class PoSEngine : public IConsensusEngine
        {
        public:
            std::string name() const override { return "Proof-of-Stake (skeleton)"; }

            bool validateBlock(const Block &block,
                               const Block &previousBlock) const override
            {
                // Structural validation still applies
                if (block.previousHash != previousBlock.hash)
                {
                    return false;
                }
                if (block.merkleRoot != block.computeMerkleRoot())
                {
                    return false;
                }

                // TODO: Verify that the block proposer is a valid staked validator
                // TODO: Verify the validator's ECDSA signature on the block hash
                // TODO: Verify the proposer was selected by the deterministic leader schedule

                std::cerr << "[PoS] WARNING: PoS validation is a skeleton — "
                          << "block accepted without stake verification." << std::endl;
                return true;
            }

            void sealBlock(Block &block, const std::string &validatorKey) const override
            {
                // In a real PoS:
                //   1. Look up validatorKey in stake registry
                //   2. Verify the validator is the scheduled proposer for this slot
                //   3. Sign the block hash with the validator's private key
                //   4. Set block.hash = calculateHash() (no mining needed)

                (void)validatorKey;

                // For the skeleton, just compute the hash without mining
                block.merkleRoot = block.computeMerkleRoot();
                block.nonce = 0; // No nonce grinding in PoS
                block.hash = block.calculateHash();

                std::cout << "[PoS] Block #" << block.index
                          << " sealed (skeleton — no stake verification)." << std::endl;
            }

            bool shouldAcceptChain(
                const std::vector<Block> &currentChain,
                const std::vector<Block> &candidateChain) const override
            {
                // In PoS, chain selection might use total stake weight instead of length.
                // For the skeleton, fall back to longest chain.
                return candidateChain.size() > currentChain.size();
            }

            uint32_t nextDifficulty(
                const std::vector<Block> & /*chain*/,
                uint32_t /*currentDifficulty*/) const override
            {
                // PoS doesn't use difficulty — return 0 to signal "not applicable"
                return 0;
            }
        };

    } // namespace consensus
} // namespace blockchain
