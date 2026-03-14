#pragma once

#include "core/blockchain.h"
#include "network/node.h"
#include "wallet/wallet.h"

#include <boost/asio.hpp>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace blockchain
{
  namespace network
  {

    /**
     * Lightweight HTTP server for blockchain API endpoints.
     * Uses raw Boost.Asio TCP (not Beast) for simplicity and fewer dependencies.
     *
     * Endpoints:
     *   POST /wallet/create      - Create new wallet
     *   POST /transaction/send   - Submit transaction
     *   POST /mine               - Mine pending transactions
     *   GET  /chain              - View full chain
     *   GET  /mempool            - View pending transactions
     *   GET  /peers              - View connected peers
     *   GET  /peers/scores       - View peer reputation scores
     *   GET  /health             - Health check (+ consensus info)
     */
    class HttpServer
    {
    public:
      HttpServer(boost::asio::io_context &ioContext, Blockchain &blockchain,
                 Node &node, uint16_t httpPort);

      void start();

    private:
      using tcp = boost::asio::ip::tcp;

      struct HttpRequest
      {
        std::string method;
        std::string path;
        std::map<std::string, std::string> headers;
        std::string body;
      };

      struct HttpResponse
      {
        int statusCode = 200;
        std::string statusText = "OK";
        std::string contentType = "application/json";
        std::string body;

        std::string serialize() const;
      };

      void doAccept();
      void handleConnection(std::shared_ptr<tcp::socket> socket);
      HttpRequest parseRequest(const std::string &raw);
      HttpResponse handleRequest(const HttpRequest &req);

      // Route handlers
      HttpResponse handleCreateWallet();
      HttpResponse handleSendTransaction(const std::string &body);
      HttpResponse handleMine(const std::string &body);
      HttpResponse handleGetChain();
      HttpResponse handleGetMempool();
      HttpResponse handleGetPeers();
      HttpResponse handleGetPeerScores();
      HttpResponse handleNetworkStats();
      HttpResponse handleHealth();

      boost::asio::io_context &ioContext_;
      tcp::acceptor acceptor_;
      Blockchain &blockchain_;
      Node &node_;
      uint16_t httpPort_;

      // Store wallets created via API (in-memory only)
      std::map<std::string, std::shared_ptr<Wallet>> wallets_;
      std::mutex walletsMutex_;
    };

  } // namespace network
} // namespace blockchain
