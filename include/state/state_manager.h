#pragma once

#include "block/block.h"
#include <map>
#include <mutex>
#include <set>
#include <string>

namespace blockchain
{

  /**
   * Manages the in-memory state of the blockchain.
   * Tracks transaction ownership for double-spend detection.
   */
  class StateManager
  {
  public:
    /**
     * Apply a confirmed block to update state.
     * Records all transaction IDs as spent by their senders.
     */
    void applyBlock(const Block &block);

    /**
     * Apply a confirmed block to balance/nonce/transaction state.
     * This is shared by local mining and network block acceptance flows.
     */
    void applyBlockState(const Block &block);

    /**
     * Check if a transaction would be a double-spend.
     * @param txID The transaction ID to check
     * @return true if this txID has already been spent
     */
    bool isDoubleSpend(const std::string &txID) const;

    /**
     * Rebuild the entire state from a chain of blocks.
     * Clears existing state first.
     */
    void rebuildState(const std::vector<Block> &chain);

    /**
     * Clear all state.
     */
    void clear();

    /**
     * Get the number of transactions associated with a public key.
     */
    size_t getTransactionCount(const std::string &publicKey) const;

    /** Get confirmed balance for wallet address. */
    uint64_t getBalance(const std::string &address) const;

    /** Get latest confirmed sender nonce for wallet address. */
    uint64_t getLastNonce(const std::string &address) const;

    /** Get all confirmed transactions touching an address. */
    std::vector<Transaction> getTransactionsForAddress(const std::string &address) const;

    /**
     * Get all transaction IDs.
     */
    std::set<std::string> getAllSpentTxIDs() const;

  private:
    mutable std::mutex mutex_;
    // Maps publicKey -> set of txIDs they've sent
    std::map<std::string, std::set<std::string>> ownershipMap_;
    // Global set of all spent txIDs for quick double-spend lookup
    std::set<std::string> spentTxIDs_;
    std::map<std::string, uint64_t> balances_;
    std::map<std::string, uint64_t> lastNonceMap_;
    std::map<std::string, std::vector<Transaction>> transactionsByAddress_;
  };

} // namespace blockchain
