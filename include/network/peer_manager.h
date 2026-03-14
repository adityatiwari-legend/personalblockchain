#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace blockchain
{
    namespace network
    {

        class PeerManager
        {
        public:
            void addKnownPeer(const std::string &endpoint);
            void mergeKnownPeers(const std::vector<std::string> &peers);
            bool hasPeer(const std::string &endpoint) const;

            std::vector<std::string> getKnownPeers() const;
            std::vector<std::string> getRandomPeers(
                size_t maxCount,
                const std::vector<std::string> &exclude = {}) const;

            bool shouldAttemptConnection(
                const std::string &endpoint,
                int64_t cooldownMs) const;
            void markDialAttempt(const std::string &endpoint);

            void quarantinePeer(const std::string &endpoint, int64_t quarantineSeconds);
            bool isQuarantined(const std::string &endpoint) const;
            int64_t quarantinedPeerCount() const;

            static bool parseEndpoint(const std::string &endpoint, std::string &host, uint16_t &port);

        private:
            bool isQuarantinedUnlocked(const std::string &endpoint) const;

            mutable std::mutex mutex_;
            std::vector<std::string> knownPeers_;
            std::unordered_map<std::string, std::chrono::steady_clock::time_point> lastDialAttempt_;
            std::unordered_map<std::string, std::chrono::steady_clock::time_point> quarantinedUntil_;
        };

    } // namespace network
} // namespace blockchain
