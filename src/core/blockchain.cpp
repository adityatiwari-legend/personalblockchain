#include "core/blockchain.h"
#include "crypto/sha256.h"
#include <algorithm>
#include <iostream>

namespace blockchain
{

  namespace
  {
    constexpr uint64_t kBlockReward = 50;
  }

  Blockchain::Blockchain(uint32_t difficulty,
                         consensus::IConsensusEngine *consensusEngine,
                         storage::LevelDBManager *storage)
      : difficulty_(difficulty),
        consensusEngine_(consensusEngine),
        storage_(storage)
  {
    std::cout << "[Blockchain] Initializing with difficulty " << difficulty
              << std::endl;

    if (consensusEngine_)
    {
      std::cout << "[Blockchain] Consensus engine: " << consensusEngine_->name() << std::endl;
    }

    // Try to load chain from persistent storage
    if (storage_ && storage_->hasPersistedChain())
    {
      std::cout << "[Blockchain] Loading chain from disk..." << std::endl;
      auto loaded = storage_->loadChain();
      if (!loaded.empty() && validateChainAgainstConsensus(loaded))
      {
        chain_ = std::move(loaded);
        stateManager_.rebuildState(chain_);
        std::cout << "[Blockchain] Loaded " << chain_.size()
                  << " blocks from disk. Tip: " << chain_.back().hash << std::endl;
        return;
      }
      else
      {
        std::cerr << "[Blockchain] Persisted chain invalid or empty. Creating fresh genesis." << std::endl;
      }
    }

    std::cout << "[Blockchain] Mining genesis block..." << std::endl;

    Block genesis = Block::createGenesis(difficulty);
    chain_.push_back(genesis);
    stateManager_.applyBlock(genesis);

    // Persist genesis block
    if (storage_)
    {
      storage_->saveChain(chain_);
    }

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

    // 2.5 Validate amount, sender address mapping, and balance.
    if (tx.amount == 0)
    {
      std::cerr << "[TX] Rejected: Amount must be greater than 0 txID="
                << tx.txID << std::endl;
      return false;
    }

    const std::string expectedFrom = crypto::sha256(tx.senderPublicKey);
    if (tx.fromAddress != expectedFrom)
    {
      std::cerr << "[TX] Rejected: Sender address/public key mismatch txID="
                << tx.txID << std::endl;
      return false;
    }

    uint64_t confirmedBalance = stateManager_.getBalance(tx.fromAddress);
    if (confirmedBalance < tx.amount)
    {
      std::cerr << "[TX] Rejected: Insufficient balance txID=" << tx.txID
                << " balance=" << confirmedBalance << " amount=" << tx.amount << std::endl;
      return false;
    }

    uint64_t expectedNextNonce = stateManager_.getLastNonce(tx.fromAddress) + 1;
    if (tx.nonce < expectedNextNonce)
    {
      std::cerr << "[TX] Rejected: Nonce replay txID=" << tx.txID
                << " nonce=" << tx.nonce << " expectedAtLeast=" << expectedNextNonce << std::endl;
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

    // Dynamic difficulty retarget via consensus engine
    uint32_t blockDifficulty = difficulty_;
    if (consensusEngine_)
    {
      blockDifficulty = consensusEngine_->nextDifficulty(chain_, difficulty_);
      if (blockDifficulty != difficulty_)
      {
        std::cout << "[Blockchain] Difficulty retarget: " << difficulty_
                  << " -> " << blockDifficulty << std::endl;
        difficulty_ = blockDifficulty;
      }
    }

    // Create coinbase (mining reward) transaction
    Transaction coinbase;
    coinbase.senderPublicKey = "COINBASE";
    coinbase.receiverPublicKey = "";
    coinbase.fromAddress = "COINBASE";
    coinbase.toAddress = minerAddress;
    coinbase.amount = kBlockReward;
    coinbase.payload = "Mining Reward";
    coinbase.timestamp = Transaction::currentTimestamp();
    coinbase.computeTxID();

    // Collect transactions for this block (cap at 100 to bound block size)
    std::vector<Transaction> blockTxs;
    blockTxs.push_back(coinbase);

    static constexpr size_t MAX_TXS_PER_BLOCK = 100;
    size_t txCount = std::min(mempool_.size(), MAX_TXS_PER_BLOCK);
    for (size_t i = 0; i < txCount; i++)
    {
      blockTxs.push_back(mempool_[i]);
    }

    // Create and mine the block
    Block newBlock(chain_.size(), chain_.back().hash, blockTxs, blockDifficulty);

    if (consensusEngine_)
    {
      consensusEngine_->sealBlock(newBlock, minerAddress);
    }
    else
    {
      newBlock.mineBlock();
    }

    // Add to chain
    chain_.push_back(newBlock);

    // Update state
    stateManager_.applyBlock(newBlock);

    // Remove mined transactions from mempool
    if (txCount >= mempool_.size())
    {
      mempool_.clear();
    }
    else
    {
      mempool_.erase(mempool_.begin(), mempool_.begin() + static_cast<long>(txCount));
    }

    std::cout << "[Blockchain] Block #" << newBlock.index
              << " added to chain. Chain length: " << chain_.size() << std::endl;

    // Persist to disk
    if (storage_)
    {
      storage_->appendBlock(newBlock);
    }

    if (onBlockAdded_)
    {
      onBlockAdded_(newBlock);
    }

    return newBlock;
  }

  bool Blockchain::isChainValid() const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return validateChainAgainstConsensus(chain_);
  }

