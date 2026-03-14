#pragma once

#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace blockchain
{
    namespace network
    {

        class BootstrapManager
        {
        public:
            void setBootstrapPeers(const std::vector<std::string> &peers);
            std::vector<std::string> bootstrapPeers() const;

            void registerPeer(const std::string &endpoint);
            std::vector<std::string> knownTopology(size_t maxCount = 256) const;

        private:
            mutable std::mutex mutex_;
            std::vector<std::string> bootstrapPeers_;
            std::unordered_set<std::string> topologyPeers_;
        };

    } // namespace network
} // namespace blockchain
