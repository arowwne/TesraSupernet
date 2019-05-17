#include <map>
#include <sstream>
#include <util.h>
#include "main.h"
#include "chainparams.h"
#include "tesrastate.h"
#include "utilstrencodings.h"
#include "contractbase.h"
#include "libethereum/Transaction.h"
using namespace std;
using namespace dev;
using namespace dev::eth;

#define HDR_LEN_SIZE    (2)
#define HDR_TYPE_SIZE   (1)
#define HDR_KEY_SIZE    (32)

static const size_t MAX_CONTRACT_VOUTS = 1000;

TesraState::TesraState(u256 const &_accountStartNonce, OverlayDB const &_db, const string &_path, BaseState _bs) :
        State(_accountStartNonce, _db, _bs)
{
    dbUTXO = TesraState::openDB(_path + "/tesraDB", sha3(rlp("")), WithExisting::Trust);
    stateUTXO = SecureTrieDB<Address, OverlayDB>(&dbUTXO);
}

TesraState::TesraState() : dev::eth::State(dev::Invalid256, dev::OverlayDB(), dev::eth::BaseState::PreExisting)
{
    dbUTXO = OverlayDB();
    stateUTXO = SecureTrieDB<Address, OverlayDB>(&dbUTXO);
}

ResultExecute
TesraState::execute(EnvInfo const &_envInfo, SealEngineFace const &_sealEngine, TesraTransaction const &_t, Permanence _p,
                   OnOpFunc const &_onOp)
{

    assert(_t.getVersion().toRaw() == VersionVM::GetEVMDefault().toRaw());

    addBalance(_t.sender(), _t.value() + (_t.gas() * _t.gasPrice()));
    newAddress = _t.isCreation() ? createTesraAddress(_t.getHashWith(), _t.getNVout()) : dev::Address();

    
    
   
    _sealEngine.deleteAddresses.insert({_t.sender(), _envInfo.author()});

    h256 oldStateRoot = rootHash();
    bool voutLimit = false;

    auto onOp = _onOp;
#if ETH_VMTRACE
    if (isChannelVisible<VMTraceChannel>())
        onOp = Executive::simpleTrace(); 
#endif
    
    
    Executive e(*this, _envInfo, _sealEngine);
    ExecutionResult res;
    e.setResultRecipient(res);

    CTransaction tx;
    u256 startGasUsed;
    try
    {
        if (_t.isCreation() && _t.value())
            BOOST_THROW_EXCEPTION(CreateWithValue());
       
        e.initialize(_t);
        
        startGasUsed = _envInfo.gasUsed();
        if (!e.execute())
        {
            e.go(onOp);
        } else
        {

            e.revert();
            throw Exception();
        }
        e.finalize();
        if (_p == Permanence::Reverted)
        {
            m_cache.clear();
            cacheUTXO.clear();
        } else
        {
            
            deleteAccounts(_sealEngine.deleteAddresses);
            if (res.excepted == TransactionException::None)
            {
                CondensingTX ctx(this, transfers, _t, _sealEngine.deleteAddresses);
                tx = ctx.createCondensingTX();  
                if (ctx.reachedVoutLimit())
                {
                   
                    voutLimit = true;
                    e.revert();
                    throw Exception();
                }
                std::unordered_map<dev::Address, Vin> vins = ctx.createVin(tx);

                updateUTXO(vins);
            } else
            {
                printfErrorLog(res.excepted);
            }

          
            tesra::commit(cacheUTXO, stateUTXO, m_cache);
            cacheUTXO.clear();
          

            
            
            bool removeEmptyAccounts = false;
            commit(removeEmptyAccounts ? State::CommitBehaviour::RemoveEmptyAccounts
                                       : State::CommitBehaviour::KeepEmptyAccounts);

          

        }
    }
    catch (Exception const &_e)
    {
      

        printfErrorLog(dev::eth::toTransactionException(_e));
        res.excepted = dev::eth::toTransactionException(_e);
        res.gasUsed = _t.gas();

        
        
        
        
        
        {
            m_cache.clear();
            cacheUTXO.clear();
        }
    }
    

    if (!_t.isCreation())
        res.newAddress = _t.receiveAddress();

   

    newAddress = dev::Address();

    

    transfers.clear();
    

    if (voutLimit)
    {
      

        
        LogEntries logs;
        u256 gas = _t.gas();
        ExecutionResult ex;
        ex.gasRefunded = 0;
        ex.gasUsed = gas;
        ex.excepted = TransactionException();
        
        CMutableTransaction refund;
        if (_t.value() > 0)
        {
            refund.vin.push_back(CTxIn(h256Touint(_t.getHashWith()), _t.getNVout(), CScript() << OP_SPEND));
            
            CScript script(CScript() << OP_DUP << OP_HASH160 << _t.sender().asBytes() << OP_EQUALVERIFY << OP_CHECKSIG);
            refund.vout.push_back(CTxOut(CAmount(_t.value().convert_to<uint64_t>()), script));
        }
        
      
        return ResultExecute{ex, dev::eth::TransactionReceipt(oldStateRoot, gas, e.logs()),
                             refund.vout.empty() ? CTransaction() : CTransaction(refund)};
    } else
    {
       

        return ResultExecute{res, dev::eth::TransactionReceipt(rootHash(), startGasUsed + e.gasUsed(), e.logs()),
                             tx};
    }
}

