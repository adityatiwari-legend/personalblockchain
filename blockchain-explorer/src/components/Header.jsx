import { Bell, Search, User, Cpu } from 'lucide-react';

export default function Header({ chainName = 'PersonalBlockchain' }) {
  return (
    <header className="flex items-center justify-between pb-6 border-b border-gray-800/50 mb-8 w-full sticky top-0 md:bg-[#0b0f19]/80 md:backdrop-blur-md z-40 px-8 pt-6">
      <div className="flex flex-col gap-1">
        <div className="flex items-center gap-2 text-xs font-medium text-gray-500">
          <Cpu className="w-3 h-3" />
          <span>Blockchain</span>
          <span className="text-gray-700">/</span>
          <span className="text-white">Dashboard</span>
        </div>
        <h1 className="text-2xl font-bold text-white tracking-tight">{chainName}</h1>
      </div>

      <div className="flex items-center gap-6">
        {/* Search */}
        <div className="relative group hidden md:block">
          <Search className="absolute left-3 top-1/2 -translate-y-1/2 w-4 h-4 text-gray-500 group-focus-within:text-[#00ff9d] transition-colors" />
          <input 
            type="text" 
            placeholder="Search..." 
            className="bg-[#1f2937]/50 border border-gray-800 rounded-xl pl-10 pr-4 py-2.5 text-sm text-gray-300 focus:outline-none focus:border-[#00ff9d]/50 focus:bg-[#1f2937] transition-all w-64"
          />
        </div>

        {/* Actions */}
        <div className="flex items-center gap-4">
          <button className="relative p-2.5 rounded-xl bg-[#1f2937]/50 border border-gray-800 hover:bg-[#1f2937] hover:border-gray-700 transition-all text-gray-400 hover:text-white">
            <Bell className="w-5 h-5" />
            <span className="absolute top-2.5 right-3 w-1.5 h-1.5 bg-[#00ff9d] rounded-full ring-2 ring-[#0b0f19]"></span>
          </button>
          
          <button className="flex items-center gap-3 pl-2 pr-4 py-1.5 rounded-xl bg-[#1f2937]/50 border border-gray-800 hover:bg-[#1f2937] transition-all">
            <div className="w-8 h-8 rounded-lg bg-gradient-to-br from-[#00ff9d] to-[#00b8ff] flex items-center justify-center">
              <User className="w-4 h-4 text-black" />
            </div>
            <div className="text-left hidden lg:block">
              <p className="text-xs font-bold text-white">Miner</p>
              <p className="text-[10px] text-gray-400">Pro Account</p>
            </div>
          </button>
        </div>
      </div>
    </header>
  );
}
