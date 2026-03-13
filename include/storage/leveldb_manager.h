#pragma once

#include "block/block.h"
#include "state/state_manager.h"
#include "utils/json.hpp"

#include <cstdint>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace blockchain
{
    namespace storage
    {

        /**
         * LevelDBManager — File-based persistent chain & state storage.
         *
         * Uses a simple, crash-safe file approach with atomic writes:
         *   - chain.json  : Array of serialized blocks (the canonical chain)
         *   - state.json  : Serialized UTXO / spent-txID state
         *   - chain.json.tmp / state.json.tmp : Written first, then renamed (atomic swap)
         *
         * On startup:
         *   1. Load chain from disk
         *   2. Validate full chain integrity (hash links, PoW, Merkle, signatures)
         *   3. Rebuild state from chain if state file is missing or corrupt
         *
         * Why not actual LevelDB?
         *   LevelDB adds a native dependency that complicates Docker cross-compilation.
         *   This implementation provides the same guarantees (atomic writes, crash safety)
         *   using the rename-over-tmp pattern used by Redis, SQLite WAL, and many LSM stores.
         *   Swapping to real LevelDB later only requires replacing read/write internals.
         *
         * Thread safety: All public methods are mutex-guarded.
         */
        class LevelDBManager
        {
        public:
            /**
             * @param dataDir Directory where chain.json and state.json are stored.
             *                Created if it doesn't exist.
             */
            explicit LevelDBManager(const std::string &dataDir);

            // ─── Chain persistence ───────────────────────────────────────────

            /**
             * Load the full chain from disk.
             * @return The deserialized chain, or empty vector if no file / corrupt file.
             */
            std::vector<Block> loadChain();

            /**
             * Persist the full chain to disk atomically.
             * Writes to a .tmp file first, then renames over the target (crash safe).
             * @return true on success
             */
            bool saveChain(const std::vector<Block> &chain);

            /**
             * Append a single block to the persisted chain.
             * Reads current file, appends block, writes atomically.
             * @return true on success
             */
            bool appendBlock(const Block &block);

            // ─── State persistence ──────────────────────────────────────────

            /**
             * Save the UTXO / spent-tx state to disk atomically.
             * The state is stored as two JSON objects:
             *   - "ownershipMap" : { publicKey -> [txID, ...] }
             *   - "spentTxIDs"   : [txID, ...]
             */
            bool saveState(const StateManager &state);

            /**
             * Load state from disk into a StateManager.
             * @return true if state was loaded successfully; false = needs rebuild
             */
            bool loadState(StateManager &state);

            // ─── Integrity ─────────────────────────────────────────────────

            /**
             * Validate a loaded chain's integrity end-to-end.
             * Delegates to Blockchain::isChainValid (static).
             */
            static bool validateChainIntegrity(const std::vector<Block> &chain);

            /**
             * Get the data directory path.
             */
            const std::string &getDataDir() const { return dataDir_; }

            /**
             * Check if a persisted chain exists on disk.
             */
            bool hasPersistedChain() const;

        private:
            // Atomic write: write to .tmp, then rename to target
            bool atomicWrite(const std::string &filePath, const std::string &content);

            // Read a file into a string
            std::string readFile(const std::string &filePath);

            // Ensure directory exists
            void ensureDirectory(const std::string &dir);

            std::string dataDir_;
            std::string chainFilePath_;
            std::string stateFilePath_;
            mutable std::mutex mutex_;
        };

    } // namespace storage
} // namespace blockchain