std::unordered_map<dev::Address, Vin> TesraState::vins() const 
{
    std::unordered_map<dev::Address, Vin> ret;
    for (auto &i: cacheUTXO)
        if (i.second.alive)
            ret[i.first] = i.second;
    auto addrs = addresses();
    for (auto &i : addrs)
    {
        if (cacheUTXO.find(i.first) == cacheUTXO.end() && vin(i.first))
            ret[i.first] = *vin(i.first);
    }
    return ret;
}

void TesraState::transferBalance(dev::Address const &_from, dev::Address const &_to, dev::u256 const &_value)
{
    subBalance(_from, _value);
    addBalance(_to, _value);
    if (_value > 0)
    {
        LogPrint("TesraState::transferBalance ", "_from=%s", HexStr(_from.asBytes())); 
        LogPrint("TesraState::transferBalance ", "_to=%s", HexStr(_to.asBytes())); 
        LogPrint("TesraState::transferBalance ", "_value=%s", _value.str(0, std::ios_base::hex).c_str()); 
        transfers.push_back({_from, _to, _value});
    }
}

static bool findExtendedKeyData(const std::vector<unsigned char> vExtendData, const std::string strKey, eExtendDataType & type, std::vector<uint8_t>& value)
{
    uint32_t        u32Offset;
    uint32_t        u32SecLens;
    uint32_t        u32TotalLens;
    uint32_t        u32TotalCounts;

    if (vExtendData.empty() || 0 == strKey.size())
    {
        return false;
    }
    uint8_t *pu8Params = (uint8_t *)vExtendData.data();

    
    u32TotalLens = (pu8Params[0] << 8) | (pu8Params[1]);
    u32TotalCounts = pu8Params[2];
    u32Offset = 3;

    
    if (u32TotalLens > vExtendData.size())
    {
        return false;
    }

    while (u32Offset < u32TotalLens && u32TotalCounts > 0)
    {
        u32SecLens = (pu8Params[u32Offset] << 8) | (pu8Params[u32Offset + 1]);
        u32TotalCounts--;

        
        switch (eExtendDataType(pu8Params[u32Offset + 2]))
        {
            case EXT_DATA_STRING:
            case EXT_DATA_DOUBLE:
                break;

            case EXT_DATA_BOOL:
                if ((HDR_LEN_SIZE + HDR_TYPE_SIZE + HDR_KEY_SIZE + sizeof(int8_t)) != u32SecLens) return false;
                break;

            case EXT_DATA_INT8:
            case EXT_DATA_UINT8:
                if ((HDR_LEN_SIZE + HDR_TYPE_SIZE + HDR_KEY_SIZE + sizeof(int8_t)) != u32SecLens) return false;
                break;

            case EXT_DATA_INT16:
            case EXT_DATA_UINT16:
                if ((HDR_LEN_SIZE + HDR_TYPE_SIZE + HDR_KEY_SIZE + sizeof(int16_t)) != u32SecLens) return false;
                break;

            case EXT_DATA_INT32:
            case EXT_DATA_UINT32:
                if ((HDR_LEN_SIZE + HDR_TYPE_SIZE + HDR_KEY_SIZE + sizeof(int32_t)) != u32SecLens) return false;
                break;

            case EXT_DATA_INT64:
            case EXT_DATA_UINT64:
                if ((HDR_LEN_SIZE + HDR_TYPE_SIZE + HDR_KEY_SIZE + sizeof(int64_t)) != u32SecLens) return false;
                break;

            case EXT_DATA_INT128:
            case EXT_DATA_UINT128:
                if ((HDR_LEN_SIZE + HDR_TYPE_SIZE + HDR_KEY_SIZE + 16 * sizeof(int8_t)) != u32SecLens) return false;
                break;

            case EXT_DATA_INT256:
            case EXT_DATA_UINT256:
                if ((HDR_LEN_SIZE + HDR_TYPE_SIZE + HDR_KEY_SIZE + 32 * sizeof(int8_t)) != u32SecLens) return false;
                break;

            default:
                return false;
                break;
        }

        u32Offset += u32SecLens;
    }

    if (u32Offset != u32TotalLens || 0 != u32TotalCounts)
    {
        return false;
    }

    u32Offset = 3;
    while (u32Offset < u32TotalLens)
    {
        std::string         strSecKey;

        uint32_t            u32SecKeyLens = 32;

        eExtendDataType     eType;

        u32SecLens = (pu8Params[u32Offset] << 8) | (pu8Params[u32Offset + 1]);
        eType = eExtendDataType(pu8Params[u32Offset + 2]);

        if (strlen((char *)(pu8Params + u32Offset + 3)) <= u32SecKeyLens)
            u32SecKeyLens = strlen((char *)(pu8Params + u32Offset + 3));

        strSecKey = std::string((char *)(pu8Params + u32Offset + 3), u32SecKeyLens);

        if (0 == strKey.compare(strSecKey) && u32SecLens > 35)
        {
            type = eType;
            value.resize(u32SecLens - 35);
            memcpy(value.data(), pu8Params + u32Offset + 35, value.size());
            
            return true;
        }
        u32Offset += u32SecLens;
    }

    return false;
}

