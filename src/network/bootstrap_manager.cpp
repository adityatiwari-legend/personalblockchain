#include "network/bootstrap_manager.h"

#include <algorithm>

namespace blockchain
{
    namespace network
    {

        void BootstrapManager::setBootstrapPeers(const std::vector<std::string> &peers)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            bootstrapPeers_.clear();
            topologyPeers_.clear();

            for (const auto &peer : peers)
            {
                if (peer.empty())
                {
                    continue;
                }

                if (std::find(bootstrapPeers_.begin(), bootstrapPeers_.end(), peer) == bootstrapPeers_.end())
                {
                    bootstrapPeers_.push_back(peer);
                }

                topologyPeers_.insert(peer);
            }
        }

        std::vector<std::string> BootstrapManager::bootstrapPeers() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return bootstrapPeers_;
        }

        void BootstrapManager::registerPeer(const std::string &endpoint)
        {
            if (endpoint.empty())
            {
                return;
            }

            std::lock_guard<std::mutex> lock(mutex_);
            topologyPeers_.insert(endpoint);
        }

        std::vector<std::string> BootstrapManager::knownTopology(size_t maxCount) const
        {
            std::lock_guard<std::mutex> lock(mutex_);

            std::vector<std::string> peers;
            peers.reserve(topologyPeers_.size());
            for (const auto &peer : topologyPeers_)
            {
                peers.push_back(peer);
            }

            if (peers.size() > maxCount)
            {
                peers.resize(maxCount);
            }
            return peers;
        }

    } // namespace network
} // namespace blockchain
