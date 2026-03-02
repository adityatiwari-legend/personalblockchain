import { useState } from 'react';
import { ArrowDown, Coins, User, Send, Loader2 } from 'lucide-react';
import { blockchainApi } from '../services/api';
import toast from 'react-hot-toast';

export default function TransactionForm({ onSuccess }) {
  const [formData, setFormData] = useState({
    sender: '',
    receiver: '',
    amount: ''
  });
  const [isSubmitting, setIsSubmitting] = useState(false);

  const handleSubmit = async (e) => {
    e.preventDefault();
    if (!formData.sender || !formData.receiver || !formData.amount) {
      toast.error('All fields are required');
      return;
    }
    
    setIsSubmitting(true);
    try {
      await blockchainApi.addTransaction(
        formData.sender,
        formData.receiver,
        formData.amount
      );
      toast.success('Transaction Broadcasted!');
      setFormData({ sender: '', receiver: '', amount: '' });
      if (onSuccess) onSuccess();
    } catch (err) {
      toast.error('Failed to submit');
    } finally {
      setIsSubmitting(false);
    }
  };

  return (
    <form onSubmit={handleSubmit} className="flex flex-col gap-4">
      {/* Sender Input */}
      <div className="bg-[#0b0f19] p-4 rounded-2xl border border-gray-800 relative group focus-within:border-[#00ff9d]/50 transition-colors">
        <label className="text-xs text-gray-500 font-medium mb-2 block">From (Public Key)</label>
        <div className="flex justify-between items-center">
          <input 
            type="text" 
            placeholder="0x..." 
            value={formData.sender}
            onChange={e => setFormData({...formData, sender: e.target.value})}
            className="bg-transparent text-white font-mono text-sm w-full outline-none placeholder-gray-700"
          />
          <User className="w-5 h-5 text-gray-600 group-focus-within:text-[#00ff9d]" />
        </div>
      </div>

      {/* Accessor Icon */}
      <div className="flex justify-center -my-2 z-10">
        <div className="bg-[#1f2937] p-2 rounded-xl border border-gray-700 shadow-xl">
          <ArrowDown className="w-4 h-4 text-gray-400" />
        </div>
      </div>

      {/* Receiver Input */}
      <div className="bg-[#0b0f19] p-4 rounded-2xl border border-gray-800 relative group focus-within:border-[#00ff9d]/50 transition-colors">
        <label className="text-xs text-gray-500 font-medium mb-2 block">To (Public Key)</label>
        <div className="flex justify-between items-center">
          <input 
            type="text" 
            placeholder="0x..." 
            value={formData.receiver}
            onChange={e => setFormData({...formData, receiver: e.target.value})}
            className="bg-transparent text-white font-mono text-sm w-full outline-none placeholder-gray-700"
          />
          <User className="w-5 h-5 text-gray-600 group-focus-within:text-[#00ff9d]" />
        </div>
      </div>

      {/* Amount Input */}
      <div className="bg-[#0b0f19] p-4 rounded-2xl border border-gray-800 relative group focus-within:border-[#00ff9d]/50 transition-colors mt-2">
        <label className="text-xs text-gray-500 font-medium mb-2 block">Amount (NEX)</label>
        <div className="flex justify-between items-center">
           <input 
            type="number" 
            placeholder="0.00" 
            value={formData.amount}
            onChange={e => setFormData({...formData, amount: e.target.value})}
            className="bg-transparent text-white font-bold text-lg w-full outline-none placeholder-gray-700"
          />
          <div className="bg-[#1f2937] px-3 py-1.5 rounded-lg text-xs font-bold text-white flex items-center gap-2">
            <Coins className="w-3 h-3 text-[#00ff9d]" /> NEX
          </div>
        </div>
      </div>

      <button 
        type="submit" 
        disabled={isSubmitting}
        className="mt-2 w-full bg-[#1f2937] hover:bg-[#00ff9d] hover:text-black hover:font-bold text-white font-medium py-4 rounded-2xl transition-all flex items-center justify-center gap-2 border border-gray-700 hover:border-[#00ff9d] disabled:opacity-50"
      >
        {isSubmitting ? <Loader2 className="animate-spin w-5 h-5" /> : 'Visualize Swap >'}
      </button>
    </form>
  );
}