bool getData(uint32_t height, const std::string & strKey, eExtendDataType & type, std::vector<uint8_t>& value, dev::Address const& _owner)
{
    if (height > chainActive.Tip()->nHeight || NULL == chainActive[height])
    {
        return 0;
    }

    bool            find;

    CBlock          block;
    
    dev::Address    invalid;
    
    CBlockIndex *pindex = chainActive[height];

    if (!ReadBlockFromDisk(block, pindex))
        return 0;

    find = false;

    
    for (auto tx : block.vtx)
    {
        if (tx.vin.size() > 0)
        {
            CScript     sender;
            CScript     receiver;

            uint256     prevhash;

            CTransaction prevtx;

            if (!GetTransaction(tx.vin[0].prevout.hash, prevtx, prevhash, true))  {
                continue;
            }
            sender = prevtx.vout[tx.vin[0].prevout.n].scriptPubKey;

            CTxDestination txSender;
            txnouttype txType = TX_NONSTANDARD;
            if (ExtractDestination(sender, txSender, &txType))
            {
                if ((txType == TX_PUBKEY || txType == TX_PUBKEYHASH) && txSender.type() == typeid(CKeyID))
                {
                    CKeyID senderAddress(boost::get<CKeyID>(txSender));

                    if ((0 == memcmp(invalid.data(), _owner.data(), dev::Address::size)) ||
                        (0 == memcmp(senderAddress.begin(), _owner.data(), dev::Address::size)))
                    {
                        for (auto txOut : tx.vout)
                        {
                            if (txOut.scriptPubKey.HasOpExtData())
                            {
                                txnouttype whichType = TX_NONSTANDARD;
                                std::vector<std::vector<unsigned char>> vSolutions;

                                receiver = txOut.scriptPubKey;
                                if (Solver(receiver, whichType, vSolutions, true) && (TX_EXT_DATA == whichType) && (vSolutions.size() > 0))
                                {
                                    find = findExtendedKeyData(vSolutions[0], strKey, type, value);
                                    if (find)
                                        break;
                                }
                            }
                        }
                    }
                }
            }
        }

        if (find)
            break;
    }

    return find;
}

