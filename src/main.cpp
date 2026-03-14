#include "consensus/pow_engine.h"
#include "core/blockchain.h"
#include "network/http_server.h"
#include "network/node.h"
#include "storage/leveldb_manager.h"

#include "utils/json.hpp"

#include <atomic>
#include <boost/asio.hpp>
#include <csignal>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

static std::atomic<bool> running{true};
static std::atomic<bool> shutdownRequested{false};
static boost::asio::io_context *g_ioContext = nullptr;

void signalHandler(int)
{
  shutdownRequested.store(true);
  running = false;
  if (g_ioContext)
  {
    g_ioContext->stop();
  }
}

struct PeerAddress
{
  std::string host;
  uint16_t port = 0;
};

struct Config
{
  uint16_t p2pPort = 5000;
  uint16_t httpPort = 8000;
  uint32_t difficulty = 2;
  std::string dataDir = "./data";
  std::string consensus = "pow";
  std::string name = "PersonalBlockchain";
  std::string configPath = "config.json";
  std::string publicIp;

  std::vector<PeerAddress> peers;
  std::vector<PeerAddress> bootstrapNodes;

  size_t maxOutboundPeers = 8;
  size_t maxInboundPeers = 32;

  blockchain::network::NetworkTimeouts networkTimeouts;
  blockchain::network::RetryPolicy retryPolicy;
};

static bool parseEndpoint(const std::string &entry, PeerAddress &out)
{
  if (entry.empty())
  {
    return false;
  }

  size_t colonPos = entry.rfind(':');
  if (colonPos != std::string::npos && colonPos > 0)
  {
    out.host = entry.substr(0, colonPos);
    out.port = static_cast<uint16_t>(std::stoi(entry.substr(colonPos + 1)));
    return out.port > 0;
  }

  out.host = "127.0.0.1";
  out.port = static_cast<uint16_t>(std::stoi(entry));
  return out.port > 0;
}

static void parsePeerList(const std::string &peersStr, std::vector<PeerAddress> &target)
{
  std::istringstream ss(peersStr);
  std::string peerEntry;
  while (std::getline(ss, peerEntry, ','))
  {
    if (peerEntry.empty())
    {
      continue;
    }

    PeerAddress pa;
    if (parseEndpoint(peerEntry, pa))
    {
      target.push_back(pa);
    }
  }
}

