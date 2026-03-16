import Sidebar from './Sidebar';
import Header from './Header';

export default function ExplorerLayout({ children, chainName = 'PersonalBlockchain', section = 'Dashboard' }) {
  return (
    <div className="flex min-h-screen bg-[#0b0f19] text-gray-200 font-sans selection:bg-[#00ff9d] selection:text-black overflow-hidden">
      <Sidebar />
      <div className="flex-1 ml-64 relative overflow-y-auto h-screen">
        <Header chainName={chainName} section={section} />
        <main className="px-8 pb-12">{children}</main>
      </div>
    </div>
  );
}
