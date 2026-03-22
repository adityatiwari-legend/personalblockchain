import { useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';

import SendForm from '../components/SendForm';
import TransactionTable from '../components/TransactionTable';
import { blockchainApi } from '../services/api';
import { computeTransactionId, currentUtcNoZ, signMessage } from '../services/walletCrypto';
import { isValidAddress } from '../services/validation';
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
    onSuccess: (result) => {
      const tx = result?.transaction;
      const walletSnapshot = result?.wallet;

      if (tx) {
        queryClient.setQueryData(['mempool'], (old) => {
          const list = Array.isArray(old) ? old : [];
          if (list.some((item) => (item.txID || item.id) === tx.txID)) {
            return list;
          }
          return [tx, ...list];
        });

        queryClient.setQueryData(['transactions', address], (old) => {
          const current = old?.transactions || [];
          const exists = current.some((item) => (item.txID || item.id) === tx.txID);
          if (exists) return old;
          return {
            ...(old || {}),
            count: (old?.count || current.length) + 1,
            transactions: [tx, ...current],
          };
        });

        queryClient.setQueryData(['wallet-transactions', address], (old) => {
          const current = old?.transactions || old || [];
          const exists = current.some((item) => (item.txID || item.id) === tx.txID);
          if (exists) return old;
          if (Array.isArray(old)) return [tx, ...old];
          return {
            ...(old || {}),
            count: (old?.count || current.length) + 1,
            transactions: [tx, ...current],
          };
        });

        queryClient.setQueryData(['all-transactions'], (old) => {
          const list = Array.isArray(old) ? old : (old?.transactions || []);
          const exists = list.some((item) => (item.txID || item.id) === tx.txID);
          if (exists) return old;
          if (Array.isArray(old)) return [tx, ...old];
          return {
            ...(old || {}),
            count: (old?.count || list.length) + 1,
            transactions: [tx, ...list],
          };
        });
      }

      if (walletSnapshot) {
        queryClient.setQueryData(['balance', address], (old) => ({
          ...(old || {}),
          balance: walletSnapshot.confirmedBalance,
          confirmedBalance: walletSnapshot.confirmedBalance,
          pendingBalance: walletSnapshot.pendingBalance,
          pendingSent: walletSnapshot.pendingSent,
          pendingReceived: walletSnapshot.pendingReceived,
          nextNonce: walletSnapshot.nextNonce,
        }));
      }

      queryClient.invalidateQueries({ queryKey: ['balance', address] });
      queryClient.invalidateQueries({ queryKey: ['transactions', address] });
      queryClient.invalidateQueries({ queryKey: ['wallet-transactions', address] });
      queryClient.invalidateQueries({ queryKey: ['all-transactions'] });
      queryClient.invalidateQueries({ queryKey: ['mempool'] });
    }
  });

  const onSubmit = async (formData) => {
    try {
      if (formData?.error) {
        setError(formData.error);
        return false;
      }

      if (!isValidAddress(formData.toAddress)) {
        setError('Receiver address must match format PCN_ + 40 hex characters');
        return false;
      }

      setSending(true);
      setError('');
      await sendMutation.mutateAsync(formData);
      return true;
    } catch (e) {
      setError(e?.response?.data?.error || e.message || 'Transaction failed');
      return false;
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
          <button className="btn-secondary" onClick={() => navigate('/wallet')}>Back</button>
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
