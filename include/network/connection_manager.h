#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace blockchain
{
    namespace network
    {

        enum class ConnectionDirection
        {
            INBOUND,
            OUTBOUND
        };

        class ConnectionManager
        {
        public:
            ConnectionManager(size_t maxOutboundPeers, size_t maxInboundPeers);

            bool canAccept(ConnectionDirection direction) const;
            void registerConnection(const std::string &endpoint, ConnectionDirection direction);
            void unregisterConnection(const std::string &endpoint);

            size_t inboundCount() const;
            size_t outboundCount() const;
            size_t totalCount() const;

            std::string selectLowestScorePeerToDrop(
                const std::vector<std::pair<std::string, int32_t>> &scores) const;

            size_t maxOutboundPeers() const { return maxOutboundPeers_; }
            size_t maxInboundPeers() const { return maxInboundPeers_; }

        private:
            mutable std::mutex mutex_;
            std::unordered_map<std::string, ConnectionDirection> peers_;
            size_t maxOutboundPeers_;
            size_t maxInboundPeers_;
        };

    } // namespace network
} // namespace blockchain
