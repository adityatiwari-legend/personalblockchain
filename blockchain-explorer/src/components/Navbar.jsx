import { Activity, RadioReceiver } from 'lucide-react';

export default function Navbar({ isReachable }) {
  return (
    <nav className="glass-card sticky top-0 z-50 rounded-none border-t-0 border-x-0 px-6 py-4 flex items-center justify-between">
      <div className="flex items-center gap-3">
        <div className="w-10 h-10 rounded-xl bg-gradient-to-br from-[#00ff9d] to-[#00b8ff] flex items-center justify-center shadow-[0_0_15px_rgba(0,255,157,0.4)]">
          <Activity className="text-[#0b0f19] w-6 h-6" />
        </div>
        <div>
          <h1 className="text-xl font-bold text-white tracking-wide">
            NEXUS<span className="text-[#00ff9d]">CHAIN</span>
          </h1>
          <p className="text-xs text-gray-400 font-medium tracking-widest">ENTERPRISE EXPLORER</p>
        </div>
      </div>

      <div className="hidden md:flex items-center gap-8">
        <a href="#" className="text-sm font-semibold text-[#00ff9d] border-b-2 border-[#00ff9d] pb-1">Dashboard</a>
        <a href="#explorer" className="text-sm font-medium text-gray-400 hover:text-white transition-colors">Explorer</a>
        <a href="#network" className="text-sm font-medium text-gray-400 hover:text-white transition-colors">Network</a>
        <a href="#transact" className="text-sm font-medium text-gray-400 hover:text-white transition-colors">Transact</a>
      </div>

      <div className="flex items-center gap-4">
        <div className="flex items-center gap-2 bg-[#0f172a] px-4 py-2 rounded-full border border-gray-800">
          <RadioReceiver className="w-4 h-4 text-gray-400" />
          <span className="text-xs font-mono text-gray-300">PORT: 5000</span>
        </div>
        <div className={`flex items-center gap-2 px-4 py-2 rounded-full border ${isReachable ? 'bg-[#00ff9d11] border-[#00ff9d44]' : 'bg-red-500/10 border-red-500/30'}`}>
          <div className={`w-2 h-2 rounded-full ${isReachable ? 'bg-[#00ff9d] shadow-[0_0_8px_#00ff9d]' : 'bg-red-500 shadow-[0_0_8px_red]'}`}></div>
          <span className={`text-xs font-bold ${isReachable ? 'text-[#00ff9d]' : 'text-red-500'}`}>
            {isReachable ? 'NODE ONLINE' : 'NODE OFFLINE'}
          </span>
        </div>
      </div>
    </nav>
  );
}
