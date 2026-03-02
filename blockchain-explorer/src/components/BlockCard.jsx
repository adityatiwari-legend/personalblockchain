import { Box, Clock, Hash, CheckCircle2 } from 'lucide-react';
import { motion } from 'framer-motion';

export default function BlockCard({ block, onClick, index }) {
  if (!block) return null;
  const isGenesis = block.index === 0;

  return (
    <motion.div
      initial={{ opacity: 0, x: -20 }}
      animate={{ opacity: 1, x: 0 }}
      transition={{ duration: 0.3, delay: index * 0.1 }}
      onClick={() => onClick(block)}
      className="glass-card p-5 cursor-pointer hover:-translate-y-1 group relative overflow-hidden"
    >
      <div className="absolute top-0 left-0 w-1 h-full bg-[#00ff9d] opacity-0 group-hover:opacity-100 transition-opacity"></div>
      
      <div className="flex justify-between items-start mb-4">
        <div className="flex items-center gap-3">
          <div className="w-10 h-10 rounded-lg bg-gray-900 flex items-center justify-center border border-gray-800">
            <Box className="w-5 h-5 text-[#00b8ff]" />
          </div>
          <div>
            <span className="text-xs text-gray-500 font-bold uppercase tracking-wider block">Block</span>
            <span className="text-lg font-mono font-semibold text-white">#{block.index}</span>
          </div>
        </div>
        {isGenesis && (
          <span className="px-2 py-1 text-[10px] font-bold bg-[#00ff9d]/20 text-[#00ff9d] rounded border border-[#00ff9d]/30">GENESIS</span>
        )}
      </div>

      <div className="space-y-3">
        <div className="flex justify-between items-center bg-gray-900/50 p-2 rounded border border-gray-800">
          <div className="flex items-center gap-2">
            <Hash className="w-4 h-4 text-gray-400" />
            <span className="text-xs text-gray-400">Hash</span>
          </div>
          <span className="text-xs font-mono text-gray-300">
            {block.hash ? `${block.hash.substring(0, 10)}...` : 'N/A'}
          </span>
        </div>
        
        <div className="flex justify-between items-center bg-gray-900/50 p-2 rounded border border-gray-800">
          <div className="flex items-center gap-2">
            <Clock className="w-4 h-4 text-gray-400" />
            <span className="text-xs text-gray-400">Time</span>
          </div>
          <span className="text-xs text-gray-300">
            {new Date(block.timestamp * 1000).toLocaleTimeString()}
          </span>
        </div>

        <div className="flex justify-between items-center bg-gray-900/50 p-2 rounded border border-gray-800">
          <div className="flex items-center gap-2">
            <CheckCircle2 className="w-4 h-4 text-gray-400" />
            <span className="text-xs text-gray-400">Tx Count</span>
          </div>
          <span className="text-xs font-bold text-[#00ff9d]">
            {block.transactions ? block.transactions.length : 0}
          </span>
        </div>
      </div>
    </motion.div>
  );
}
