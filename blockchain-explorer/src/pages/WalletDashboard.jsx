import { useEffect, useMemo, useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { Area, AreaChart, ResponsiveContainer, Tooltip, XAxis, YAxis } from 'recharts';

import WalletCard from '../components/WalletCard';
import BalanceCard from '../components/BalanceCard';
import ReceivePanel from '../components/ReceivePanel';
import TransactionTable from '../components/TransactionTable';
import { blockchainApi } from '../services/api';

export default function WalletDashboard() {
  const navigate = useNavigate();
  const session = JSON.parse(localStorage.getItem('walletSession') || '{}');

  const [balance, setBalance] = useState(0);
  const [nextNonce, setNextNonce] = useState(1);
  const [transactions, setTransactions] = useState([]);
  const [health, setHealth] = useState({});

  const refresh = async () => {
    if (!session.address) return;
    const [b, tx, h] = await Promise.all([
      blockchainApi.getWalletBalance(session.address),
      blockchainApi.getWalletTransactions(session.address),
      blockchainApi.getHealth(),
    ]);
    setBalance(b.balance || 0);
    setNextNonce(b.nextNonce || 1);
    setTransactions(tx.transactions || []);
    setHealth(h || {});
  };

  useEffect(() => {
    refresh();
    const t = setInterval(refresh, 4000);
    return () => clearInterval(t);
  }, []);

  const rewards = useMemo(
    () => transactions.filter((t) => t.direction === 'reward').reduce((sum, t) => sum + Number(t.amount || 0), 0),
    [transactions],
  );

  const chartData = useMemo(
    () => transactions.slice(-20).map((t, idx) => ({ idx: idx + 1, amount: Number(t.amount || 0) })),
    [transactions],
  );

  const mine = async () => {
    await blockchainApi.mineBlock(session.address);
    await refresh();
  };

  const logout = () => {
    localStorage.removeItem('walletSession');
    navigate('/login');
  };

  return (
    <div className="min-h-screen bg-[radial-gradient(circle_at_10%_20%,#10243f_0%,#090f1a_40%,#05080f_100%)] text-white p-6 lg:p-8">
      <div className="max-w-7xl mx-auto">
        <div className="flex flex-wrap justify-between items-center gap-4 mb-6">
          <div>
            <h1 className="text-3xl font-black tracking-tight">Wallet Dashboard</h1>
            <p className="text-zinc-400 text-sm mt-1">Personalized account view with live chain updates</p>
          </div>
          <div className="flex gap-2">
            <button className="btn-secondary" onClick={() => navigate('/send')}>Send</button>
            <button className="btn-primary" onClick={mine}>Mine 50 PCN</button>
            <button className="btn-secondary" onClick={logout}>Logout</button>
          </div>
        </div>

        <div className="grid grid-cols-1 xl:grid-cols-3 gap-6">
          <div className="xl:col-span-2 space-y-6">
            <BalanceCard balance={balance} rewards={rewards} />

            <div className="dashboard-card h-[280px]">
              <p className="text-xs uppercase tracking-[0.2em] text-cyan-300/80 mb-4">Recent Amount Flow</p>
              <ResponsiveContainer width="100%" height="90%">
                <AreaChart data={chartData}>
                  <defs>
                    <linearGradient id="walletFlow" x1="0" y1="0" x2="0" y2="1">
                      <stop offset="5%" stopColor="#22d3ee" stopOpacity={0.5} />
                      <stop offset="95%" stopColor="#22d3ee" stopOpacity={0.02} />
                    </linearGradient>
                  </defs>
                  <XAxis dataKey="idx" stroke="#64748b" />
                  <YAxis stroke="#64748b" />
                  <Tooltip />
                  <Area dataKey="amount" stroke="#22d3ee" fill="url(#walletFlow)" />
                </AreaChart>
              </ResponsiveContainer>
            </div>

            <TransactionTable transactions={transactions.slice().reverse()} />
          </div>

          <div className="space-y-6">
            <WalletCard address={session.address} publicKey={session.publicKey} />
            <ReceivePanel address={session.address} />
            <div className="dashboard-card">
              <p className="text-xs uppercase tracking-[0.2em] text-cyan-300/80 mb-3">Network Status</p>
              <p className="text-zinc-300 text-sm">Chain Height: <span className="text-white font-semibold">{health.chainLength ?? '-'}</span></p>
              <p className="text-zinc-300 text-sm mt-2">Mempool Size: <span className="text-white font-semibold">{health.mempoolSize ?? '-'}</span></p>
              <p className="text-zinc-300 text-sm mt-2">Connected Peers: <span className="text-white font-semibold">{health.peerCount ?? '-'}</span></p>
              <p className="text-zinc-500 text-xs mt-4">Next nonce: {nextNonce}</p>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
