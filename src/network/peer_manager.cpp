#include "network/peer_manager.h"

#include <algorithm>
#include <random>
#include <set>

namespace blockchain
{
    namespace network
    {

        void PeerManager::addKnownPeer(const std::string &endpoint)
        {
            if (endpoint.empty())
            {
                return;
            }

            std::lock_guard<std::mutex> lock(mutex_);
            if (std::find(knownPeers_.begin(), knownPeers_.end(), endpoint) == knownPeers_.end())
            {
                knownPeers_.push_back(endpoint);
            }
        }

        void PeerManager::mergeKnownPeers(const std::vector<std::string> &peers)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto &peer : peers)
            {
                if (peer.empty())
                {
                    continue;
                }
                if (std::find(knownPeers_.begin(), knownPeers_.end(), peer) == knownPeers_.end())
                {
                    knownPeers_.push_back(peer);
                }
            }
        }

        bool PeerManager::hasPeer(const std::string &endpoint) const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return std::find(knownPeers_.begin(), knownPeers_.end(), endpoint) != knownPeers_.end();
        }

        std::vector<std::string> PeerManager::getKnownPeers() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return knownPeers_;
        }

        std::vector<std::string> PeerManager::getRandomPeers(
            size_t maxCount,
            const std::vector<std::string> &exclude) const
        {
            std::vector<std::string> result;
            std::lock_guard<std::mutex> lock(mutex_);

            std::set<std::string> excludeSet(exclude.begin(), exclude.end());
            std::vector<std::string> candidates;
            candidates.reserve(knownPeers_.size());
            for (const auto &peer : knownPeers_)
            {
                if (excludeSet.find(peer) == excludeSet.end() && !isQuarantinedUnlocked(peer))
                {
                    candidates.push_back(peer);
                }
            }

            std::random_device rd;
            std::mt19937 gen(rd());
            std::shuffle(candidates.begin(), candidates.end(), gen);

            if (candidates.size() > maxCount)
            {
                candidates.resize(maxCount);
            }
            return candidates;
        }

        bool PeerManager::shouldAttemptConnection(
            const std::string &endpoint,
            int64_t cooldownMs) const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (isQuarantinedUnlocked(endpoint))
            {
                return false;
            }

            auto it = lastDialAttempt_.find(endpoint);
            if (it == lastDialAttempt_.end())
            {
                return true;
            }

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - it->second);
            return elapsed.count() >= cooldownMs;
        }

        void PeerManager::markDialAttempt(const std::string &endpoint)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            lastDialAttempt_[endpoint] = std::chrono::steady_clock::now();
        }

        void PeerManager::quarantinePeer(const std::string &endpoint, int64_t quarantineSeconds)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            quarantinedUntil_[endpoint] = std::chrono::steady_clock::now() +
                                          std::chrono::seconds(quarantineSeconds);
        }

        bool PeerManager::isQuarantined(const std::string &endpoint) const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return isQuarantinedUnlocked(endpoint);
        }

        bool PeerManager::isQuarantinedUnlocked(const std::string &endpoint) const
        {
            auto it = quarantinedUntil_.find(endpoint);
            if (it == quarantinedUntil_.end())
            {
                return false;
            }
            return std::chrono::steady_clock::now() < it->second;
        }

        int64_t PeerManager::quarantinedPeerCount() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            int64_t count = 0;
            const auto now = std::chrono::steady_clock::now();
            for (const auto &item : quarantinedUntil_)
            {
                if (now < item.second)
                {
                    ++count;
                }
            }
            return count;
        }

        bool PeerManager::parseEndpoint(const std::string &endpoint, std::string &host, uint16_t &port)
        {
            const size_t split = endpoint.rfind(':');
            if (split == std::string::npos || split == 0)
            {
                return false;
            }

            host = endpoint.substr(0, split);
            try
            {
                port = static_cast<uint16_t>(std::stoi(endpoint.substr(split + 1)));
            }
            catch (...)
            {
                return false;
            }

            return port > 0;
        }

    } // namespace network
} // namespace blockchain
