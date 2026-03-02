import { useMemo, useEffect, useState } from 'react';
import ForceGraph2D from 'react-force-graph-2d';
import { Network } from 'lucide-react';
import { motion } from 'framer-motion';

export default function NetworkGraph({ peers }) {
  const [dimensions, setDimensions] = useState({ width: 0, height: 300 });

  useEffect(() => {
    // Basic resize observer
    const container = document.getElementById('graph-container');
    if (container) {
      setDimensions({ width: container.offsetWidth, height: 300 });
      const handleResize = () => setDimensions({ width: container.offsetWidth, height: 300 });
      window.addEventListener('resize', handleResize);
      return () => window.removeEventListener('resize', handleResize);
    }
  }, []);

  const graphData = useMemo(() => {
    const nodes = [{ id: 'localhost:5000', label: 'Local Node (5000)', color: '#00ff9d', val: 5 }];
    const links = [];

    if (Array.isArray(peers)) {
      peers.forEach(peer => {
        nodes.push({ id: peer, label: peer, color: '#00b8ff', val: 3 });
        links.push({ source: 'localhost:5000', target: peer });
      });
    }

    return { nodes, links };
  }, [peers]);

  return (
    <motion.div 
      initial={{ opacity: 0, y: 20 }}
      animate={{ opacity: 1, y: 0 }}
      transition={{ delay: 0.2 }}
      className="glass-card relative overflow-hidden flex flex-col"
      id="network"
    >
      <div className="absolute top-0 left-0 p-6 z-10 w-full pointer-events-none border-b border-gray-800 bg-[#0f172a]/50 backdrop-blur-sm">
        <div className="flex items-center gap-3">
          <div className="p-2 bg-purple-500/10 rounded-lg text-purple-400">
            <Network className="w-5 h-5" />
          </div>
          <h2 className="text-xl font-bold text-white">Network Topology</h2>
        </div>
      </div>
      
      <div id="graph-container" className="flex-grow min-h-[300px] w-full pt-20 bg-[#0b0f19]/50">
        {dimensions.width > 0 && (
          <ForceGraph2D
            width={dimensions.width}
            height={dimensions.height}
            graphData={graphData}
            nodeLabel="label"
            nodeColor="color"
            nodeRelSize={6}
            linkColor={() => 'rgba(255,255,255,0.1)'}
            linkWidth={1.5}
            backgroundColor="transparent"
            d3AlphaDecay={0.01}
            d3VelocityDecay={0.08}
          />
        )}
      </div>
      
      <div className="absolute bottom-4 right-4 flex gap-4 text-xs font-mono z-10 pointer-events-none">
        <div className="flex items-center gap-2">
          <span className="w-3 h-3 rounded-full bg-[#00ff9d] shadow-[0_0_8px_#00ff9d]"></span>
          <span className="text-gray-400">Local Node</span>
        </div>
        <div className="flex items-center gap-2">
          <span className="w-3 h-3 rounded-full bg-[#00b8ff] shadow-[0_0_8px_#00b8ff]"></span>
          <span className="text-gray-400">Peers ({peers?.length || 0})</span>
        </div>
      </div>
    </motion.div>
  );
}
