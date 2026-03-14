# PersonalBlockchain Networking Upgrade (Scalable P2P)

Date: 2026-03-14

## 1. Network Architecture Design

This upgrade moves the network from a mostly static small-cluster topology to a dynamic mesh-oriented topology with bounded fan-out.

### Core design principles

- Bootstrap-assisted discovery, not bootstrap dependence.
- Random partial peer sets per node to avoid fully connected graph explosion.
- Separate known-peer graph from active-connection set.
- Hard inbound/outbound limits with low-score eviction.
- Persistent node identity for stable network membership.
- Quarantine and jittered reconnect to avoid storm behavior.

### Components added

- NodeIdentity: persistent identity and advertised endpoint.
- BootstrapManager: bootstrap peer registration and topology serving.
- PeerManager: known peer set, random selection, dial cooldown, quarantine.
- ConnectionManager: inbound/outbound limits and low-score peer eviction selection.
- NetworkMetrics: runtime counters and uptime for observability.

## 2. Protocol Message Changes

Existing messages retained:

- NEW_TRANSACTION
- NEW_BLOCK
- REQUEST_CHAIN
- RESPONSE_CHAIN
- PING
- PONG
- REQUEST_PEERS
- RESPONSE_PEERS
- PEER_LIST (legacy compatibility)

New messages added:

- NODE_ANNOUNCE
  - Purpose: exchange identity and public endpoint.
  - Payload fields: nodeId, publicKey, publicIp, port, endpoint.

- REGISTER_NODE
  - Purpose: register with bootstrap node.
  - Payload fields: endpoint, nodeId.

- REQUEST_BOOTSTRAP
  - Purpose: ask bootstrap node for known topology.

- RESPONSE_BOOTSTRAP
  - Purpose: bootstrap response with peer list array.

## 3. C++ File Structure Changes

### New headers

- include/network/node_identity.h
- include/network/bootstrap_manager.h
- include/network/peer_manager.h
- include/network/connection_manager.h
- include/network/network_metrics.h

### New sources

- src/network/node_identity.cpp
- src/network/bootstrap_manager.cpp
- src/network/peer_manager.cpp
- src/network/connection_manager.cpp
- src/network/network_metrics.cpp

### Updated files

- include/network/message.h
- src/network/message.cpp
- include/network/node.h
- src/network/node.cpp
- include/network/http_server.h
- src/network/http_server.cpp
- src/main.cpp

### Added configuration template

- config.json

## 4. Integration Steps

1. Pull/build with updated sources.
2. Create or update config.json for each node.
3. Start at least one bootstrap node.
4. Start additional nodes with --bootstrap or config bootstrap_nodes.
5. Query GET /network/stats to verify growth and health.

### Startup behavior now

- Node loads/creates persistent identity in data_dir/node_identity.json.
- Node connects to configured bootstrap peers.
- Node registers via REGISTER_NODE and requests topology.
- Node merges discovered peers and connects to random subset.
- Node performs periodic peer rotation and heartbeat checks.

## 5. Security and Stability Risks

### Risks addressed

- Reconnect storms: mitigated with exponential backoff + jitter + cooldown.
- Gossip flooding: mitigated with message dedup cache window.
- Resource exhaustion: bounded inbound/outbound limits and queue caps.
- Peer poisoning: scoring + ban + quarantine.

### Remaining risks to harden next

- Sybil resistance (currently identity is persistent but permissionless).
- Signed gossip metadata (currently trust is mostly reputation-based).
- Strong NAT traversal (current mode is NAT-aware endpoint reporting, not hole punching).
- Replay-resistant nonce/timestamp on all control messages.

## 6. Testing Guide

## Test 1: Node joins network

1. Start bootstrap node A.
2. Start node B with bootstrap=A.
3. Verify B appears in A topology and A appears in B known peers.
4. Validate GET /network/stats on both nodes.

Expected:

- connectedPeers > 0
- knownPeers increases after bootstrap response

## Test 2: Node disconnect recovery

1. Stop one connected peer.
2. Observe heartbeat timeout and quarantine.
3. Restart peer after quarantine interval.

Expected:

- temporary drop in connectedPeers
- reconnect with jitter/backoff

## Test 3: Network partition recovery

1. Split 10-node cluster into two groups (network ACL/firewall).
2. Allow each side to evolve briefly.
3. Remove partition.

Expected:

- peers rediscovered by gossip/bootstrap
- mesh reconverges over rotation cycles

## Test 4: 10-node simulation

1. Start 1 bootstrap + 9 regular nodes.
2. Ensure each node has max_outbound_peers <= 8 and max_inbound_peers <= 32.
3. Mine and broadcast transactions from random nodes.

Expected:

- no full-mesh explosion
- propagation reaches all nodes within a few gossip cycles

