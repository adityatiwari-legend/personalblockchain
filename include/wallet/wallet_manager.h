#pragma once

#include "core/blockchain.h"
#include "wallet/wallet.h"

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace blockchain
{

    class WalletManager
    {
    public:
        explicit WalletManager(Blockchain &blockchain);

        Wallet createWallet() const;
        Wallet importWallet(const std::string &privateKeyHex) const;

        std::string addressFromPublicKey(const std::string &publicKey) const;

        std::string createLoginChallenge(const std::string &address,
                                         const std::string &publicKey);

        std::optional<std::string> verifyLogin(const std::string &address,
                                               const std::string &publicKey,
                                               const std::string &challenge,
                                               const std::string &signatureHex);

        std::optional<std::string> verifyPrivateKeyLogin(const std::string &privateKeyHex,
                                                         std::string &outAddress,
                                                         std::string &outPublicKey);

        static bool isValidPrivateKeyHex(const std::string &privateKeyHex);
        static bool isValidWalletAddress(const std::string &address);

        uint64_t getBalance(const std::string &address) const;
        uint64_t getLastNonce(const std::string &address) const;
        std::vector<Transaction> getTransactions(const std::string &address) const;

    private:
        struct ChallengeRecord
        {
            std::string address;
            std::string publicKey;
            std::chrono::system_clock::time_point expiresAt;
        };

        std::string randomChallenge() const;

        Blockchain &blockchain_;
        mutable std::mutex mutex_;
        std::unordered_map<std::string, ChallengeRecord> challengeStore_;
    };

} // namespace blockchain
