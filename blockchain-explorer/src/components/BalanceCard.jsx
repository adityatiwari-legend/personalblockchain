export default function BalanceCard({ balance, rewards, confirmedBalance }) {
  const total = Number(balance || 0);
  const confirmed = Number(confirmedBalance ?? balance ?? 0);
  const pendingDelta = total - confirmed;

  return (
    <div className="dashboard-card bg-gradient-to-br from-[#121726] via-[#151d31] to-[#0d111c] border-cyan-500/30">
      <p className="text-xs uppercase tracking-[0.2em] text-cyan-300/80">Wallet Overview</p>
      <h2 className="text-4xl font-black mt-3 text-white">{total.toLocaleString()} PCN</h2>
      <p className="text-zinc-400 mt-2">
        Confirmed: <span className="text-white">{confirmed.toLocaleString()} PCN</span>
        {' · '}
        Pending delta: <span className={pendingDelta < 0 ? 'text-amber-300' : 'text-emerald-300'}>{pendingDelta >= 0 ? '+' : ''}{pendingDelta.toLocaleString()} PCN</span>
      </p>
      <p className="text-zinc-400 mt-2">Mining rewards earned: <span className="text-cyan-300">{Number(rewards || 0).toLocaleString()} PCN</span></p>
    </div>
  );
}
