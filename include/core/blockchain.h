#pragma once

#include "block/block.h"
#include "state/state_manager.h"
#include "transaction/transaction.h"


#include <functional>
#include <mutex>
#include <string>
#include <vector>


namespace blockchain {

/**
 * The main Blockchain class. Manages the chain of blocks,
 * the transaction mempool, and consensus rules.
 */
class Blockchain {
public:
  /**
   * Construct a new blockchain with the given difficulty.
   * Creates the genesis block automatically.
   */
  explicit Blockchain(uint32_t difficulty = 2);

  /**
   * Add a transaction to the mempool after validation.
   * Checks: signature verification, replay prevention, double-spend.
   * @return true if transaction was accepted
   */
  bool addTransaction(const Transaction &tx);

  /**
   * Mine pending transactions into a new block.
   * @param minerAddress The public key to receive the mining reward
   * @return The newly mined block
   */
  Block minePendingTransactions(const std::string &minerAddress);

  /**
   * Validate the entire chain from genesis to tip.
   * @return true if the chain is valid
   */
  bool isChainValid() const;

<<<<<<< Updated upstream
  /**
   * Validate an external chain.
   * @return true if the chain is valid
   */
  static bool isChainValid(const std::vector<Block> &chain);

  /**
   * Replace the current chain if the new chain is longer and valid.
   * Implements the longest-valid-chain consensus rule.
   * @return true if chain was replaced
   */
  bool replaceChain(const std::vector<Block> &newChain);
=======
    /**
     * Validate the entire chain from genesis to tip.
     * Uses the consensus engine if available.
     * @return true if the chain is valid
     */
    bool isChainValid() const;

    /**
     * Validate an external chain using consensus engine if available.
     * @return true if the chain is valid
     */
    bool isChainValid(const std::vector<Block> &chain) const;

    /**
     * Validate an external chain using basic structural checks only.
     * Used by storage layer where no consensus engine is available.
     * @return true if the chain is valid
     */
    static bool isChainValidBasic(const std::vector<Block> &chain);
>>>>>>> Stashed changes

  /** Get the full chain. */
  std::vector<Block> getChain() const;

  /** Get the latest block. */
  Block getLatestBlock() const;

  /** Get the mempool (pending transactions). */
  std::vector<Transaction> getMempool() const;

  /** Get chain length. */
  size_t getChainLength() const;

  /** Get difficulty. */
  uint32_t getDifficulty() const { return difficulty_; }

<<<<<<< Updated upstream
  /** Serialize full chain to JSON. */
  nlohmann::json chainToJson() const;

  /** Deserialize chain from JSON. */
  void chainFromJson(const nlohmann::json &j);
=======
    /** Get difficulty (may change dynamically). */
    uint32_t getDifficulty() const
    {
      std::lock_guard<std::mutex> lock(mutex_);
      return difficulty_;
    }

    /** Get blockchain name. */
    std::string getName() const
    {
      return name_; // name_ is set-once in constructor, safe to read without lock
    }
>>>>>>> Stashed changes

  /** Set callback for when a new block is added. */
  void setOnBlockAdded(std::function<void(const Block &)> callback);

  /** Set callback for when a new transaction is added to mempool. */
  void setOnTransactionAdded(std::function<void(const Transaction &)> callback);

private:
  uint32_t difficulty_;
  std::vector<Block> chain_;
  std::vector<Transaction> mempool_;
  StateManager stateManager_;
  mutable std::mutex mutex_;

  // Event callbacks
  std::function<void(const Block &)> onBlockAdded_;
  std::function<void(const Transaction &)> onTransactionAdded_;

<<<<<<< Updated upstream
  /** Check if a transaction already exists in the mempool. */
  bool isInMempool(const std::string &txID) const;
};
=======
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

    /** Internal chain validation that uses the consensus engine if available. */
    bool isChainValidImpl(const std::vector<Block> &chain) const;

    /** Persist chain to disk if storage is enabled. */
    void persistChainLocked();

    /** Persist a single block (append) if storage is enabled. */
    void persistBlockLocked(const Block &block);
  };
>>>>>>> Stashed changes

} // namespace blockchain
