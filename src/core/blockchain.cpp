#include "core/blockchain.h"
#include <algorithm>
#include <iostream>

namespace blockchain
{

  Blockchain::Blockchain(uint32_t difficulty) : difficulty_(difficulty)
  {
    std::cout << "[Blockchain] Initializing with difficulty " << difficulty
              << std::endl;
<<<<<<< Updated upstream
=======

    if (consensusEngine_)
    {
      std::cout << "[Blockchain] Consensus engine: " << consensusEngine_->name() << std::endl;
    }
    else
    {
      std::cout << "[Blockchain] Consensus engine: None (legacy mode)" << std::endl;
    }

    // ─── Initialize persistent storage ─────────────────────────────
    if (!dataDir.empty())
    {
      storage_ = std::make_unique<storage::LevelDBManager>(dataDir);

      // Try to load persisted chain
      if (storage_->hasPersistedChain())
      {
        std::cout << "[Blockchain] Loading chain from disk..." << std::endl;
        auto loadedChain = storage_->loadChain();

        if (!loadedChain.empty() && isChainValidImpl(loadedChain))
        {
          chain_ = std::move(loadedChain);
          stateManager_.rebuildState(chain_);
          std::cout << "[Blockchain] Loaded " << chain_.size()
                    << " blocks from disk. Chain is valid." << std::endl;
          return; // Skip genesis creation
        }
        else
        {
          std::cerr << "[Blockchain] Persisted chain is invalid or empty. "
                    << "Creating fresh chain." << std::endl;
        }
      }
    }

    // ─── Create genesis block ─────────────────────────────────────
>>>>>>> Stashed changes
    std::cout << "[Blockchain] Mining genesis block..." << std::endl;

    Block genesis = Block::createGenesis(difficulty);
    chain_.push_back(genesis);
    stateManager_.applyBlock(genesis);

    std::cout << "[Blockchain] Genesis block created: " << genesis.hash
              << std::endl;
  }

  bool Blockchain::addTransaction(const Transaction &tx)
  {
    std::lock_guard<std::mutex> lock(mutex_);

    // 1. Reject externally submitted coinbase transactions
    if (tx.isCoinbase())
    {
      std::cerr << "[TX] Rejected: Coinbase transactions cannot be submitted externally txID="
                << tx.txID << std::endl;
      return false;
    }

    // 2. Verify signature
    if (!tx.verify())
    {
      std::cerr << "[TX] Rejected: Invalid signature for txID=" << tx.txID
                << std::endl;
      return false;
    }

    // 3. Check replay attack (timestamp)
    if (tx.isExpired(300))
    {
      std::cerr << "[TX] Rejected: Transaction expired (replay prevention) txID="
                << tx.txID << std::endl;
      return false;
    }

    // 4. Check double-spend
    if (stateManager_.isDoubleSpend(tx.txID))
    {
      std::cerr << "[TX] Rejected: Double-spend detected txID=" << tx.txID
                << std::endl;
      return false;
    }

    // 5. Check if already in mempool
    if (isInMempool(tx.txID))
    {
      std::cerr << "[TX] Rejected: Already in mempool txID=" << tx.txID
                << std::endl;
      return false;
    }

    mempool_.push_back(tx);
    std::cout << "[TX] Accepted transaction txID=" << tx.txID << std::endl;

    if (onTransactionAdded_)
    {
      onTransactionAdded_(tx);
    }

    return true;
  }

  Block Blockchain::minePendingTransactions(const std::string &minerAddress)
  {
    std::lock_guard<std::mutex> lock(mutex_);

    // Create coinbase (mining reward) transaction
    Transaction coinbase;
    coinbase.senderPublicKey = "COINBASE";
    coinbase.receiverPublicKey = minerAddress;
    coinbase.payload = "Mining Reward";
    coinbase.timestamp = Transaction::currentTimestamp();
    coinbase.computeTxID();

    // Collect transactions for this block
    std::vector<Transaction> blockTxs;
    blockTxs.push_back(coinbase);

    // Add mempool transactions
    for (const auto &tx : mempool_)
    {
      blockTxs.push_back(tx);
    }

    // Create and mine the block
    Block newBlock(chain_.size(), chain_.back().hash, blockTxs, difficulty_);
    newBlock.mineBlock();

    // Add to chain
    chain_.push_back(newBlock);

    // Update state
    stateManager_.applyBlock(newBlock);

    // Clear mempool
    mempool_.clear();

    std::cout << "[Blockchain] Block #" << newBlock.index
              << " added to chain. Chain length: " << chain_.size() << std::endl;

    if (onBlockAdded_)
    {
      onBlockAdded_(newBlock);
    }

    return newBlock;
  }