Vin const *TesraState::vin(dev::Address const &_a) const
{
    return const_cast<TesraState *>(this)->vin(_a);
}

Vin *TesraState::vin(dev::Address const &_addr)
{
    auto it = cacheUTXO.find(_addr);
    if (it == cacheUTXO.end())
    {
        std::string stateBack = stateUTXO.at(_addr);
        LogPrint("TesraState::stateBack ", "%s", stateBack); 
        if (stateBack.empty())
            return nullptr;

        dev::RLP state(stateBack);
        auto i = cacheUTXO.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(_addr),
                std::forward_as_tuple(
                        Vin{state[0].toHash<dev::h256>(), state[1].toInt<uint32_t>(), state[2].toInt<dev::u256>(),
                            state[3].toInt<uint8_t>()})
        );
       
        return &i.first->second;
    }
  
    return &it->second;
}















void TesraState::kill(dev::Address _addr)
{
    
    if (auto a = account(_addr))
        a->kill();
    if (auto v = vin(_addr))
        v->alive = 0;
}

void TesraState::addBalance(dev::Address const &_id, dev::u256 const &_amount)
{
    if (dev::eth::Account *a = account(_id))
    {
        
        
        
        
        
        
        if (!a->isDirty() && a->isEmpty())
            m_changeLog.emplace_back(dev::eth::detail::Change::Touch, _id);

        
        
        
        a->addBalance(_amount);
    } else
    {
        if (!addressInUse(newAddress) && newAddress != dev::Address())
        {
            const_cast<dev::Address &>(_id) = newAddress;
            newAddress = dev::Address();
        }
        createAccount(_id, {requireAccountStartNonce(), _amount});
    }

    if (_amount)
        m_changeLog.emplace_back(dev::eth::detail::Change::Balance, _id, _amount);
}

dev::Address TesraState::createTesraAddress(dev::h256 hashTx, uint32_t voutNumber)
{
    uint256 hashTXid(h256Touint(hashTx));
    std::vector<unsigned char> txIdAndVout(hashTXid.begin(), hashTXid.end());
    std::vector<unsigned char> voutNumberChrs;
    if (voutNumberChrs.size() < sizeof(voutNumber))
        voutNumberChrs.resize(sizeof(voutNumber));
    std::memcpy(voutNumberChrs.data(), &voutNumber, sizeof(voutNumber));
    txIdAndVout.insert(txIdAndVout.end(), voutNumberChrs.begin(), voutNumberChrs.end());

    std::vector<unsigned char> SHA256TxVout(32);
    CSHA256().Write(txIdAndVout.data(), txIdAndVout.size()).Finalize(SHA256TxVout.data());

    std::vector<unsigned char> hashTxIdAndVout(20);
    CRIPEMD160().Write(SHA256TxVout.data(), SHA256TxVout.size()).Finalize(hashTxIdAndVout.data());

    return dev::Address(hashTxIdAndVout);
}

