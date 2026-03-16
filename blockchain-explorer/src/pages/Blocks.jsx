import { useMemo, useState } from 'react';
import { useQuery } from '@tanstack/react-query';
import ExplorerLayout from '../components/ExplorerLayout';
import BlockTable from '../components/BlockTable';
import BlockModal from '../components/BlockModal';
import { blockchainApi } from '../services/api';
import { normalizeChain } from '../services/explorerData';

export default function Blocks() {
  const [selectedBlock, setSelectedBlock] = useState(null);

  const { data, isLoading, isError, error, refetch, isFetching } = useQuery({
    queryKey: ['chain'],
    queryFn: blockchainApi.getChain,
    refetchInterval: 5000,
    retry: 2,
  });

  const blocks = useMemo(() => normalizeChain(data || []).slice().reverse(), [data]);

  return (
    <ExplorerLayout section="Blocks">
      <div className="space-y-6">
        <div className="flex items-center justify-between">
          <div>
            <h2 className="text-2xl font-bold text-white">Block Explorer</h2>
            <p className="text-sm text-gray-400">Browse all blocks in the chain.</p>
          </div>
          <button type="button" className="btn-secondary" onClick={() => refetch()} disabled={isFetching}>
            {isFetching ? 'Refreshing...' : 'Refresh'}
          </button>
        </div>

        {isLoading && (
          <div className="dashboard-card text-gray-400">Loading chain data...</div>
        )}

        {isError && (
          <div className="dashboard-card border-red-500/30 bg-red-500/5">
            <p className="text-red-300 text-sm">Failed to load blocks: {error?.message || 'Unknown error'}</p>
            <button type="button" className="btn-secondary mt-4" onClick={() => refetch()}>
              Retry
            </button>
          </div>
        )}

        {!isLoading && !isError && (
          <BlockTable blocks={blocks} onBlockClick={setSelectedBlock} />
        )}

        {selectedBlock && (
          <BlockModal block={selectedBlock} onClose={() => setSelectedBlock(null)} />
        )}
      </div>
    </ExplorerLayout>
  );
}
