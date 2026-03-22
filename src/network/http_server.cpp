#include "network/http_server.h"
#include "utils/json.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <regex>
#include <sstream>
#include <unordered_set>

namespace blockchain
{
  namespace network
  {

    HttpServer::HttpServer(boost::asio::io_context &ioContext,
                           Blockchain &blockchain, Node &node, uint16_t httpPort)
        : ioContext_(ioContext),
          acceptor_(ioContext, tcp::endpoint(tcp::v4(), httpPort)),
          blockchain_(blockchain), node_(node), httpPort_(httpPort),
          walletManager_(blockchain)
    {
      knownAddresses_.insert("GENESIS");
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
      const std::string routePath = req.path.substr(0, req.path.find('?'));

      // Handle CORS preflight
      if (req.method == "OPTIONS")
      {
        HttpResponse resp;
        resp.statusCode = 204;
        resp.statusText = "No Content";
        resp.body = "";
        return resp;
      }

      if (req.method == "POST" && routePath == "/wallet/create")
      {
        return handleCreateWallet();
      }
      if (req.method == "POST" && routePath == "/wallet/import")
      {
        return handleImportWallet(req.body);
      }
      if (req.method == "POST" && routePath == "/wallet/login")
      {
        return handleWalletLogin(req.body);
      }
      if (req.method == "POST" && routePath == "/wallet/loginChallenge")
      {
        return handleLoginChallenge(req.body);
      }
      if (req.method == "POST" && routePath == "/wallet/verifyLogin")
      {
        return handleVerifyLogin(req.body);
      }
      if (req.method == "POST" && routePath == "/transaction/send")
      {
        return handleSendTransaction(req.body);
      }
      if (req.method == "POST" && routePath.rfind("/mine", 0) == 0)
      {
        return handleMine(req.body, req.path);
      }
      if (req.method == "POST" && routePath == "/api/trade")
      {
        return handleTrade(req.body);
      }
      if (req.method == "GET" && routePath == "/chain")
      {
        return handleGetChain();
      }
      if (req.method == "GET" && routePath == "/transactions")
      {
        return handleGetTransactions();
      }
      if (req.method == "GET" && routePath == "/mempool")
      {
        return handleGetMempool();
      }
      if (req.method == "GET" && routePath.rfind("/wallet/balance/", 0) == 0)
      {
        return handleGetWalletBalance(routePath.substr(std::string("/wallet/balance/").size()));
      }
      if (req.method == "GET" && routePath.rfind("/wallet/transactions/", 0) == 0)
      {
        return handleGetWalletTransactions(routePath.substr(std::string("/wallet/transactions/").size()));
      }
      if (req.method == "GET" && routePath == "/peers")
      {
        return handleGetPeers();
      }
      if (req.method == "GET" && routePath == "/peers/scores")
      {
        return handleGetPeerScores();
      }
      if (req.method == "GET" && routePath == "/network/stats")
      {
        return handleNetworkStats();
      }
      if (req.method == "GET" && routePath == "/health")
      {
        return handleHealth();
      }
      if (req.method == "GET" && routePath == "/status")
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

    std::string HttpServer::queryParam(const std::string &path, const std::string &key)
    {
      size_t q = path.find('?');
      if (q == std::string::npos || q + 1 >= path.size())
      {
        return "";
      }

      std::string qs = path.substr(q + 1);
      std::istringstream ss(qs);
      std::string pair;
      while (std::getline(ss, pair, '&'))
      {
        size_t eq = pair.find('=');
        if (eq == std::string::npos)
        {
          continue;
        }
        if (pair.substr(0, eq) == key)
        {
          return pair.substr(eq + 1);
        }
      }

      return "";
    }

    bool HttpServer::isLikelyHex(const std::string &value)
    {
      if (value.empty())
      {
        return false;
      }
      for (char c : value)
      {
        if (!std::isxdigit(static_cast<unsigned char>(c)))
        {
          return false;
        }
      }
      return true;
    }

    std::string HttpServer::trim(const std::string &value)
    {
      size_t start = 0;
      while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])))
      {
        ++start;
      }
      size_t end = value.size();
      while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])))
      {
        --end;
      }
      return value.substr(start, end - start);
    }

    std::string HttpServer::toLower(std::string value)
    {
      std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
                     { return static_cast<char>(std::tolower(c)); });
      return value;
    }

    HttpServer::HttpResponse HttpServer::errorResponse(int statusCode,
                                                       const std::string &statusText,
                                                       const std::string &errorCode,
                                                       const std::string &message)
    {
      HttpResponse resp;
      resp.statusCode = statusCode;
      resp.statusText = statusText;
      resp.body = nlohmann::json({{"error", message},
                                  {"errorCode", errorCode},
                                  {"message", message}})
                      .dump();
      return resp;
    }

    bool HttpServer::isValidPrivateKey(const std::string &privateKey)
    {
      return WalletManager::isValidPrivateKeyHex(privateKey);
    }

    bool HttpServer::isValidAddress(const std::string &address)
    {
      return WalletManager::isValidWalletAddress(address);
    }

    bool HttpServer::walletExists(const std::string &address) const
    {
      if (!isValidAddress(address))
      {
        return false;
      }

      if (knownAddresses_.count(address) > 0)
      {
        return true;
      }

      if (blockchain_.getBalanceForAddress(address) > 0)
      {
        return true;
      }

      const auto txs = blockchain_.getTransactionsForAddress(address);
      return !txs.empty();
    }

    nlohmann::json HttpServer::withTxAliases(const nlohmann::json &txJson)
    {
      nlohmann::json row = txJson;
      row["transactionId"] = txJson.value("txID", "");
      row["sender"] = txJson.value("fromAddress", "");
      row["receiver"] = txJson.value("toAddress", "");
      return row;
    }

    HttpServer::HttpResponse HttpServer::handleCreateWallet()
    {
      try
      {
        Wallet wallet = walletManager_.createWallet();
        std::string address = walletManager_.addressFromPublicKey(wallet.getPublicKey());
        knownAddresses_.insert(address);

        nlohmann::json j;
        j["publicKey"] = wallet.getPublicKey();
        j["privateKey"] = wallet.getPrivateKey();
        j["address"] = address;
        j["message"] = "Wallet created. Save your private key locally.";

        HttpResponse resp;
        resp.body = j.dump(2);
        return resp;
      }
      catch (const std::exception &e)
      {
        return errorResponse(500, "Internal Server Error", "WALLET_CREATE_FAILED", e.what());
      }
    }

    HttpServer::HttpResponse HttpServer::handleImportWallet(const std::string &body)
    {
      try
      {
        auto j = nlohmann::json::parse(body);
        std::string privateKey = toLower(trim(j.value("privateKey", "")));
        if (!isValidPrivateKey(privateKey))
        {
          return errorResponse(400, "Bad Request", "INVALID_PRIVATE_KEY", "Private key must be exactly 64 hexadecimal characters");
        }

        Wallet wallet = walletManager_.importWallet(privateKey);
        const std::string address = walletManager_.addressFromPublicKey(wallet.getPublicKey());
        knownAddresses_.insert(address);

        HttpResponse resp;
        resp.body = nlohmann::json({{"publicKey", wallet.getPublicKey()},
                                    {"address", address},
                                    {"message", "Wallet imported"}})
                        .dump(2);
        return resp;
      }
      catch (const std::exception &e)
      {
        return errorResponse(400, "Bad Request", "WALLET_IMPORT_FAILED", e.what());
      }
    }

    HttpServer::HttpResponse HttpServer::handleWalletLogin(const std::string &body)
    {
      try
      {
        auto j = nlohmann::json::parse(body);
        const std::string privateKey = toLower(trim(j.value("privateKey", "")));

        if (!isValidPrivateKey(privateKey))
        {
          return errorResponse(400, "Bad Request", "INVALID_PRIVATE_KEY", "Private key must be exactly 64 hexadecimal characters");
        }

        std::string address;
        std::string publicKey;
        auto token = walletManager_.verifyPrivateKeyLogin(privateKey, address, publicKey);
        if (!token.has_value())
        {
          return errorResponse(401, "Unauthorized", "AUTH_FAILED", "Private key verification failed");
        }

        knownAddresses_.insert(address);

        HttpResponse resp;
        resp.body = nlohmann::json({{"address", address},
                                    {"publicKey", publicKey},
                                    {"token", token.value()},
                                    {"message", "Wallet authenticated"}})
                        .dump(2);
        return resp;
      }
      catch (const std::exception &e)
      {
        return errorResponse(400, "Bad Request", "AUTH_FAILED", e.what());
      }
    }

    HttpServer::HttpResponse HttpServer::handleLoginChallenge(const std::string &body)
    {
      try
      {
        auto j = nlohmann::json::parse(body);
        const std::string address = j.value("address", "");
        const std::string publicKey = j.value("publicKey", "");
        if (address.empty() || publicKey.empty())
        {
          return errorResponse(400, "Bad Request", "AUTH_FAILED", "address and publicKey are required");
        }

        if (!isValidAddress(address))
        {
          return errorResponse(400, "Bad Request", "INVALID_ADDRESS", "Wallet address format is invalid");
        }

        const std::string challenge = walletManager_.createLoginChallenge(address, publicKey);
        HttpResponse resp;
        resp.body = nlohmann::json({{"challenge", challenge},
                                    {"expiresInSec", 300}})
                        .dump(2);
        return resp;
      }
      catch (const std::exception &e)
      {
        return errorResponse(400, "Bad Request", "AUTH_FAILED", e.what());
      }
    }

    HttpServer::HttpResponse HttpServer::handleVerifyLogin(const std::string &body)
    {
      try
      {
        auto j = nlohmann::json::parse(body);
        const std::string address = j.value("address", "");
        const std::string publicKey = j.value("publicKey", "");
        const std::string challenge = j.value("challenge", "");
        const std::string signature = j.value("signature", "");

        if (address.empty() || publicKey.empty() || challenge.empty() || signature.empty())
        {
          return errorResponse(400, "Bad Request", "AUTH_FAILED", "address, publicKey, challenge, signature are required");
        }

        if (!isValidAddress(address))
        {
          return errorResponse(400, "Bad Request", "INVALID_ADDRESS", "Wallet address format is invalid");
        }

        auto token = walletManager_.verifyLogin(address, publicKey, challenge, signature);
        if (!token.has_value())
        {
          return errorResponse(401, "Unauthorized", "AUTH_FAILED", "Invalid login signature or expired challenge");
        }

        knownAddresses_.insert(address);

        HttpResponse resp;
        resp.body = nlohmann::json({{"address", address},
                                    {"token", token.value()}})
                        .dump(2);
        return resp;
      }
      catch (const std::exception &e)
      {
        return errorResponse(400, "Bad Request", "AUTH_FAILED", e.what());
      }
    }

    HttpServer::HttpResponse
    HttpServer::handleSendTransaction(const std::string &body)
    {
      try
      {
        auto j = nlohmann::json::parse(body);

        const std::string fromAddress = trim(j.value("fromAddress", ""));
        const std::string toAddress = trim(j.value("toAddress", ""));
        const std::string senderPublicKey = j.value("senderPublicKey", "");
        const std::string receiverPublicKey = j.value("receiverPublicKey", "");
        const std::string payload = j.value("payload", "");
        const std::string timestamp = j.value("timestamp", "");
        const std::string txID = j.value("txID", "");
        const std::string signature = j.value("signature", j.value("digitalSignature", ""));
        const uint64_t amount = j.value("amount", uint64_t(0));
        const uint64_t nonce = j.value("nonce", uint64_t(0));

        if (fromAddress.empty() || toAddress.empty() || senderPublicKey.empty() || signature.empty())
        {
          return errorResponse(400, "Bad Request", "INVALID_TRANSACTION", "Missing required fields: fromAddress,toAddress,senderPublicKey,signature");
        }

        if (amount == 0)
        {
          return errorResponse(400, "Bad Request", "INVALID_TRANSACTION", "amount must be > 0");
        }

        if (payload.size() > 2048)
        {
          return errorResponse(413, "Payload Too Large", "INVALID_TRANSACTION", "payload too large");
        }

        const std::string expectedFrom = walletManager_.addressFromPublicKey(senderPublicKey);
        if (!isValidAddress(fromAddress) || fromAddress != expectedFrom)
        {
          return errorResponse(400, "Bad Request", "INVALID_ADDRESS", "Sender address is invalid or does not match sender public key");
        }

        if (!isValidAddress(toAddress))
        {
          return errorResponse(400, "Bad Request", "INVALID_ADDRESS", "Receiver address format is invalid");
        }

        if (!walletExists(toAddress))
        {
          return errorResponse(400, "Bad Request", "RECEIVER_NOT_FOUND", "Receiver wallet does not exist");
        }

        if (blockchain_.getBalanceForAddress(fromAddress) < amount)
        {
          return errorResponse(400, "Bad Request", "INSUFFICIENT_BALANCE", "Insufficient balance");
        }

        Transaction tx;
        tx.senderPublicKey = senderPublicKey;
        tx.receiverPublicKey = receiverPublicKey;
        tx.fromAddress = fromAddress;
        tx.toAddress = toAddress;
        tx.amount = amount;
        tx.nonce = nonce;
        tx.payload = payload;
        tx.timestamp = timestamp.empty() ? Transaction::currentTimestamp() : timestamp;
        tx.computeTxID();

        if (!txID.empty() && txID != tx.txID)
        {
          return errorResponse(400, "Bad Request", "INVALID_TRANSACTION", "txID mismatch");
        }

        tx.digitalSignature = signature;

        bool accepted = blockchain_.addTransaction(tx);

        if (accepted)
        {
          nlohmann::json resp;
          resp["message"] = "Transaction accepted";
          resp["txID"] = tx.txID;
          resp["transactionId"] = tx.txID;
          nlohmann::json txJson = withTxAliases(tx.toJson());
          txJson["status"] = "pending";
          txJson["blockHeight"] = nullptr;
          txJson["blockHash"] = "";
          resp["transaction"] = txJson;

          const auto mempool = blockchain_.getMempool();
          uint64_t pendingSent = 0;
          uint64_t pendingReceived = 0;
          uint64_t highestPendingNonce = blockchain_.getLastNonceForAddress(fromAddress);
          for (const auto &pendingTx : mempool)
          {
            if (pendingTx.fromAddress == fromAddress)
            {
              pendingSent += pendingTx.amount;
              highestPendingNonce = std::max(highestPendingNonce, pendingTx.nonce);
            }
            if (pendingTx.toAddress == fromAddress)
            {
              pendingReceived += pendingTx.amount;
            }
          }

          const uint64_t confirmedBalance = walletManager_.getBalance(fromAddress);
          int64_t pendingBalance = static_cast<int64_t>(confirmedBalance);
          pendingBalance += static_cast<int64_t>(pendingReceived);
          pendingBalance -= static_cast<int64_t>(pendingSent);

          resp["wallet"] = {
              {"address", fromAddress},
              {"confirmedBalance", confirmedBalance},
              {"pendingSent", pendingSent},
              {"pendingReceived", pendingReceived},
              {"pendingBalance", pendingBalance},
              {"nextNonce", highestPendingNonce + 1}};
          resp["mempoolSize"] = mempool.size();

          HttpResponse httpResp;
          httpResp.body = resp.dump(2);
          return httpResp;
        }
        else
        {
          return errorResponse(400, "Bad Request", "INVALID_TRANSACTION", "Transaction rejected (invalid signature, nonce replay, insufficient balance, expired, or double-spend)");
        }
      }
      catch (const nlohmann::json::exception &e)
      {
        return errorResponse(400, "Bad Request", "INVALID_TRANSACTION", std::string("JSON parse error: ") + e.what());
      }
      catch (const std::exception &e)
      {
        return errorResponse(500, "Internal Server Error", "TX_SEND_FAILED", e.what());
      }
    }

    HttpServer::HttpResponse HttpServer::handleMine(const std::string &body, const std::string &path)
    {
      try
      {
        static constexpr int64_t kMiningCooldownSeconds = 30;
        std::string minerAddress = queryParam(path, "minerAddress");

        if (!body.empty())
        {
          if (body.front() == '{')
          {
            auto j = nlohmann::json::parse(body);
            minerAddress = j.value("minerAddress", "");
          }
        }

        if (minerAddress.empty())
        {
          minerAddress = "GENESIS";
        }

        if (minerAddress != "GENESIS" && !isValidAddress(minerAddress))
        {
          return errorResponse(400, "Bad Request", "INVALID_ADDRESS", "Miner address format is invalid");
        }

        {
          std::lock_guard<std::mutex> lock(miningMutex_);
          const auto now = std::chrono::steady_clock::now();
          const auto it = lastMineAt_.find(minerAddress);
          if (it != lastMineAt_.end())
          {
            const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second).count();
            if (elapsed < kMiningCooldownSeconds)
            {
              const int64_t remaining = kMiningCooldownSeconds - elapsed;
              HttpResponse cooldownResp = errorResponse(429, "Too Many Requests", "MINING_COOLDOWN_ACTIVE", "Mining cooldown active");
              cooldownResp.body = nlohmann::json({{"error", "Mining cooldown active"},
                                                  {"errorCode", "MINING_COOLDOWN_ACTIVE"},
                                                  {"message", "Mining cooldown active"},
                                                  {"cooldownRemainingSeconds", remaining}})
                                      .dump();
              return cooldownResp;
            }
          }
        }

        const auto mempool = blockchain_.getMempool();
        const size_t pendingCountBeforeMine = mempool.size();

        Block newBlock = blockchain_.minePendingTransactions(minerAddress);

        {
          std::lock_guard<std::mutex> lock(miningMutex_);
          lastMineAt_[minerAddress] = std::chrono::steady_clock::now();
        }
        knownAddresses_.insert(minerAddress);

        nlohmann::json resp;
        resp["message"] = "Block mined successfully";
        resp["block"] = newBlock.toJson();
        resp["minerAddress"] = minerAddress;
        resp["reward"] = 50;
        resp["notification"] = "50 PCN mined successfully";
        resp["cooldownSeconds"] = kMiningCooldownSeconds;
        resp["mempoolTransactionsMined"] = pendingCountBeforeMine;
        resp["rewardOnlyBlock"] = (pendingCountBeforeMine == 0);
        resp["newBalance"] = walletManager_.getBalance(minerAddress);

        HttpResponse httpResp;
        httpResp.body = resp.dump(2);
        return httpResp;
      }
      catch (const std::exception &e)
      {
        return errorResponse(500, "Internal Server Error", "MINE_FAILED", e.what());
      }
    }

    HttpServer::HttpResponse HttpServer::handleTrade(const std::string & /*body*/)
    {
      return errorResponse(501, "Not Implemented", "TRADE_NOT_IMPLEMENTED", "Trade endpoint is not implemented yet");
    }

    HttpServer::HttpResponse HttpServer::handleGetChain()
    {
      try
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

    HttpServer::HttpResponse HttpServer::handleGetTransactions()
    {
      try
      {
        const auto chain = blockchain_.getChain();
        const auto mempool = blockchain_.getMempool();

        nlohmann::json transactions = nlohmann::json::array();
        nlohmann::json confirmedTransactions = nlohmann::json::array();
        nlohmann::json pendingTransactions = nlohmann::json::array();
        std::unordered_set<std::string> confirmedTxIds;

        for (const auto &block : chain)
        {
          for (const auto &tx : block.transactions)
          {
            nlohmann::json row = withTxAliases(tx.toJson());
            row["status"] = "confirmed";
            row["blockHeight"] = block.index;
            row["blockHash"] = block.hash;
            transactions.push_back(row);
            confirmedTransactions.push_back(row);
            confirmedTxIds.insert(tx.txID);
          }
        }

        for (const auto &tx : mempool)
        {
          if (confirmedTxIds.count(tx.txID) > 0)
          {
            continue;
          }

          nlohmann::json row = withTxAliases(tx.toJson());
          row["status"] = "pending";
          row["blockHeight"] = nullptr;
          row["blockHash"] = "";
          transactions.push_back(row);
          pendingTransactions.push_back(row);
        }

        HttpResponse resp;
        resp.body = nlohmann::json({{"count", transactions.size()},
                                    {"transactions", transactions},
                                    {"confirmedTransactions", confirmedTransactions},
                                    {"pendingTransactions", pendingTransactions}})
                        .dump(2);
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
          nlohmann::json row = withTxAliases(tx.toJson());
          row["status"] = "pending";
          j.push_back(row);
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

    HttpServer::HttpResponse HttpServer::handleGetWalletBalance(const std::string &address)
    {
      try
      {
        if (!isValidAddress(address))
        {
          return errorResponse(400, "Bad Request", "INVALID_ADDRESS", "Wallet address format is invalid");
        }

        knownAddresses_.insert(address);

        const uint64_t confirmedBalance = walletManager_.getBalance(address);
        const uint64_t confirmedNonce = walletManager_.getLastNonce(address);
        const auto mempool = blockchain_.getMempool();

        uint64_t pendingSent = 0;
        uint64_t pendingReceived = 0;
        uint64_t highestPendingNonce = confirmedNonce;

        for (const auto &tx : mempool)
        {
          if (tx.fromAddress == address)
          {
            pendingSent += tx.amount;
            highestPendingNonce = std::max(highestPendingNonce, tx.nonce);
          }
          if (tx.toAddress == address)
          {
            pendingReceived += tx.amount;
          }
        }

        int64_t pendingBalance = static_cast<int64_t>(confirmedBalance);
        pendingBalance += static_cast<int64_t>(pendingReceived);
        pendingBalance -= static_cast<int64_t>(pendingSent);

        HttpResponse resp;
        resp.body = nlohmann::json({{"address", address},
                                    {"balance", confirmedBalance},
                                    {"confirmedBalance", confirmedBalance},
                                    {"pendingBalance", pendingBalance},
                                    {"pendingSent", pendingSent},
                                    {"pendingReceived", pendingReceived},
                                    {"nextNonce", highestPendingNonce + 1}})
                        .dump(2);
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

    HttpServer::HttpResponse HttpServer::handleGetWalletTransactions(const std::string &address)
    {
      try
      {
        if (!isValidAddress(address))
        {
          return errorResponse(400, "Bad Request", "INVALID_ADDRESS", "Wallet address format is invalid");
        }

        auto txs = walletManager_.getTransactions(address);
        auto mempool = blockchain_.getMempool();
        auto chain = blockchain_.getChain();
        std::unordered_set<std::string> confirmedTxIds;
        std::map<std::string, std::pair<size_t, std::string>> txBlockMeta;

        for (const auto &block : chain)
        {
          for (const auto &tx : block.transactions)
          {
            txBlockMeta[tx.txID] = {block.index, block.hash};
          }
        }
        nlohmann::json list = nlohmann::json::array();
        nlohmann::json confirmedTransactions = nlohmann::json::array();
        nlohmann::json pendingTransactions = nlohmann::json::array();
        for (const auto &tx : txs)
        {
          nlohmann::json row = withTxAliases(tx.toJson());
          row["direction"] = tx.isCoinbase() ? "reward" : (tx.fromAddress == address ? "sent" : "received");
          row["status"] = "confirmed";
          const auto metaIt = txBlockMeta.find(tx.txID);
          if (metaIt != txBlockMeta.end())
          {
            row["blockHeight"] = metaIt->second.first;
            row["blockHash"] = metaIt->second.second;
          }
          else
          {
            row["blockHeight"] = nullptr;
            row["blockHash"] = "";
          }
          list.push_back(row);
          confirmedTransactions.push_back(row);
          confirmedTxIds.insert(tx.txID);
        }

        for (const auto &tx : mempool)
        {
          if (tx.fromAddress != address && tx.toAddress != address)
          {
            continue;
          }
          if (confirmedTxIds.count(tx.txID) > 0)
          {
            continue;
          }

          nlohmann::json row = withTxAliases(tx.toJson());
          row["direction"] = tx.isCoinbase() ? "reward" : (tx.fromAddress == address ? "sent" : "received");
          row["status"] = "pending";
          row["blockHeight"] = nullptr;
          row["blockHash"] = "";
          list.push_back(row);
          pendingTransactions.push_back(row);
        }

        HttpResponse resp;
        resp.body = nlohmann::json({{"address", address},
                                    {"count", list.size()},
                                    {"transactions", list},
                                    {"confirmedTransactions", confirmedTransactions},
                                    {"pendingTransactions", pendingTransactions}})
                        .dump(2);
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
      try
      {
        nlohmann::json j;
        j["status"] = "ok";
        j["chainLength"] = blockchain_.getChainLength();
        j["chainUpdateVersion"] = blockchain_.getChainUpdateVersion();
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

    HttpServer::HttpResponse HttpServer::handleGetPeerScores()
    {
      try
      {
        auto allPeers = node_.getPeerScorer().getAllPeers();
        nlohmann::json j = nlohmann::json::array();
        for (const auto &[key, info] : allPeers)
        {
          nlohmann::json p;
          p["endpoint"] = key;
          p["host"] = info.host;
          p["port"] = info.port;
          p["score"] = info.score;
          p["banned"] = info.banned;
          p["invalidBlockCount"] = info.invalidBlockCount;
          j.push_back(p);
        }

        nlohmann::json out;
        out["count"] = allPeers.size();
        out["peers"] = j;

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

    HttpServer::HttpResponse HttpServer::handleNetworkStats()
    {
      try
      {
        HttpResponse resp;
        resp.body = node_.getNetworkStats().dump(2);
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