static void loadConfigFile(const std::string &configPath, Config &config)
{
  std::ifstream in(configPath);
  if (!in.good())
  {
    return;
  }

  nlohmann::json j;
  in >> j;

  config.p2pPort = static_cast<uint16_t>(j.value("p2p_port", config.p2pPort));
  config.httpPort = static_cast<uint16_t>(j.value("http_port", config.httpPort));
  config.difficulty = static_cast<uint32_t>(j.value("difficulty", config.difficulty));
  config.dataDir = j.value("data_dir", config.dataDir);
  config.consensus = j.value("consensus", config.consensus);
  config.name = j.value("name", config.name);
  config.publicIp = j.value("public_ip", config.publicIp);

  if (j.contains("bootstrap_nodes") && j["bootstrap_nodes"].is_array())
  {
    config.bootstrapNodes.clear();
    for (const auto &item : j["bootstrap_nodes"])
    {
      if (!item.is_string())
      {
        continue;
      }
      PeerAddress pa;
      if (parseEndpoint(item.get<std::string>(), pa))
      {
        config.bootstrapNodes.push_back(pa);
      }
    }
  }

  if (j.contains("seed_peers") && j["seed_peers"].is_array())
  {
    config.peers.clear();
    for (const auto &item : j["seed_peers"])
    {
      if (!item.is_string())
      {
        continue;
      }
      PeerAddress pa;
      if (parseEndpoint(item.get<std::string>(), pa))
      {
        config.peers.push_back(pa);
      }
    }
  }
  else if (j.contains("peers") && j["peers"].is_array())
  {
    config.peers.clear();
    for (const auto &item : j["peers"])
    {
      if (!item.is_string())
      {
        continue;
      }
      PeerAddress pa;
      if (parseEndpoint(item.get<std::string>(), pa))
      {
        config.peers.push_back(pa);
      }
    }
  }

  if (j.contains("peer_limits") && j["peer_limits"].is_object())
  {
    const auto &limits = j["peer_limits"];
    config.maxOutboundPeers = limits.value("max_outbound_peers", config.maxOutboundPeers);
    config.maxInboundPeers = limits.value("max_inbound_peers", config.maxInboundPeers);
  }

  if (j.contains("network_timeouts") && j["network_timeouts"].is_object())
  {
    const auto &t = j["network_timeouts"];
    config.networkTimeouts.heartbeatIntervalSec =
        t.value("heartbeat_interval_sec", config.networkTimeouts.heartbeatIntervalSec);
    config.networkTimeouts.heartbeatTimeoutSec =
        t.value("heartbeat_timeout_sec", config.networkTimeouts.heartbeatTimeoutSec);
    config.networkTimeouts.gossipIntervalSec =
        t.value("gossip_interval_sec", config.networkTimeouts.gossipIntervalSec);
    config.networkTimeouts.peerRotationSec =
        t.value("peer_rotation_sec", config.networkTimeouts.peerRotationSec);
    config.networkTimeouts.quarantineSec =
        t.value("quarantine_sec", config.networkTimeouts.quarantineSec);
    config.networkTimeouts.dedupWindowSec =
        t.value("dedup_window_sec", config.networkTimeouts.dedupWindowSec);
  }

  if (j.contains("retry_policy") && j["retry_policy"].is_object())
  {
    const auto &r = j["retry_policy"];
    config.retryPolicy.maxRetries =
        r.value("max_retries", config.retryPolicy.maxRetries);
    config.retryPolicy.baseDelayMs =
        r.value("base_delay_ms", config.retryPolicy.baseDelayMs);
    config.retryPolicy.maxDelayMs =
        r.value("max_delay_ms", config.retryPolicy.maxDelayMs);
    config.retryPolicy.jitterMs =
        r.value("jitter_ms", config.retryPolicy.jitterMs);
    config.retryPolicy.reconnectCooldownMs =
        r.value("reconnect_cooldown_ms", config.retryPolicy.reconnectCooldownMs);
  }
}

static Config parseArgs(int argc, char *argv[])
{
  Config config;

  for (int i = 1; i < argc; i++)
  {
    std::string arg = argv[i];
    if (arg == "--config" && i + 1 < argc)
    {
      config.configPath = argv[++i];
    }
  }

  loadConfigFile(config.configPath, config);

  for (int i = 1; i < argc; i++)
  {
    std::string arg = argv[i];

    if (arg == "--port" && i + 1 < argc)
    {
      config.p2pPort = static_cast<uint16_t>(std::stoi(argv[++i]));
    }
    else if (arg == "--http-port" && i + 1 < argc)
    {
      config.httpPort = static_cast<uint16_t>(std::stoi(argv[++i]));
    }
    else if (arg == "--difficulty" && i + 1 < argc)
    {
      config.difficulty = static_cast<uint32_t>(std::stoi(argv[++i]));
    }
    else if (arg == "--data-dir" && i + 1 < argc)
    {
      config.dataDir = argv[++i];
    }
    else if (arg == "--consensus" && i + 1 < argc)
    {
      config.consensus = argv[++i];
    }
    else if (arg == "--name" && i + 1 < argc)
    {
      config.name = argv[++i];
    }
    else if (arg == "--peers" && i + 1 < argc)
    {
      parsePeerList(argv[++i], config.peers);
    }
    else if (arg == "--bootstrap" && i + 1 < argc)
    {
      config.bootstrapNodes.clear();
      parsePeerList(argv[++i], config.bootstrapNodes);
    }
    else if (arg == "--public-ip" && i + 1 < argc)
    {
      config.publicIp = argv[++i];
    }
    else if (arg == "--max-outbound-peers" && i + 1 < argc)
    {
      config.maxOutboundPeers = static_cast<size_t>(std::stoul(argv[++i]));
    }
    else if (arg == "--max-inbound-peers" && i + 1 < argc)
    {
      config.maxInboundPeers = static_cast<size_t>(std::stoul(argv[++i]));
    }
    else if (arg == "--help" || arg == "-h")
    {
      std::cout << "PersonalBlockchain Node v3.0.0\n"
                << "Usage: blockchain_node [options]\n\n"
                << "Options:\n"
                << "  --config <path>                  Config file path (default: config.json)\n"
                << "  --port <port>                    P2P port\n"
                << "  --http-port <port>               HTTP API port\n"
                << "  --difficulty <d>                 Mining difficulty\n"
                << "  --data-dir <path>                Data directory\n"
                << "  --consensus <type>               Consensus engine (pow)\n"
                << "  --name <name>                    Chain name\n"
                << "  --peers <host:port,...>          Seed peers\n"
                << "  --bootstrap <host:port,...>      Bootstrap peers\n"
                << "  --public-ip <ip-or-host>         Public endpoint host for NAT/public nodes\n"
                << "  --max-outbound-peers <n>         Outbound peer cap\n"
                << "  --max-inbound-peers <n>          Inbound peer cap\n"
                << "  --help                           Show this help message\n";
      std::exit(0);
    }
  }

  return config;
}

