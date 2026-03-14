#include "network/connection_manager.h"

#include <algorithm>

namespace blockchain
{
    namespace network
    {

        ConnectionManager::ConnectionManager(size_t maxOutboundPeers, size_t maxInboundPeers)
            : maxOutboundPeers_(maxOutboundPeers),
              maxInboundPeers_(maxInboundPeers)
        {
        }

        bool ConnectionManager::canAccept(ConnectionDirection direction) const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            size_t inbound = 0;
            size_t outbound = 0;
            for (const auto &entry : peers_)
            {
                if (entry.second == ConnectionDirection::INBOUND)
                {
                    ++inbound;
                }
                else
                {
                    ++outbound;
                }
            }

            if (direction == ConnectionDirection::INBOUND)
            {
                return inbound < maxInboundPeers_;
            }
            return outbound < maxOutboundPeers_;
        }

        void ConnectionManager::registerConnection(const std::string &endpoint, ConnectionDirection direction)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            peers_[endpoint] = direction;
        }

        void ConnectionManager::unregisterConnection(const std::string &endpoint)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            peers_.erase(endpoint);
        }

        size_t ConnectionManager::inboundCount() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            size_t count = 0;
            for (const auto &entry : peers_)
            {
                if (entry.second == ConnectionDirection::INBOUND)
                {
                    ++count;
                }
            }
            return count;
        }

        size_t ConnectionManager::outboundCount() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            size_t count = 0;
            for (const auto &entry : peers_)
            {
                if (entry.second == ConnectionDirection::OUTBOUND)
                {
                    ++count;
                }
            }
            return count;
        }

        size_t ConnectionManager::totalCount() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return peers_.size();
        }

        std::string ConnectionManager::selectLowestScorePeerToDrop(
            const std::vector<std::pair<std::string, int32_t>> &scores) const
        {
            if (scores.empty())
            {
                return std::string();
            }

            auto lowest = std::min_element(scores.begin(), scores.end(),
                                           [](const auto &a, const auto &b)
                                           {
                                               return a.second < b.second;
                                           });
            return lowest->first;
        }

    } // namespace network
} // namespace blockchain
