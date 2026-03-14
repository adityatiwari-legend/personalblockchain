#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace blockchain
{
    namespace network
    {

        /**
         * PeerInfo — Tracks metadata and reputation score for each known peer.
         *
         * Scoring system:
         *   - Starts at 100
         *   - +1 for valid block relayed
         *   - +1 for valid transaction relayed
         *   - -10 for invalid block
         *   - -5 for invalid transaction
         *   - -20 for repeated misbehavior (rapid penalty)
         *   - Peer is banned when score drops below banThreshold (default: 0)
         */
        struct PeerInfo
        {
            std::string host;
            uint16_t port = 0;
            int32_t score = 100;                            // Reputation score
            bool banned = false;                            // Hard ban flag
            int32_t invalidBlockCount = 0;                  // Track repeated offenses
            std::chrono::steady_clock::time_point lastSeen; // Last activity
            std::chrono::steady_clock::time_point lastPingSent;
            bool pendingPong = false; // Waiting for PONG response

            /** Unique key for this peer address. */
            std::string key() const { return host + ":" + std::to_string(port); }
        };

        /**
         * PeerScorer — Manages reputation scores for all known peers.
         *
         * Used by the Node to decide:
         *   - Whether to accept connections from a peer
         *   - Whether to drop a misbehaving peer
         *   - Which peers to share in gossip responses
         */
        class PeerScorer
        {
        public:
            static constexpr int32_t INITIAL_SCORE = 100;
            static constexpr int32_t BAN_THRESHOLD = 0;
            static constexpr int32_t REWARD_VALID_BLOCK = 1;
            static constexpr int32_t REWARD_VALID_TX = 1;
            static constexpr int32_t PENALTY_INVALID_BLOCK = -10;
            static constexpr int32_t PENALTY_INVALID_TX = -5;
            static constexpr int32_t PENALTY_RAPID_MISBEHAVIOR = -20;

            /**
             * Register a known peer. No-op if already known.
             */
            void addPeer(const std::string &host, uint16_t port);

            /**
             * Reward a peer for good behavior.
             */
            void rewardPeer(const std::string &key, int32_t amount);

            /**
             * Penalize a peer. If score drops below threshold, bans the peer.
             * @return true if the peer is now banned
             */
            bool penalizePeer(const std::string &key, int32_t penalty);

            /**
             * Check if a peer is banned.
             */
            bool isBanned(const std::string &key) const;

            /**
             * Get score for a peer. Returns 0 if unknown.
             */
            int32_t getScore(const std::string &key) const;

            /**
             * Update last-seen timestamp for a peer.
             */
            void markSeen(const std::string &key);

            /** Mark that a heartbeat ping was sent to a peer. */
            void markPingSent(const std::string &key);

            /** Mark that a heartbeat pong was received from a peer. */
            void markPongReceived(const std::string &key);

            /**
             * Return peers with overdue pongs.
             * @param timeoutSeconds Max allowed age of pending pong
             */
            std::vector<std::string> getHeartbeatTimeoutPeers(int64_t timeoutSeconds) const;

            /**
             * Get all known, non-banned peer addresses (for gossip sharing).
             * Returns list of "host:port" strings.
             */
            std::vector<std::string> getShareablePeers() const;

            /**
             * Get full peer info map (for diagnostics).
             */
            std::map<std::string, PeerInfo> getAllPeers() const;

            /**
             * Remove peers that haven't been seen for a given duration.
             * @param staleSeconds Duration in seconds after which a peer is stale
             */
            void evictStalePeers(int64_t staleSeconds = 300);

        private:
            mutable std::mutex mutex_;
            std::map<std::string, PeerInfo> peers_;
        };

    } // namespace network
} // namespace blockchain