  bool Blockchain::isChainValid() const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return isChainValidImpl(chain_);
  }

  bool Blockchain::isChainValid(const std::vector<Block> &chain) const
  {
    return isChainValidImpl(chain);
  }

  // Static version for contexts without a consensus engine (e.g., storage layer)
  bool Blockchain::isChainValidBasic(const std::vector<Block> &chain)
  {
    if (chain.empty())
      return false;

    // Validate genesis block fully
    const Block &genesis = chain[0];
    if (genesis.previousHash != std::string(64, '0'))
    {
      std::cerr << "[Validate] Invalid genesis previousHash" << std::endl;
      return false;
    }
    if (genesis.hash != genesis.calculateHash())
    {
      std::cerr << "[Validate] Genesis block hash mismatch" << std::endl;
      return false;
    }
    if (genesis.merkleRoot != genesis.computeMerkleRoot())
    {
      std::cerr << "[Validate] Genesis block invalid Merkle root" << std::endl;
      return false;
    }

    for (size_t i = 1; i < chain.size(); i++)
    {
      const Block &current = chain[i];
      const Block &previous = chain[i - 1];

      // Check hash integrity
      if (current.hash != current.calculateHash())
      {
        std::cerr << "[Validate] Block #" << i << " hash mismatch" << std::endl;
        return false;
      }

      // Check previous hash link
      if (current.previousHash != previous.hash)
      {
        std::cerr << "[Validate] Block #" << i << " invalid previousHash" << std::endl;
        return false;
      }

      // Check Merkle root
      if (current.merkleRoot != current.computeMerkleRoot())
      {
        std::cerr << "[Validate] Block #" << i << " invalid Merkle root" << std::endl;
        return false;
      }

<<<<<<< Updated upstream
      // Check difficulty target
      std::string target(current.difficulty, '0');
      if (current.hash.substr(0, current.difficulty) != target)
      {
        std::cerr << "[Validate] Block #" << i << " doesn't meet difficulty"
                  << std::endl;
=======
      // BUG-4 fix: reject blocks with difficulty < 1
      if (current.difficulty < 1)
      {
        std::cerr << "[Validate] Block #" << i << " has invalid difficulty (0)" << std::endl;
>>>>>>> Stashed changes
        return false;
      }

      // BUG-5 fix: validate difficulty target is met (no longer optional)
      if (current.difficulty > 64)
      {
        std::cerr << "[Validate] Block #" << i << " has unreasonable difficulty ("
                  << current.difficulty << ")" << std::endl;
        return false;
      }
      std::string target(current.difficulty, '0');
      if (current.hash.substr(0, current.difficulty) != target)
      {
        std::cerr << "[Validate] Block #" << i << " doesn't meet difficulty" << std::endl;
        return false;
      }

      // Check block index continuity
      if (current.index != i)
      {
        std::cerr << "[Validate] Block #" << i << " has wrong index ("
                  << current.index << ")" << std::endl;
        return false;
      }

      // Validate all transactions + coinbase count
      if (!current.hasValidTransactions())
      {
        std::cerr << "[Validate] Block #" << i << " has invalid transactions" << std::endl;
        return false;
      }

      // Validate exactly one coinbase transaction (must be first)
      size_t coinbaseCount = 0;
      for (size_t t = 0; t < current.transactions.size(); t++)
      {
        if (current.transactions[t].isCoinbase())
        {
          coinbaseCount++;
          if (t != 0)
          {
            std::cerr << "[Validate] Block #" << i
                      << " has coinbase transaction at position " << t
                      << " (must be first)" << std::endl;
            return false;
          }
        }
      }
      if (coinbaseCount != 1)
      {
        std::cerr << "[Validate] Block #" << i << " has " << coinbaseCount
                  << " coinbase transactions (expected 1)" << std::endl;
        return false;
      }
    }

    return true;
  }

  bool Blockchain::isChainValidImpl(const std::vector<Block> &chain) const
  {
    // First run the basic structural checks
    if (!isChainValidBasic(chain))
      return false;

    // BUG-6: If we have a consensus engine, also validate each block through it
    if (consensusEngine_ && chain.size() > 1)
    {
      for (size_t i = 1; i < chain.size(); i++)
      {
        if (!consensusEngine_->validateBlock(chain[i], chain[i - 1]))
        {
          std::cerr << "[Validate] Block #" << i
                    << " failed consensus engine validation" << std::endl;
          return false;
        }
      }
    }

    return true;
  }

  bool Blockchain::replaceChain(const std::vector<Block> &newChain)
  {
    std::lock_guard<std::mutex> lock(mutex_);

    if (newChain.size() <= chain_.size())
    {
      std::cout
          << "[Consensus] Received chain is not longer. Keeping current chain."
          << std::endl;
      return false;
    }

    if (!isChainValidImpl(newChain))
    {
      std::cerr << "[Consensus] Received chain is invalid. Rejecting."
                << std::endl;
      return false;
    }

    std::cout << "[Consensus] Replacing chain. Old length: " << chain_.size()
              << ", New length: " << newChain.size() << std::endl;

    chain_ = newChain;

    // Rebuild state from new chain
    stateManager_.rebuildState(chain_);

    // Remove mempool transactions that are now in blocks
    auto spentTxIDs = stateManager_.getAllSpentTxIDs();
    mempool_.erase(std::remove_if(mempool_.begin(), mempool_.end(),
                                  [&spentTxIDs](const Transaction &tx)
                                  {
                                    return spentTxIDs.count(tx.txID) > 0;
                                  }),
                   mempool_.end());

    return true;
  }

  std::vector<Block> Blockchain::getChain() const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return chain_;
  }

  Block Blockchain::getLatestBlock() const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return chain_.back();
  }

  std::vector<Transaction> Blockchain::getMempool() const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return mempool_;
  }

  size_t Blockchain::getChainLength() const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return chain_.size();
  }

  nlohmann::json Blockchain::chainToJson() const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json j = nlohmann::json::array();
    for (const auto &block : chain_)
    {
      j.push_back(block.toJson());
    }
    return j;
  }

  void Blockchain::chainFromJson(const nlohmann::json &j)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Block> newChain;
    for (const auto &blockJson : j)
    {
      newChain.push_back(Block::fromJson(blockJson));
    }

    // Use consensus engine for chain acceptance if available
    bool shouldAccept = false;
    if (consensusEngine_)
    {
      shouldAccept = consensusEngine_->shouldAcceptChain(chain_, newChain);
    }
    else
    {
      shouldAccept = newChain.size() > chain_.size();
    }

    if (shouldAccept && isChainValidImpl(newChain))
    {
      chain_ = newChain;
      stateManager_.rebuildState(chain_);
<<<<<<< Updated upstream
=======

      // Prune mempool of transactions now in the chain
      auto spentTxIDs = stateManager_.getAllSpentTxIDs();
      mempool_.erase(std::remove_if(mempool_.begin(), mempool_.end(),
                                    [&spentTxIDs](const Transaction &tx)
                                    {
                                      return spentTxIDs.count(tx.txID) > 0;
                                    }),
                     mempool_.end());

      persistChainLocked();
>>>>>>> Stashed changes
    }
  }

  void Blockchain::setOnBlockAdded(std::function<void(const Block &)> callback)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    onBlockAdded_ = std::move(callback);
  }

  void Blockchain::setOnTransactionAdded(
      std::function<void(const Transaction &)> callback)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    onTransactionAdded_ = std::move(callback);
  }

  bool Blockchain::isInMempool(const std::string &txID) const
  {
    for (const auto &tx : mempool_)
    {
      if (tx.txID == txID)
        return true;
    }
    return false;
  }

<<<<<<< Updated upstream
=======
  // ─── Private persistence helpers ──────────────────────────────

  void Blockchain::persistChainLocked()
  {
    if (storage_)
    {
      if (!storage_->saveChain(chain_))
      {
        std::cerr << "[Blockchain] WARNING: Failed to persist chain to disk!" << std::endl;
      }
      if (!storage_->saveState(stateManager_))
      {
        std::cerr << "[Blockchain] WARNING: Failed to persist state to disk!" << std::endl;
      }
    }
  }

  void Blockchain::persistBlockLocked(const Block &block)
  {
    if (storage_)
    {
      if (!storage_->appendBlock(block))
      {
        std::cerr << "[Blockchain] WARNING: Failed to append block to disk!" << std::endl;
      }
      if (!storage_->saveState(stateManager_))
      {
        std::cerr << "[Blockchain] WARNING: Failed to persist state to disk!" << std::endl;
      }
    }
  }

>>>>>>> Stashed changes
} // namespace blockchain
