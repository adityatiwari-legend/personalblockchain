import { useMemo, useState } from 'react';
import { useQuery } from '@tanstack/react-query';
import ExplorerLayout from '../components/ExplorerLayout';
import TransactionTable from '../components/TransactionTable';
import TransactionDetailModal from '../components/TransactionDetailModal';
import { blockchainApi } from '../services/api';
import { normalizeTransactions, normalizeChain, txType, toEpochMs } from '../services/explorerData';
import { useWalletStore } from '../store/useWalletStore';

const FILTERS = [
  { id: 'all', label: 'All' },
  { id: 'sent', label: 'Sent' },
  { id: 'received', label: 'Received' },
  { id: 'reward', label: 'Mining rewards' },
];

export default function Transactions() {
  const [activeFilter, setActiveFilter] = useState('all');
  const [selectedTx, setSelectedTx] = useState(null);
  const walletAddress = useWalletStore((state) => state.wallet?.address || '');

  const chainQuery = useQuery({
    queryKey: ['chain'],
    queryFn: blockchainApi.getChain,
    refetchInterval: 5000,
    retry: 2,
  });

  const txQuery = useQuery({
    queryKey: ['all-transactions'],
    queryFn: blockchainApi.getTransactions,
    refetchInterval: 5000,
    retry: 2,
  });

  const transactions = useMemo(() => {
    const chain = normalizeChain(chainQuery.data || []);
    const normalized = normalizeTransactions(txQuery.data || [], chain)
      .map((tx) => ({ ...tx, type: txType(tx, walletAddress) }))
      .sort((a, b) => toEpochMs(b.timestamp) - toEpochMs(a.timestamp));

    if (activeFilter === 'all') return normalized;
    return normalized.filter((tx) => tx.type === activeFilter);
  }, [txQuery.data, chainQuery.data, activeFilter, walletAddress]);

  const isLoading = chainQuery.isLoading || txQuery.isLoading;
  const isError = chainQuery.isError && txQuery.isError;

  return (
    <ExplorerLayout section="Transactions">
      <div className="space-y-6">
        <div className="flex flex-wrap gap-4 items-center justify-between">
          <div>
            <h2 className="text-2xl font-bold text-white">Transaction Explorer</h2>
            <p className="text-sm text-gray-400">Full transaction history with live updates.</p>
          </div>
          <button
            type="button"
            className="btn-secondary"
            onClick={() => {
              chainQuery.refetch();
              txQuery.refetch();
            }}
            disabled={chainQuery.isFetching || txQuery.isFetching}
          >
            {chainQuery.isFetching || txQuery.isFetching ? 'Refreshing...' : 'Refresh'}
          </button>
        </div>

        <div className="flex flex-wrap gap-2">
          {FILTERS.map((filter) => (
            <button
              key={filter.id}
              type="button"
              onClick={() => setActiveFilter(filter.id)}
              className={`px-3 py-2 rounded-lg text-sm border transition-all ${
                activeFilter === filter.id
                  ? 'bg-[#00ff9d]/15 border-[#00ff9d]/40 text-[#00ff9d]'
                  : 'bg-[#111827] border-gray-700 text-gray-300 hover:border-gray-500'
              }`}
            >
              {filter.label}
            </button>
          ))}
        </div>

        {isLoading && <div className="dashboard-card text-gray-400">Loading transactions...</div>}

        {isError && (
          <div className="dashboard-card border-red-500/30 bg-red-500/5">
            <p className="text-red-300 text-sm">Unable to load transactions from API and chain fallback.</p>
            <button type="button" className="btn-secondary mt-4" onClick={() => { chainQuery.refetch(); txQuery.refetch(); }}>
              Retry
            </button>
          </div>
        )}

        {!isLoading && !isError && (
          <>
            {transactions.length === 0 ? (
              <div className="dashboard-card text-gray-500">No transactions found for this filter.</div>
            ) : (
              <TransactionTable
                transactions={transactions}
                mode="explorer"
                enableVirtualization
                onRowClick={setSelectedTx}
              />
            )}
          </>
        )}

        {selectedTx && (
          <TransactionDetailModal transaction={selectedTx} onClose={() => setSelectedTx(null)} />
        )}
      </div>
    </ExplorerLayout>
  );
}