void TesraState::deleteAccounts(std::set<dev::Address> &addrs)
{
    for (dev::Address addr : addrs)
    {
        dev::eth::Account *acc = const_cast<dev::eth::Account *>(account(addr));
        if (acc)
            acc->kill();
        Vin *in = const_cast<Vin *>(vin(addr));
        if (in)
            in->alive = 0;
    }
}


void logVin(string str, Vin vin)
{
    LogPrint("", "%s", str); 
    LogPrint("", "hash=%s", vin.hash.hex()); 
    LogPrint("", "nVout=%d", vin.nVout); 
    LogPrint("", "value=%s", vin.value.str(0, std::ios_base::hex).c_str()); 
    LogPrint("", "alive=%d", vin.alive); 
}

void TesraState::updateUTXO(const std::unordered_map<dev::Address, Vin> &vins)
{
    for (auto &v : vins)
    {
        Vin *vi = const_cast<Vin *>(vin(v.first));

        LogPrint("TesraState::", "updateUTXO inaddress=%s", v.first.hex()); 
        logVin("updateUTXO invin", v.second);
        if (vi)
        {
            logVin("TesraState::updateUTXO 00", *vi);
            vi->hash = v.second.hash;
            vi->nVout = v.second.nVout;
            vi->value = v.second.value;
            vi->alive = v.second.alive;
            logVin("TesraState::updateUTXO 11", *vi);
        } else if (v.second.alive > 0)
        {
            LogPrint("TesraState::", "updateUTXO 22"); 
            cacheUTXO[v.first] = v.second;
        }
    }
}

void TesraState::printfErrorLog(const dev::eth::TransactionException er)
{
    std::stringstream ss;
    ss << er;
    LogPrintf("TesraState::printfErrorLog  VM exception: %s\n", ss.str().c_str());

}


CTransaction CondensingTX::createCondensingTX()
{
    selectionVin();
    calculatePlusAndMinus();
    if (!createNewBalances())
        return CTransaction();
    CMutableTransaction tx;
    tx.vin = createVins();;
    tx.vout = createVout();
    
    return !tx.vin.size() || !tx.vout.size() ? CTransaction() : CTransaction(tx);
}

std::unordered_map<dev::Address, Vin> CondensingTX::createVin(const CTransaction &tx)
{
    std::unordered_map<dev::Address, Vin> vins;
    for (auto &b : balances)
    {
        if (b.first == transaction.sender())
            continue;

        if (b.second > 0)
        {
            vins[b.first] = Vin{uintToh256(tx.GetHash()), nVouts[b.first], b.second, 1};
        } else
        {
            vins[b.first] = Vin{uintToh256(tx.GetHash()), 0, 0, 0};
        }
    }
    return vins;
}

void CondensingTX::selectionVin()
{
    for (const TransferInfo &ti : transfers)
    {
        if (!vins.count(ti.from))
        {
            LogPrint("CondensingTX::selectionVin ", "from=%s", HexStr(ti.from.asBytes())); 
            if (auto a = state->vin(ti.from))
            {
                vins[ti.from] = *a;
                logVin("CondensingTX::selectionVin0", *a);
            }
            if (ti.from == transaction.sender() && transaction.value() > 0)
            {
                vins[ti.from] = Vin{transaction.getHashWith(), transaction.getNVout(), transaction.value(), 1};
                logVin("CondensingTX::selectionVin1", vins[ti.from]);
            }
        }
        if (!vins.count(ti.to))
        {
            LogPrint("CondensingTX::selectionVin ", "to=%s", HexStr(ti.to.asBytes())); 
            if (auto a = state->vin(ti.to))
            {
                vins[ti.to] = *a;
                logVin("CondensingTX::selectionVin2", *a);
            }
        }
    }
}

