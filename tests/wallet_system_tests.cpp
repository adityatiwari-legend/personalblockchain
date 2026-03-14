#include "core/blockchain.h"
#include "consensus/pow_engine.h"
#include "crypto/sha256.h"
#include "transaction/transaction.h"
#include "wallet/wallet.h"

#include <cassert>
#include <iostream>

using namespace blockchain;

static Transaction makeSignedTx(const Wallet &sender,
                                const std::string &toAddress,
                                uint64_t amount,
                                uint64_t nonce)
{
    Transaction tx;
    tx.senderPublicKey = sender.getPublicKey();
    tx.fromAddress = crypto::sha256(sender.getPublicKey());
    tx.toAddress = toAddress;
    tx.amount = amount;
    tx.nonce = nonce;
    tx.payload = "test-transfer";
    tx.timestamp = Transaction::currentTimestamp();
    tx.computeTxID();
    tx.sign(sender.getPrivateKey());
    return tx;
}

int main()
{
    consensus::PoWEngine pow;
    Blockchain chain(2, &pow, nullptr);

    Wallet miner;
    Wallet receiver;
    const std::string minerAddress = crypto::sha256(miner.getPublicKey());
    const std::string receiverAddress = crypto::sha256(receiver.getPublicKey());

    // Test 1: mine reward updates balance
    chain.minePendingTransactions(minerAddress);
    assert(chain.getBalanceForAddress(minerAddress) >= 50);

    // Test 2: signed transfer accepted and balance changes after mining
    Transaction tx = makeSignedTx(miner, receiverAddress, 20, chain.getLastNonceForAddress(minerAddress) + 1);
    assert(chain.addTransaction(tx));
    chain.minePendingTransactions(minerAddress);
    assert(chain.getBalanceForAddress(receiverAddress) >= 20);

    // Test 3: invalid signature rejected
    Transaction badSig = tx;
    badSig.nonce = chain.getLastNonceForAddress(minerAddress) + 1;
    badSig.computeTxID();
    badSig.digitalSignature = "deadbeef";
    assert(!chain.addTransaction(badSig));

    // Test 4: replay nonce rejected
    Transaction replay = makeSignedTx(miner, receiverAddress, 1, 1);
    assert(!chain.addTransaction(replay));

    // Test 5: receiver balance updates when a new network block is accepted
    Blockchain peerChain(2, &pow, nullptr);
    assert(peerChain.replaceChain(chain.getChain()));

    Transaction networkTx = makeSignedTx(miner, receiverAddress, 7,
                                         chain.getLastNonceForAddress(minerAddress) + 1);
    assert(chain.addTransaction(networkTx));
    Block propagatedBlock = chain.minePendingTransactions(minerAddress);

    const uint64_t beforeNetworkAccept = peerChain.getBalanceForAddress(receiverAddress);
    assert(peerChain.acceptBlock(propagatedBlock));
    const uint64_t afterNetworkAccept = peerChain.getBalanceForAddress(receiverAddress);
    assert(afterNetworkAccept >= beforeNetworkAccept + 7);

    std::cout << "wallet_system_tests passed" << std::endl;
    return 0;
}
