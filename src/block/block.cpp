#include "block/block.h"
#include "crypto/sha256.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace blockchain
{

  Block::Block() : index(0), nonce(0), difficulty(1) {}

  Block::Block(uint64_t idx, const std::string &prevHash,
               const std::vector<Transaction> &txs, uint32_t diff)
      : index(idx), previousHash(prevHash), transactions(txs), nonce(0),
        difficulty(diff)
  {
    // Set timestamp
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
#ifdef _WIN32
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    timestamp = oss.str();

    // Compute merkle root
    merkleRoot = computeMerkleRoot();
  }

  std::string Block::computeMerkleRoot() const
  {
    if (transactions.empty())
    {
      return crypto::sha256("");
    }

    // Start with transaction IDs
    std::vector<std::string> hashes;
    hashes.reserve(transactions.size());
    for (const auto &tx : transactions)
    {
      hashes.push_back(tx.txID.empty() ? crypto::sha256(tx.senderPublicKey +
                                                        tx.receiverPublicKey +
                                                        tx.payload + tx.timestamp)
                                       : tx.txID);
    }

    // Build Merkle tree bottom-up
    while (hashes.size() > 1)
    {
      std::vector<std::string> newLevel;
      for (size_t i = 0; i < hashes.size(); i += 2)
      {
        if (i + 1 < hashes.size())
        {
          newLevel.push_back(crypto::sha256(hashes[i] + hashes[i + 1]));
        }
        else
        {
          // Odd number: duplicate last hash
          newLevel.push_back(crypto::sha256(hashes[i] + hashes[i]));
        }
      }
      hashes = std::move(newLevel);
    }

    return hashes[0];
  }

  std::string Block::calculateHash() const
  {
    std::stringstream ss;
    ss << index << timestamp << merkleRoot << previousHash << nonce << difficulty;
    return crypto::sha256(ss.str());
  }

  void Block::mineBlock()
  {
    merkleRoot = computeMerkleRoot();

    std::string target(difficulty, '0');

    std::cout << "[Mining] Block #" << index << " | Difficulty: " << difficulty
              << std::endl;

    nonce = 0;
    hash = calculateHash();
    while (hash.substr(0, difficulty) != target)
    {
      nonce++;
      hash = calculateHash();
    }

    std::cout << "[Mined]  Block #" << index << " | Hash: " << hash
              << " | Nonce: " << nonce << std::endl;
  }

  bool Block::isValid() const
  {
    // Check hash matches contents
    if (hash != calculateHash())
    {
      return false;
    }

    // Check difficulty target
    std::string target(difficulty, '0');
    if (hash.substr(0, difficulty) != target)
    {
      return false;
    }

    // Check Merkle root
    if (merkleRoot != computeMerkleRoot())
    {
      return false;
    }

    return true;
  }

  bool Block::hasValidTransactions() const
  {
    for (const auto &tx : transactions)
    {
      if (!tx.verify())
      {
        return false;
      }
    }
    return true;
  }

  nlohmann::json Block::toJson() const
  {
    nlohmann::json j;
    j["index"] = index;
    j["timestamp"] = timestamp;
    j["previousHash"] = previousHash;
    j["merkleRoot"] = merkleRoot;
    j["nonce"] = nonce;
    j["difficulty"] = difficulty;
    j["hash"] = hash;

    j["transactions"] = nlohmann::json::array();
    for (const auto &tx : transactions)
    {
      j["transactions"].push_back(tx.toJson());
    }

    return j;
  }

  Block Block::fromJson(const nlohmann::json &j)
  {
    Block b;
    b.index = j.value("index", uint64_t(0));
    b.timestamp = j.value("timestamp", "");
    b.previousHash = j.value("previousHash", "");
    b.merkleRoot = j.value("merkleRoot", "");
    b.nonce = j.value("nonce", uint64_t(0));
    b.difficulty = j.value("difficulty", uint32_t(1));
    b.hash = j.value("hash", "");

    if (j.contains("transactions") && j["transactions"].is_array())
    {
      for (const auto &txJson : j["transactions"])
      {
        b.transactions.push_back(Transaction::fromJson(txJson));
      }
    }

    return b;
  }

  Block Block::createGenesis(uint32_t difficulty)
  {
    Block genesis;
    genesis.index = 0;
    genesis.previousHash = std::string(64, '0');
    genesis.difficulty = difficulty;

    // Fixed genesis timestamp — must be identical across all nodes
    genesis.timestamp = "2026-01-01T00:00:00Z";

    // Coinbase transaction in genesis
    Transaction coinbase;
    coinbase.senderPublicKey = "COINBASE";
    coinbase.fromAddress = "COINBASE";
    coinbase.toAddress = "GENESIS";
    coinbase.receiverPublicKey = "";
    coinbase.amount = 50;
    coinbase.payload = "Genesis Block";
    coinbase.timestamp = genesis.timestamp;
    coinbase.computeTxID();
    genesis.transactions.push_back(coinbase);

    genesis.merkleRoot = genesis.computeMerkleRoot();
    genesis.mineBlock();

    return genesis;
  }

} // namespace blockchain
