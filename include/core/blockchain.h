#pragma once

#include "block/block.h"
#include "consensus/consensus_engine.h"
#include "state/state_manager.h"
#include "storage/leveldb_manager.h"
#include "transaction/transaction.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace blockchain
{

  /**
   * The main Blockchain class. Manages the chain of blocks,
   * the transaction mempool, consensus rules (via pluggable engine),
   * persistent storage, and dynamic difficulty retargeting.
   */
  class Blockchain
  {
  public:
    /**
     * Construct a new blockchain with the given difficulty, name, and consensus engine.
     * If a persisted chain exists in dataDir, it is loaded and validated.
     * Otherwise, creates the genesis block and persists it.
     *
     * @param difficulty Initial mining difficulty
     * @param name Blockchain network name
     * @param consensusEngine Pluggable consensus engine (PoW, PoS, etc.)
     * @param dataDir Directory for persistent chain storage ("" = no persistence)
     */
    explicit Blockchain(uint32_t difficulty = 2,
                        std::string name = "PersonalBlockchain",
                        std::shared_ptr<consensus::IConsensusEngine> consensusEngine = nullptr,
                        const std::string &dataDir = "");

    /**
     * Add a transaction to the mempool after validation.
     * Checks: signature verification, replay prevention, double-spend.
     * @return true if transaction was accepted
     */
    bool addTransaction(const Transaction &tx);

    /**
     * Mine pending transactions into a new block.
     * Uses the consensus engine to seal the block.
     * After mining, persists the block and updates difficulty.
     * @param minerAddress The public key to receive the mining reward
     * @return The newly mined block
     */
    Block minePendingTransactions(const std::string &minerAddress);

    /**
     * Validate the entire chain from genesis to tip.
     * @return true if the chain is valid
     */
    bool isChainValid() const;

    /**
     * Validate an external chain (static, uses basic structural checks).
     * @return true if the chain is valid
     */
    static bool isChainValid(const std::vector<Block> &chain);

    /**
     * Replace the current chain if the new chain is longer and valid.
     * Uses the consensus engine's shouldAcceptChain() to decide.
     * Persists the new chain to disk after replacement.
     * @return true if chain was replaced
     */
    bool replaceChain(const std::vector<Block> &newChain);

    /** Get the full chain. */
    std::vector<Block> getChain() const;

    /** Get the latest block. */
    Block getLatestBlock() const;

    /** Get the mempool (pending transactions). */
    std::vector<Transaction> getMempool() const;

    /** Get chain length. */
    size_t getChainLength() const;

    /** Get difficulty (may change dynamically). */
    uint32_t getDifficulty() const { return difficulty_; }

    /** Get blockchain name. */
    std::string getName() const { return name_; }

    /** Get consensus engine name. */
    std::string getConsensusName() const;

    /** Serialize full chain to JSON. */
    nlohmann::json chainToJson() const;

    /** Deserialize chain from JSON. */
    void chainFromJson(const nlohmann::json &j);

    /** Set callback for when a new block is added. */
    void setOnBlockAdded(std::function<void(const Block &)> callback);

    /** Set callback for when a new transaction is added to mempool. */
    void setOnTransactionAdded(std::function<void(const Transaction &)> callback);

    /** Force a full chain save to disk (e.g., on graceful shutdown). */
    void persistChain();

  private:
    uint32_t difficulty_;
    std::string name_;
    std::vector<Block> chain_;
    std::vector<Transaction> mempool_;
    StateManager stateManager_;
    mutable std::mutex mutex_;

    // Pluggable consensus engine
    std::shared_ptr<consensus::IConsensusEngine> consensusEngine_;

    // Persistent storage (nullptr if persistence disabled)
    std::unique_ptr<storage::LevelDBManager> storage_;

    // Event callbacks
    std::function<void(const Block &)> onBlockAdded_;
    std::function<void(const Transaction &)> onTransactionAdded_;

    /** Check if a transaction already exists in the mempool. */
    bool isInMempool(const std::string &txID) const;

    /** Persist chain to disk if storage is enabled. */
    void persistChainLocked();

    /** Persist a single block (append) if storage is enabled. */
    void persistBlockLocked(const Block &block);
  };

} // namespace blockchain
