#pragma once

#include "utils/json.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>

namespace blockchain
{
    namespace network
    {

        class NetworkMetrics
        {
        public:
            NetworkMetrics();

            void setConnectedPeers(size_t count);
            void setKnownPeers(size_t count);
            void setBannedPeers(size_t count);

            void incrementRejectedPeers();

            nlohmann::json toJson() const;

        private:
            std::chrono::steady_clock::time_point startedAt_;
            std::atomic<size_t> connectedPeers_;
            std::atomic<size_t> knownPeers_;
            std::atomic<size_t> rejectedPeers_;
            std::atomic<size_t> bannedPeers_;
        };

    } // namespace network
} // namespace blockchain
