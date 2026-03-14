#include "network/peer_scorer.h"

#include <algorithm>
#include <iostream>

namespace blockchain
{
    namespace network
    {

        void PeerScorer::addPeer(const std::string &host, uint16_t port)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            std::string k = host + ":" + std::to_string(port);
            if (peers_.find(k) == peers_.end())
            {
                PeerInfo info;
                info.host = host;
                info.port = port;
                info.score = INITIAL_SCORE;
                info.banned = false;
                info.invalidBlockCount = 0;
                info.lastSeen = std::chrono::steady_clock::now();
                peers_[k] = info;
                std::cout << "[PeerScore] Registered peer: " << k << std::endl;
            }
        }

        void PeerScorer::rewardPeer(const std::string &key, int32_t amount)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = peers_.find(key);
            if (it != peers_.end())
            {
                it->second.score += amount;
                // Cap score at 200 to prevent infinite accumulation
                if (it->second.score > 200)
                    it->second.score = 200;

                // Allow unbanning if score has recovered above threshold
                if (it->second.banned && it->second.score > BAN_THRESHOLD + 20)
                {
                    it->second.banned = false;
                    it->second.invalidBlockCount = 0;
                    std::cout << "[PeerScore] Unbanned peer " << key
                              << " (score recovered to " << it->second.score << ")" << std::endl;
                }
            }
        }

        bool PeerScorer::penalizePeer(const std::string &key, int32_t penalty)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = peers_.find(key);
            if (it == peers_.end())
                return false;

            it->second.score += penalty; // penalty is negative

            // Track repeated bad blocks (reset count periodically via eviction)
            if (penalty == PENALTY_INVALID_BLOCK)
            {
                it->second.invalidBlockCount++;
                // Only apply rapid misbehavior on the exact threshold crossing
                if (it->second.invalidBlockCount == 3)
                {
                    it->second.score += PENALTY_RAPID_MISBEHAVIOR;
                }
            }

            if (it->second.score <= BAN_THRESHOLD)
            {
                it->second.banned = true;
                std::cerr << "[PeerScore] BANNED peer " << key
                          << " (score: " << it->second.score << ")" << std::endl;
                return true;
            }

            std::cout << "[PeerScore] Penalized peer " << key
                      << " by " << penalty
                      << " (score: " << it->second.score << ")" << std::endl;
            return false;
        }

        bool PeerScorer::isBanned(const std::string &key) const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = peers_.find(key);
            if (it == peers_.end())
                return false;
            return it->second.banned;
        }

        int32_t PeerScorer::getScore(const std::string &key) const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = peers_.find(key);
            if (it == peers_.end())
                return 0;
            return it->second.score;
        }

        void PeerScorer::markSeen(const std::string &key)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = peers_.find(key);
            if (it != peers_.end())
            {
                it->second.lastSeen = std::chrono::steady_clock::now();
            }
        }

        void PeerScorer::markPingSent(const std::string &key)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = peers_.find(key);
            if (it == peers_.end())
            {
                return;
            }

            if (it->second.pendingPong)
            {
                return;
            }

            it->second.lastPingSent = std::chrono::steady_clock::now();
            it->second.pendingPong = true;
        }

        void PeerScorer::markPongReceived(const std::string &key)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = peers_.find(key);
            if (it == peers_.end())
            {
                return;
            }

            it->second.pendingPong = false;
            it->second.lastSeen = std::chrono::steady_clock::now();
        }

        std::vector<std::string> PeerScorer::getHeartbeatTimeoutPeers(int64_t timeoutSeconds) const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            std::vector<std::string> timedOut;
            auto now = std::chrono::steady_clock::now();
            auto timeout = std::chrono::seconds(timeoutSeconds);

            for (const auto &[key, info] : peers_)
            {
                if (info.pendingPong && now - info.lastPingSent > timeout)
                {
                    timedOut.push_back(key);
                }
            }

            return timedOut;
        }

        std::vector<std::string> PeerScorer::getShareablePeers() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            std::vector<std::string> result;
            for (const auto &[key, info] : peers_)
            {
                if (!info.banned && info.score > 20) // Only share reputable peers
                {
                    result.push_back(key);
                }
            }
            return result;
        }

        std::map<std::string, PeerInfo> PeerScorer::getAllPeers() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return peers_;
        }

        void PeerScorer::evictStalePeers(int64_t staleSeconds)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto now = std::chrono::steady_clock::now();
            auto threshold = std::chrono::seconds(staleSeconds);

            for (auto it = peers_.begin(); it != peers_.end();)
            {
                if (now - it->second.lastSeen > threshold)
                {
                    std::cout << "[PeerScore] Evicting stale peer: " << it->first << std::endl;
                    it = peers_.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

    } // namespace network
} // namespace blockchain
