#include "network/http_server.h"
#include "utils/json.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
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

    HttpServer::HttpResponse HttpServer::handleCreateWallet()
    {
      try
      {
        Wallet wallet = walletManager_.createWallet();
        std::string address = walletManager_.addressFromPublicKey(wallet.getPublicKey());

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
        HttpResponse resp;
        resp.statusCode = 500;
        resp.statusText = "Internal Server Error";
        resp.body = nlohmann::json({{"error", e.what()}}).dump();
        return resp;
      }
    }

    HttpServer::HttpResponse HttpServer::handleImportWallet(const std::string &body)
    {
      try
      {
        auto j = nlohmann::json::parse(body);
        std::string privateKey = j.value("privateKey", "");
        if (!isLikelyHex(privateKey))
        {
          HttpResponse resp;
          resp.statusCode = 400;
          resp.statusText = "Bad Request";
          resp.body = R"({"error":"Invalid private key"})";
          return resp;
        }

        Wallet wallet = walletManager_.importWallet(privateKey);
        const std::string address = walletManager_.addressFromPublicKey(wallet.getPublicKey());

        HttpResponse resp;
        resp.body = nlohmann::json({{"publicKey", wallet.getPublicKey()},
                                    {"address", address}})
                        .dump(2);
        return resp;
      }
      catch (const std::exception &e)
      {
        HttpResponse resp;
        resp.statusCode = 400;
        resp.statusText = "Bad Request";
        resp.body = nlohmann::json({{"error", e.what()}}).dump();
        return resp;
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
          HttpResponse resp;
          resp.statusCode = 400;
          resp.statusText = "Bad Request";
          resp.body = R"({"error":"address and publicKey are required"})";
          return resp;
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
        HttpResponse resp;
        resp.statusCode = 400;
        resp.statusText = "Bad Request";
        resp.body = nlohmann::json({{"error", e.what()}}).dump();
        return resp;
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
          HttpResponse resp;
          resp.statusCode = 400;
          resp.statusText = "Bad Request";
          resp.body = R"({"error":"address, publicKey, challenge, signature are required"})";
          return resp;
        }

        auto token = walletManager_.verifyLogin(address, publicKey, challenge, signature);
        if (!token.has_value())
        {
          HttpResponse resp;
          resp.statusCode = 401;
          resp.statusText = "Unauthorized";
          resp.body = R"({"error":"Invalid login signature or expired challenge"})";
          return resp;
        }

        HttpResponse resp;
        resp.body = nlohmann::json({{"address", address},
                                    {"token", token.value()}})
                        .dump(2);
        return resp;
      }
      catch (const std::exception &e)
      {
        HttpResponse resp;
        resp.statusCode = 400;
        resp.statusText = "Bad Request";
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

        const std::string fromAddress = j.value("fromAddress", "");
        const std::string toAddress = j.value("toAddress", "");
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
          HttpResponse resp;
          resp.statusCode = 400;
          resp.statusText = "Bad Request";
          resp.body = R"({"error":"Missing required fields: fromAddress,toAddress,senderPublicKey,signature"})";
          return resp;
        }

        if (amount == 0)
        {
          HttpResponse resp;
          resp.statusCode = 400;
          resp.statusText = "Bad Request";
          resp.body = R"({"error":"amount must be > 0"})";
          return resp;
        }

        if (payload.size() > 2048)
        {
          HttpResponse resp;
          resp.statusCode = 413;
          resp.statusText = "Payload Too Large";
          resp.body = R"({"error":"payload too large"})";
          return resp;
        }

        if (blockchain_.getBalanceForAddress(fromAddress) < amount)
        {
          HttpResponse resp;
          resp.statusCode = 400;
          resp.statusText = "Bad Request";
          resp.body = R"({"error":"Insufficient balance"})";
          return resp;
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
          HttpResponse resp;
          resp.statusCode = 400;
          resp.statusText = "Bad Request";
          resp.body = R"({"error":"txID mismatch"})";
          return resp;
        }

        tx.digitalSignature = signature;

        bool accepted = blockchain_.addTransaction(tx);

        if (accepted)
        {
          nlohmann::json resp;
          resp["message"] = "Transaction accepted";
          resp["txID"] = tx.txID;
          nlohmann::json txJson = tx.toJson();
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
          HttpResponse resp;
          resp.statusCode = 400;
          resp.statusText = "Bad Request";
          resp.body = "{\"error\":\"Transaction rejected (invalid signature, nonce replay, insufficient balance, expired, or double-spend)\"}";
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

    HttpServer::HttpResponse HttpServer::handleMine(const std::string &body, const std::string &path)
    {
      try
      {
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

        Block newBlock = blockchain_.minePendingTransactions(minerAddress);

        nlohmann::json resp;
        resp["message"] = "Block mined successfully";
        resp["block"] = newBlock.toJson();
        resp["minerAddress"] = minerAddress;
        resp["newBalance"] = walletManager_.getBalance(minerAddress);

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
        std::unordered_set<std::string> confirmedTxIds;

        for (const auto &block : chain)
        {
          for (const auto &tx : block.transactions)
          {
            nlohmann::json row = tx.toJson();
            row["status"] = "confirmed";
            row["blockHeight"] = block.index;
            row["blockHash"] = block.hash;
            transactions.push_back(row);
            confirmedTxIds.insert(tx.txID);
          }
        }

        for (const auto &tx : mempool)
        {
          if (confirmedTxIds.count(tx.txID) > 0)
          {
            continue;
          }

          nlohmann::json row = tx.toJson();
          row["status"] = "pending";
          row["blockHeight"] = nullptr;
          row["blockHash"] = "";
          transactions.push_back(row);
        }

        HttpResponse resp;
        resp.body = nlohmann::json({{"count", transactions.size()},
                                    {"transactions", transactions}})
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

    HttpServer::HttpResponse HttpServer::handleGetWalletBalance(const std::string &address)
    {
      try
      {
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
        for (const auto &tx : txs)
        {
          nlohmann::json row = tx.toJson();
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

          nlohmann::json row = tx.toJson();
          row["direction"] = tx.isCoinbase() ? "reward" : (tx.fromAddress == address ? "sent" : "received");
          row["status"] = "pending";
          row["blockHeight"] = nullptr;
          row["blockHash"] = "";
          list.push_back(row);
        }

        HttpResponse resp;
        resp.body = nlohmann::json({{"address", address},
                                    {"count", list.size()},
                                    {"transactions", list}})
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
