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
            recoverTempFiles();
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
                        std::cerr << "[Storage] appendBlock aborted: existing chain file is not an array." << std::endl;
                        return false;
                    }
                }
                catch (...)
                {
                    std::cerr << "[Storage] appendBlock aborted: existing chain file is malformed." << std::endl;
                    return false;
                }
            }
            else
            {
                j = nlohmann::json::array();
            }

            if (!j.empty())
            {
                Block tip = Block::fromJson(j.back());
                if (block.index != tip.index + 1 || block.previousHash != tip.hash)
                {
                    std::cerr << "[Storage] appendBlock aborted: block does not extend persisted tip." << std::endl;
                    return false;
                }
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
            return Blockchain::isChainValid(chain);
        }

        bool LevelDBManager::hasPersistedChain() const
        {
            if (!fs::exists(chainFilePath_) || !fs::is_regular_file(chainFilePath_))
            {
                return false;
            }

            std::error_code ec;
            auto sz = fs::file_size(chainFilePath_, ec);
            if (ec)
            {
                return false;
            }

            return sz > 2;
        }

        // ─── Private helpers ──────────────────────────────────────────────

        bool LevelDBManager::atomicWrite(const std::string &filePath, const std::string &content)
        {
            // Write to tmp file first, then rename — prevents partial writes on crash
            std::string tmpPath = filePath + ".tmp";
            std::string bakPath = filePath + ".bak";

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

                std::error_code ec;
                if (fs::exists(filePath, ec) && !ec)
                {
                    fs::remove(bakPath, ec);
                    ec.clear();
                    fs::rename(filePath, bakPath, ec);
                    if (ec)
                    {
                        std::cerr << "[Storage] Failed to create backup for " << filePath
                                  << ": " << ec.message() << std::endl;
                        return false;
                    }
                }

                ec.clear();
                fs::rename(tmpPath, filePath, ec);
                if (ec)
                {
                    std::cerr << "[Storage] Failed to install new file " << filePath
                              << ": " << ec.message() << std::endl;
                    if (fs::exists(bakPath))
                    {
                        std::error_code restoreEc;
                        fs::rename(bakPath, filePath, restoreEc);
                    }
                    return false;
                }

                fs::remove(bakPath, ec);
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

        void LevelDBManager::recoverTempFiles()
        {
            const std::vector<std::string> canonicalFiles = {chainFilePath_, stateFilePath_};

            for (const auto &filePath : canonicalFiles)
            {
                std::string tmpPath = filePath + ".tmp";

                std::error_code ec;
                if (!fs::exists(tmpPath, ec) || ec)
                {
                    continue;
                }

                if (fs::exists(filePath, ec) && !ec)
                {
                    fs::remove(tmpPath, ec);
                    continue;
                }

                fs::rename(tmpPath, filePath, ec);
                if (!ec)
                {
                    std::cout << "[Storage] Recovered " << filePath
                              << " from leftover tmp file." << std::endl;
                }
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
