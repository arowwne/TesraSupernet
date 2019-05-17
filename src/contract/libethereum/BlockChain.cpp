/*
	This file is part of cpp-ethereum.

	cpp-ethereum is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	cpp-ethereum is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http:
*/
/** @file BlockChain.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "BlockChain.h"

#if ETH_PROFILING_GPERF
#include <gperftools/profiler.h>
#endif

#include <boost/timer.hpp>
#include <boost/filesystem.hpp>
#include <json_spirit/JsonSpiritHeaders.h>
#include <libdevcore/Common.h>
#include <libdevcore/Assertions.h>
#include <libdevcore/RLP.h>
#include <libdevcore/TrieHash.h>
#include <libdevcore/FileSystem.h>
#include <libethcore/Exceptions.h>
#include <libethcore/BlockHeader.h>
#include "GenesisInfo.h"
#include "State.h"
#include "Block.h"
#include "Defaults.h"
using namespace std;
using namespace dev;
using namespace dev::eth;
namespace js = json_spirit;
namespace fs = boost::filesystem;

#define ETH_CATCH 1
#define ETH_TIMED_IMPORTS 1

#if defined(_WIN32)
const char* BlockChainDebug::name() { return EthBlue "8" EthWhite " <>"; }
const char* BlockChainWarn::name() { return EthBlue "8" EthOnRed EthBlackBold " X"; }
const char* BlockChainNote::name() { return EthBlue "8" EthBlue " i"; }
const char* BlockChainChat::name() { return EthBlue "8" EthWhite " o"; }
#else
const char* BlockChainDebug::name() { return EthBlue "☍" EthWhite " ◇"; }
const char* BlockChainWarn::name() { return EthBlue "☍" EthOnRed EthBlackBold " ✘"; }
const char* BlockChainNote::name() { return EthBlue "☍" EthBlue " ℹ"; }
const char* BlockChainChat::name() { return EthBlue "☍" EthWhite " ◌"; }
#endif

std::ostream& dev::eth::operator<<(std::ostream& _out, BlockChain const& _bc)
{
	string cmp = toBigEndianString(_bc.currentHash());
	auto it = _bc.m_blocksDB->NewIterator(_bc.m_readOptions);
	for (it->SeekToFirst(); it->Valid(); it->Next())
		if (it->key().ToString() != "best")
		{
			try {
				BlockHeader d(bytesConstRef(it->value()));
				_out << toHex(it->key().ToString()) << ":   " << d.number() << " @ " << d.parentHash() << (cmp == it->key().ToString() ? "  BEST" : "") << std::endl;
			}
			catch (...) {
				cwarn << "Invalid DB entry:" << toHex(it->key().ToString()) << " -> " << toHex(bytesConstRef(it->value()));
			}
		}
	delete it;
	return _out;
}

ldb::Slice dev::eth::toSlice(h256 const& _h, unsigned _sub)
{
#if ALL_COMPILERS_ARE_CPP11_COMPLIANT
	static thread_local FixedHash<33> h = _h;
	h[32] = (uint8_t)_sub;
	return (ldb::Slice)h.ref();
#else
	static boost::thread_specific_ptr<FixedHash<33>> t_h;
	if (!t_h.get())
		t_h.reset(new FixedHash<33>);
	*t_h = FixedHash<33>(_h);
	(*t_h)[32] = (uint8_t)_sub;
	return (ldb::Slice)t_h->ref();
#endif 
}

ldb::Slice dev::eth::toSlice(uint64_t _n, unsigned _sub)
{
#if ALL_COMPILERS_ARE_CPP11_COMPLIANT
	static thread_local FixedHash<33> h;
	toBigEndian(_n, bytesRef(h.data() + 24, 8));
	h[32] = (uint8_t)_sub;
	return (ldb::Slice)h.ref();
#else
	static boost::thread_specific_ptr<FixedHash<33>> t_h;
	if (!t_h.get())
		t_h.reset(new FixedHash<33>);
	bytesRef ref(t_h->data() + 24, 8);
	toBigEndian(_n, ref);
	(*t_h)[32] = (uint8_t)_sub;
	return (ldb::Slice)t_h->ref();
#endif
}

namespace dev
{
class WriteBatchNoter: public ldb::WriteBatch::Handler
{
	virtual void Put(ldb::Slice const& _key, ldb::Slice const& _value) { cnote << "Put" << toHex(bytesConstRef(_key)) << "=>" << toHex(bytesConstRef(_value)); }
	virtual void Delete(ldb::Slice const& _key) { cnote << "Delete" << toHex(bytesConstRef(_key)); }
};
}

#if ETH_DEBUG&&0
static const chrono::system_clock::duration c_collectionDuration = chrono::seconds(15);
static const unsigned c_collectionQueueSize = 2;
static const unsigned c_maxCacheSize = 1024 * 1024 * 1;
static const unsigned c_minCacheSize = 1;
#else


static const chrono::system_clock::duration c_collectionDuration = chrono::seconds(60);


static const unsigned c_collectionQueueSize = 20;


static const unsigned c_maxCacheSize = 1024 * 1024 * 64;


static const unsigned c_minCacheSize = 1024 * 1024 * 32;

#endif

BlockChain::BlockChain(ChainParams const& _p, std::string const& _dbPath, WithExisting _we, ProgressCallback const& _pc):
	m_dbPath(_dbPath)
{
	init(_p);
	open(_dbPath, _we, _pc);
}

BlockChain::~BlockChain()
{
	close();
}

BlockHeader const& BlockChain::genesis() const
{
	UpgradableGuard l(x_genesis);
	if (!m_genesis)
	{
		auto gb = m_params.genesisBlock();
		UpgradeGuard ul(l);
		m_genesis = BlockHeader(gb);
		m_genesisHeaderBytes = BlockHeader::extractHeader(&gb).data().toBytes();
		m_genesisHash = m_genesis.hash();
	}
	return m_genesis;
}

void BlockChain::init(ChainParams const& _p)
{
	
	m_cacheUsage.resize(c_collectionQueueSize);
	m_lastCollection = chrono::system_clock::now();

	
	m_params = _p;
	m_sealEngine.reset(m_params.createSealEngine());
	m_genesis.clear();
	genesis();
}

unsigned BlockChain::open(std::string const& _path, WithExisting _we)
{
	string path = _path.empty() ? Defaults::get()->m_dbPath : _path;
	string chainPath = path + "/" + toHex(m_genesisHash.ref().cropped(0, 4));
	string extrasPath = chainPath + "/" + toString(c_databaseVersion);

	fs::create_directories(extrasPath);
	DEV_IGNORE_EXCEPTIONS(fs::permissions(extrasPath, fs::owner_all));

	bytes status = contents(extrasPath + "/minor");
	unsigned lastMinor = c_minorProtocolVersion;
	if (!status.empty())
		DEV_IGNORE_EXCEPTIONS(lastMinor = (unsigned)RLP(status));
	if (c_minorProtocolVersion != lastMinor)
	{
		cnote << "Killing extras database (DB minor version:" << lastMinor << " != our miner version: " << c_minorProtocolVersion << ").";
		DEV_IGNORE_EXCEPTIONS(boost::filesystem::remove_all(extrasPath + "/details.old"));
		boost::filesystem::rename(extrasPath + "/extras", extrasPath + "/extras.old");
		boost::filesystem::remove_all(extrasPath + "/state");
		writeFile(extrasPath + "/minor", rlp(c_minorProtocolVersion));
		lastMinor = (unsigned)RLP(status);
	}
	if (_we == WithExisting::Kill)
	{
		cnote << "Killing blockchain & extras database (WithExisting::Kill).";
		boost::filesystem::remove_all(chainPath + "/blocks");
		boost::filesystem::remove_all(extrasPath + "/extras");
	}

	ldb::Options o;
	o.create_if_missing = true;
	o.max_open_files = 256;
	ldb::DB::Open(o, chainPath + "/blocks", &m_blocksDB);
	ldb::DB::Open(o, extrasPath + "/extras", &m_extrasDB);
	if (!m_blocksDB || !m_extrasDB)
	{
		if (boost::filesystem::space(chainPath + "/blocks").available < 1024)
		{
			cwarn << "Not enough available space found on hard drive. Please free some up and then re-run. Bailing.";
			BOOST_THROW_EXCEPTION(NotEnoughAvailableSpace());
		}
		else
		{
			cwarn <<
				"Database " <<
				(chainPath + "/blocks") <<
				"or " <<
				(extrasPath + "/extras") <<
				"already open. You appear to have another instance of ethereum running. Bailing.";
			BOOST_THROW_EXCEPTION(DatabaseAlreadyOpen());
		}
	}



	if (_we != WithExisting::Verify && !details(m_genesisHash))
	{
		BlockHeader gb(m_params.genesisBlock());
		
		m_details[m_genesisHash] = BlockDetails(0, gb.difficulty(), h256(), {});
		auto r = m_details[m_genesisHash].rlp();
		m_extrasDB->Put(m_writeOptions, toSlice(m_genesisHash, ExtraDetails), (ldb::Slice)dev::ref(r));
		assert(isKnown(gb.hash()));
	}

#if ETH_PARANOIA
	checkConsistency();
#endif

	
	std::string l;
	m_extrasDB->Get(m_readOptions, ldb::Slice("best"), &l);
	m_lastBlockHash = l.empty() ? m_genesisHash : *(h256*)l.data();
	m_lastBlockNumber = number(m_lastBlockHash);

	ctrace << "Opened blockchain DB. Latest: " << currentHash() << (lastMinor == c_minorProtocolVersion ? "(rebuild not needed)" : "*** REBUILD NEEDED ***");
	return lastMinor;
}

void BlockChain::open(std::string const& _path, WithExisting _we, ProgressCallback const& _pc)
{
	if (open(_path, _we) != c_minorProtocolVersion || _we == WithExisting::Verify)
		rebuild(_path, _pc);
}

void BlockChain::reopen(ChainParams const& _p, WithExisting _we, ProgressCallback const& _pc)
{
	close();
	init(_p);
	open(m_dbPath, _we, _pc);
}

void BlockChain::close()
{
	ctrace << "Closing blockchain DB";
	
	delete m_extrasDB;
	delete m_blocksDB;
	m_lastBlockHash = m_genesisHash;
	m_lastBlockNumber = 0;
	m_details.clear();
	m_blocks.clear();
	m_logBlooms.clear();
	m_receipts.clear();
	m_transactionAddresses.clear();
	m_blockHashes.clear();
	m_blocksBlooms.clear();
	m_cacheUsage.clear();
	m_inUse.clear();
	m_lastLastHashes.clear();
}

void BlockChain::rebuild(std::string const& _path, std::function<void(unsigned, unsigned)> const& _progress)
{
	string path = _path.empty() ? Defaults::get()->m_dbPath : _path;
	string chainPath = path + "/" + toHex(m_genesisHash.ref().cropped(0, 4));
	string extrasPath = chainPath + "/" + toString(c_databaseVersion);

#if ETH_PROFILING_GPERF
	ProfilerStart("BlockChain_rebuild.log");
#endif

	unsigned originalNumber = m_lastBlockNumber;

	
	
	
	
	

	
	delete m_extrasDB;
	m_extrasDB = nullptr;
	boost::filesystem::rename(extrasPath + "/extras", extrasPath + "/extras.old");
	ldb::DB* oldExtrasDB;
	ldb::Options o;
	o.create_if_missing = true;
	ldb::DB::Open(o, extrasPath + "/extras.old", &oldExtrasDB);
	ldb::DB::Open(o, extrasPath + "/extras", &m_extrasDB);

	
	Block s = genesisBlock(State::openDB(path, m_genesisHash, WithExisting::Kill));

	
	m_details.clear();
	m_logBlooms.clear();
	m_receipts.clear();
	m_transactionAddresses.clear();
	m_blockHashes.clear();
	m_blocksBlooms.clear();
	m_lastLastHashes.clear();
	m_lastBlockHash = genesisHash();
	m_lastBlockNumber = 0;

	m_details[m_lastBlockHash].totalDifficulty = s.info().difficulty();

	m_extrasDB->Put(m_writeOptions, toSlice(m_lastBlockHash, ExtraDetails), (ldb::Slice)dev::ref(m_details[m_lastBlockHash].rlp()));

	h256 lastHash = m_lastBlockHash;
	Timer t;
	for (unsigned d = 1; d <= originalNumber; ++d)
	{
		if (!(d % 1000))
		{
			cerr << "\n1000 blocks in " << t.elapsed() << "s = " << (1000.0 / t.elapsed()) << "b/s" << endl;
			t.restart();
		}
		try
		{
			bytes b = block(queryExtras<BlockHash, uint64_t, ExtraBlockHash>(d, m_blockHashes, x_blockHashes, NullBlockHash, oldExtrasDB).value);

			BlockHeader bi(&b);

			if (bi.parentHash() != lastHash)
			{
				cwarn << "DISJOINT CHAIN DETECTED; " << bi.hash() << "#" << d << " -> parent is" << bi.parentHash() << "; expected" << lastHash << "#" << (d - 1);
				return;
			}
			lastHash = bi.hash();
			import(b, s.db(), 0);
		}
		catch (...)
		{
			
			break;
		}

		if (_progress)
			_progress(d, originalNumber);
	}

#if ETH_PROFILING_GPERF
	ProfilerStop();
#endif

	delete oldExtrasDB;
	boost::filesystem::remove_all(path + "/extras.old");
}

string BlockChain::dumpDatabase() const
{
	stringstream ss;

	ss << m_lastBlockHash << endl;
	ldb::Iterator* i = m_extrasDB->NewIterator(m_readOptions);
	for (i->SeekToFirst(); i->Valid(); i->Next())
		ss << toHex(bytesConstRef(i->key())) << "/" << toHex(bytesConstRef(i->value())) << endl;
	return ss.str();
}

LastHashes BlockChain::lastHashes(h256 const& _parent) const
{
	Guard l(x_lastLastHashes);
	if (m_lastLastHashes.empty() || m_lastLastHashes.back() != _parent)
	{
		m_lastLastHashes.resize(256);
		m_lastLastHashes[0] = _parent;
		for (unsigned i = 0; i < 255; ++i)
			m_lastLastHashes[i + 1] = m_lastLastHashes[i] ? info(m_lastLastHashes[i]).parentHash() : h256();
	}
	return m_lastLastHashes;
}

tuple<ImportRoute, bool, unsigned> BlockChain::sync(BlockQueue& _bq, OverlayDB const& _stateDB, unsigned _max)
{


	VerifiedBlocks blocks;
	_bq.drain(blocks, _max);

	h256s fresh;
	h256s dead;
	h256s badBlocks;
	Transactions goodTransactions;
	unsigned count = 0;
	for (VerifiedBlock const& block: blocks)
	{
		do {
			try
			{
				
				ImportRoute r;
				DEV_TIMED_ABOVE("Block import " + toString(block.verified.info.number()), 500)
					r = import(block.verified, _stateDB, (ImportRequirements::Everything & ~ImportRequirements::ValidSeal & ~ImportRequirements::CheckUncles) != 0);
				fresh += r.liveBlocks;
				dead += r.deadBlocks;
				goodTransactions.reserve(goodTransactions.size() + r.goodTranactions.size());
				std::move(std::begin(r.goodTranactions), std::end(r.goodTranactions), std::back_inserter(goodTransactions));
				++count;
			}
			catch (dev::eth::AlreadyHaveBlock const&)
			{
				cwarn << "ODD: Import queue contains already imported block";
				continue;
			}
			catch (dev::eth::UnknownParent)
			{
				cwarn << "ODD: Import queue contains block with unknown parent.";
				
				
				badBlocks.push_back(block.verified.info.hash());
			}
			catch (dev::eth::FutureTime)
			{
				cwarn << "ODD: Import queue contains a block with future time.";
				this_thread::sleep_for(chrono::seconds(1));
				continue;
			}
			catch (dev::eth::TransientError)
			{
				this_thread::sleep_for(chrono::milliseconds(100));
				continue;
			}
			catch (Exception& ex)
			{

				if (m_onBad)
					m_onBad(ex);
				
				
				badBlocks.push_back(block.verified.info.hash());
			}
		} while (false);
	}
	return make_tuple(ImportRoute{dead, fresh, goodTransactions}, _bq.doneDrain(badBlocks), count);
}

pair<ImportResult, ImportRoute> BlockChain::attemptImport(bytes const& _block, OverlayDB const& _stateDB, bool _mustBeNew) noexcept
{
	try
	{
		return make_pair(ImportResult::Success, import(verifyBlock(&_block, m_onBad, ImportRequirements::OutOfOrderChecks), _stateDB, _mustBeNew));
	}
	catch (UnknownParent&)
	{
		return make_pair(ImportResult::UnknownParent, ImportRoute());
	}
	catch (AlreadyHaveBlock&)
	{
		return make_pair(ImportResult::AlreadyKnown, ImportRoute());
	}
	catch (FutureTime&)
	{
		return make_pair(ImportResult::FutureTimeKnown, ImportRoute());
	}
	catch (Exception& ex)
	{
		if (m_onBad)
			m_onBad(ex);
		return make_pair(ImportResult::Malformed, ImportRoute());
	}
}

ImportRoute BlockChain::import(bytes const& _block, OverlayDB const& _db, bool _mustBeNew)
{
	
	VerifiedBlockRef block;

#if ETH_CATCH
	try
#endif
	{
		block = verifyBlock(&_block, m_onBad, ImportRequirements::OutOfOrderChecks);
	}
#if ETH_CATCH
	catch (Exception& ex)
	{

		ex << errinfo_phase(2);
		ex << errinfo_now(time(0));
		throw;
	}
#endif

	return import(block, _db, _mustBeNew);
}

void BlockChain::insert(bytes const& _block, bytesConstRef _receipts, bool _mustBeNew)
{
	
	VerifiedBlockRef block;

#if ETH_CATCH
	try
#endif
	{
		block = verifyBlock(&_block, m_onBad, ImportRequirements::OutOfOrderChecks);
	}
#if ETH_CATCH
	catch (Exception& ex)
	{

		ex << errinfo_phase(2);
		ex << errinfo_now(time(0));
		throw;
	}
#endif

	insert(block, _receipts, _mustBeNew);
}

void BlockChain::insert(VerifiedBlockRef _block, bytesConstRef _receipts, bool _mustBeNew)
{
	
	if (isKnown(_block.info.hash()) && _mustBeNew)
	{
		clog(BlockChainNote) << _block.info.hash() << ": Not new.";
		BOOST_THROW_EXCEPTION(AlreadyHaveBlock());
	}

	
	if (!isKnown(_block.info.parentHash(), false))
	{
		clog(BlockChainNote) << _block.info.hash() << ": Unknown parent " << _block.info.parentHash();
		
		BOOST_THROW_EXCEPTION(UnknownParent());
	}

	
	vector<bytesConstRef> receipts;
	for (auto i: RLP(_receipts))
		receipts.push_back(i.data());
	h256 receiptsRoot = orderedTrieRoot(receipts);
	if (_block.info.receiptsRoot() != receiptsRoot)
	{
		clog(BlockChainNote) << _block.info.hash() << ": Invalid receipts root " << _block.info.receiptsRoot() << " not " << receiptsRoot;
		
		BOOST_THROW_EXCEPTION(InvalidReceiptsStateRoot());
	}

	auto pd = details(_block.info.parentHash());
	if (!pd)
	{
		auto pdata = pd.rlp();
		clog(BlockChainDebug) << "Details is returning false despite block known:" << RLP(pdata);
		auto parentBlock = block(_block.info.parentHash());
		clog(BlockChainDebug) << "isKnown:" << isKnown(_block.info.parentHash());
		clog(BlockChainDebug) << "last/number:" << m_lastBlockNumber << m_lastBlockHash << _block.info.number();
		clog(BlockChainDebug) << "Block:" << BlockHeader(&parentBlock);
		clog(BlockChainDebug) << "RLP:" << RLP(parentBlock);
		clog(BlockChainDebug) << "DATABASE CORRUPTION: CRITICAL FAILURE";
		exit(-1);
	}

	
	if (_block.info.timestamp() > utcTime() && !m_params.otherParams.count("allowFutureBlocks"))
	{
		clog(BlockChainChat) << _block.info.hash() << ": Future time " << _block.info.timestamp() << " (now at " << utcTime() << ")";
		
		BOOST_THROW_EXCEPTION(FutureTime());
	}

	
	verifyBlock(_block.block, m_onBad, ImportRequirements::InOrderChecks);

	
	ldb::WriteBatch blocksBatch;
	ldb::WriteBatch extrasBatch;

	BlockLogBlooms blb;
	for (auto i: RLP(_receipts))
		blb.blooms.push_back(TransactionReceipt(i.data()).bloom());

	
	
	
	
	
	details(_block.info.parentHash());
	DEV_WRITE_GUARDED(x_details)
	{
		if (!dev::contains(m_details[_block.info.parentHash()].children, _block.info.hash()))
			m_details[_block.info.parentHash()].children.push_back(_block.info.hash());
	}

	blocksBatch.Put(toSlice(_block.info.hash()), ldb::Slice(_block.block));
	DEV_READ_GUARDED(x_details)
		extrasBatch.Put(toSlice(_block.info.parentHash(), ExtraDetails), (ldb::Slice)dev::ref(m_details[_block.info.parentHash()].rlp()));

	BlockDetails bd((unsigned)pd.number + 1, pd.totalDifficulty + _block.info.difficulty(), _block.info.parentHash(), {});
	extrasBatch.Put(toSlice(_block.info.hash(), ExtraDetails), (ldb::Slice)dev::ref(bd.rlp()));
	extrasBatch.Put(toSlice(_block.info.hash(), ExtraLogBlooms), (ldb::Slice)dev::ref(blb.rlp()));
	extrasBatch.Put(toSlice(_block.info.hash(), ExtraReceipts), (ldb::Slice)_receipts);

	ldb::Status o = m_blocksDB->Write(m_writeOptions, &blocksBatch);
	if (!o.ok())
	{
		cwarn << "Error writing to blockchain database: " << o.ToString();
		WriteBatchNoter n;
		blocksBatch.Iterate(&n);
		cwarn << "Fail writing to blockchain database. Bombing out.";
		exit(-1);
	}

	o = m_extrasDB->Write(m_writeOptions, &extrasBatch);
	if (!o.ok())
	{
		cwarn << "Error writing to extras database: " << o.ToString();
		WriteBatchNoter n;
		extrasBatch.Iterate(&n);
		cwarn << "Fail writing to extras database. Bombing out.";
		exit(-1);
	}
}

ImportRoute BlockChain::import(VerifiedBlockRef const& _block, OverlayDB const& _db, bool _mustBeNew)
{
	

#if ETH_TIMED_IMPORTS
	Timer total;
	double preliminaryChecks;
	double enactment;
	double collation;
	double writing;
	double checkBest;
	Timer t;
#endif

	
	if (isKnown(_block.info.hash()) && _mustBeNew)
	{
		clog(BlockChainNote) << _block.info.hash() << ": Not new.";
		BOOST_THROW_EXCEPTION(AlreadyHaveBlock() << errinfo_block(_block.block.toBytes()));
	}

	
	if (!isKnown(_block.info.parentHash(), false))	
	{
		clog(BlockChainNote) << _block.info.hash() << ": Unknown parent " << _block.info.parentHash();
		
		BOOST_THROW_EXCEPTION(UnknownParent() << errinfo_hash256(_block.info.parentHash()));
	}

	auto pd = details(_block.info.parentHash());
	if (!pd)
	{
		auto pdata = pd.rlp();
		clog(BlockChainDebug) << "Details is returning false despite block known:" << RLP(pdata);
		auto parentBlock = block(_block.info.parentHash());
		clog(BlockChainDebug) << "isKnown:" << isKnown(_block.info.parentHash());
		clog(BlockChainDebug) << "last/number:" << m_lastBlockNumber << m_lastBlockHash << _block.info.number();
		clog(BlockChainDebug) << "Block:" << BlockHeader(&parentBlock);
		clog(BlockChainDebug) << "RLP:" << RLP(parentBlock);
		clog(BlockChainDebug) << "DATABASE CORRUPTION: CRITICAL FAILURE";
		exit(-1);
	}

	
	if (_block.info.timestamp() > utcTime() && !m_params.otherParams.count("allowFutureBlocks"))
	{
		clog(BlockChainChat) << _block.info.hash() << ": Future time " << _block.info.timestamp() << " (now at " << utcTime() << ")";
		
		BOOST_THROW_EXCEPTION(FutureTime());
	}

	
	verifyBlock(_block.block, m_onBad, ImportRequirements::InOrderChecks);

	clog(BlockChainChat) << "Attempting import of " << _block.info.hash() << "...";

#if ETH_TIMED_IMPORTS
	preliminaryChecks = t.elapsed();
	t.restart();
#endif

	ldb::WriteBatch blocksBatch;
	ldb::WriteBatch extrasBatch;
	h256 newLastBlockHash = currentHash();
	unsigned newLastBlockNumber = number();

	BlockLogBlooms blb;
	BlockReceipts br;

	u256 td;
	Transactions goodTransactions;
#if ETH_CATCH
	try
#endif
	{
		
		
		Block s(*this, _db);
		auto tdIncrease = s.enactOn(_block, *this);

		for (unsigned i = 0; i < s.pending().size(); ++i)
		{
			blb.blooms.push_back(s.receipt(i).bloom());
			br.receipts.push_back(s.receipt(i));
			goodTransactions.push_back(s.pending()[i]);
		}

		s.cleanup(true);

		td = pd.totalDifficulty + tdIncrease;

#if ETH_TIMED_IMPORTS
		enactment = t.elapsed();
		t.restart();
#endif 

#if ETH_PARANOIA
		checkConsistency();
#endif 

		

		
		
		
		
		
		details(_block.info.parentHash());
		DEV_WRITE_GUARDED(x_details)
			m_details[_block.info.parentHash()].children.push_back(_block.info.hash());

#if ETH_TIMED_IMPORTS
		collation = t.elapsed();
		t.restart();
#endif 

		blocksBatch.Put(toSlice(_block.info.hash()), ldb::Slice(_block.block));
		DEV_READ_GUARDED(x_details)
			extrasBatch.Put(toSlice(_block.info.parentHash(), ExtraDetails), (ldb::Slice)dev::ref(m_details[_block.info.parentHash()].rlp()));

		extrasBatch.Put(toSlice(_block.info.hash(), ExtraDetails), (ldb::Slice)dev::ref(BlockDetails((unsigned)pd.number + 1, td, _block.info.parentHash(), {}).rlp()));
		extrasBatch.Put(toSlice(_block.info.hash(), ExtraLogBlooms), (ldb::Slice)dev::ref(blb.rlp()));
		extrasBatch.Put(toSlice(_block.info.hash(), ExtraReceipts), (ldb::Slice)dev::ref(br.rlp()));

#if ETH_TIMED_IMPORTS
		writing = t.elapsed();
		t.restart();
#endif 
	}

#if ETH_CATCH
	catch (BadRoot& ex)
	{
		cwarn << "*** BadRoot error! Trying to import" << _block.info.hash() << "needed root" << ex.root;
		cwarn << _block.info;
		
		BOOST_THROW_EXCEPTION(TransientError());
	}
	catch (Exception& ex)
	{
		ex << errinfo_now(time(0));
		ex << errinfo_block(_block.block.toBytes());
		
		
		if (!_block.info.extraData().empty())
			ex << errinfo_extraData(_block.info.extraData());
		throw;
	}
#endif 

	h256s route;
	h256 common;
	bool isImportedAndBest = false;
	
	h256 last = currentHash();
	if (td > details(last).totalDifficulty || (m_sealEngine->chainParams().tieBreakingGas && td == details(last).totalDifficulty && _block.info.gasUsed() > info(last).gasUsed()))
	{
		
		
		unsigned commonIndex;
		tie(route, common, commonIndex) = treeRoute(last, _block.info.parentHash());
		route.push_back(_block.info.hash());

		
		if (common != last)
			clearCachesDuringChainReversion(number(common) + 1);

		
		
		for (auto i = route.rbegin(); i != route.rend() && *i != common; ++i)
		{
			BlockHeader tbi;
			if (*i == _block.info.hash())
				tbi = _block.info;
			else
				tbi = BlockHeader(block(*i));

			
			h256s alteredBlooms;
			{
				LogBloom blockBloom = tbi.logBloom();
				blockBloom.shiftBloom<3>(sha3(tbi.author().ref()));

				
				for (unsigned level = 0, index = (unsigned)tbi.number(); level < c_bloomIndexLevels; level++, index /= c_bloomIndexSize)
					blocksBlooms(chunkId(level, index / c_bloomIndexSize));

				WriteGuard l(x_blocksBlooms);
				for (unsigned level = 0, index = (unsigned)tbi.number(); level < c_bloomIndexLevels; level++, index /= c_bloomIndexSize)
				{
					unsigned i = index / c_bloomIndexSize;
					unsigned o = index % c_bloomIndexSize;
					alteredBlooms.push_back(chunkId(level, i));
					m_blocksBlooms[alteredBlooms.back()].blooms[o] |= blockBloom;
				}
			}
			
			
			{
				bytes blockBytes;
				RLP blockRLP(*i == _block.info.hash() ? _block.block : &(blockBytes = block(*i)));
				TransactionAddress ta;
				ta.blockHash = tbi.hash();
				for (ta.index = 0; ta.index < blockRLP[1].itemCount(); ++ta.index)
					extrasBatch.Put(toSlice(sha3(blockRLP[1][ta.index].data()), ExtraTransactionAddress), (ldb::Slice)dev::ref(ta.rlp()));
			}

			
			ReadGuard l1(x_blocksBlooms);
			for (auto const& h: alteredBlooms)
				extrasBatch.Put(toSlice(h, ExtraBlocksBlooms), (ldb::Slice)dev::ref(m_blocksBlooms[h].rlp()));
			extrasBatch.Put(toSlice(h256(tbi.number()), ExtraBlockHash), (ldb::Slice)dev::ref(BlockHash(tbi.hash()).rlp()));
		}

		
		{
			newLastBlockHash = _block.info.hash();
			newLastBlockNumber = (unsigned)_block.info.number();
			isImportedAndBest = true;
		}

		clog(BlockChainNote) << "   Imported and best" << td << " (#" << _block.info.number() << "). Has" << (details(_block.info.parentHash()).children.size() - 1) << "siblings. Route:" << route;

	}
	else
	{
		clog(BlockChainChat) << "   Imported but not best (oTD:" << details(last).totalDifficulty << " > TD:" << td << "; " << details(last).number << ".." << _block.info.number() << ")";
	}

	ldb::Status o = m_blocksDB->Write(m_writeOptions, &blocksBatch);
	if (!o.ok())
	{
		cwarn << "Error writing to blockchain database: " << o.ToString();
		WriteBatchNoter n;
		blocksBatch.Iterate(&n);
		cwarn << "Fail writing to blockchain database. Bombing out.";
		exit(-1);
	}
	
	o = m_extrasDB->Write(m_writeOptions, &extrasBatch);
	if (!o.ok())
	{
		cwarn << "Error writing to extras database: " << o.ToString();
		WriteBatchNoter n;
		extrasBatch.Iterate(&n);
		cwarn << "Fail writing to extras database. Bombing out.";
		exit(-1);
	}

#if ETH_PARANOIA
	if (isKnown(_block.info.hash()) && !details(_block.info.hash()))
	{
		clog(BlockChainDebug) << "Known block just inserted has no details.";
		clog(BlockChainDebug) << "Block:" << _block.info;
		clog(BlockChainDebug) << "DATABASE CORRUPTION: CRITICAL FAILURE";
		exit(-1);
	}

	try
	{
		State canary(_db, BaseState::Empty);
		canary.populateFromChain(*this, _block.info.hash());
	}
	catch (...)
	{
		clog(BlockChainDebug) << "Failed to initialise State object form imported block.";
		clog(BlockChainDebug) << "Block:" << _block.info;
		clog(BlockChainDebug) << "DATABASE CORRUPTION: CRITICAL FAILURE";
		exit(-1);
	}
#endif 

	if (m_lastBlockHash != newLastBlockHash)
		DEV_WRITE_GUARDED(x_lastBlockHash)
		{
			m_lastBlockHash = newLastBlockHash;
			m_lastBlockNumber = newLastBlockNumber;
			o = m_extrasDB->Put(m_writeOptions, ldb::Slice("best"), ldb::Slice((char const*)&m_lastBlockHash, 32));
			if (!o.ok())
			{
				cwarn << "Error writing to extras database: " << o.ToString();
				cout << "Put" << toHex(bytesConstRef(ldb::Slice("best"))) << "=>" << toHex(bytesConstRef(ldb::Slice((char const*)&m_lastBlockHash, 32)));
				cwarn << "Fail writing to extras database. Bombing out.";
				exit(-1);
			}
		}

#if ETH_PARANOIA
	checkConsistency();
#endif 

#if ETH_TIMED_IMPORTS
	checkBest = t.elapsed();
	if (total.elapsed() > 0.5)
	{
		unsigned const gasPerSecond = static_cast<double>(_block.info.gasUsed()) / enactment;
		cnote << "SLOW IMPORT: " 
			<< "{ \"blockHash\": \"" << _block.info.hash() << "\", "
			<< "\"blockNumber\": " << _block.info.number() << ", " 
			<< "\"importTime\": " << total.elapsed() << ", "
			<< "\"gasPerSecond\": " << gasPerSecond << ", "
			<< "\"preliminaryChecks\":" << preliminaryChecks << ", "
			<< "\"enactment\":" << enactment << ", "
			<< "\"collation\":" << collation << ", "
			<< "\"writing\":" << writing << ", "
			<< "\"checkBest\":" << checkBest << ", "
			<< "\"transactions\":" << _block.transactions.size() << ", "
			<< "\"gasUsed\":" << _block.info.gasUsed() << " }";
	}
#endif 

	if (!route.empty())
		noteCanonChanged();

	if (isImportedAndBest && m_onBlockImport)
		m_onBlockImport(_block.info);

	h256s fresh;
	h256s dead;
	bool isOld = true;
	for (auto const& h: route)
		if (h == common)
			isOld = false;
		else if (isOld)
			dead.push_back(h);
		else
			fresh.push_back(h);
	return ImportRoute{dead, fresh, move(goodTransactions)};
}

void BlockChain::clearBlockBlooms(unsigned _begin, unsigned _end)
{
	
	
	
	
	
	
	

	
	
	
	
	

	

	unsigned beginDirty = _begin;
	unsigned endDirty = _end;
	for (unsigned level = 0; level < c_bloomIndexLevels; level++, beginDirty /= c_bloomIndexSize, endDirty = (endDirty - 1) / c_bloomIndexSize + 1)
	{
		
		for (unsigned item = beginDirty; item != endDirty; ++item)
		{
			unsigned bunch = item / c_bloomIndexSize;
			unsigned offset = item % c_bloomIndexSize;
			auto id = chunkId(level, bunch);
			LogBloom acc;
			if (!!level)
			{
				
				auto lowerChunkId = chunkId(level - 1, item);
				for (auto const& bloom: blocksBlooms(lowerChunkId).blooms)
					acc |= bloom;
			}
			blocksBlooms(id);	
			m_blocksBlooms[id].blooms[offset] = acc;
		}
	}
}

void BlockChain::rescue(OverlayDB const& _db)
{
	cout << "Rescuing database..." << endl;

	unsigned u = 1;
	while (true)
	{
		try {
			if (isKnown(numberHash(u)))
				u *= 2;
			else
				break;
		}
		catch (...)
		{
			break;
		}
	}
	unsigned l = u / 2;
	cout << "Finding last likely block number..." << endl;
	while (u - l > 1)
	{
		unsigned m = (u + l) / 2;
		cout << " " << m << flush;
		if (isKnown(numberHash(m)))
			l = m;
		else
			u = m;
	}
	cout << "  lowest is " << l << endl;
	for (; l > 0; --l)
	{
		h256 h = numberHash(l);
		cout << "Checking validity of " << l << " (" << h << ")..." << flush;
		try
		{
			cout << "block..." << flush;
			BlockHeader bi(block(h));
			cout << "extras..." << flush;
			details(h);
			cout << "state..." << flush;
			if (_db.exists(bi.stateRoot()))
				break;
		}
		catch (...) {}
	}
	cout << "OK." << endl;
	rewind(l);
}

void BlockChain::rewind(unsigned _newHead)
{
	DEV_WRITE_GUARDED(x_lastBlockHash)
	{
		if (_newHead >= m_lastBlockNumber)
			return;
		clearCachesDuringChainReversion(_newHead + 1);
		m_lastBlockHash = numberHash(_newHead);
		m_lastBlockNumber = _newHead;
		auto o = m_extrasDB->Put(m_writeOptions, ldb::Slice("best"), ldb::Slice((char const*)&m_lastBlockHash, 32));
		if (!o.ok())
		{
			cwarn << "Error writing to extras database: " << o.ToString();
			cout << "Put" << toHex(bytesConstRef(ldb::Slice("best"))) << "=>" << toHex(bytesConstRef(ldb::Slice((char const*)&m_lastBlockHash, 32)));
			cwarn << "Fail writing to extras database. Bombing out.";
			exit(-1);
		}
		noteCanonChanged();
	}
}

tuple<h256s, h256, unsigned> BlockChain::treeRoute(h256 const& _from, h256 const& _to, bool _common, bool _pre, bool _post) const
{

	if (!_from || !_to)
		return make_tuple(h256s(), h256(), 0);
	h256s ret;
	h256s back;
	unsigned fn = details(_from).number;
	unsigned tn = details(_to).number;

	h256 from = _from;
	while (fn > tn)
	{
		if (_pre)
			ret.push_back(from);
		from = details(from).parent;
		fn--;

	}
	h256 to = _to;
	while (fn < tn)
	{
		if (_post)
			back.push_back(to);
		to = details(to).parent;
		tn--;

	}
	for (;; from = details(from).parent, to = details(to).parent)
	{
		if (_pre && (from != to || _common))
			ret.push_back(from);
		if (_post && (from != to || (!_pre && _common)))
			back.push_back(to);
		fn--;
		tn--;

		if (from == to)
			break;
		if (!from)
			assert(from);
		if (!to)
			assert(to);
	}
	ret.reserve(ret.size() + back.size());
	unsigned i = ret.size() - (int)(_common && !ret.empty() && !back.empty());
	for (auto it = back.rbegin(); it != back.rend(); ++it)
		ret.push_back(*it);
	return make_tuple(ret, from, i);
}

void BlockChain::noteUsed(h256 const& _h, unsigned _extra) const
{
	auto id = CacheID(_h, _extra);
	Guard l(x_cacheUsage);
	m_cacheUsage[0].insert(id);
	if (m_cacheUsage[1].count(id))
		m_cacheUsage[1].erase(id);
	else
		m_inUse.insert(id);
}

template <class K, class T> static unsigned getHashSize(unordered_map<K, T> const& _map)
{
	unsigned ret = 0;
	for (auto const& i: _map)
		ret += i.second.size + 64;
	return ret;
}

void BlockChain::updateStats() const
{
	m_lastStats.memBlocks = 0;
	DEV_READ_GUARDED(x_blocks)
		for (auto const& i: m_blocks)
			m_lastStats.memBlocks += i.second.size() + 64;
	DEV_READ_GUARDED(x_details)
		m_lastStats.memDetails = getHashSize(m_details);
	DEV_READ_GUARDED(x_logBlooms)
		DEV_READ_GUARDED(x_blocksBlooms)
			m_lastStats.memLogBlooms = getHashSize(m_logBlooms) + getHashSize(m_blocksBlooms);
	DEV_READ_GUARDED(x_receipts)
		m_lastStats.memReceipts = getHashSize(m_receipts);
	DEV_READ_GUARDED(x_blockHashes)
		m_lastStats.memBlockHashes = getHashSize(m_blockHashes);
	DEV_READ_GUARDED(x_transactionAddresses)
		m_lastStats.memTransactionAddresses = getHashSize(m_transactionAddresses);
}

void BlockChain::garbageCollect(bool _force)
{
	updateStats();

	if (!_force && chrono::system_clock::now() < m_lastCollection + c_collectionDuration && m_lastStats.memTotal() < c_maxCacheSize)
		return;
	if (m_lastStats.memTotal() < c_minCacheSize)
		return;

	m_lastCollection = chrono::system_clock::now();

	Guard l(x_cacheUsage);
	WriteGuard l1(x_blocks);
	WriteGuard l2(x_details);
	WriteGuard l3(x_blockHashes);
	WriteGuard l4(x_receipts);
	WriteGuard l5(x_logBlooms);
	WriteGuard l6(x_transactionAddresses);
	WriteGuard l7(x_blocksBlooms);
	for (CacheID const& id: m_cacheUsage.back())
	{
		m_inUse.erase(id);
		
		switch (id.second)
		{
		case (unsigned)-1:
			m_blocks.erase(id.first);
			break;
		case ExtraDetails:
			m_details.erase(id.first);
			break;
		case ExtraReceipts:
			m_receipts.erase(id.first);
			break;
		case ExtraLogBlooms:
			m_logBlooms.erase(id.first);
			break;
		case ExtraTransactionAddress:
			m_transactionAddresses.erase(id.first);
			break;
		case ExtraBlocksBlooms:
			m_blocksBlooms.erase(id.first);
			break;
		}
	}
	m_cacheUsage.pop_back();
	m_cacheUsage.push_front(std::unordered_set<CacheID>{});
}

void BlockChain::checkConsistency()
{
	DEV_WRITE_GUARDED(x_details)
		m_details.clear();
	ldb::Iterator* it = m_blocksDB->NewIterator(m_readOptions);
	for (it->SeekToFirst(); it->Valid(); it->Next())
		if (it->key().size() == 32)
		{
			h256 h((byte const*)it->key().data(), h256::ConstructFromPointer);
			auto dh = details(h);
			auto p = dh.parent;
			if (p != h256() && p != m_genesisHash)	
			{
				auto dp = details(p);
				if (asserts(contains(dp.children, h)))
					cnote << "Apparently the database is corrupt. Not much we can do at this stage...";
				if (assertsEqual(dp.number, dh.number - 1))
					cnote << "Apparently the database is corrupt. Not much we can do at this stage...";
			}
		}
	delete it;
}

void BlockChain::clearCachesDuringChainReversion(unsigned _firstInvalid)
{
	unsigned end = number() + 1;
	DEV_WRITE_GUARDED(x_blockHashes)
		for (auto i = _firstInvalid; i < end; ++i)
			m_blockHashes.erase(i);
	DEV_WRITE_GUARDED(x_transactionAddresses)
		m_transactionAddresses.clear();	

	
	
	clearBlockBlooms(_firstInvalid, end);
}

static inline unsigned upow(unsigned a, unsigned b) { if (!b) return 1; while (--b > 0) a *= a; return a; }
static inline unsigned ceilDiv(unsigned n, unsigned d) { return (n + d - 1) / d; }














vector<unsigned> BlockChain::withBlockBloom(LogBloom const& _b, unsigned _earliest, unsigned _latest) const
{
	vector<unsigned> ret;

	
	unsigned u = upow(c_bloomIndexSize, c_bloomIndexLevels);

	
	for (unsigned index = _earliest / u; index <= ceilDiv(_latest, u); ++index)				
		ret += withBlockBloom(_b, _earliest, _latest, c_bloomIndexLevels - 1, index);

	return ret;
}

vector<unsigned> BlockChain::withBlockBloom(LogBloom const& _b, unsigned _earliest, unsigned _latest, unsigned _level, unsigned _index) const
{
	
		
		
		

	vector<unsigned> ret;

	unsigned uCourse = upow(c_bloomIndexSize, _level + 1);
	
		
	unsigned uFine = upow(c_bloomIndexSize, _level);
	
		

	unsigned obegin = _index == _earliest / uCourse ? _earliest / uFine % c_bloomIndexSize : 0;
	
		
		
		
	unsigned oend = _index == _latest / uCourse ? (_latest / uFine) % c_bloomIndexSize + 1 : c_bloomIndexSize;
	
		
		
		

	BlocksBlooms bb = blocksBlooms(_level, _index);
	for (unsigned o = obegin; o < oend; ++o)
		if (bb.blooms[o].contains(_b))
		{
			
			if (_level > 0)
				ret += withBlockBloom(_b, _earliest, _latest, _level - 1, o + _index * c_bloomIndexSize);
			else
				ret.push_back(o + _index * c_bloomIndexSize);
		}
	return ret;
}

h256Hash BlockChain::allKinFrom(h256 const& _parent, unsigned _generations) const
{
	
	h256 p = _parent;
	h256Hash ret = { p };
	
	for (unsigned i = 0; i < _generations && p != m_genesisHash; ++i, p = details(p).parent)
	{
		ret.insert(details(p).parent);
		auto b = block(p);
		for (auto i: RLP(b)[2])
			ret.insert(sha3(i.data()));
	}
	return ret;
}

bool BlockChain::isKnown(h256 const& _hash, bool _isCurrent) const
{
	if (_hash == m_genesisHash)
		return true;

	DEV_READ_GUARDED(x_blocks)
		if (!m_blocks.count(_hash))
		{
			string d;
			m_blocksDB->Get(m_readOptions, toSlice(_hash), &d);
			if (d.empty())
				return false;
		}
	DEV_READ_GUARDED(x_details)
		if (!m_details.count(_hash))
		{
			string d;
			m_extrasDB->Get(m_readOptions, toSlice(_hash, ExtraDetails), &d);
			if (d.empty())
				return false;
		}

	return !_isCurrent || details(_hash).number <= m_lastBlockNumber;		
}

bytes BlockChain::block(h256 const& _hash) const
{
	if (_hash == m_genesisHash)
		return m_params.genesisBlock();

	{
		ReadGuard l(x_blocks);
		auto it = m_blocks.find(_hash);
		if (it != m_blocks.end())
			return it->second;
	}

	string d;
	m_blocksDB->Get(m_readOptions, toSlice(_hash), &d);

	if (d.empty())
	{
		cwarn << "Couldn't find requested block:" << _hash;
		return bytes();
	}

	noteUsed(_hash);

	WriteGuard l(x_blocks);
	m_blocks[_hash].resize(d.size());
	memcpy(m_blocks[_hash].data(), d.data(), d.size());

	return m_blocks[_hash];
}

bytes BlockChain::headerData(h256 const& _hash) const
{
	if (_hash == m_genesisHash)
		return m_genesisHeaderBytes;

	{
		ReadGuard l(x_blocks);
		auto it = m_blocks.find(_hash);
		if (it != m_blocks.end())
			return BlockHeader::extractHeader(&it->second).data().toBytes();
	}

	string d;
	m_blocksDB->Get(m_readOptions, toSlice(_hash), &d);

	if (d.empty())
	{
		cwarn << "Couldn't find requested block:" << _hash;
		return bytes();
	}

	noteUsed(_hash);

	WriteGuard l(x_blocks);
	m_blocks[_hash].resize(d.size());
	memcpy(m_blocks[_hash].data(), d.data(), d.size());

	return BlockHeader::extractHeader(&m_blocks[_hash]).data().toBytes();
}

Block BlockChain::genesisBlock(OverlayDB const& _db) const
{
	h256 r = BlockHeader(m_params.genesisBlock()).stateRoot();
	Block ret(*this, _db, BaseState::Empty);
	if (!_db.exists(r))
	{
		ret.noteChain(*this);
		dev::eth::commit(m_params.genesisState, ret.mutableState().m_state);		
		ret.mutableState().db().commit();											
		if (ret.mutableState().rootHash() != r)
		{
			cwarn << "Hinted genesis block's state root hash is incorrect!";
			cwarn << "Hinted" << r << ", computed" << ret.mutableState().rootHash();
			
			exit(-1);
		}
	}
	ret.m_previousBlock = BlockHeader(m_params.genesisBlock());
	ret.resetCurrent();
	return ret;
}

VerifiedBlockRef BlockChain::verifyBlock(bytesConstRef _block, std::function<void(Exception&)> const& _onBad, ImportRequirements::value _ir) const
{
	VerifiedBlockRef res;
	BlockHeader h;
	try
	{
		h = BlockHeader(_block);
		if (!!(_ir & ImportRequirements::PostGenesis) && (!h.parentHash() || h.number() == 0))
			BOOST_THROW_EXCEPTION(InvalidParentHash() << errinfo_required_h256(h.parentHash()) << errinfo_currentNumber(h.number()));

		BlockHeader parent;
		if (!!(_ir & ImportRequirements::Parent))
		{
			bytes parentHeader(headerData(h.parentHash()));
			if (parentHeader.empty())
				BOOST_THROW_EXCEPTION(InvalidParentHash() << errinfo_required_h256(h.parentHash()) << errinfo_currentNumber(h.number()));
			parent = BlockHeader(parentHeader, HeaderData, h.parentHash());
		}
		sealEngine()->verify((_ir & ImportRequirements::ValidSeal) ? Strictness::CheckEverything : Strictness::QuickNonce, h, parent, _block);
		res.info = h;
	}
	catch (Exception& ex)
	{
		ex << errinfo_phase(1);
		ex << errinfo_now(time(0));
		ex << errinfo_block(_block.toBytes());
		
		
		if (!h.extraData().empty())
			ex << errinfo_extraData(h.extraData());
		if (_onBad)
			_onBad(ex);
		throw;
	}

	RLP r(_block);
	unsigned i = 0;
	if (_ir & (ImportRequirements::UncleBasic | ImportRequirements::UncleParent | ImportRequirements::UncleSeals))
		for (auto const& uncle: r[2])
		{
			BlockHeader uh(uncle.data(), HeaderData);
			try
			{
				BlockHeader parent;
				if (!!(_ir & ImportRequirements::UncleParent))
				{
					bytes parentHeader(headerData(uh.parentHash()));
					if (parentHeader.empty())
						BOOST_THROW_EXCEPTION(InvalidUncleParentHash() << errinfo_required_h256(uh.parentHash()) << errinfo_currentNumber(h.number()) << errinfo_uncleNumber(uh.number()));
					parent = BlockHeader(parentHeader, HeaderData, uh.parentHash());
				}
				sealEngine()->verify((_ir & ImportRequirements::UncleSeals) ? Strictness::CheckEverything : Strictness::IgnoreSeal, uh, parent);
			}
			catch (Exception& ex)
			{
				ex << errinfo_phase(1);
				ex << errinfo_uncleIndex(i);
				ex << errinfo_now(time(0));
				ex << errinfo_block(_block.toBytes());
				
				
				if (!uh.extraData().empty())
					ex << errinfo_extraData(uh.extraData());
				if (_onBad)
					_onBad(ex);
				throw;
			}
			++i;
		}
	i = 0;
	if (_ir & (ImportRequirements::TransactionBasic | ImportRequirements::TransactionSignatures))
		for (RLP const& tr: r[1])
		{
			bytesConstRef d = tr.data();
			try
			{
				Transaction t(d, (_ir & ImportRequirements::TransactionSignatures) ? CheckTransaction::Everything : CheckTransaction::None);
				m_sealEngine->verifyTransaction(_ir, t, h);
				res.transactions.push_back(t);
			}
			catch (Exception& ex)
			{
				ex << errinfo_phase(1);
				ex << errinfo_transactionIndex(i);
				ex << errinfo_transaction(d.toBytes());
				ex << errinfo_block(_block.toBytes());
				
				
				if (!h.extraData().empty())
					ex << errinfo_extraData(h.extraData());
				if (_onBad)
					_onBad(ex);
				throw;
			}
			++i;
		}
	res.block = bytesConstRef(_block);
	return res;
}
