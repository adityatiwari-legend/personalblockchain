import { memo, useMemo, useState } from 'react';
import { formatTimestamp, shorten } from '../services/explorerData';

const PAGE_SIZE = 20;
const VIRTUAL_ROW_HEIGHT = 56;
const VIRTUAL_VIEWPORT_HEIGHT = 520;

function TransactionTableBase({
  transactions,
  mode = 'wallet',
  onRowClick,
  enableVirtualization = false,
}) {
  const [page, setPage] = useState(1);
  const [scrollTop, setScrollTop] = useState(0);

  const list = transactions || [];
  const totalPages = Math.max(1, Math.ceil(list.length / PAGE_SIZE));

  const pageData = useMemo(() => {
    if (mode !== 'explorer') return list;
    const start = (page - 1) * PAGE_SIZE;
    return list.slice(start, start + PAGE_SIZE);
  }, [list, mode, page]);

  const virtualRows = useMemo(() => {
    if (!enableVirtualization || mode !== 'explorer') {
      return {
        rows: pageData,
        offsetTop: 0,
        containerHeight: pageData.length * VIRTUAL_ROW_HEIGHT,
      };
    }

    const totalRows = pageData.length;
    const startIndex = Math.max(0, Math.floor(scrollTop / VIRTUAL_ROW_HEIGHT) - 4);
    const visibleCount = Math.ceil(VIRTUAL_VIEWPORT_HEIGHT / VIRTUAL_ROW_HEIGHT) + 8;
    const endIndex = Math.min(totalRows, startIndex + visibleCount);
    const rows = pageData.slice(startIndex, endIndex);

    return {
      rows,
      offsetTop: startIndex * VIRTUAL_ROW_HEIGHT,
      containerHeight: totalRows * VIRTUAL_ROW_HEIGHT,
    };
  }, [enableVirtualization, mode, pageData, scrollTop]);

  const rowsToRender = mode === 'explorer' ? virtualRows.rows : pageData;

  return (
    <div className="dashboard-card">
      <p className="text-xs uppercase tracking-[0.2em] text-cyan-300/80 mb-4">
        {mode === 'explorer' ? 'Transaction Explorer' : 'Transaction History'}
      </p>

      <div
        className="overflow-x-auto"
        style={mode === 'explorer' && enableVirtualization ? { maxHeight: `${VIRTUAL_VIEWPORT_HEIGHT}px`, overflowY: 'auto' } : undefined}
        onScroll={mode === 'explorer' && enableVirtualization ? (e) => setScrollTop(e.currentTarget.scrollTop) : undefined}
      >
        <table className="w-full text-left">
          <thead>
            <tr className="text-xs text-zinc-400 border-b border-zinc-800">
              {mode === 'explorer' ? (
                <>
                  <th className="py-2">Transaction ID</th>
                  <th className="py-2">Sender</th>
                  <th className="py-2">Receiver</th>
                  <th className="py-2">Amount</th>
                  <th className="py-2">Timestamp</th>
                  <th className="py-2">Block Height</th>
                  <th className="py-2">Status</th>
                </>
              ) : (
                <>
                  <th className="py-2">Type</th>
                  <th className="py-2">Amount</th>
                  <th className="py-2">Counterparty</th>
                  <th className="py-2">Time</th>
                </>
              )}
            </tr>
          </thead>
          <tbody style={mode === 'explorer' && enableVirtualization ? { position: 'relative', display: 'block', height: `${virtualRows.containerHeight}px` } : undefined}>
            {mode === 'explorer' && enableVirtualization && (
              <tr style={{ height: `${virtualRows.offsetTop}px`, display: 'block' }} />
            )}

            {rowsToRender.map((tx) => {
              const txId = tx.txID || tx.id;
              const sender = tx.sender || tx.fromAddress || tx.from || tx.senderAddress || '';
              const receiver = tx.receiver || tx.toAddress || tx.to || tx.receiverAddress || '';
              const amount = Number(tx.amount || 0);
              const status = tx.status || 'confirmed';
              const direction = tx.direction || tx.type || 'received';

              return mode === 'explorer' ? (
                <tr
                  key={txId}
                  onClick={() => onRowClick?.(tx)}
                  className="border-b border-zinc-900/70 text-sm cursor-pointer hover:bg-[#1f2937]/40"
                  style={enableVirtualization ? { display: 'table', width: '100%', tableLayout: 'fixed', height: `${VIRTUAL_ROW_HEIGHT}px` } : undefined}
                >
                  <td className="py-3 font-mono text-xs text-zinc-300">{shorten(txId)}</td>
                  <td className="py-3 font-mono text-xs text-zinc-400">{shorten(sender)}</td>
                  <td className="py-3 font-mono text-xs text-zinc-400">{shorten(receiver)}</td>
                  <td className="py-3 text-zinc-200">{amount.toLocaleString()}</td>
                  <td className="py-3 text-zinc-400">{formatTimestamp(tx.timestamp)}</td>
                  <td className="py-3 text-zinc-300">{tx.blockHeight ?? 'Pending'}</td>
                  <td className="py-3">
                    <span className="text-xs font-bold text-[#00ff9d] bg-[#00ff9d]/10 px-2 py-1 rounded capitalize">{status}</span>
                  </td>
                </tr>
              ) : (
                <tr key={txId} className="border-b border-zinc-900/70 text-sm" onClick={() => onRowClick?.(tx)}>
                  <td className="py-3 text-zinc-200 capitalize">{direction}</td>
                  <td className={`py-3 font-semibold ${direction === 'sent' ? 'text-rose-300' : 'text-emerald-300'}`}>
                    {direction === 'sent' ? '-' : '+'}
                    {amount}
                  </td>
                  <td className="py-3 font-mono text-xs text-zinc-400">
                    {direction === 'sent' ? receiver : sender}
                  </td>
                  <td className="py-3 text-zinc-400">{formatTimestamp(tx.timestamp)}</td>
                </tr>
              );
            })}

            {list.length === 0 && (
              <tr>
                <td colSpan={mode === 'explorer' ? 7 : 4} className="py-6 text-center text-zinc-500">
                  No transactions yet
                </td>
              </tr>
            )}

            {mode === 'explorer' && enableVirtualization && (
              <tr
                style={{
                  height: `${Math.max(0, virtualRows.containerHeight - virtualRows.offsetTop - rowsToRender.length * VIRTUAL_ROW_HEIGHT)}px`,
                  display: 'block',
                }}
              />
            )}
          </tbody>
        </table>
      </div>

      {mode === 'explorer' && list.length > PAGE_SIZE && (
        <div className="flex items-center justify-between mt-5">
          <p className="text-xs text-gray-400">
            Page {page} of {totalPages}
          </p>
          <div className="flex gap-2">
            <button type="button" className="btn-secondary !py-2" onClick={() => setPage((p) => Math.max(1, p - 1))} disabled={page === 1}>
              Prev
            </button>
            <button type="button" className="btn-secondary !py-2" onClick={() => setPage((p) => Math.min(totalPages, p + 1))} disabled={page === totalPages}>
              Next
            </button>
          </div>
        </div>
      )}
    </div>
  );
}

const TransactionTable = memo(TransactionTableBase);

export default TransactionTable;
