import { useState } from 'react';
import { ArrowDown, Key, User, Send, Loader2, FileText } from 'lucide-react';
import { blockchainApi } from '../services/api';
import toast from 'react-hot-toast';

export default function TransactionForm({ onSuccess }) {
  const [formData, setFormData] = useState({
    senderPrivateKey: '',
    senderPublicKey: '',
    receiverPublicKey: '',
    payload: ''
  });
  const [isSubmitting, setIsSubmitting] = useState(false);

  const handleSubmit = async (e) => {
    e.preventDefault();
    if (!formData.senderPrivateKey || !formData.senderPublicKey || !formData.receiverPublicKey || !formData.payload) {
      toast.error('All fields are required');
      return;
    }

    setIsSubmitting(true);
    try {
      await blockchainApi.addTransaction(
        formData.senderPrivateKey,
        formData.senderPublicKey,
        formData.receiverPublicKey,
        formData.payload
      );
      toast.success('Transaction Broadcasted!');
      setFormData({ senderPrivateKey: '', senderPublicKey: '', receiverPublicKey: '', payload: '' });
      if (onSuccess) onSuccess();
    } catch (err) {
      const msg = err.response?.data?.error || err.response?.data?.message || 'Failed to submit';
      toast.error(msg);
    } finally {
      setIsSubmitting(false);
    }
  };

  return (
    <form onSubmit={handleSubmit} className="flex flex-col gap-4">
      {/* Sender Private Key Input */}
      <div className="bg-[#0b0f19] p-4 rounded-2xl border border-gray-800 relative group focus-within:border-[#00ff9d]/50 transition-colors">
        <label className="text-xs text-gray-500 font-medium mb-2 block">Sender Private Key</label>
        <div className="flex justify-between items-center">
          <input
            type="password"
            placeholder="Your private key..."
            value={formData.senderPrivateKey}
            onChange={e => setFormData({...formData, senderPrivateKey: e.target.value})}
            className="bg-transparent text-white font-mono text-sm w-full outline-none placeholder-gray-700"
          />
          <Key className="w-5 h-5 text-gray-600 group-focus-within:text-[#00ff9d]" />
        </div>
      </div>

      {/* Sender Public Key Input */}
      <div className="bg-[#0b0f19] p-4 rounded-2xl border border-gray-800 relative group focus-within:border-[#00ff9d]/50 transition-colors">
        <label className="text-xs text-gray-500 font-medium mb-2 block">Sender Public Key</label>
        <div className="flex justify-between items-center">
          <input
            type="text"
            placeholder="0x..."
            value={formData.senderPublicKey}
            onChange={e => setFormData({...formData, senderPublicKey: e.target.value})}
            className="bg-transparent text-white font-mono text-sm w-full outline-none placeholder-gray-700"
          />
          <User className="w-5 h-5 text-gray-600 group-focus-within:text-[#00ff9d]" />
        </div>
      </div>

      {/* Arrow Icon */}
      <div className="flex justify-center -my-2 z-10">
        <div className="bg-[#1f2937] p-2 rounded-xl border border-gray-700 shadow-xl">
          <ArrowDown className="w-4 h-4 text-gray-400" />
        </div>
      </div>

      {/* Receiver Public Key Input */}
      <div className="bg-[#0b0f19] p-4 rounded-2xl border border-gray-800 relative group focus-within:border-[#00ff9d]/50 transition-colors">
        <label className="text-xs text-gray-500 font-medium mb-2 block">Receiver Public Key</label>
        <div className="flex justify-between items-center">
          <input
            type="text"
            placeholder="0x..."
            value={formData.receiverPublicKey}
            onChange={e => setFormData({...formData, receiverPublicKey: e.target.value})}
            className="bg-transparent text-white font-mono text-sm w-full outline-none placeholder-gray-700"
          />
          <User className="w-5 h-5 text-gray-600 group-focus-within:text-[#00ff9d]" />
        </div>
      </div>

      {/* Payload Input */}
      <div className="bg-[#0b0f19] p-4 rounded-2xl border border-gray-800 relative group focus-within:border-[#00ff9d]/50 transition-colors mt-2">
        <label className="text-xs text-gray-500 font-medium mb-2 block">Payload</label>
        <div className="flex justify-between items-center">
           <input
            type="text"
            placeholder="Transaction data..."
            value={formData.payload}
            onChange={e => setFormData({...formData, payload: e.target.value})}
            className="bg-transparent text-white font-mono text-sm w-full outline-none placeholder-gray-700"
          />
          <FileText className="w-5 h-5 text-gray-600 group-focus-within:text-[#00ff9d]" />
        </div>
      </div>

      <button
        type="submit"
        disabled={isSubmitting}
        className="mt-2 w-full bg-[#1f2937] hover:bg-[#00ff9d] hover:text-black hover:font-bold text-white font-medium py-4 rounded-2xl transition-all flex items-center justify-center gap-2 border border-gray-700 hover:border-[#00ff9d] disabled:opacity-50"
      >
        {isSubmitting ? <Loader2 className="animate-spin w-5 h-5" /> : <><Send className="w-4 h-4" /> Send Transaction</>}
      </button>
    </form>
  );
}
