#include "wallet/wallet_manager.h"

#include "crypto/ecdsa.h"
#include "crypto/sha256.h"

#include <random>
#include <sstream>
#include <stdexcept>

namespace blockchain {

WalletManager::WalletManager(Blockchain &blockchain) : blockchain_(blockchain) {}

Wallet WalletManager::createWallet() const {
  return Wallet();
}

Wallet WalletManager::importWallet(const std::string &privateKeyHex) const {
  return Wallet(privateKeyHex);
}

std::string WalletManager::addressFromPublicKey(const std::string &publicKey) const {
  return crypto::sha256(publicKey);
}

std::string WalletManager::randomChallenge() const {
  std::random_device rd;
  std::mt19937_64 gen(rd());
  std::uniform_int_distribution<uint64_t> dist;

  std::ostringstream oss;
  oss << dist(gen) << ':' << dist(gen) << ':'
      << std::chrono::system_clock::now().time_since_epoch().count();
  return crypto::sha256(oss.str());
}

std::string WalletManager::createLoginChallenge(const std::string &address,
                                                const std::string &publicKey) {
  const std::string derived = addressFromPublicKey(publicKey);
  if (derived != address) {
    throw std::runtime_error("Address/public key mismatch");
  }

  const std::string challenge = randomChallenge();
  ChallengeRecord rec;
  rec.address = address;
  rec.publicKey = publicKey;
  rec.expiresAt = std::chrono::system_clock::now() + std::chrono::minutes(5);

  std::lock_guard<std::mutex> lock(mutex_);
  challengeStore_[challenge] = std::move(rec);
  return challenge;
}

std::optional<std::string> WalletManager::verifyLogin(const std::string &address,
                                                      const std::string &publicKey,
                                                      const std::string &challenge,
                                                      const std::string &signatureHex) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = challengeStore_.find(challenge);
  if (it == challengeStore_.end()) {
    return std::nullopt;
  }

  const auto now = std::chrono::system_clock::now();
  if (now > it->second.expiresAt) {
    challengeStore_.erase(it);
    return std::nullopt;
  }

  if (it->second.address != address || it->second.publicKey != publicKey) {
    challengeStore_.erase(it);
    return std::nullopt;
  }

  if (addressFromPublicKey(publicKey) != address) {
    challengeStore_.erase(it);
    return std::nullopt;
  }

  const bool ok = crypto::verify(challenge, signatureHex, publicKey);
  challengeStore_.erase(it);
  if (!ok) {
    return std::nullopt;
  }

  return crypto::sha256(address + ":" + std::to_string(now.time_since_epoch().count()));
}

uint64_t WalletManager::getBalance(const std::string &address) const {
  return blockchain_.getBalanceForAddress(address);
}

uint64_t WalletManager::getLastNonce(const std::string &address) const {
  return blockchain_.getLastNonceForAddress(address);
}

std::vector<Transaction> WalletManager::getTransactions(const std::string &address) const {
  return blockchain_.getTransactionsForAddress(address);
}

} // namespace blockchain
