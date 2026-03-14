#include "state/state_manager.h"

#include <algorithm>

namespace blockchain
{

  void StateManager::applyBlock(const Block &block)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    applyBlockState(block);
  }

  void StateManager::applyBlockState(const Block &block)
  {
    for (const auto &tx : block.transactions)
    {
      if (!tx.toAddress.empty())
      {
        balances_[tx.toAddress] += tx.amount;
        transactionsByAddress_[tx.toAddress].push_back(tx);
      }

      if (!tx.isCoinbase())
      {
        ownershipMap_[tx.senderPublicKey].insert(tx.txID);
        spentTxIDs_.insert(tx.txID);
        if (!tx.fromAddress.empty())
        {
          auto it = balances_.find(tx.fromAddress);
          if (it != balances_.end())
          {
            it->second = (it->second > tx.amount) ? (it->second - tx.amount) : 0;
          }
          lastNonceMap_[tx.fromAddress] = std::max(lastNonceMap_[tx.fromAddress], tx.nonce);
          transactionsByAddress_[tx.fromAddress].push_back(tx);
        }
      }
    }
  }

  bool StateManager::isDoubleSpend(const std::string &txID) const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return spentTxIDs_.count(txID) > 0;
  }

  void StateManager::rebuildState(const std::vector<Block> &chain)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    ownershipMap_.clear();
    spentTxIDs_.clear();
    balances_.clear();
    lastNonceMap_.clear();
    transactionsByAddress_.clear();

    for (const auto &block : chain)
    {
      applyBlockState(block);
    }
  }

  void StateManager::clear()
  {
    std::lock_guard<std::mutex> lock(mutex_);
    ownershipMap_.clear();
    spentTxIDs_.clear();
    balances_.clear();
    lastNonceMap_.clear();
    transactionsByAddress_.clear();
  }

  size_t StateManager::getTransactionCount(const std::string &publicKey) const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = ownershipMap_.find(publicKey);
    if (it != ownershipMap_.end())
    {
      return it->second.size();
    }
    return 0;
  }

  std::set<std::string> StateManager::getAllSpentTxIDs() const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return spentTxIDs_;
  }

  uint64_t StateManager::getBalance(const std::string &address) const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = balances_.find(address);
    return it == balances_.end() ? 0 : it->second;
  }

  uint64_t StateManager::getLastNonce(const std::string &address) const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = lastNonceMap_.find(address);
    return it == lastNonceMap_.end() ? 0 : it->second;
  }

  std::vector<Transaction> StateManager::getTransactionsForAddress(const std::string &address) const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = transactionsByAddress_.find(address);
    if (it == transactionsByAddress_.end())
    {
      return {};
    }
    return it->second;
  }

} // namespace blockchain
