Technical Progress Report — PersonalBlockchain (CryptixChain)
Date: March 2, 2026 | Status: Functional prototype, pre-deployment

1. Architecture
The project implements a multi-node blockchain network written in C++17, built as a single binary node executable (blockchain_node). The architecture is strictly layered:

Each layer has clean header/implementation separation under include and src. The build system is CMake 3.16+, and the binary exposes two ports — P2P (default 5000) and HTTP REST API (default 8000) — configured entirely via CLI flags.

2. Technologies Used
Component	Technology
Language	C++17
Build	CMake 3.16+
Cryptography	OpenSSL EVP API (SHA-256), secp256k1 curve (ECDSA)
Networking	Boost.Asio 1.70+ — async TCP, no third-party HTTP library
Serialization	nlohmann/json (header-only, bundled)
Frontend	React + Vite, Recharts, react-force-graph-2d, Framer Motion, Axios
Containerization	Docker (multi-stage Ubuntu 22.04), Docker Compose (3-node bridge network)
The HTTP server is implemented from scratch over raw Boost.Asio TCP — no Boost.Beast or similar — with manual HTTP/1.1 request parsing, CORS headers, and content-length body handling.

3. Security Mechanisms — Implemented & Enforced
All five of the following checks are enforced sequentially inside Blockchain::addTransaction() before any transaction reaches the mempool:

Coinbase injection prevention — External transactions with senderPublicKey == "COINBASE" are hard-rejected. Coinbase transactions are only created internally during minePendingTransactions().
ECDSA signature verification — Every transaction is signed with the sender's secp256k1 private key. tx.verify() uses OpenSSL to verify the hex-encoded signature against the sender's compressed public key.
Replay attack prevention — tx.isExpired(300) enforces a 5-minute timestamp window. Transactions older than 5 minutes are rejected outright.
Double-spend detection — StateManager maintains a global std::set<std::string> spentTxIDs_. Every confirmed txID is inserted on block confirmation. Any mempool submission that matches a spent txID is rejected.
Mempool deduplication — A second lookup prevents a transaction from appearing in the mempool twice.
At the P2P layer:

Per-peer rate limiting — peer->isRateLimited(50) caps message volume per peer.
Message deduplication — seenTxIDs_ and seenBlockHashes_ sets prevent gossip re-broadcast storms. Both sets are bounded (10,000 and 1,000 entries respectively) to prevent unbounded memory growth.
4. Networking Layer
Transport: TCP with length-prefix framing (Boost.Asio async I/O)
Message protocol: 7 typed messages — NEW_TRANSACTION, NEW_BLOCK, REQUEST_CHAIN, RESPONSE_CHAIN, PING, PONG, PEER_LIST
Connection model: Each peer runs as a shared Peer object with async read/write loops
Reconnection: Exponential backoff on failed connections (2 → 4 → 8 → 16 → 30s, max 5 retries)
Peer seeding: Static, via --peers host:port,host:port CLI flag. PEER_LIST message handler exists as a stub but peer discovery (gossip protocol) is not yet implemented
The node broadcasts new transactions and blocks via Blockchain event callbacks wired up in the Node constructor — i.e., the network layer reacts to blockchain events rather than polling.

5. Consensus
Algorithm: Proof of Work (PoW)
Hash function: SHA256(index + timestamp + merkleRoot + previousHash + nonce + difficulty)
Target: nonce is incremented until the hash has difficulty leading zero characters
Difficulty: Configurable at startup via --difficulty flag (default: 2). Static during runtime — no dynamic retargeting is implemented.
Longest-valid-chain rule: replaceChain() accepts an incoming chain only if newChain.size() > chain_.size() AND isChainValid(newChain) passes
Chain validation checks: Genesis block structure, sequential previousHash linking, per-block PoW target, Merkle root integrity, all transaction ECDSA signatures
6. Transaction Validation Logic
Five-stage pipeline (detailed in Section 3). Additional note: the payload field is a freeform string, which means any data can be embedded. There is no fee mechanism — all valid transactions are included in the next mined block regardless of priority.

7. Node Synchronization
Chain sync occurs in three situations:

On node start — requestChainFromPeers() fires 2 seconds after startup, broadcasting REQUEST_CHAIN to all connected peers
On peer connect — The connecting side sends REQUEST_CHAIN immediately after the TCP handshake
On NEW_BLOCK receipt — If the incoming block index equals chain.size() and previousHash matches, the block is appended and replaceChain() is called. If the incoming block index is ahead of the local tip, the node requests the full chain from that peer
State is fully rebuilt (stateManager_.rebuildState(chain_)) whenever replaceChain() succeeds. The mempool is purged of any transactions that appear in the new chain using getAllSpentTxIDs().

