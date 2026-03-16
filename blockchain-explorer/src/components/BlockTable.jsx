import { memo, useMemo, useState } from 'react';
import { Box } from 'lucide-react';
import { formatTimestamp, shorten } from '../services/explorerData';

const PAGE_SIZE = 20;

function BlockTableBase({ blocks = [], onBlockClick }) {
  const [page, setPage] = useState(1);
  const totalPages = Math.max(1, Math.ceil(blocks.length / PAGE_SIZE));

  const pagedBlocks = useMemo(() => {
    const start = (page - 1) * PAGE_SIZE;
    return blocks.slice(start, start + PAGE_SIZE);
  }, [blocks, page]);

  const goNext = () => setPage((prev) => Math.min(totalPages, prev + 1));
  const goPrev = () => setPage((prev) => Math.max(1, prev - 1));

  return (
    <div className="dashboard-card">
      <div className="overflow-x-auto">
        <table className="w-full text-left border-collapse">
          <thead>
            <tr className="text-xs text-gray-500 border-b border-gray-800">
              <th className="pb-3 pl-2 font-medium">Block Height</th>
              <th className="pb-3 font-medium">Hash</th>
              <th className="pb-3 font-medium hidden lg:table-cell">Previous Hash</th>
              <th className="pb-3 font-medium">Timestamp</th>
              <th className="pb-3 font-medium">Tx Count</th>
              <th className="pb-3 font-medium">Difficulty</th>
              <th className="pb-3 pr-2 text-right font-medium">Nonce</th>
            </tr>
          </thead>
          <tbody className="text-sm">
            {pagedBlocks.map((block) => (
              <tr
                key={`${block.index}-${block.hash}`}
                className="group hover:bg-[#1f2937]/30 transition-colors border-b border-gray-800/50 last:border-0 cursor-pointer"
                onClick={() => onBlockClick?.(block)}
              >
                <td className="py-4 pl-2 font-bold text-white flex items-center gap-3">
                  <div className="w-8 h-8 rounded-full bg-[#1f2937] flex items-center justify-center text-gray-400 border border-gray-700">
                    <Box className="w-4 h-4" />
                  </div>
                  #{block.index}
                </td>
                <td className="py-4 font-mono text-gray-300">{shorten(block.hash)}</td>
                <td className="py-4 font-mono text-gray-400 hidden lg:table-cell">{shorten(block.previousHash)}</td>
                <td className="py-4 text-gray-300">{formatTimestamp(block.timestamp)}</td>
                <td className="py-4 text-white font-medium">{block.transactionCount}</td>
                <td className="py-4 text-gray-300">{block.difficulty}</td>
                <td className="py-4 pr-2 text-right text-gray-300">{block.nonce}</td>
              </tr>
            ))}

            {pagedBlocks.length === 0 && (
              <tr>
                <td colSpan={7} className="py-8 text-center text-gray-500">
                  No blocks available.
                </td>
              </tr>
            )}
          </tbody>
        </table>
      </div>

      {blocks.length > PAGE_SIZE && (
        <div className="flex items-center justify-between mt-5">
          <p className="text-xs text-gray-400">
            Page {page} of {totalPages}
          </p>
          <div className="flex items-center gap-2">
            <button type="button" className="btn-secondary !py-2" onClick={goPrev} disabled={page === 1}>
              Prev
            </button>
            <button type="button" className="btn-secondary !py-2" onClick={goNext} disabled={page === totalPages}>
              Next
            </button>
          </div>
        </div>
      )}
    </div>
  );
}

const BlockTable = memo(BlockTableBase);

export default BlockTable;
