import { motion, AnimatePresence } from 'framer-motion';
import { X, Hash, Cpu, Link as LinkIcon, Database } from 'lucide-react';

export default function BlockModal({ block, onClose }) {
  if (!block) return null;

  return (
    <AnimatePresence>
      <div className="fixed inset-0 z-50 flex items-center justify-center pt-10 pb-10 px-4">
        {/* Backdrop */}
        <motion.div 
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          exit={{ opacity: 0 }}
          onClick={onClose}
          className="absolute inset-0 bg-black/80 backdrop-blur-sm"
        />

        {/* Modal */}
        <motion.div 
          initial={{ opacity: 0, scale: 0.95, y: 20 }}
          animate={{ opacity: 1, scale: 1, y: 0 }}
          exit={{ opacity: 0, scale: 0.95, y: 20 }}
          className="relative max-h-full overflow-y-auto w-full max-w-3xl glass-card border-[#00ff9d]/30 shadow-[0_0_50px_rgba(0,0,0,0.5)] p-0"
        >
          <div className="sticky top-0 bg-[#0f172a]/95 backdrop-blur-md border-b border-gray-800 p-6 flex justify-between items-center z-10">
            <div>
              <h2 className="text-2xl font-bold text-white flex items-center gap-3">
                <BoxIcon /> Block <span className="text-[#00ff9d]">#{block.index}</span>
              </h2>
              <p className="text-gray-400 text-sm mt-1">
                Mined on {new Date(block.timestamp * 1000).toLocaleString()}
              </p>
            </div>
            <button 
              onClick={onClose}
              className="p-2 rounded-full hover:bg-gray-800 text-gray-400 hover:text-white transition-colors"
            >
              <X className="w-6 h-6" />
            </button>
          </div>

          <div className="p-6 space-y-6">
            <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
              <DetailItem icon={<Hash />} label="Block Hash" value={block.hash} isCode />
              <DetailItem icon={<LinkIcon />} label="Previous Hash" value={block.previous_hash} isCode />
              <DetailItem icon={<Database />} label="Merkle Root" value={block.merkle_root} isCode />
              <div className="grid grid-cols-2 gap-4">
                <DetailItem icon={<Cpu />} label="Nonce" value={block.nonce} isCode />
                <DetailItem icon={<Cpu />} label="Difficulty" value={block.difficulty} isCode />
              </div>
            </div>

            <div className="mt-8">
              <h3 className="text-lg font-semibold text-white mb-4 flex items-center gap-2 border-b border-gray-800 pb-2">
                Transactions ({block.transactions?.length || 0})
              </h3>
              
              {block.transactions && block.transactions.length > 0 ? (
                <div className="space-y-3">
                  {block.transactions.map((tx, idx) => (
                    <div key={idx} className="bg-gray-900/50 rounded-lg p-4 border border-gray-800">
                      <div className="flex justify-between mb-3 text-sm">
                        <span className="text-gray-400">ID: <span className="text-gray-300 font-mono">{tx.id?.substring(0,16)}...</span></span>
                        <span className="text-[#00ff9d] font-bold">{tx.amount} NEX</span>
                      </div>
                      <div className="flex items-center gap-4 text-xs font-mono">
                        <div className="flex-1 bg-gray-950 p-2 rounded border border-gray-800 truncate">
                          <span className="text-gray-500 block text-[10px] mb-1">SENDER</span>
                          <span className="text-gray-300">{tx.sender}</span>
                        </div>
                        <div className="text-gray-600">→</div>
                        <div className="flex-1 bg-gray-950 p-2 rounded border border-gray-800 truncate">
                          <span className="text-gray-500 block text-[10px] mb-1">RECEIVER</span>
                          <span className="text-gray-300">{tx.receiver}</span>
                        </div>
                      </div>
                    </div>
                  ))}
                </div>
              ) : (
                <div className="text-center py-8 text-gray-500 bg-gray-900/30 rounded-lg border border-gray-800 border-dashed">
                  No transactions in this block.
                </div>
              )}
            </div>

            <div className="mt-6">
              <h3 className="text-sm font-semibold text-gray-400 mb-2">Raw JSON</h3>
              <pre className="bg-gray-950 p-4 rounded-lg overflow-x-auto text-xs text-gray-300 font-mono border border-gray-800">
                {JSON.stringify(block, null, 2)}
              </pre>
            </div>
          </div>
        </motion.div>
      </div>
    </AnimatePresence>
  );
}

function DetailItem({ icon, label, value, isCode }) {
  return (
    <div className="bg-gray-900/50 p-4 rounded-xl border border-gray-800">
      <div className="flex items-center gap-2 text-gray-400 mb-2">
        {icon && <span className="w-4 h-4">{icon}</span>}
        <span className="text-xs font-semibold uppercase">{label}</span>
      </div>
      <div className={`text-sm text-gray-200 break-all ${isCode ? 'font-mono' : ''}`}>
        {value || 'N/A'}
      </div>
    </div>
  );
}

function BoxIcon() {
  return (
    <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinelinejoin="round" className="text-[#00ff9d]">
      <path d="M21 16V8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16z"></path>
      <polyline points="3.27 6.96 12 12.01 20.73 6.96"></polyline>
      <line x1="12" y1="22.08" x2="12" y2="12"></line>
    </svg>
  );
}
