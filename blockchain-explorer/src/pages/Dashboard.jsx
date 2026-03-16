import { useMemo, useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { useQuery } from '@tanstack/react-query';
import { AreaChart, Area, XAxis, YAxis, Tooltip, ResponsiveContainer } from 'recharts';
import { Box, Activity, Database } from 'lucide-react';
import ExplorerLayout from '../components/ExplorerLayout';
import TransactionTable from '../components/TransactionTable';
import TransactionDetailModal from '../components/TransactionDetailModal';
import TransactionForm from '../components/TransactionForm';
import MiningPanel from '../components/MiningPanel';
import { blockchainApi } from '../services/api';
import { normalizeChain, normalizeTransactions, toEpochMs, txType, formatTimestamp, shorten } from '../services/explorerData';
import { useWalletStore } from '../store/useWalletStore';

export default function Dashboard() {
  const navigate = useNavigate();
  const walletAddress = useWalletStore((state) => state.wallet?.address || '');
  const [selectedTx, setSelectedTx] = useState(null);

  const chainQuery = useQuery({
    queryKey: ['chain'],
    queryFn: blockchainApi.getChain,
    refetchInterval: 5000,
    retry: 2,
  });

  const statusQuery = useQuery({
    queryKey: ['status'],
    queryFn: blockchainApi.getStatus,
    refetchInterval: 5000,
    retry: 2,
  });

  const mempoolQuery = useQuery({
    queryKey: ['mempool'],
    queryFn: blockchainApi.getMempool,
    refetchInterval: 5000,
    retry: 2,
  });

  const balanceQuery = useQuery({
    queryKey: ['balance', walletAddress],
    queryFn: () => blockchainApi.getWalletBalance(walletAddress),
    refetchInterval: 5000,
    retry: 2,
  });

  const txQuery = useQuery({
    queryKey: ['transactions', walletAddress],
    queryFn: async () => {
      if (walletAddress) {
        const walletTx = await blockchainApi.getWalletTransactions(walletAddress);
        return walletTx?.transactions || walletTx || [];
      }
      return blockchainApi.getTransactions();
    },
    refetchInterval: 5000,
    retry: 2,
  });

  const chain = useMemo(() => normalizeChain(chainQuery.data || []), [chainQuery.data]);

  const latestBlocks = useMemo(() => chain.slice().reverse().slice(0, 5), [chain]);

  const transactions = useMemo(() => {
    const normalized = normalizeTransactions(txQuery.data || [], chain)
      .map((tx) => ({ ...tx, direction: txType(tx, walletAddress) }))
      .sort((a, b) => toEpochMs(b.timestamp) - toEpochMs(a.timestamp));
    return normalized;
  }, [txQuery.data, chain, walletAddress]);

  const chartData = useMemo(() => {
    return chain.slice(-20).map((block) => ({
      name: block.index,
      transactions: block.transactionCount,
    }));
  }, [chain]);

  const isLoading = chainQuery.isLoading || statusQuery.isLoading;
  const hasError = chainQuery.isError && statusQuery.isError;

  return (
    <ExplorerLayout section="Dashboard" chainName={statusQuery.data?.name || 'PersonalBlockchain'}>
      <div className="grid grid-cols-12 gap-6">
        <div className="col-span-12 xl:col-span-8 flex flex-col gap-6">
          <div className="dashboard-card relative overflow-hidden min-h-[360px] flex flex-col justify-between">
            <div className="flex justify-between items-start mb-6 z-10">
              <div>
                <h2 className="text-gray-400 text-sm font-medium mb-1">Blockchain Height</h2>
                <div className="flex items-baseline gap-3">
                  <span className="text-5xl font-bold text-white tracking-tight">{chain.length.toLocaleString()}</span>
                  <span className="text-[#00ff9d] text-sm font-bold flex items-center bg-[#00ff9d]/10 px-2 py-1 rounded-lg">
                    Live
                  </span>
                </div>
              </div>
              <button
                type="button"
                className="btn-secondary"
                onClick={() => {
                  chainQuery.refetch();
                  statusQuery.refetch();
                  mempoolQuery.refetch();
                  balanceQuery.refetch();
                  txQuery.refetch();
                }}
              >
                Refresh
              </button>
            </div>

            <div className="absolute inset-x-0 bottom-0 h-[250px]">
              <ResponsiveContainer width="100%" height="100%">
                <AreaChart data={chartData}>
                  <defs>
                    <linearGradient id="colorTx" x1="0" y1="0" x2="0" y2="1">
                      <stop offset="5%" stopColor="#fff" stopOpacity={0.05} />
                      <stop offset="95%" stopColor="#fff" stopOpacity={0} />
                    </linearGradient>
                  </defs>
                  <XAxis dataKey="name" hide />
                  <YAxis hide domain={['auto', 'auto']} />
                  <Tooltip
                    contentStyle={{ backgroundColor: '#111827', borderColor: '#374151', borderRadius: '12px', color: '#fff' }}
                    itemStyle={{ color: '#fff' }}
                  />
                  <Area type="monotone" dataKey="transactions" stroke="#9ca3af" strokeWidth={2} fillOpacity={1} fill="url(#colorTx)" />
                </AreaChart>
              </ResponsiveContainer>
            </div>
          </div>

          <div className="dashboard-card">
            <div className="flex justify-between items-center mb-6">
              <h3 className="text-lg font-bold text-white">Latest Blocks</h3>
              <button type="button" className="text-xs font-bold text-[#00ff9d] hover:underline" onClick={() => navigate('/blocks')}>
                View All
              </button>
            </div>

            <div className="overflow-x-auto">
              <table className="w-full text-left border-collapse">
                <thead>
                  <tr className="text-xs text-gray-500 border-b border-gray-800">
                    <th className="pb-3 pl-2 font-medium">Block Height</th>
                    <th className="pb-3 font-medium hidden md:table-cell">Hash</th>
                    <th className="pb-3 font-medium">Time Mined</th>
                    <th className="pb-3 font-medium">Tx Count</th>
                    <th className="pb-3 pr-2 text-right font-medium">Status</th>
                  </tr>
                </thead>
                <tbody className="text-sm">
                  {latestBlocks.map((block) => (
                    <tr key={`${block.index}-${block.hash}`} className="group hover:bg-[#1f2937]/30 transition-colors border-b border-gray-800/50 last:border-0">
                      <td className="py-4 pl-2 font-bold text-white flex items-center gap-3">
                        <div className="w-8 h-8 rounded-full bg-[#1f2937] flex items-center justify-center text-gray-400 border border-gray-700">
                          <Box className="w-4 h-4" />
                        </div>
                        #{block.index}
                      </td>
                      <td className="py-4 font-mono text-gray-400 hidden md:table-cell">{shorten(block.hash)}</td>
                      <td className="py-4 text-gray-300">{formatTimestamp(block.timestamp)}</td>
                      <td className="py-4 text-white font-medium pl-4">{block.transactionCount}</td>
                      <td className="py-4 pr-2 text-right">
                        <span className="text-xs font-bold text-[#00ff9d] bg-[#00ff9d]/10 px-2 py-1 rounded">Confirmed</span>
                      </td>
                    </tr>
                  ))}
                  {!isLoading && latestBlocks.length === 0 && (
                    <tr>
                      <td colSpan={5} className="py-6 text-center text-gray-500">No blocks found.</td>
                    </tr>
                  )}
                </tbody>
              </table>
            </div>
          </div>

          <div>
            {transactions.length > 0 ? (
              <TransactionTable transactions={transactions.slice(0, 20)} onRowClick={setSelectedTx} />
            ) : (
              <div className="dashboard-card text-gray-500">No recent transactions.</div>
            )}
          </div>
        </div>

        <div className="col-span-12 xl:col-span-4 flex flex-col gap-6">
          <div className="dashboard-card">
            <h3 className="text-lg font-bold text-white mb-4">Network Status</h3>
            {hasError && <p className="text-sm text-red-300">Node status unavailable.</p>}
            {!hasError && (
              <div className="space-y-2 text-sm text-gray-300">
                <p>Peers: <span className="text-white font-semibold">{statusQuery.data?.peerCount ?? statusQuery.data?.peers ?? 0}</span></p>
                <p>Chain Height: <span className="text-white font-semibold">{statusQuery.data?.chainLength ?? chain.length}</span></p>
                <p>Mempool Size: <span className="text-white font-semibold">{Array.isArray(mempoolQuery.data) ? mempoolQuery.data.length : 0}</span></p>
                <p>Wallet Balance: <span className="text-white font-semibold">{Number(balanceQuery.data?.balance || 0).toLocaleString()}</span></p>
              </div>
            )}
          </div>

          <div className="dashboard-card">
            <h3 className="text-lg font-bold text-white mb-6">Quick Transaction</h3>
            <TransactionForm onSuccess={() => txQuery.refetch()} />
          </div>

          <div className="dashboard-card flex flex-col h-[340px] p-0 overflow-hidden relative border border-[#1f2937]">
            <div className="absolute top-6 left-6 z-10">
              <h3 className="text-lg font-bold text-white">Network Mining</h3>
            </div>
            <MiningPanel onSuccess={() => chainQuery.refetch()} minimal />
          </div>

          <div className="grid grid-cols-2 gap-3">
            <div className="dashboard-card p-4">
              <div className="flex items-center gap-2 text-gray-400 mb-1"><Activity className="w-4 h-4" /> Live</div>
              <p className="text-white font-semibold">5s refresh</p>
            </div>
            <div className="dashboard-card p-4">
              <div className="flex items-center gap-2 text-gray-400 mb-1"><Database className="w-4 h-4" /> API</div>
              <p className="text-white font-semibold">Dynamic</p>
            </div>
          </div>
        </div>
      </div>

      {isLoading && (
        <div className="dashboard-card mt-6 text-gray-400">Loading dashboard data...</div>
      )}

      {hasError && (
        <div className="dashboard-card mt-6 border-red-500/30 bg-red-500/5">
          <p className="text-red-300 text-sm">Both /chain and /status are unavailable.</p>
          <button
            type="button"
            className="btn-secondary mt-4"
            onClick={() => {
              chainQuery.refetch();
              statusQuery.refetch();
            }}
          >
            Retry
          </button>
        </div>
      )}

      {selectedTx && (
        <TransactionDetailModal transaction={selectedTx} onClose={() => setSelectedTx(null)} />
      )}
    </ExplorerLayout>
  );
}
