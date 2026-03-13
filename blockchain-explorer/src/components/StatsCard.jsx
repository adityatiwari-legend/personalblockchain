import { motion } from 'framer-motion';

export default function StatsCard({ title, value, icon: Icon, delay = 0 }) {
  return (
    <motion.div 
      initial={{ opacity: 0, y: 20 }}
      animate={{ opacity: 1, y: 0 }}
      transition={{ duration: 0.5, delay }}
      className="glass-card p-6 relative overflow-hidden group"
    >
      <div className="absolute -right-4 -top-4 w-24 h-24 bg-[#00ff9d] rounded-full blur-[50px] opacity-10 group-hover:opacity-20 transition-opacity"></div>
      <div className="flex justify-between items-start">
        <div>
          <p className="text-sm font-medium text-gray-400 mb-2 uppercase tracking-wider">{title}</p>
          <h3 className="text-3xl font-bold text-white text-glow font-mono">
            {value}
          </h3>
        </div>
        <div className="w-12 h-12 rounded-xl bg-gray-800/50 flex items-center justify-center border border-gray-700/50 group-hover:border-[#00ff9d]/30 transition-colors">
          <Icon className="w-6 h-6 text-[#00ff9d]" />
        </div>
      </div>
    </motion.div>
  );
}