int main(int argc, char *argv[])
{
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  Config config = parseArgs(argc, argv);

  std::cout << "========================================\n"
            << "  PersonalBlockchain Node v3.0.0\n"
            << "========================================\n"
            << "  Config:      " << config.configPath << "\n"
            << "  P2P Port:    " << config.p2pPort << "\n"
            << "  HTTP Port:   " << config.httpPort << "\n"
            << "  Difficulty:  " << config.difficulty << "\n"
            << "  Data Dir:    " << config.dataDir << "\n"
            << "  Public IP:   " << (config.publicIp.empty() ? "(auto)" : config.publicIp) << "\n"
            << "  Peer Limits: outbound=" << config.maxOutboundPeers
            << ", inbound=" << config.maxInboundPeers << "\n"
            << "========================================\n"
            << std::endl;

  try
  {
    blockchain::storage::LevelDBManager storage(config.dataDir);

    blockchain::consensus::PoWEngine powEngine;
    blockchain::consensus::IConsensusEngine *engine = &powEngine;

    blockchain::Blockchain chain(config.difficulty, engine, &storage);

    boost::asio::io_context ioContext;
    g_ioContext = &ioContext;

    blockchain::network::NodeOptions nodeOptions;
    nodeOptions.dataDir = config.dataDir;
    nodeOptions.publicIp = config.publicIp;
    nodeOptions.maxOutboundPeers = config.maxOutboundPeers;
    nodeOptions.maxInboundPeers = config.maxInboundPeers;
    nodeOptions.timeouts = config.networkTimeouts;
    nodeOptions.retryPolicy = config.retryPolicy;

    for (const auto &peer : config.bootstrapNodes)
    {
      nodeOptions.bootstrapNodes.push_back(peer.host + ":" + std::to_string(peer.port));
    }

    blockchain::network::Node p2pNode(ioContext, chain, config.p2pPort, nodeOptions);
    p2pNode.start();

    for (const auto &peer : config.peers)
    {
      p2pNode.connectToPeer(peer.host, peer.port);
    }

    blockchain::network::HttpServer httpServer(ioContext, chain, p2pNode,
                                               config.httpPort);
    httpServer.start();

    if (!chain.isChainValid())
    {
      std::cerr << "[Main] Chain validation failed" << std::endl;
      return 1;
    }

    std::cout << "[Main] Node started\n"
              << "[Main] HTTP API: http://localhost:" << config.httpPort << "\n"
              << "[Main] Network stats: /network/stats\n"
              << std::endl;

    unsigned int threadCount = std::max(2u, std::thread::hardware_concurrency());
    std::vector<std::thread> threads;
    for (unsigned int i = 0; i < threadCount - 1; i++)
    {
      threads.emplace_back([&ioContext]()
                           { ioContext.run(); });
    }

    ioContext.run();

    for (auto &t : threads)
    {
      if (t.joinable())
      {
        t.join();
      }
    }

    if (shutdownRequested.load())
    {
      chain.persistChain();
    }
  }
  catch (const std::exception &e)
  {
    std::cerr << "[Main] Fatal error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
