import { useEffect, useState } from 'react';
import { useNavigate } from 'react-router-dom';

import SendForm from '../components/SendForm';
import TransactionTable from '../components/TransactionTable';
import { blockchainApi } from '../services/api';
import { computeTransactionId, currentUtcNoZ, signMessage } from '../services/walletCrypto';

export default function Send() {
  const navigate = useNavigate();
  const session = JSON.parse(localStorage.getItem('walletSession') || '{}');
  const [nextNonce, setNextNonce] = useState(1);
  const [history, setHistory] = useState([]);
  const [sending, setSending] = useState(false);
  const [error, setError] = useState('');

  const refresh = async () => {
    if (!session.address) return;
    const [balance, txs] = await Promise.all([
      blockchainApi.getWalletBalance(session.address),
      blockchainApi.getWalletTransactions(session.address),
    ]);
    setNextNonce(balance.nextNonce || 1);
    setHistory((txs.transactions || []).slice().reverse());
  };

  useEffect(() => {
    refresh();
  }, []);

  const onSubmit = async ({ toAddress, amount, payload, nonce }) => {
    try {
      setSending(true);
      setError('');

      const tx = {
        fromAddress: session.address,
        toAddress,
        senderPublicKey: session.publicKey,
        receiverPublicKey: '',
        amount,
        nonce,
        payload,
        timestamp: currentUtcNoZ(),
      };

      tx.txID = await computeTransactionId(tx);
      tx.signature = await signMessage(session.privateKey, tx.txID);

      await blockchainApi.sendTransaction(tx);
      await refresh();
    } catch (e) {
      setError(e?.response?.data?.error || e.message || 'Transaction failed');
    } finally {
      setSending(false);
    }
  };

  return (
    <div className="min-h-screen bg-[radial-gradient(circle_at_top_left,#122949_0%,#0a0f18_45%,#05080e_100%)] text-white p-6 lg:p-8">
      <div className="max-w-6xl mx-auto space-y-6">
        <div className="flex justify-between items-center">
          <div>
            <h1 className="text-3xl font-black tracking-tight">Send Transaction</h1>
            <p className="text-zinc-400 text-sm">Sign locally. Private key never leaves browser storage.</p>
          </div>
          <button className="btn-secondary" onClick={() => navigate('/')}>Back</button>
        </div>

        {error && <div className="p-3 rounded-xl bg-rose-500/10 border border-rose-500/30 text-rose-300 text-sm">{error}</div>}

        <div className="grid grid-cols-1 xl:grid-cols-2 gap-6">
          <SendForm onSubmit={onSubmit} sending={sending} nextNonce={nextNonce} />
          <div className="dashboard-card">
            <p className="text-xs uppercase tracking-[0.2em] text-cyan-300/80 mb-3">Security Checks</p>
            <ul className="text-sm text-zinc-300 space-y-2">
              <li>Signature verified by backend</li>
              <li>Balance validated before mempool admission</li>
              <li>Nonce-based replay protection</li>
              <li>Payload and body size limits enforced</li>
            </ul>
          </div>
        </div>

        <TransactionTable transactions={history} />
      </div>
    </div>
  );
}
