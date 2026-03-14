export default function TransactionTable({ transactions }) {
  return (
    <div className="dashboard-card">
      <p className="text-xs uppercase tracking-[0.2em] text-cyan-300/80 mb-4">Transaction History</p>
      <div className="overflow-x-auto">
        <table className="w-full text-left">
          <thead>
            <tr className="text-xs text-zinc-400 border-b border-zinc-800">
              <th className="py-2">Type</th>
              <th className="py-2">Amount</th>
              <th className="py-2">Counterparty</th>
              <th className="py-2">Time</th>
            </tr>
          </thead>
          <tbody>
            {(transactions || []).map((tx) => (
              <tr key={tx.txID} className="border-b border-zinc-900/70 text-sm">
                <td className="py-3 text-zinc-200 capitalize">{tx.direction}</td>
                <td className={`py-3 font-semibold ${tx.direction === 'sent' ? 'text-rose-300' : 'text-emerald-300'}`}>
                  {tx.direction === 'sent' ? '-' : '+'}{tx.amount}
                </td>
                <td className="py-3 font-mono text-xs text-zinc-400">
                  {tx.direction === 'sent' ? tx.toAddress : tx.fromAddress}
                </td>
                <td className="py-3 text-zinc-400">{tx.timestamp}</td>
              </tr>
            ))}
            {(!transactions || transactions.length === 0) && (
              <tr>
                <td colSpan={4} className="py-6 text-center text-zinc-500">No transactions yet</td>
              </tr>
            )}
          </tbody>
        </table>
      </div>
    </div>
  );
}
