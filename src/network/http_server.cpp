#include "network/http_server.h"
#include "utils/json.hpp"

#include <algorithm>
#include <iostream>
#include <sstream>

namespace blockchain
{
  namespace network
  {

    HttpServer::HttpServer(boost::asio::io_context &ioContext,
                           Blockchain &blockchain, Node &node, uint16_t httpPort)
        : ioContext_(ioContext),
          acceptor_(ioContext, tcp::endpoint(tcp::v4(), httpPort)),
          blockchain_(blockchain), node_(node), httpPort_(httpPort)
    {
      std::cout << "[HTTP] Server listening on port " << httpPort << std::endl;
    }

    void HttpServer::start() { doAccept(); }

    // --- HTTP Response Serialization ---

    std::string HttpServer::HttpResponse::serialize() const
    {
      std::ostringstream oss;
      oss << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n";
      oss << "Content-Type: " << contentType << "\r\n";
      oss << "Content-Length: " << body.size() << "\r\n";
      oss << "Access-Control-Allow-Origin: *\r\n";
      oss << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
      oss << "Access-Control-Allow-Headers: Content-Type\r\n";
      oss << "Connection: close\r\n";
      oss << "\r\n";
      oss << body;
      return oss.str();
    }

    // --- Accept Loop ---

    void HttpServer::doAccept()
    {
      auto socket = std::make_shared<tcp::socket>(ioContext_);
      acceptor_.async_accept(*socket, [this, socket](boost::system::error_code ec)
                             {
    if (!ec) {
      handleConnection(socket);
    }
    doAccept(); });
    }

    // --- Connection Handler ---

    void HttpServer::handleConnection(std::shared_ptr<tcp::socket> socket)
    {
      auto buf = std::make_shared<boost::asio::streambuf>();

      boost::asio::async_read_until(
          *socket, *buf, "\r\n\r\n",
          [this, socket, buf](boost::system::error_code ec, std::size_t bytesTransferred)
          {
            if (ec)
            {
              return;
            }

            // Extract the raw data from streambuf
            std::string rawData(
                boost::asio::buffers_begin(buf->data()),
                boost::asio::buffers_end(buf->data()));
            buf->consume(buf->size());

            // Split headers and any body data that was already read
            size_t headerEnd = rawData.find("\r\n\r\n");
            std::string rawHeaders = rawData.substr(0, headerEnd + 4);
            std::string extraBody = rawData.substr(headerEnd + 4);

            HttpRequest req = parseRequest(rawHeaders);

            // Read body if Content-Length is specified
            auto clIt = req.headers.find("content-length");
            if (clIt != req.headers.end())
            {
              size_t contentLen = 0;
              try
              {
                contentLen = std::stoul(clIt->second);
              }
              catch (...)
              {
                // Malformed Content-Length
                HttpResponse resp;
                resp.statusCode = 400;
                resp.statusText = "Bad Request";
                resp.body = R"({"error": "Invalid Content-Length header"})";
                auto data = std::make_shared<std::string>(resp.serialize());
                boost::asio::async_write(
                    *socket, boost::asio::buffer(*data),
                    [socket, data](boost::system::error_code, std::size_t)
                    {
                      boost::system::error_code shutdownEc;
                      socket->shutdown(tcp::socket::shutdown_both, shutdownEc);
                    });
                return;
              }

              // Reject bodies larger than 1 MB to prevent OOM
              static constexpr size_t MAX_BODY_SIZE = 1 * 1024 * 1024;
              if (contentLen > MAX_BODY_SIZE)
              {
                HttpResponse resp;
                resp.statusCode = 413;
                resp.statusText = "Payload Too Large";
                resp.body = "{\"error\": \"Request body too large (max 1MB)\"}";
                auto data = std::make_shared<std::string>(resp.serialize());
                boost::asio::async_write(
                    *socket, boost::asio::buffer(*data),
                    [socket, data](boost::system::error_code, std::size_t)
                    {
                      boost::system::error_code shutdownEc;
                      socket->shutdown(tcp::socket::shutdown_both, shutdownEc);
                    });
                return;
              }

              size_t alreadyRead = extraBody.size();
              size_t remaining =
                  (contentLen > alreadyRead) ? (contentLen - alreadyRead) : 0;

              if (remaining > 0)
              {
                auto bodyBuf = std::make_shared<std::vector<char>>(remaining);
                auto extraBodyPtr = std::make_shared<std::string>(std::move(extraBody));
                boost::asio::async_read(
                    *socket, boost::asio::buffer(*bodyBuf),
                    [this, socket, req, bodyBuf,
                     extraBodyPtr](boost::system::error_code ec2,
                                   std::size_t /*bytes*/) mutable
                    {
                      if (!ec2)
                      {
                        req.body = *extraBodyPtr + std::string(bodyBuf->begin(), bodyBuf->end());
                      }

                      HttpResponse resp = handleRequest(req);
                      auto data = std::make_shared<std::string>(resp.serialize());

                      boost::asio::async_write(
                          *socket, boost::asio::buffer(*data),
                          [socket, data](boost::system::error_code /*ec*/,
                                         std::size_t /*bytes*/)
                          {
                            boost::system::error_code shutdownEc;
                            socket->shutdown(tcp::socket::shutdown_both, shutdownEc);
                          });
                    });
                return;
              }
              else
              {
                // All body data was already in the header read
                req.body = extraBody.substr(0, contentLen);
              }
            }

            HttpResponse resp = handleRequest(req);
            auto data = std::make_shared<std::string>(resp.serialize());

            boost::asio::async_write(
                *socket, boost::asio::buffer(*data),
                [socket, data](boost::system::error_code /*ec*/, std::size_t /*bytes*/)
                {
                  boost::system::error_code shutdownEc;
                  socket->shutdown(tcp::socket::shutdown_both, shutdownEc);
                });
          });
    }

