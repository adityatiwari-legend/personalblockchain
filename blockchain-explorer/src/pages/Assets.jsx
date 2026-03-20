import { useMemo } from 'react';
import { useQuery } from '@tanstack/react-query';
import ExplorerLayout from '../components/ExplorerLayout';
import TransactionTable from '../components/TransactionTable';
import { blockchainApi } from '../services/api';
import { normalizeTransactions, txType, toEpochMs } from '../services/explorerData';
import { useWalletStore } from '../store/useWalletStore';

export default function Assets() {
  const wallet = useWalletStore((state) => state.wallet);
  const address = wallet?.address;

  const balanceQuery = useQuery({
    queryKey: ['balance', address],
    queryFn: () => blockchainApi.getWalletBalance(address),
    enabled: !!address,
    refetchInterval: 5000,
    retry: 2,
  });

  const txQuery = useQuery({
    queryKey: ['wallet-transactions', address],
    queryFn: () => blockchainApi.getWalletTransactions(address),
    enabled: !!address,
    refetchInterval: 5000,
    retry: 2,
  });

  const transactions = useMemo(() => {
    const raw = txQuery.data?.transactions || txQuery.data || [];
    return normalizeTransactions(raw)
      .map((tx) => ({ ...tx, type: txType(tx, address), direction: txType(tx, address) }))
      .sort((a, b) => toEpochMs(b.timestamp) - toEpochMs(a.timestamp));
  }, [txQuery.data, address]);

  const summary = useMemo(() => {
    return transactions.reduce(
      (acc, tx) => {
        if (tx.type === 'reward') acc.rewards += Number(tx.amount || 0);
        if (tx.type === 'received') acc.received += Number(tx.amount || 0);
        if (tx.type === 'sent') acc.sent += Number(tx.amount || 0);
        return acc;
      },
      { rewards: 0, received: 0, sent: 0 },
    );
  }, [transactions]);

  const confirmedBalance = Number(balanceQuery.data?.confirmedBalance ?? balanceQuery.data?.balance ?? 0);
  const pendingBalance = Number(balanceQuery.data?.pendingBalance ?? confirmedBalance);

  return (
    <ExplorerLayout section="Assets">
      <div className="space-y-6">
        <div className="flex items-center justify-between">
          <div>
            <h2 className="text-2xl font-bold text-white">Assets</h2>
            <p className="text-sm text-gray-400">Wallet balance and transaction-derived asset summary.</p>
          </div>
          <button
            type="button"
            className="btn-secondary"
            onClick={() => {
              balanceQuery.refetch();
              txQuery.refetch();
            }}
            disabled={balanceQuery.isFetching || txQuery.isFetching}
          >
            {balanceQuery.isFetching || txQuery.isFetching ? 'Refreshing...' : 'Refresh'}
          </button>
        </div>

        {!address && (
          <div className="dashboard-card text-gray-400">Login required to view wallet assets.</div>
        )}

        {address && (
          <>
            <div className="grid grid-cols-1 md:grid-cols-2 xl:grid-cols-4 gap-4">
              <div className="dashboard-card">
                <p className="text-xs uppercase tracking-[0.2em] text-cyan-300/80 mb-2">Wallet Balance</p>
                <p className="text-2xl font-bold text-white">{pendingBalance.toLocaleString()}</p>
                <p className="text-xs text-zinc-400 mt-1">Confirmed: {confirmedBalance.toLocaleString()}</p>
              </div>
              <div className="dashboard-card">
                <p className="text-xs uppercase tracking-[0.2em] text-cyan-300/80 mb-2">Mining Rewards</p>
                <p className="text-2xl font-bold text-white">{summary.rewards.toLocaleString()}</p>
              </div>
              <div className="dashboard-card">
                <p className="text-xs uppercase tracking-[0.2em] text-cyan-300/80 mb-2">Total Received</p>
                <p className="text-2xl font-bold text-white">{summary.received.toLocaleString()}</p>
              </div>
              <div className="dashboard-card">
                <p className="text-xs uppercase tracking-[0.2em] text-cyan-300/80 mb-2">Total Sent</p>
                <p className="text-2xl font-bold text-white">{summary.sent.toLocaleString()}</p>
              </div>
            </div>

            {(balanceQuery.isLoading || txQuery.isLoading) && (
              <div className="dashboard-card text-gray-400">Loading wallet asset history...</div>
            )}

            {(balanceQuery.isError || txQuery.isError) && (
              <div className="dashboard-card border-red-500/30 bg-red-500/5">
                <p className="text-red-300 text-sm">Failed to load wallet asset data.</p>
                <button type="button" className="btn-secondary mt-4" onClick={() => { balanceQuery.refetch(); txQuery.refetch(); }}>
                  Retry
                </button>
              </div>
            )}

            {!balanceQuery.isLoading && !txQuery.isLoading && !balanceQuery.isError && !txQuery.isError && (
              transactions.length > 0 ? (
                <TransactionTable transactions={transactions} />
              ) : (
                <div className="dashboard-card text-gray-500">No asset activity found yet.</div>
              )
            )}
          </>
        )}
      </div>
    </ExplorerLayout>
  );
}