8. Current Limitations
Limitation	Impact
No persistent storage	Chain is lost on process restart
Static difficulty	No adjustment for network hashrate changes
Static peer list	No automatic peer discovery; nodes must be seeded manually
In-memory wallets only	Private keys are not persisted — wallet is lost on restart
No transaction fees	No priority mechanism for mempool inclusion
No mempool size cap	Unbounded memory growth under spam conditions
PEER_LIST handler is a stub	Gossip-based peer discovery is scaffolded but not functional
Frontend hardcoded to localhost:8001	Multi-node or remote deployment requires manual URL changes
No public deployment	No cloud hosting, no domain — accessible only via Docker Compose locally
9. Future Roadmap (Prioritised)
Persistent chain storage — Serialize chain to disk (LevelDB or flat JSON file) so state survives restarts
Dynamic difficulty retargeting — Adjust difficulty every N blocks based on actual vs target block time
Gossip-based peer discovery — Activate the PEER_LIST handler; implement fan-out discovery
Proof-of-Stake consensus — Replace PoW in Block::mineBlock() and replaceChain() with a pluggable consensus interface (the codebase already documents the interface design in the GUIDE)
Token economy / smart contracts — Add typed transaction payloads, asset balances, and scripting
Blockchain explorer enhancements — Multi-node view, transaction search, wallet balance queries
Simplified Public-Facing Summary
CryptixChain is a private blockchain network built entirely from scratch in C++. It simulates the core mechanics of how public blockchains like Bitcoin work — including cryptographic signatures, Proof-of-Work mining, peer-to-peer networking, and tamper-proof chain validation — running as a 3-node network inside Docker containers.

Every transaction is signed using the same elliptic curve cryptography (secp256k1) that Bitcoin uses. Every block is mined by solving a computational puzzle. Every node independently verifies the full chain before accepting updates from peers. And every component — from the hash function to the HTTP API — is written in C++, without relying on any existing blockchain framework.

It includes a live web explorer where you can watch blocks being mined, see the network topology, and submit transactions in real time.

Built solo, in under a week, as a prototype for a real product.

LinkedIn Post
I built a blockchain network from scratch in C++. Here's what that actually means technically.

Not a tutorial clone. Not an Ethereum fork. A full implementation of core blockchain mechanics — written from scratch in C++17.

What's working today:

ECDSA digital signatures (secp256k1 — same curve as Bitcoin) — every transaction is cryptographically signed and verified before it touches the mempool
SHA-256 Proof of Work — blocks are mined by finding a nonce such that SHA256(index + timestamp + merkleRoot + previousHash + nonce) meets the difficulty target
Merkle tree — transaction integrity is verified per-block using a bottom-up pairwise hash tree
5-layer transaction validation pipeline — coinbase injection prevention → signature verification → replay attack prevention (5-min expiry window) → double-spend detection → mempool deduplication
Longest-valid-chain consensus — nodes resolve forks by accepting only chains that are longer and cryptographically valid
Async P2P networking built on Boost.Asio — TCP with length-prefix framing, exponential backoff reconnection, per-peer rate limiting, and message dedup to prevent gossip storms
REST HTTP API — built over raw TCP (no frameworks), with endpoints for wallet creation, transaction submission, mining, and chain inspection
3-node Docker Compose network — multi-stage build, all nodes auto-connect and sync chains on startup
React live explorer — polls the chain every 4 seconds, renders block history, network graph, and a mining panel
The honest part: I used GitHub Copilot extensively during the build. What took less than a week would've taken much longer without it. But understanding why each piece exists — why you need a Merkle root, why replay prevention requires a timestamp window, why replaceChain() must validate before accepting — that understanding is mine, and it's the part that matters.

What's next:

Persistent chain storage (LevelDB)
Dynamic difficulty retargeting
Gossip-based peer discovery
Pluggable consensus (PoS interface)
Public deployment
If you're learning how blockchains actually work at the protocol level — skip the online courses. Build one.

GitHub: [link]

#Blockchain #CPlusPlus #SystemsEngineering #Cryptography #P2P #OpenSource #BuildInPublic

Note on deployment (your stated blocker): To go public, your two immediate options are: (1) deploy the Docker Compose stack to a cloud VM (AWS EC2, DigitalOcean Droplet, Hetzner) with open inbound ports 5001-5003 and 8001-8003, then point the frontend API_BASE_URL at the VM's public IP; or (2) use ngrok for fast temporary public access. The chain has no persistence yet, so add disk storage first or the chain resets every time the process restarts.