    // --- Request Parser ---

    HttpServer::HttpRequest HttpServer::parseRequest(const std::string &raw)
    {
      HttpRequest req;
      std::istringstream stream(raw);
      std::string line;

      // Parse request line: METHOD PATH HTTP/1.1
      if (std::getline(stream, line))
      {
        // Remove trailing \r
        if (!line.empty() && line.back() == '\r')
          line.pop_back();

        size_t sp1 = line.find(' ');
        size_t sp2 = line.find(' ', sp1 + 1);

        if (sp1 != std::string::npos && sp2 != std::string::npos)
        {
          req.method = line.substr(0, sp1);
          req.path = line.substr(sp1 + 1, sp2 - sp1 - 1);
        }
      }

      // Parse headers
      while (std::getline(stream, line))
      {
        if (!line.empty() && line.back() == '\r')
          line.pop_back();
        if (line.empty())
          break;

        size_t colon = line.find(':');
        if (colon != std::string::npos)
        {
          std::string key = line.substr(0, colon);
          std::string val = line.substr(colon + 1);
          // Trim leading whitespace
          val.erase(0, val.find_first_not_of(' '));
          // Lowercase key for case-insensitive lookup
          std::transform(key.begin(), key.end(), key.begin(),
                         [](char c)
                         { return std::tolower(c); });
          req.headers[key] = val;
        }
      }

      return req;
    }

    // --- Request Router ---