void CondensingTX::calculatePlusAndMinus()
{
    for (const TransferInfo &ti : transfers)
    {
        LogPrint("CondensingTX::calculatePlusAndMinus ", "from=%s", HexStr(ti.from.asBytes())); 
        if (!plusMinusInfo.count(ti.from))
        {
            plusMinusInfo[ti.from] = std::make_pair(0, ti.value);
            LogPrint("CondensingTX::calculatePlusAndMinus ", "first= %d,second value=%s", 0, ti.value.str(0, std::ios_base::hex).c_str()); 
        } else
        {
            plusMinusInfo[ti.from] = std::make_pair(plusMinusInfo[ti.from].first,
                                                    plusMinusInfo[ti.from].second + ti.value);
            LogPrint("CondensingTX::calculatePlusAndMinus ", "first= %s,second value=%s", plusMinusInfo[ti.from].first.str(0, std::ios_base::hex).c_str(),
                       plusMinusInfo[ti.from].second.str(0, std::ios_base::hex).c_str()); 
        }

        LogPrint("CondensingTX::calculatePlusAndMinus ", "to=%s", HexStr(ti.to.asBytes())); 
        if (!plusMinusInfo.count(ti.to))
        {
            plusMinusInfo[ti.to] = std::make_pair(ti.value, 0);
            LogPrint("CondensingTX::calculatePlusAndMinus ", "first value= %s,second=%d", ti.value.str(0, std::ios_base::hex).c_str(), 0); 
        } else
        {
            plusMinusInfo[ti.to] = std::make_pair(plusMinusInfo[ti.to].first + ti.value, plusMinusInfo[ti.to].second);
            LogPrint("CondensingTX::calculatePlusAndMinus ", "first value= %s,second=%s", plusMinusInfo[ti.to].first.str(0, std::ios_base::hex).c_str(),
                       plusMinusInfo[ti.to].second.str(0, std::ios_base::hex).c_str()); 
        }
    }
}

bool CondensingTX::createNewBalances()
{
    for (auto &p : plusMinusInfo)
    {
        dev::u256 balance = 0;
        if ((vins.count(p.first) && vins[p.first].alive) || (!vins[p.first].alive && !checkDeleteAddress(p.first)))
        {
            balance = vins[p.first].value;
        }
        balance += p.second.first;

        if (balance < p.second.second)
            return false;
        balance -= p.second.second;
        balances[p.first] = balance;

    }
    return true;
}

std::vector<CTxIn> CondensingTX::createVins()
{
    std::vector<CTxIn> ins;
    LogPrint("CondensingTX::createVins",""); 
    for (auto &v : vins)
    {
        if ((v.second.value > 0 && v.second.alive) ||
            (v.second.value > 0 && !vins[v.first].alive && !checkDeleteAddress(v.first)))
        {
            LogPrint("CondensingTX::createVins", "hash=%s", v.second.hash.hex()); 
            LogPrint("CondensingTX::createVins", "nVout=%d", v.second.nVout); 
            ins.push_back(CTxIn(h256Touint(v.second.hash), v.second.nVout, CScript() << OP_SPEND));
        }
    }
    return ins;
}

std::vector<CTxOut> CondensingTX::createVout()
{
    size_t count = 0;
    std::vector<CTxOut> outs;
    for (auto &b : balances)
    {
        if (b.second > 0)
        {
            CScript script;
            auto *a = state->account(b.first);
            if (a && a->isAlive())
            {
                
                script = CScript() << valtype{0} << valtype{0} << valtype{0} << valtype{0} << b.first.asBytes()
                                   << OP_CALL;
            } else
            {
                script = CScript() << OP_DUP << OP_HASH160 << b.first.asBytes() << OP_EQUALVERIFY << OP_CHECKSIG;
            }
            outs.push_back(CTxOut(CAmount(b.second), script));
            nVouts[b.first] = count;
            count++;
        }
        if (count > MAX_CONTRACT_VOUTS)
        {
            voutOverflow = true;
            return outs;
        }
    }
    return outs;
}

bool CondensingTX::checkDeleteAddress(dev::Address addr)
{
    return deleteAddresses.count(addr) != 0;
}

