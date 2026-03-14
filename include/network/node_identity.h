#pragma once

#include <cstdint>
#include <mutex>
#include <string>

#include "utils/json.hpp"

namespace blockchain
{
    namespace network
    {

        struct NodeIdentityRecord
        {
            std::string nodeId;
            std::string publicKey;
            std::string privateKey;
            std::string advertisedIp;
            uint16_t port = 0;
            int32_t reputation = 100;
        };

        class NodeIdentity
        {
        public:
            explicit NodeIdentity(std::string dataDir);

            bool loadOrCreate(uint16_t listenPort, const std::string &publicIpOverride);
            bool persist() const;

            std::string endpoint() const;
            nlohmann::json toJson() const;

            const NodeIdentityRecord &record() const { return record_; }
            const std::string &identityPath() const { return identityPath_; }

            void setReputation(int32_t reputation);

        private:
            std::string detectAdvertisedIp() const;

            std::string dataDir_;
            std::string identityPath_;
            NodeIdentityRecord record_;
            mutable std::mutex mutex_;
        };

    } // namespace network
} // namespace blockchain
