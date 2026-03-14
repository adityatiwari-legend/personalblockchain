export default function WalletCard({ address, publicKey }) {
  return (
    <div className="dashboard-card">
      <p className="text-xs uppercase tracking-[0.2em] text-cyan-300/80 mb-3">Wallet Identity</p>
      <p className="text-xs text-zinc-400 mb-1">Address</p>
      <p className="font-mono text-sm break-all text-white">{address}</p>
      <p className="text-xs text-zinc-400 mt-4 mb-1">Public Key</p>
      <p className="font-mono text-xs break-all text-zinc-300">{publicKey}</p>
    </div>
  );
}
