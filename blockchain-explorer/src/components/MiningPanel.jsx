import { useEffect, useState } from 'react';
import { Pickaxe, Loader2, Award } from 'lucide-react';
import { motion } from 'framer-motion';
import { blockchainApi } from '../services/api';
import toast from 'react-hot-toast';

export default function MiningPanel({ onSuccess, minimal }) {
  const [isMining, setIsMining] = useState(false);
  const [lastMined, setLastMined] = useState(null);
  const [cooldownRemaining, setCooldownRemaining] = useState(0);

  useEffect(() => {
    if (cooldownRemaining <= 0) return undefined;
    const timer = window.setInterval(() => {
      setCooldownRemaining((current) => Math.max(0, current - 1));
    }, 1000);
    return () => window.clearInterval(timer);
  }, [cooldownRemaining]);

  const handleMine = async () => {
    setIsMining(true);
    try {
      const result = await blockchainApi.mineBlock();
      setLastMined(result);
      const reward = Number(result?.reward || 50);
      toast.success(`${reward} PCN mined successfully`);
      setCooldownRemaining(Number(result?.cooldownSeconds || 30));
      if (onSuccess) onSuccess();
    } catch (err) {
      const remaining = Number(err.response?.data?.cooldownRemainingSeconds || 0);
      if (remaining > 0) {
        setCooldownRemaining(remaining);
      }
      const msg = err.response?.data?.error || err.response?.data?.message || 'Mining failed';
      toast.error(msg);
    } finally {
      setIsMining(false);
    }
  };

  return (
    <div className="absolute inset-0 flex flex-col items-center justify-center p-6 bg-gradient-to-br from-[#1f2937]/50 to-transparent">
      
      {!isMining && !lastMined && (
        <div className="text-center">
            <div className="bg-[#0b0f19] p-4 rounded-full inline-block mb-4 shadow-lg border border-[#00ff9d]/20">
                <Pickaxe className="w-8 h-8 text-[#00ff9d]" />
            </div>
            <h4 className="text-white font-bold text-lg mb-2">Start Mining</h4>
            <p className="text-gray-400 text-xs mb-6 px-4">Contribute hash power to secure the network.</p>
            <button 
                onClick={handleMine}
              disabled={cooldownRemaining > 0}
                className="btn-primary w-full shadow-[0_0_20px_rgba(0,255,157,0.3)]"
            >
              {cooldownRemaining > 0 ? `Cooldown ${cooldownRemaining}s` : 'Start Node'}
            </button>
        </div>
      )}

      {isMining && (
        <div className="flex flex-col items-center gap-4">
            <div className="relative">
                <div className="absolute inset-0 bg-[#00ff9d] blur-xl opacity-20 animate-pulse"></div>
                <Loader2 className="w-12 h-12 text-[#00ff9d] animate-spin relative z-10" />
            </div>
            <p className="text-[#00ff9d] font-mono text-sm tracking-widest animate-pulse">HASHING...</p>
        </div>
      )}

      {lastMined && !isMining && (
        <div className="text-center w-full">
            <div className="bg-[#00ff9d]/10 p-4 rounded-full inline-block mb-4 border border-[#00ff9d]/30">
                <Award className="w-8 h-8 text-[#00ff9d]" />
            </div>
            <h4 className="text-white font-bold mb-1">Block Found!</h4>
            <div className="bg-[#0b0f19] p-3 rounded-lg border border-gray-800 mb-4 overflow-hidden">
                <p className="text-[10px] text-gray-400 font-mono truncate">{lastMined.block?.hash || '00000abc...'}</p>
            </div>
            <button 
                onClick={handleMine}
              disabled={cooldownRemaining > 0}
                className="btn-secondary w-full text-xs"
            >
              {cooldownRemaining > 0 ? `Cooldown ${cooldownRemaining}s` : 'Mine Another Block'}
            </button>
        </div>
      )}

    </div>
  );
}
