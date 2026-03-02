#include "state/state_manager.h"

namespace blockchain {

void StateManager::applyBlock(const Block &block) {
  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto &tx : block.transactions) {
    if (!tx.isCoinbase()) {
      ownershipMap_[tx.senderPublicKey].insert(tx.txID);
      spentTxIDs_.insert(tx.txID);
    }
  }
}

bool StateManager::isDoubleSpend(const std::string &txID) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return spentTxIDs_.count(txID) > 0;
}

void StateManager::rebuildState(const std::vector<Block> &chain) {
  std::lock_guard<std::mutex> lock(mutex_);
  ownershipMap_.clear();
  spentTxIDs_.clear();

  for (const auto &block : chain) {
    for (const auto &tx : block.transactions) {
      if (!tx.isCoinbase()) {
        ownershipMap_[tx.senderPublicKey].insert(tx.txID);
        spentTxIDs_.insert(tx.txID);
      }
    }
  }
}

void StateManager::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  ownershipMap_.clear();
  spentTxIDs_.clear();
}

size_t StateManager::getTransactionCount(const std::string &publicKey) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = ownershipMap_.find(publicKey);
  if (it != ownershipMap_.end()) {
    return it->second.size();
  }
  return 0;
}

std::set<std::string> StateManager::getAllSpentTxIDs() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return spentTxIDs_;
}

} // namespace blockchain
