#include "network/network_metrics.h"

namespace blockchain
{
    namespace network
    {

        NetworkMetrics::NetworkMetrics()
            : startedAt_(std::chrono::steady_clock::now()),
              connectedPeers_(0),
              knownPeers_(0),
              rejectedPeers_(0),
              bannedPeers_(0)
        {
        }

        void NetworkMetrics::setConnectedPeers(size_t count)
        {
            connectedPeers_.store(count, std::memory_order_relaxed);
        }

        void NetworkMetrics::setKnownPeers(size_t count)
        {
            knownPeers_.store(count, std::memory_order_relaxed);
        }

        void NetworkMetrics::setBannedPeers(size_t count)
        {
            bannedPeers_.store(count, std::memory_order_relaxed);
        }

        void NetworkMetrics::incrementRejectedPeers()
        {
            rejectedPeers_.fetch_add(1, std::memory_order_relaxed);
        }

        nlohmann::json NetworkMetrics::toJson() const
        {
            const auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - startedAt_);

            nlohmann::json j;
            j["connectedPeers"] = connectedPeers_.load(std::memory_order_relaxed);
            j["knownPeers"] = knownPeers_.load(std::memory_order_relaxed);
            j["rejectedPeers"] = rejectedPeers_.load(std::memory_order_relaxed);
            j["bannedPeers"] = bannedPeers_.load(std::memory_order_relaxed);
            j["networkUptimeSeconds"] = uptime.count();
            return j;
        }

    } // namespace network
} // namespace blockchain
