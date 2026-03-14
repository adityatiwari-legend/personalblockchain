export default function ReceivePanel({ address }) {
  const copyAddress = async () => {
    await navigator.clipboard.writeText(address);
  };

  return (
    <div className="dashboard-card">
      <p className="text-xs uppercase tracking-[0.2em] text-cyan-300/80 mb-3">Receive Coins</p>
      <p className="text-zinc-400 text-sm">Share this address:</p>
      <p className="font-mono text-xs mt-3 break-all text-white">{address}</p>
      <button className="btn-primary mt-4 w-full" onClick={copyAddress}>Copy Address</button>
    </div>
  );
}