## Test 5: Malicious peer rejection

1. Inject malformed or high-rate messages from one node.
2. Confirm penalties accumulate.
3. Confirm ban/quarantine activation and disconnect.

Expected:

- rejectedPeers and bannedPeers metrics increase
- malicious peer disconnected and quarantined

## 7. Deployment Guide

### Docker multi-node

- Run one or more bootstrap-designated nodes first.
- Provide stable container hostnames/IPs in bootstrap_nodes.
- Keep unique data_dir mounts per node to preserve identity.
- Expose P2P and HTTP ports per node.

### Public internet nodes

- Set public_ip to externally reachable address or DNS.
- Configure cloud firewall/security group for P2P/HTTP ports.
- Use static egress or DNS if behind dynamic WAN.

### Observability endpoint

- GET /network/stats provides:
  - connectedPeers
  - knownPeers
  - rejectedPeers
  - bannedPeers
  - networkUptimeSeconds
  - connection limit/current occupancy
  - node identity metadata

## 8. Example Commands for Running 10 Nodes

Assuming one machine, distinct ports, and local bootstrap on 5000.

### Node 0 (bootstrap)

./blockchain_node --config config.json --port 5000 --http-port 8000 --data-dir ./data/node0 --public-ip 127.0.0.1 --bootstrap 127.0.0.1:5000

### Nodes 1..9

./blockchain_node --config config.json --port 5001 --http-port 8001 --data-dir ./data/node1 --bootstrap 127.0.0.1:5000
./blockchain_node --config config.json --port 5002 --http-port 8002 --data-dir ./data/node2 --bootstrap 127.0.0.1:5000
./blockchain_node --config config.json --port 5003 --http-port 8003 --data-dir ./data/node3 --bootstrap 127.0.0.1:5000
./blockchain_node --config config.json --port 5004 --http-port 8004 --data-dir ./data/node4 --bootstrap 127.0.0.1:5000
./blockchain_node --config config.json --port 5005 --http-port 8005 --data-dir ./data/node5 --bootstrap 127.0.0.1:5000
./blockchain_node --config config.json --port 5006 --http-port 8006 --data-dir ./data/node6 --bootstrap 127.0.0.1:5000
./blockchain_node --config config.json --port 5007 --http-port 8007 --data-dir ./data/node7 --bootstrap 127.0.0.1:5000
./blockchain_node --config config.json --port 5008 --http-port 8008 --data-dir ./data/node8 --bootstrap 127.0.0.1:5000
./blockchain_node --config config.json --port 5009 --http-port 8009 --data-dir ./data/node9 --bootstrap 127.0.0.1:5000

### Validate each node

- curl http://localhost:8000/network/stats
- curl http://localhost:8005/network/stats
- curl http://localhost:8009/network/stats

## 9. VM and Environment Changes Required

For your VM (Windows/Linux), ensure:

1. OpenSSL dev libraries are installed and discoverable by CMake.
2. Boost.System is installed.
3. C++17 toolchain is available (MSVC, GCC, or Clang).
4. Firewall allows chosen P2P and HTTP port ranges.
5. If internet deployment is needed, set public_ip per node and open inbound rules.

### Windows-specific notes

- Install OpenSSL and set OPENSSL_ROOT_DIR environment variable if CMake cannot locate it.
- Ensure Visual Studio Build Tools with Desktop C++ workload are installed.
- Keep one data directory per node to preserve identity and prevent key collisions.

### Linux-specific notes

- Install libssl-dev, libboost-system-dev, cmake, and a C++ compiler.
- Use systemd service files for persistent multi-node boot if needed.

## 10. Config Schema Summary

Supported config.json keys:

- p2p_port
- http_port
- difficulty
- data_dir
- consensus
- name
- public_ip
- bootstrap_nodes (array of host:port)
- seed_peers (array of host:port)
- peer_limits:
  - max_outbound_peers
  - max_inbound_peers
- network_timeouts:
  - heartbeat_interval_sec
  - heartbeat_timeout_sec
  - gossip_interval_sec
  - peer_rotation_sec
  - quarantine_sec
  - dedup_window_sec
- retry_policy:
  - max_retries
  - base_delay_ms
  - max_delay_ms
  - jitter_ms
  - reconnect_cooldown_ms

## 11. Current Build Status in This Workspace

Code integration is complete, but full local build verification is currently blocked by missing OpenSSL detection in CMake on this VM.

Observed issue:

- CMake could NOT find OpenSSL (missing OPENSSL_CRYPTO_LIBRARY and OPENSSL_INCLUDE_DIR).

Action needed on VM:

- Install OpenSSL development package and set OPENSSL_ROOT_DIR (Windows) or install libssl-dev (Linux).