    HttpServer::HttpResponse HttpServer::handleRequest(const HttpRequest &req)
    {
      // Handle CORS preflight
      if (req.method == "OPTIONS")
      {
        HttpResponse resp;
        resp.statusCode = 204;
        resp.statusText = "No Content";
        resp.body = "";
        return resp;
      }

      if (req.method == "POST" && req.path == "/wallet/create")
      {
        return handleCreateWallet();
      }
      if (req.method == "POST" && req.path == "/transaction/send")
      {
        return handleSendTransaction(req.body);
      }
      if (req.method == "POST" && req.path == "/mine")
      {
        return handleMine(req.body);
      }
      if (req.method == "GET" && req.path == "/chain")
      {
        return handleGetChain();
      }
      if (req.method == "GET" && req.path == "/mempool")
      {
        return handleGetMempool();
      }
      if (req.method == "GET" && req.path == "/peers")
      {
        return handleGetPeers();
      }
      if (req.method == "GET" && req.path == "/health")
      {
        return handleHealth();
      }
      if (req.method == "GET" && req.path == "/status")
      {
        return handleHealth();
      }

      HttpResponse resp;
      resp.statusCode = 404;
      resp.statusText = "Not Found";
      resp.body = R"({"error": "Endpoint not found"})";
      return resp;
    }

    // --- Route Handlers ---

    HttpServer::HttpResponse HttpServer::handleCreateWallet()
    {
      try
      {
        auto wallet = std::make_shared<Wallet>();

        {
          std::lock_guard<std::mutex> lock(walletsMutex_);
          wallets_[wallet->getPublicKey()] = wallet;
        }

        nlohmann::json j;
        j["publicKey"] = wallet->getPublicKey();
        j["privateKey"] = wallet->getPrivateKey();
        j["message"] =
            "Wallet created. SAVE YOUR PRIVATE KEY - it cannot be recovered!";

        HttpResponse resp;
        resp.body = j.dump(2);
        return resp;
      }
      catch (const std::exception &e)
      {
        HttpResponse resp;
        resp.statusCode = 500;
        resp.statusText = "Internal Server Error";
        resp.body = nlohmann::json({{"error", e.what()}}).dump();
        return resp;
      }
    }

    HttpServer::HttpResponse
    HttpServer::handleSendTransaction(const std::string &body)
    {
      try
      {
        auto j = nlohmann::json::parse(body);

        std::string senderPrivKey = j.value("senderPrivateKey", "");
        std::string senderPubKey = j.value("senderPublicKey", "");
        std::string receiverPubKey = j.value("receiverPublicKey", "");
        std::string payload = j.value("payload", "");

        if (senderPrivKey.empty() || receiverPubKey.empty())
        {
          HttpResponse resp;
          resp.statusCode = 400;
          resp.statusText = "Bad Request";
          resp.body =
              R"({"error": "Missing required fields: senderPrivateKey, receiverPublicKey"})";
          return resp;
        }

        // Create the wallet from private key to derive public key
        Wallet senderWallet(senderPrivKey);
        if (!senderPubKey.empty() && senderWallet.getPublicKey() != senderPubKey)
        {
          HttpResponse resp;
          resp.statusCode = 400;
          resp.statusText = "Bad Request";
          resp.body = R"({"error": "Public key does not match private key"})";
          return resp;
        }

        Transaction tx;
        tx.senderPublicKey = senderWallet.getPublicKey();
        tx.receiverPublicKey = receiverPubKey;
        tx.payload = payload;
        tx.timestamp = Transaction::currentTimestamp();
        tx.computeTxID();
        tx.sign(senderPrivKey);

        bool accepted = blockchain_.addTransaction(tx);

        if (accepted)
        {
          // Note: broadcast happens automatically via blockchain callback

          nlohmann::json resp;
          resp["message"] = "Transaction accepted";
          resp["txID"] = tx.txID;
          resp["transaction"] = tx.toJson();

          HttpResponse httpResp;
          httpResp.body = resp.dump(2);
          return httpResp;
        }
        else
        {
          HttpResponse resp;
          resp.statusCode = 400;
          resp.statusText = "Bad Request";
          resp.body = "{\"error\": \"Transaction rejected (invalid signature, "
                      "expired, or double-spend)\"}";
          return resp;
        }
      }
      catch (const nlohmann::json::exception &e)
      {
        HttpResponse resp;
        resp.statusCode = 400;
        resp.statusText = "Bad Request";
        resp.body = nlohmann::json(
                        {{"error", std::string("JSON parse error: ") + e.what()}})
                        .dump();
        return resp;
      }
      catch (const std::exception &e)
      {
        HttpResponse resp;
        resp.statusCode = 500;
        resp.statusText = "Internal Server Error";
        resp.body = nlohmann::json({{"error", e.what()}}).dump();
        return resp;
      }
    }

