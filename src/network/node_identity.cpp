#include "network/node_identity.h"

#include "crypto/sha256.h"
#include "utils/json.hpp"
#include "wallet/wallet.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace blockchain
{
    namespace network
    {

        NodeIdentity::NodeIdentity(std::string dataDir)
            : dataDir_(std::move(dataDir)),
              identityPath_(dataDir_ + "/node_identity.json")
        {
        }

        bool NodeIdentity::loadOrCreate(uint16_t listenPort, const std::string &publicIpOverride)
        {
            std::error_code ec;
            std::filesystem::create_directories(dataDir_, ec);

            std::ifstream in(identityPath_);
            if (in.good())
            {
                try
                {
                    nlohmann::json j;
                    in >> j;

                    NodeIdentityRecord loaded;
                    loaded.publicKey = j.value("publicKey", "");
                    loaded.privateKey = j.value("privateKey", "");
                    loaded.nodeId = j.value("nodeId", "");
                    loaded.advertisedIp = j.value("advertisedIp", "");
                    loaded.port = static_cast<uint16_t>(j.value("port", listenPort));
                    loaded.reputation = j.value("reputation", 100);

                    if (loaded.publicKey.empty() || loaded.privateKey.empty() || loaded.nodeId.empty())
                    {
                        throw std::runtime_error("Identity file is incomplete");
                    }

                    if (!publicIpOverride.empty())
                    {
                        loaded.advertisedIp = publicIpOverride;
                    }
                    if (loaded.advertisedIp.empty())
                    {
                        loaded.advertisedIp = detectAdvertisedIp();
                    }

                    loaded.port = listenPort;

                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        record_ = std::move(loaded);
                    }

                    return persist();
                }
                catch (const std::exception &e)
                {
                    std::cerr << "[Identity] Failed to load existing identity, regenerating: "
                              << e.what() << std::endl;
                }
            }

            Wallet wallet;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                record_.publicKey = wallet.getPublicKey();
                record_.privateKey = wallet.getPrivateKey();
                record_.nodeId = blockchain::crypto::sha256(record_.publicKey);
                record_.advertisedIp = publicIpOverride.empty() ? detectAdvertisedIp() : publicIpOverride;
                record_.port = listenPort;
                record_.reputation = 100;
            }

            return persist();
        }

        bool NodeIdentity::persist() const
        {
            nlohmann::json out;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                out["nodeId"] = record_.nodeId;
                out["publicKey"] = record_.publicKey;
                out["privateKey"] = record_.privateKey;
                out["advertisedIp"] = record_.advertisedIp;
                out["port"] = record_.port;
                out["reputation"] = record_.reputation;
            }

            std::ofstream file(identityPath_, std::ios::trunc);
            if (!file.good())
            {
                std::cerr << "[Identity] Unable to persist identity file: " << identityPath_ << std::endl;
                return false;
            }

            file << out.dump(2) << std::endl;
            return true;
        }

        std::string NodeIdentity::detectAdvertisedIp() const
        {
            return "127.0.0.1";
        }

        std::string NodeIdentity::endpoint() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return record_.advertisedIp + ":" + std::to_string(record_.port);
        }

        nlohmann::json NodeIdentity::toJson() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            nlohmann::json j;
            j["nodeId"] = record_.nodeId;
            j["publicKey"] = record_.publicKey;
            j["advertisedIp"] = record_.advertisedIp;
            j["port"] = record_.port;
            j["reputation"] = record_.reputation;
            return j;
        }

        void NodeIdentity::setReputation(int32_t reputation)
        {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                record_.reputation = reputation;
            }
            persist();
        }

    } // namespace network
} // namespace blockchain
