#include "storage/leveldb_manager.h"
#include "core/blockchain.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace blockchain
{
    namespace storage
    {

        // ─── Constructor ──────────────────────────────────────────────────

        LevelDBManager::LevelDBManager(const std::string &dataDir)
            : dataDir_(dataDir),
              chainFilePath_(dataDir + "/chain.json"),
              stateFilePath_(dataDir + "/state.json")
        {
            ensureDirectory(dataDir_);
            std::cout << "[Storage] Data directory: " << dataDir_ << std::endl;
        }

        // ─── Chain persistence ────────────────────────────────────────────

        std::vector<Block> LevelDBManager::loadChain()
        {
            std::lock_guard<std::mutex> lock(mutex_);

            std::vector<Block> chain;
            std::string raw = readFile(chainFilePath_);
            if (raw.empty())
            {
                std::cout << "[Storage] No persisted chain found." << std::endl;
                return chain;
            }

            try
            {
                nlohmann::json j = nlohmann::json::parse(raw);
                if (!j.is_array())
                {
                    std::cerr << "[Storage] Chain file is not a JSON array. Ignoring." << std::endl;
                    return chain;
                }

                for (const auto &blockJson : j)
                {
                    chain.push_back(Block::fromJson(blockJson));
                }

                std::cout << "[Storage] Loaded " << chain.size()
                          << " blocks from disk." << std::endl;
            }
            catch (const std::exception &e)
            {
                std::cerr << "[Storage] Failed to parse chain file: " << e.what()
                          << std::endl;
                chain.clear();
            }

            return chain;
        }

        bool LevelDBManager::saveChain(const std::vector<Block> &chain)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            nlohmann::json j = nlohmann::json::array();
            for (const auto &block : chain)
            {
                j.push_back(block.toJson());
            }

            bool ok = atomicWrite(chainFilePath_, j.dump(2));
            if (ok)
            {
                std::cout << "[Storage] Saved " << chain.size()
                          << " blocks to disk." << std::endl;
            }
            return ok;
        }

        bool LevelDBManager::appendBlock(const Block &block)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            // Read existing chain
            nlohmann::json j;
            std::string raw = readFile(chainFilePath_);
            if (!raw.empty())
            {
                try
                {
                    j = nlohmann::json::parse(raw);
                    if (!j.is_array())
                    {
                        j = nlohmann::json::array();
                    }
                }
                catch (...)
                {
                    j = nlohmann::json::array();
                }
            }
            else
            {
                j = nlohmann::json::array();
            }

            j.push_back(block.toJson());

            bool ok = atomicWrite(chainFilePath_, j.dump(2));
            if (ok)
            {
                std::cout << "[Storage] Appended block #" << block.index
                          << " to disk. Total blocks: " << j.size() << std::endl;
            }
            return ok;
        }

        // ─── State persistence ────────────────────────────────────────────

        bool LevelDBManager::saveState(const StateManager &state)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            nlohmann::json j;
            j["spentTxIDs"] = state.getAllSpentTxIDs();

            // We serialize what we can get from the public API.
            // The ownership map is not directly exposed, but the spent txIDs
            // are sufficient — state can always be rebuilt from the chain.
            // This is a fast-path cache.

            bool ok = atomicWrite(stateFilePath_, j.dump(2));
            if (ok)
            {
                std::cout << "[Storage] Saved state to disk." << std::endl;
            }
            return ok;
        }

        bool LevelDBManager::loadState(StateManager & /*state*/)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            std::string raw = readFile(stateFilePath_);
            if (raw.empty())
            {
                std::cout << "[Storage] No persisted state found. Will rebuild from chain."
                          << std::endl;
                return false;
            }

            try
            {
                nlohmann::json j = nlohmann::json::parse(raw);
                if (!j.contains("spentTxIDs"))
                {
                    std::cerr << "[Storage] State file missing spentTxIDs field." << std::endl;
                    return false;
                }

                // State will be rebuilt from chain for full correctness.
                // The state file acts as a corruption-detection hint.
                std::cout << "[Storage] State file found (" << j["spentTxIDs"].size()
                          << " spent txIDs). Will validate against chain." << std::endl;
                return true;
            }
            catch (const std::exception &e)
            {
                std::cerr << "[Storage] Failed to parse state file: " << e.what()
                          << std::endl;
                return false;
            }
        }

        // ─── Integrity ────────────────────────────────────────────────────

        bool LevelDBManager::validateChainIntegrity(const std::vector<Block> &chain)
        {
            return Blockchain::isChainValidBasic(chain);
        }

        bool LevelDBManager::hasPersistedChain() const
        {
            return fs::exists(chainFilePath_) && fs::file_size(chainFilePath_) > 2;
        }

        // ─── Private helpers ──────────────────────────────────────────────

        bool LevelDBManager::atomicWrite(const std::string &filePath, const std::string &content)
        {
            // Write to tmp file first, then rename — prevents partial writes on crash
            std::string tmpPath = filePath + ".tmp";

            try
            {
                std::ofstream ofs(tmpPath, std::ios::binary | std::ios::trunc);
                if (!ofs.is_open())
                {
                    std::cerr << "[Storage] Cannot open tmp file: " << tmpPath << std::endl;
                    return false;
                }

                ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
                ofs.flush();
                ofs.close();

                if (ofs.fail())
                {
                    std::cerr << "[Storage] Write failed for tmp file: " << tmpPath << std::endl;
                    return false;
                }

                // Atomic rename (POSIX rename is atomic on same filesystem)
                // On Windows, std::filesystem::rename replaces target atomically
                fs::rename(tmpPath, filePath);
                return true;
            }
            catch (const std::exception &e)
            {
                std::cerr << "[Storage] Atomic write failed: " << e.what() << std::endl;
                // Clean up tmp file if it exists
                try
                {
                    fs::remove(tmpPath);
                }
                catch (...)
                {
                }
                return false;
            }
        }

        std::string LevelDBManager::readFile(const std::string &filePath)
        {
            if (!fs::exists(filePath))
            {
                return "";
            }

            try
            {
                std::ifstream ifs(filePath, std::ios::binary);
                if (!ifs.is_open())
                {
                    return "";
                }

                std::ostringstream oss;
                oss << ifs.rdbuf();
                return oss.str();
            }
            catch (const std::exception &e)
            {
                std::cerr << "[Storage] Failed to read file " << filePath << ": "
                          << e.what() << std::endl;
                return "";
            }
        }

        void LevelDBManager::ensureDirectory(const std::string &dir)
        {
            try
            {
                if (!fs::exists(dir))
                {
                    fs::create_directories(dir);
                    std::cout << "[Storage] Created data directory: " << dir << std::endl;
                }
            }
            catch (const std::exception &e)
            {
                std::cerr << "[Storage] Failed to create directory " << dir << ": "
                          << e.what() << std::endl;
                throw;
            }
        }

    } // namespace storage
} // namespace blockchain
