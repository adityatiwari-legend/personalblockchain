import { useMemo } from 'react';
import toast from 'react-hot-toast';
import { useQuery } from '@tanstack/react-query';
import ExplorerLayout from '../components/ExplorerLayout';
import { blockchainApi } from '../services/api';
import { normalizeChain, normalizeTransactions, txType } from '../services/explorerData';
import { useWalletStore } from '../store/useWalletStore';

export default function Profile() {
  const wallet = useWalletStore((state) => state.wallet);
  const address = wallet?.address || '';

  const statusQuery = useQuery({
    queryKey: ['status'],
    queryFn: blockchainApi.getStatus,
    refetchInterval: 5000,
    retry: 2,
  });

  const chainQuery = useQuery({
    queryKey: ['chain'],
    queryFn: blockchainApi.getChain,
    refetchInterval: 5000,
    retry: 2,
  });

  const walletTxQuery = useQuery({
    queryKey: ['wallet-transactions', address],
    queryFn: () => blockchainApi.getWalletTransactions(address),
    enabled: !!address,
    refetchInterval: 5000,
    retry: 2,
  });

  const normalizedChain = useMemo(() => normalizeChain(chainQuery.data || []), [chainQuery.data]);

  const blocksMined = useMemo(() => {
    if (!address) return 0;
    return normalizedChain.filter((block) => {
      const miner = block.minerAddress || block.miner || block.beneficiary;
      return miner === address;
    }).length;
  }, [normalizedChain, address]);

  const rewards = useMemo(() => {
    const txs = normalizeTransactions(walletTxQuery.data?.transactions || walletTxQuery.data || []);
    return txs
      .filter((tx) => txType(tx, address) === 'reward')
      .reduce((sum, tx) => sum + Number(tx.amount || 0), 0);
  }, [walletTxQuery.data, address]);

  const nodeId = statusQuery.data?.nodeId || statusQuery.data?.id || statusQuery.data?.identity || 'N/A';

  const copyValue = async (label, value) => {
    if (!value || value === 'N/A') return;
    try {
      await navigator.clipboard.writeText(String(value));
      toast.success(`${label} copied`);
    } catch {
      toast.error(`Could not copy ${label.toLowerCase()}`);
    }
  };

  return (
    <ExplorerLayout section="Profile">
      <div className="space-y-6">
        <div>
          <h2 className="text-2xl font-bold text-white">Profile</h2>
          <p className="text-sm text-gray-400">Wallet and node identity details.</p>
        </div>

        {!wallet && (
          <div className="dashboard-card text-gray-400">Login required to view profile details.</div>
        )}

        {wallet && (
          <div className="grid grid-cols-1 lg:grid-cols-2 gap-4">
            <IdentityCard label="Wallet Address" value={wallet.address} onCopy={() => copyValue('Address', wallet.address)} />
            <IdentityCard label="Public Key" value={wallet.publicKey} onCopy={() => copyValue('Public key', wallet.publicKey)} />
            <IdentityCard label="Node Identity" value={nodeId} onCopy={() => copyValue('Node ID', nodeId)} />
            <div className="dashboard-card">
              <p className="text-xs uppercase tracking-[0.2em] text-cyan-300/80 mb-3">Mining Stats</p>
              <p className="text-sm text-gray-300">Blocks mined: <span className="text-white font-semibold">{blocksMined}</span></p>
              <p className="text-sm text-gray-300 mt-2">Total rewards: <span className="text-white font-semibold">{rewards.toLocaleString()}</span></p>
            </div>
          </div>
        )}

        {(statusQuery.isLoading || chainQuery.isLoading || walletTxQuery.isLoading) && (
          <div className="dashboard-card text-gray-400">Loading profile stats...</div>
        )}

        {(statusQuery.isError || chainQuery.isError || walletTxQuery.isError) && (
          <div className="dashboard-card border-red-500/30 bg-red-500/5">
            <p className="text-red-300 text-sm">Some profile data failed to load.</p>
            <button
              type="button"
              className="btn-secondary mt-4"
              onClick={() => {
                statusQuery.refetch();
                chainQuery.refetch();
                walletTxQuery.refetch();
              }}
            >
              Retry
            </button>
          </div>
        )}
      </div>
    </ExplorerLayout>
  );
}

function IdentityCard({ label, value, onCopy }) {
  return (
    <div className="dashboard-card">
      <div className="flex items-center justify-between gap-3 mb-2">
        <p className="text-xs uppercase tracking-[0.2em] text-cyan-300/80">{label}</p>
        <button type="button" className="btn-secondary !py-1.5 !px-3" onClick={onCopy}>
          Copy
        </button>
      </div>
      <p className="text-sm text-gray-300 font-mono break-all">{value || 'N/A'}</p>
    </div>
  );
}
