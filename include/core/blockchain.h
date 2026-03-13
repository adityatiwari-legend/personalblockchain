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

  /** Serialize full chain to JSON. */
  nlohmann::json chainToJson() const;

  /** Deserialize chain from JSON. */
  void chainFromJson(const nlohmann::json &j);

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

  /** Check if a transaction already exists in the mempool. */
  bool isInMempool(const std::string &txID) const;
};

} // namespace blockchain
