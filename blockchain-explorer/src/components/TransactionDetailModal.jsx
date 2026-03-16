import { AnimatePresence, motion } from 'framer-motion';
import { X } from 'lucide-react';

export default function TransactionDetailModal({ transaction, onClose }) {
  if (!transaction) return null;

  return (
    <AnimatePresence>
      <div className="fixed inset-0 z-50 flex items-center justify-center px-4 py-6">
        <motion.div
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          exit={{ opacity: 0 }}
          onClick={onClose}
          className="absolute inset-0 bg-black/80 backdrop-blur-sm"
        />

        <motion.div
          initial={{ opacity: 0, y: 20, scale: 0.98 }}
          animate={{ opacity: 1, y: 0, scale: 1 }}
          exit={{ opacity: 0, y: 20, scale: 0.98 }}
          className="relative w-full max-w-3xl max-h-full overflow-y-auto dashboard-card"
        >
          <div className="sticky top-0 bg-[#111827]/95 backdrop-blur-md border-b border-gray-800 py-4 mb-4 flex justify-between items-center z-10">
            <div>
              <h3 className="text-xl font-bold text-white">Transaction Details</h3>
              <p className="text-xs text-gray-400">{transaction.txID || transaction.id}</p>
            </div>
            <button
              type="button"
              onClick={onClose}
              className="p-2 rounded-full hover:bg-gray-800 text-gray-400 hover:text-white transition-colors"
            >
              <X className="w-5 h-5" />
            </button>
          </div>

          <div className="space-y-5">
            <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
              <Detail label="Sender" value={transaction.sender || 'System'} mono />
              <Detail label="Receiver" value={transaction.receiver || 'N/A'} mono />
              <Detail label="Amount" value={String(transaction.amount ?? 0)} />
              <Detail label="Status" value={transaction.status || 'confirmed'} />
              <Detail label="Block Height" value={String(transaction.blockHeight ?? 'N/A')} />
              <Detail label="Block Hash" value={transaction.blockHash || 'N/A'} mono />
              <Detail label="Confirmations" value={String(transaction.confirmations ?? 'N/A')} />
              <Detail label="Signature" value={transaction.signature || 'N/A'} mono />
            </div>

            <div>
              <p className="text-xs text-gray-400 mb-2">Full Transaction JSON</p>
              <pre className="bg-gray-950 p-4 rounded-lg overflow-x-auto text-xs text-gray-300 font-mono border border-gray-800">
                {JSON.stringify(transaction.raw || transaction, null, 2)}
              </pre>
            </div>
          </div>
        </motion.div>
      </div>
    </AnimatePresence>
  );
}

function Detail({ label, value, mono = false }) {
  return (
    <div className="bg-gray-900/50 p-4 rounded-xl border border-gray-800">
      <p className="text-xs text-gray-400 uppercase mb-2">{label}</p>
      <p className={`text-sm text-gray-200 break-all ${mono ? 'font-mono' : ''}`}>{value}</p>
    </div>
  );
}
