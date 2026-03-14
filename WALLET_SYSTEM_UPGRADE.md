# Wallet System Upgrade - Full Stack Change Log

## Scope

This upgrade transforms the project from a generic explorer into a wallet identity platform with challenge-based authentication, dynamic account balances, signed transfers, mining rewards, and a personalized React wallet dashboard.

## Architecture Design

- Identity: wallet address is derived as `SHA256(publicKey)`.
- Authentication: challenge-response signature verification (`/wallet/loginChallenge` -> `/wallet/verifyLogin`).
- Ledger model: account-style state indexed by wallet address.
- Balance source of truth: confirmed chain state (`StateManager` rebuilt/applied from blocks).
- Replay protection: sender `nonce` is validated against last confirmed nonce.
- Mining reward: coinbase transaction credits miner wallet address by block reward.
- Frontend key model: private key is kept in browser local storage for demo; never sent to backend.

## Backend Modifications

### New module

- `include/wallet/wallet_manager.h`
- `src/wallet/wallet_manager.cpp`

Responsibilities:

- Create/import wallets.
- Generate and validate login challenges.
- Verify login signatures.
- Expose wallet balance/nonce/history from blockchain state.

### Transaction schema upgrades

Updated `Transaction` with wallet-account fields:

- `fromAddress`
- `toAddress`
- `amount`
- `nonce`

`txID` now hashes canonical core fields including amount/nonce/addresses.

### Blockchain validation upgrades

`Blockchain::addTransaction` now enforces:

- Signature validity.
- `fromAddress == SHA256(senderPublicKey)`.
- `amount > 0`.
- Confirmed balance check before mempool admission.
- Nonce replay protection (`nonce >= lastNonce + 1`).

### State updates

`StateManager` now tracks:

- `address -> balance`
- `address -> lastNonce`
- `address -> transactions[]`

Balances and histories update when blocks are applied/rebuilt.

### Mining rewards

Coinbase now credits miner wallet address:

- Block reward set to `50`.
- `POST /mine?minerAddress=...` supported.

## API Changes

### New/Updated wallet endpoints

- `POST /wallet/create`
  - Returns `publicKey`, `privateKey`, `address`.
- `POST /wallet/import`
  - Body: `{ privateKey }`
  - Returns imported wallet identity.
- `POST /wallet/loginChallenge`
  - Body: `{ address, publicKey }`
  - Returns challenge and expiry.
- `POST /wallet/verifyLogin`
  - Body: `{ address, publicKey, challenge, signature }`
  - Returns auth token (demo token model).
- `GET /wallet/balance/{address}`
  - Returns confirmed `balance` and `nextNonce`.
- `GET /wallet/transactions/{address}`
  - Returns sent/received/reward transaction history.

### Updated transfer endpoint

- `POST /transaction/send`
  - Expects signed payload from frontend:
  - `fromAddress, toAddress, senderPublicKey, amount, nonce, payload, timestamp, txID, signature`.

### Mining endpoint

- `POST /mine?minerAddress=ADDRESS`
- Also accepts JSON body `{ minerAddress }`.

## Frontend Structure

### Pages added

- `src/pages/Login.jsx`
- `src/pages/WalletDashboard.jsx`
- `src/pages/Send.jsx`

### Components added

- `src/components/WalletCard.jsx`
- `src/components/BalanceCard.jsx`
- `src/components/TransactionTable.jsx`
- `src/components/SendForm.jsx`
- `src/components/ReceivePanel.jsx`

### Frontend service and crypto updates

- `src/services/api.js` now includes wallet auth/balance/history/send methods.
- `src/services/walletCrypto.js` added for:
  - secp256k1 key derivation/signing in browser.
  - txID generation parity with backend.
  - address derivation `SHA256(publicKey)`.

### Routing

- App migrated to `react-router-dom` with guarded routes:
  - `/login`
  - `/`
  - `/send`

## Security Considerations Implemented

- Challenge-response login (no password flow).
- Private key never sent to backend.
- Signature verification on transfer submission.
- Server-side balance check.
- Server-side nonce replay protection.
- Request body size limits + payload max length checks.
- Input validation on wallet import and send endpoints.

## Testing Plan and Added Test

### Added test target

- `tests/wallet_system_tests.cpp`

Covers:

- Create wallet and derive address.
- Mine reward and verify balance credit.
- Signed transfer and balance update after mining.
- Invalid signature rejection.
- Replay nonce rejection.

### Build test target

- `wallet_system_tests` (CMake option `BUILD_TESTS` ON by default).

## VM/Environment Changes Required

### 1) Build dependencies

Install these on the VM:

- CMake 3.16+
- C++ compiler with C++17 support
- OpenSSL dev libraries (headers + libs)
- Boost (system)
- Node.js 18+ and npm

### 2) OpenSSL on Windows VMs

If CMake cannot find OpenSSL, set:

- `OPENSSL_ROOT_DIR` to your OpenSSL install path

Example PowerShell:

```powershell
$env:OPENSSL_ROOT_DIR = "C:/Program Files/OpenSSL-Win64"
cmake -S . -B build
cmake --build build --config Release
```

### 3) Frontend dependencies

In `blockchain-explorer`:

```bash
npm install
npm run dev
```

### 4) API base URL

Set `VITE_API_BASE_URL` if backend is not on default:

```bash
VITE_API_BASE_URL=http://localhost:8001
```

## Integration Steps

1. Build backend node.
2. Start backend node with HTTP port.
3. Start frontend Vite app.
4. Open `/login`, create/import wallet, sign login challenge.
5. Mine to your wallet address.
6. Send signed transfer from `/send`.
7. Confirm balance/history update on dashboard.

## Notes

- Demo storage model uses browser local storage for private key; use encrypted storage or extension wallet model for production.
- Existing chain entries without amount/address fields are still parsed with compatibility defaults.