    HttpServer::HttpResponse HttpServer::handleMine(const std::string &body)
    {
      try
      {
        std::string minerAddress;

        if (!body.empty())
        {
          auto j = nlohmann::json::parse(body);
          minerAddress = j.value("minerAddress", "");
        }

        if (minerAddress.empty())
        {
          // Create a temporary wallet for mining reward
          Wallet minerWallet;
          minerAddress = minerWallet.getPublicKey();
        }

        Block newBlock = blockchain_.minePendingTransactions(minerAddress);

        // Note: broadcast happens automatically via blockchain callback

        nlohmann::json resp;
        resp["message"] = "Block mined successfully";
        resp["block"] = newBlock.toJson();

        HttpResponse httpResp;
        httpResp.body = resp.dump(2);
        return httpResp;
      }
      catch (const std::exception &e)
      {
        HttpResponse resp;
        resp.statusCode = 500;
        resp.statusText = "Internal Server Error";
        resp.body = nlohmann::json({{"error", e.what()}}).dump();
        return resp;
      }
    }

    HttpServer::HttpResponse HttpServer::handleGetChain()
    {
      nlohmann::json j;
      j["length"] = blockchain_.getChainLength();
      j["chain"] = blockchain_.chainToJson();

        HttpResponse resp;
        resp.body = j.dump(2);
        return resp;
      }
      catch (const std::exception &e)
      {
        HttpResponse resp;
        resp.statusCode = 500;
        resp.statusText = "Internal Server Error";
        resp.body = nlohmann::json({{"error", e.what()}}).dump();
        return resp;
      }
    }

    HttpServer::HttpResponse HttpServer::handleGetMempool()
    {
      try
      {
        auto txs = blockchain_.getMempool();
        nlohmann::json j = nlohmann::json::array();
        for (const auto &tx : txs)
        {
          j.push_back(tx.toJson());
        }

        nlohmann::json out;
        out["count"] = txs.size();
        out["transactions"] = j;

        HttpResponse resp;
        resp.body = out.dump(2);
        return resp;
      }
      catch (const std::exception &e)
      {
        HttpResponse resp;
        resp.statusCode = 500;
        resp.statusText = "Internal Server Error";
        resp.body = nlohmann::json({{"error", e.what()}}).dump();
        return resp;
      }
    }

    HttpServer::HttpResponse HttpServer::handleGetPeers()
    {
      try
      {
        auto peers = node_.getPeerList();
        nlohmann::json j;
        j["count"] = peers.size();
        j["peers"] = peers;

        HttpResponse resp;
        resp.body = j.dump(2);
        return resp;
      }
      catch (const std::exception &e)
      {
        HttpResponse resp;
        resp.statusCode = 500;
        resp.statusText = "Internal Server Error";
        resp.body = nlohmann::json({{"error", e.what()}}).dump();
        return resp;
      }
    }

    HttpServer::HttpResponse HttpServer::handleHealth()
    {
      nlohmann::json j;
      j["status"] = "ok";
      j["chainLength"] = blockchain_.getChainLength();
      j["difficulty"] = blockchain_.getDifficulty();
      j["peerCount"] = node_.getPeerList().size();
      j["mempoolSize"] = blockchain_.getMempool().size();

        HttpResponse resp;
        resp.body = j.dump(2);
        return resp;
      }
      catch (const std::exception &e)
      {
        HttpResponse resp;
        resp.statusCode = 500;
        resp.statusText = "Internal Server Error";
        resp.body = nlohmann::json({{"error", e.what()}}).dump();
        return resp;
      }
    }

  } // namespace network
} // namespace blockchain