  bool Blockchain::validateChainAgainstConsensus(const std::vector<Block> &chain) const
  {
    if (!isChainValid(chain))
    {
      return false;
    }

    if (!consensusEngine_)
    {
      return true;
    }

    for (size_t i = 1; i < chain.size(); i++)
    {
      if (!consensusEngine_->validateBlock(chain[i], chain[i - 1]))
      {
        std::cerr << "[Consensus] Block #" << i
                  << " rejected by consensus engine " << consensusEngine_->name()
                  << std::endl;
        return false;
      }
    }

    return true;
  }

  bool Blockchain::isChainValid(const std::vector<Block> &chain)
  {
    if (chain.empty())
      return false;

    // Validate genesis block
    const Block &genesis = chain[0];
    if (genesis.previousHash != std::string(64, '0'))
    {
      std::cerr << "[Validate] Invalid genesis previousHash" << std::endl;
      return false;
    }

    // Validate genesis hash integrity
    if (genesis.hash != genesis.calculateHash())
    {
      std::cerr << "[Validate] Genesis block hash mismatch" << std::endl;
      return false;
    }

    // Validate genesis Merkle root
    if (genesis.merkleRoot != genesis.computeMerkleRoot())
    {
      std::cerr << "[Validate] Genesis block invalid Merkle root" << std::endl;
      return false;
    }

    for (size_t i = 1; i < chain.size(); i++)
    {
      const Block &current = chain[i];
      const Block &previous = chain[i - 1];

      // Check block index is sequential
      if (current.index != i)
      {
        std::cerr << "[Validate] Block #" << i << " has incorrect index " << current.index << std::endl;
        return false;
      }

      // Check hash integrity
      if (current.hash != current.calculateHash())
      {
        std::cerr << "[Validate] Block #" << i << " hash mismatch" << std::endl;
        return false;
      }

      // Check previous hash link
      if (current.previousHash != previous.hash)
      {
        std::cerr << "[Validate] Block #" << i << " invalid previousHash"
                  << std::endl;
        return false;
      }

      // Check Merkle root
      if (current.merkleRoot != current.computeMerkleRoot())
      {
        std::cerr << "[Validate] Block #" << i << " invalid Merkle root"
                  << std::endl;
        return false;
      }

      // Check difficulty target
      std::string target(current.difficulty, '0');
      if (current.hash.substr(0, current.difficulty) != target)
      {
        std::cerr << "[Validate] Block #" << i << " doesn't meet difficulty"
                  << std::endl;
        return false;
      }

      // Validate all transactions
      if (!current.hasValidTransactions())
      {
        std::cerr << "[Validate] Block #" << i << " has invalid transactions"
                  << std::endl;
        return false;
      }
    }

    return true;
  }

  bool Blockchain::replaceChain(const std::vector<Block> &newChain)
  {
    std::lock_guard<std::mutex> lock(mutex_);

    // Use consensus engine for chain selection if available
    if (consensusEngine_)
    {
      if (!consensusEngine_->shouldAcceptChain(chain_, newChain))
      {
        std::cout << "[Consensus] Consensus engine rejected candidate chain." << std::endl;
        return false;
      }
    }
    else
    {
      if (newChain.size() <= chain_.size())
      {
        std::cout
            << "[Consensus] Received chain is not longer. Keeping current chain."
            << std::endl;
        return false;
      }
    }

    if (!validateChainAgainstConsensus(newChain))
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

    // Persist entire new chain
    if (storage_)
    {
      storage_->saveChain(chain_);
    }

    return true;
  }

  void Blockchain::persistChain()
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (storage_)
    {
      storage_->saveChain(chain_);
      storage_->saveState(stateManager_);
    }
  }

  void Blockchain::persistBlock(const Block &block)
  {
    // No lock needed — storage has its own mutex
    if (storage_)
    {
      storage_->appendBlock(block);
    }
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

  uint64_t Blockchain::getBalanceForAddress(const std::string &address) const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return stateManager_.getBalance(address);
  }

  uint64_t Blockchain::getLastNonceForAddress(const std::string &address) const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return stateManager_.getLastNonce(address);
  }

  std::vector<Transaction> Blockchain::getTransactionsForAddress(const std::string &address) const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return stateManager_.getTransactionsForAddress(address);
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
    std::vector<Block> newChain;
    for (const auto &blockJson : j)
    {
      newChain.push_back(Block::fromJson(blockJson));
    }

    replaceChain(newChain);
  }

  void Blockchain::setOnBlockAdded(std::function<void(const Block &)> callback)
  {
    onBlockAdded_ = std::move(callback);
  }

  void Blockchain::setOnTransactionAdded(
      std::function<void(const Transaction &)> callback)
  {
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

} // namespace blockchain
