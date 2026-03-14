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
   * the transaction mempool, and consensus rules.
   */
  class Blockchain
  {
  public:
    /**
     * Construct a new blockchain with the given difficulty.
     * Creates the genesis block automatically.
     * @param difficulty  Mining difficulty (hex leading zeros)
     * @param consensusEngine  Optional pluggable consensus engine
     * @param storage  Optional persistence layer
     */
    explicit Blockchain(uint32_t difficulty = 2,
                        consensus::IConsensusEngine *consensusEngine = nullptr,
                        storage::LevelDBManager *storage = nullptr);

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

    /**
     * Accept and append a validated next block from the network.
     * @return true if block was appended and state updated
     */
    bool acceptBlock(const Block &block);

    /** Get the full chain. */
    std::vector<Block> getChain() const;

    /** Get the latest block. */
    Block getLatestBlock() const;

    /** Get the mempool (pending transactions). */
    std::vector<Transaction> getMempool() const;

    /** Get chain length. */
    size_t getChainLength() const;

    /** Monotonic chain update version for observer refresh triggers. */
    uint64_t getChainUpdateVersion() const;

    /** Get confirmed balance for a wallet address. */
    uint64_t getBalanceForAddress(const std::string &address) const;

    /** Get last confirmed nonce for a wallet address. */
    uint64_t getLastNonceForAddress(const std::string &address) const;

    /** Get confirmed transactions involving a wallet address. */
    std::vector<Transaction> getTransactionsForAddress(const std::string &address) const;

    /** Get difficulty. */
    uint32_t getDifficulty() const { return difficulty_; }

    /** Serialize full chain to JSON. */
    nlohmann::json chainToJson() const;

    /** Deserialize chain from JSON. */
    void chainFromJson(const nlohmann::json &j);

    /** Set callback for when a new block is added. */
    void setOnBlockAdded(std::function<void(const Block &)> callback);

    /** Set callback for when a block is accepted into canonical chain state. */
    void setOnBlockAccepted(std::function<void(const Block &)> callback);

    /** Set callback for when a new transaction is added to mempool. */
    void setOnTransactionAdded(std::function<void(const Transaction &)> callback);

    /** Persist the current chain to disk (if storage is wired). */
    void persistChain();

    /** Persist a single block append to disk (if storage is wired). */
    void persistBlock(const Block &block);

  private:
    /** Validate chain using base checks + active consensus engine rules. */
    bool validateChainAgainstConsensus(const std::vector<Block> &chain) const;

    uint32_t difficulty_;
    std::vector<Block> chain_;
    std::vector<Transaction> mempool_;
    StateManager stateManager_;
    mutable std::mutex mutex_;

    // Pluggable consensus engine (nullable — falls back to hardcoded PoW)
    consensus::IConsensusEngine *consensusEngine_ = nullptr;

    // Persistent storage layer (nullable — in-memory only if null)
    storage::LevelDBManager *storage_ = nullptr;

    // Event callbacks
    std::function<void(const Block &)> onBlockAdded_;
    std::function<void(const Block &)> onBlockAccepted_;
    std::function<void(const Transaction &)> onTransactionAdded_;

    uint64_t chainUpdateVersion_ = 0;

    /** Apply block effects into state manager. */
    void applyBlockState(const Block &block);

    /** Check if a transaction already exists in the mempool. */
    bool isInMempool(const std::string &txID) const;
  };

} // namespace blockchain
