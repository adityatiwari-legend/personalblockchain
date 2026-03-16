import { LayoutDashboard, Wallet, BarChart3, ArrowRightLeft, User, Settings, HelpCircle, Sparkles } from 'lucide-react';
import { NavLink, useLocation } from 'react-router-dom';

export default function Sidebar() {
  const location = useLocation();

  const menuItems = [
    { icon: LayoutDashboard, label: 'Dashboard', path: '/dashboard' },
    { icon: Wallet, label: 'Assets', path: '/assets' },
    { icon: BarChart3, label: 'Market', path: '/blocks', badge: 'New' },
    { icon: ArrowRightLeft, label: 'Trade', path: '/transactions' },
  ];

  const bottomItems = [
    { icon: User, label: 'Profile', path: '/profile' },
    { icon: Settings, label: 'Settings', path: '/settings' },
    { icon: HelpCircle, label: 'Support', path: '/transactions' },
  ];

  const isActive = (path) => location.pathname === path;

  return (
    <aside className="fixed left-0 top-0 h-screen w-64 bg-[#0b0f19] border-r border-gray-800 flex flex-col p-6 z-50">
      {/* Logo */}
      <div className="flex items-center gap-3 mb-10">
        <div className="w-8 h-8 rounded-lg bg-[#00ff9d] flex items-center justify-center">
          <Sparkles className="w-5 h-5 text-black fill-current" />
        </div>
        <span className="text-xl font-bold text-white tracking-tight">Cryptix</span>
      </div>

      {/* Main Menu */}
      <div className="space-y-2 flex-1">
        {menuItems.map((item, index) => (
          <NavLink
            key={index}
            to={item.path}
            className={`flex items-center justify-between px-4 py-3 rounded-xl cursor-pointer transition-all ${
              isActive(item.path)
                ? 'bg-[#1f2937] text-white' 
                : 'text-gray-400 hover:text-white hover:bg-[#1f2937]/50'
            }`}
          >
            <div className="flex items-center gap-3">
              <item.icon className="w-5 h-5" />
              <span className="text-sm font-medium">{item.label}</span>
            </div>
            {item.badge && (
              <span className="text-[10px] font-bold bg-[#1f2937] border border-gray-700 text-gray-300 px-2 py-0.5 rounded">
                {item.badge}
              </span>
            )}
          </NavLink>
        ))}
      </div>

      {/* Bottom Menu */}
      <div className="space-y-2 pt-6 border-t border-gray-800">
        {bottomItems.map((item, index) => (
          <NavLink
            key={index}
            to={item.path}
            className={`flex items-center gap-3 px-4 py-3 rounded-xl cursor-pointer transition-all ${
              isActive(item.path)
                ? 'bg-[#1f2937] text-white'
                : 'text-gray-400 hover:text-white hover:bg-[#1f2937]/50'
            }`}
          >
            <item.icon className="w-5 h-5" />
            <span className="text-sm font-medium">{item.label}</span>
          </NavLink>
        ))}
      </div>

      {/* Promo Card */}
      <div className="mt-6 p-4 rounded-2xl bg-gradient-to-br from-[#1f2937] to-[#111827] border border-gray-800 relative overflow-hidden group cursor-pointer hover:border-[#00ff9d]/30 transition-colors">
        <div className="flex items-center gap-2 mb-1">
          <Sparkles className="w-4 h-4 text-[#00ff9d]" />
          <span className="text-sm font-bold text-white">Unlock CryptoAI</span>
        </div>
        <p className="text-xs text-gray-400">Your personal crypto assistant</p>
      </div>
    </aside>
  );
}
