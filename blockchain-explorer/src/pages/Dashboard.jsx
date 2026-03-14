import { useState, useEffect, useMemo, useCallback, useRef } from 'react';
import { AreaChart, Area, XAxis, YAxis, Tooltip, ResponsiveContainer } from 'recharts';
import { ArrowUpRight, ArrowDownLeft, Wallet, Bitcoin, Activity, Box, Clock, Settings, ArrowDown, WifiOff } from 'lucide-react';
import { motion } from 'framer-motion';
import { Toaster } from 'react-hot-toast';

import { blockchainApi } from '../services/api';
import Sidebar from '../components/Sidebar';
import Header from '../components/Header';
import TransactionForm from '../components/TransactionForm';
import MiningPanel from '../components/MiningPanel';
import ForceGraph2D from 'react-force-graph-2d';

export default function Dashboard() {
  const [chain, setChain] = useState([]);
  const [peers, setPeers] = useState([]);
  const [chainName, setChainName] = useState('PersonalBlockchain');
  const [isLoading, setIsLoading] = useState(true);
  const [isConnected, setIsConnected] = useState(true);
  const fetchInFlight = useRef(false);

  const fetchData = useCallback(async () => {
    // Prevent overlapping requests
    if (fetchInFlight.current) return;
    fetchInFlight.current = true;

    try {
      const [chainResult, peersResult, healthResult] = await Promise.allSettled([
        blockchainApi.getChain(),
        blockchainApi.getPeers(),
        blockchainApi.getHealth()
      ]);

      const anyFulfilled = [chainResult, peersResult, healthResult].some(r => r.status === 'fulfilled');
      setIsConnected(anyFulfilled);

      if (chainResult.status === 'fulfilled') setChain(chainResult.value || []);
      if (peersResult.status === 'fulfilled') setPeers(peersResult.value || []);
      if (healthResult.status === 'fulfilled' && healthResult.value?.name) setChainName(healthResult.value.name);
      setIsLoading(false);
    } finally {
      fetchInFlight.current = false;
    }
  }, []);

  useEffect(() => {
    fetchData();
    const interval = setInterval(fetchData, 4000);
    return () => clearInterval(interval);
  }, [fetchData]);

  // Stats
  const currentHeight = chain.length;

  // Chart Data
  const chartData = useMemo(() => {
    return chain.slice(-20).map(b => ({
      name: b.index,
      transactions: b.transactions?.length || 0,
    }));
  }, [chain]);

  // Recent Blocks for Table
  const recentBlocks = [...chain].reverse().slice(0, 5);

  // Format timestamp — handles both ISO strings and Unix timestamps
  const formatTime = (ts) => {
    if (!ts) return 'N/A';
    // If it's a string (ISO 8601), parse directly
    if (typeof ts === 'string') return new Date(ts).toLocaleTimeString();
    // If it's a number (Unix epoch), convert
    return new Date(ts * 1000).toLocaleTimeString();
  };

  if (isLoading) {
    return (
      <div className="flex min-h-screen bg-[#0b0f19] text-gray-200 items-center justify-center">
        <div className="text-center">
          <div className="w-8 h-8 border-2 border-[#00ff9d] border-t-transparent rounded-full animate-spin mx-auto mb-4"></div>
          <p className="text-gray-400">Connecting to node...</p>
        </div>
      </div>
    );
  }

  return (
    <div className="flex min-h-screen bg-[#0b0f19] text-gray-200 font-sans selection:bg-[#00ff9d] selection:text-black overflow-hidden">
      <Toaster position="bottom-right" />

      {/* Connection lost banner */}
      {!isConnected && (
        <div className="fixed top-0 left-0 right-0 z-50 bg-red-900/90 text-white text-center py-2 text-sm flex items-center justify-center gap-2">
          <WifiOff className="w-4 h-4" />
          Backend unreachable — retrying...
        </div>
      )}

      {/* 1. Sidebar */}
      <Sidebar />

      {/* Main Content */}
      <div className="flex-1 ml-64 relative overflow-y-auto h-screen">
        <Header chainName={chainName} />

        <main className="px-8 pb-12 grid grid-cols-12 gap-6">

          {/* LEFT COLUMN (8 cols) */}
          <div className="col-span-12 xl:col-span-8 flex flex-col gap-6">

            {/* Main Chart Card */}
            <div className="dashboard-card relative overflow-hidden min-h-[400px] flex flex-col justify-between">
              <div className="flex justify-between items-start mb-6 z-10">
                <div>
                  <h2 className="text-gray-400 text-sm font-medium mb-1">Blockchain Height</h2>
                  <div className="flex items-baseline gap-3">
                    <span className="text-5xl font-bold text-white tracking-tight">{currentHeight.toLocaleString()}</span>
                    <span className="text-[#00ff9d] text-sm font-bold flex items-center bg-[#00ff9d]/10 px-2 py-1 rounded-lg">
                      +1 <ArrowUpRight className="w-3 h-3 ml-1" />
                    </span>
                  </div>
                </div>
                <div className="flex gap-2">
                  {['1H', '1D', '1W', '1M', '1Y'].map(period => (
                    <button key={period} className={`text-xs font-bold px-3 py-1.5 rounded-lg transition-colors ${period === '1Y' ? 'bg-[#1f2937] text-white border border-gray-700' : 'text-gray-500 hover:text-white'}`}>
                      {period}
                    </button>
                  ))}
                </div>
              </div>

              {/* Area Chart */}
              <div className="absolute inset-x-0 bottom-0 h-[280px]">
                <ResponsiveContainer width="100%" height="100%">
                  <AreaChart data={chartData}>
                    <defs>
                      <linearGradient id="colorTx" x1="0" y1="0" x2="0" y2="1">
                        <stop offset="5%" stopColor="#fff" stopOpacity={0.05}/>
                        <stop offset="95%" stopColor="#fff" stopOpacity={0}/>
                      </linearGradient>
                    </defs>
                    <XAxis dataKey="name" hide />
                    <YAxis hide domain={['auto', 'auto']} />
                    <Tooltip
                      contentStyle={{ backgroundColor: '#111827', borderColor: '#374151', borderRadius: '12px', color: '#fff' }}
                      itemStyle={{ color: '#fff' }}
                    />
                    <Area
                      type="monotone"
                      dataKey="transactions"
                      stroke="#9ca3af"
                      strokeWidth={2}
                      fillOpacity={1}
                      fill="url(#colorTx)"
                    />
                  </AreaChart>
                </ResponsiveContainer>
              </div>
            </div>

            {/* Assets (Blocks) Table */}
            <div className="dashboard-card">
              <div className="flex justify-between items-center mb-6">
                <h3 className="text-lg font-bold text-white">Latest Blocks</h3>
                <button className="text-xs font-bold text-[#00ff9d] hover:underline">View All</button>
              </div>

              <div className="overflow-x-auto">
                <table className="w-full text-left border-collapse">
                  <thead>
                    <tr className="text-xs text-gray-500 border-b border-gray-800">
                      <th className="pb-3 pl-2 font-medium">Block Height</th>
                      <th className="pb-3 font-medium hidden md:table-cell">Hash</th>
                      <th className="pb-3 font-medium">Time Mined</th>
                      <th className="pb-3 font-medium">Tx Count</th>
                      <th className="pb-3 pr-2 text-right font-medium">Status</th>
                    </tr>
                  </thead>
                  <tbody className="text-sm">
                    {recentBlocks.map((block) => (
                      <tr key={block.index} className="group hover:bg-[#1f2937]/30 transition-colors border-b border-gray-800/50 last:border-0">
                        <td className="py-4 pl-2 font-bold text-white flex items-center gap-3">
                          <div className="w-8 h-8 rounded-full bg-[#1f2937] flex items-center justify-center text-gray-400 border border-gray-700">
                            <Box className="w-4 h-4" />
                          </div>
                          #{block.index}
                        </td>
                        <td className="py-4 font-mono text-gray-400 hidden md:table-cell">
                          {block.hash ? `${block.hash.substring(0, 8)}...` : 'Genesis'}
                        </td>
                        <td className="py-4 text-gray-300">
                          {formatTime(block.timestamp)}
                        </td>
                        <td className="py-4 text-white font-medium pl-4">
                          {block.transactions?.length || 0}
                        </td>
                        <td className="py-4 pr-2 text-right">
                          <span className="text-xs font-bold text-[#00ff9d] bg-[#00ff9d]/10 px-2 py-1 rounded">Confirmed</span>
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            </div>

            {/* Recent Transactions Stub */}
            <div className="flex gap-6">
                 <div className="flex-1 dashboard-card">
                    <h3 className="text-lg font-bold text-white mb-4">Recent Activity</h3>
                    <div className="space-y-4">
                        {[1, 2].map((_, i) => (
                            <div key={i} className="flex justify-between items-center p-3 rounded-xl hover:bg-[#1f2937] transition-colors cursor-pointer border border-transparent hover:border-gray-800">
                            <div className="flex items-center gap-3">
                                <div className="w-10 h-10 rounded-full bg-[#1f2937] flex items-center justify-center border border-gray-700">
                                <ArrowDown className="w-5 h-5 text-gray-400" />
                                </div>
                                <div>
                                <p className="font-bold text-white text-sm">Transfer</p>
                                <p className="text-xs text-gray-500">From: 0x82...3a1</p>
                                </div>
                            </div>
                            <div className="text-right">
                                <p className="font-bold text-white text-sm">2.45 NEX</p>
                                <p className="text-xs text-[#00ff9d]">+1.2%</p>
                            </div>
                            </div>
                        ))}
                    </div>
                 </div>

                 <div className="flex-1 dashboard-card bg-gradient-to-br from-[#1f2937] to-[#111827]">
                    <div className="h-full flex flex-col justify-center items-center text-center p-4">
                        <div className="w-12 h-12 bg-[#00ff9d] rounded-xl flex items-center justify-center mb-4 text-[#0b0f19]">
                            <Settings className="w-6 h-6 animate-spin-slow" />
                        </div>
                        <h4 className="font-bold text-white">Full Explorer</h4>
                        <p className="text-xs text-gray-400 mt-2">View extensive history in the dedicated explorer.</p>
                    </div>
                 </div>
            </div>

          </div>

          {/* RIGHT COLUMN (4 cols) */}
          <div className="col-span-12 xl:col-span-4 flex flex-col gap-6">

            {/* Quick Swap Form (Transaction) */}
            <div className="dashboard-card">
              <div className="flex justify-between items-center mb-6">
                <h3 className="text-lg font-bold text-white">Quick Transaction</h3>
                <Settings className="w-4 h-4 text-gray-500 cursor-pointer hover:text-white" />
              </div>

              <TransactionForm onSuccess={fetchData} />
            </div>

             {/* Repartition (Mining) */}
             <div className="dashboard-card flex flex-col h-[340px] p-0 overflow-hidden relative border border-[#1f2937]">
               {/* Header Inside */}
               <div className="absolute top-6 left-6 z-10">
                 <h3 className="text-lg font-bold text-white">Network Mining</h3>
               </div>
               <MiningPanel onSuccess={fetchData} minimal />
            </div>

            {/* Network Peers */}
            <div className="dashboard-card flex flex-col min-h-[300px] p-0 overflow-hidden relative border border-[#1f2937]">
                <div className="absolute top-6 left-6 z-10">
                    <h3 className="text-lg font-bold text-white">Network Topology</h3>
                </div>
                <div className="flex-1 bg-[#0b0f19]">
                {(typeof window !== 'undefined') && (
                    <ForceGraph2D
                    width={400}
                    height={300}
                    graphData={{
                        nodes: [{id: 'You', val: 10}, ...peers.map(p => ({id: p, val: 5}))],
                        links: peers.map(p => ({source: 'You', target: p}))
                    }}
                    nodeColor={() => '#4b5563'}
                    linkColor={() => '#374151'}
                    backgroundColor="#0b0f19"
                    />
                )}
                </div>
            </div>

          </div>

        </main>
      </div>
    </div>
  );
}
