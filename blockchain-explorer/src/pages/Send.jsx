import { useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';

import SendForm from '../components/SendForm';
import TransactionTable from '../components/TransactionTable';
import { blockchainApi } from '../services/api';
import { computeTransactionId, currentUtcNoZ, signMessage } from '../services/walletCrypto';
import { useWalletStore } from '../store/useWalletStore';

export default function Send() {
  const navigate = useNavigate();
  const queryClient = useQueryClient();
  const { wallet } = useWalletStore();
  const address = wallet?.address;

  const [sending, setSending] = useState(false);
  const [error, setError] = useState('');

  const { data: balanceData = { nextNonce: 1 } } = useQuery({
    queryKey: ['balance', address],
    queryFn: () => blockchainApi.getWalletBalance(address),
    enabled: !!address,
  });

  const { data: txData = { transactions: [] } } = useQuery({
    queryKey: ['transactions', address],
    queryFn: () => blockchainApi.getWalletTransactions(address),
    enabled: !!address,
  });

  const history = (txData.transactions || []).slice().reverse();
  const nextNonce = balanceData.nextNonce || 1;

  const sendMutation = useMutation({
    mutationFn: async ({ toAddress, amount, payload, nonce }) => {
      const tx = {
        fromAddress: wallet.address,
        toAddress,
        senderPublicKey: wallet.publicKey,
        receiverPublicKey: '',
        amount,
        nonce,
        payload,
        timestamp: currentUtcNoZ(),
      };

      tx.txID = await computeTransactionId(tx);
      tx.signature = await signMessage(wallet.privateKey, tx.txID);

      return blockchainApi.sendTransaction(tx);
    },
    onSuccess: () => {
      queryClient.invalidateQueries(['balance', address]);
      queryClient.invalidateQueries(['transactions', address]);
    }
  });

  const onSubmit = async (formData) => {
    try {
      setSending(true);
      setError('');
      await sendMutation.mutateAsync(formData);
    } catch (e) {
      setError(e?.response?.data?.error || e.message || 'Transaction failed');
    } finally {
      setSending(false);
    }
  };

  if (!wallet) return null;

  return (
    <div className="min-h-screen bg-[radial-gradient(circle_at_top_left,#122949_0%,#0a0f18_45%,#05080e_100%)] text-white p-6 lg:p-8">
      <div className="max-w-6xl mx-auto space-y-6">
        <div className="flex justify-between items-center">
          <div>
            <h1 className="text-3xl font-black tracking-tight">Send Transaction</h1>
            <p className="text-zinc-400 text-sm">Sign locally. Private key never leaves browser memory.</p>
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
