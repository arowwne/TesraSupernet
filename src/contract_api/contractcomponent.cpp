#include "util.h"
#include "contractcomponent.h"
#include "contractbase.h"
#include "univalue/include/univalue.h"
#include "timedata.h"
#include "contractconfig.h"
#include "main.h"
#include "libdevcore/Common.h"
#include "libdevcore/Log.h"

#include <fstream>
#include <boost/filesystem.hpp>

static std::unique_ptr<TesraState> globalState;
static std::shared_ptr<dev::eth::SealEngineFace> globalSealEngine;
static StorageResults *pstorageresult = NULL;
static bool fRecordLogOpcodes = false;
static bool fIsVMlogFile = false;
static bool fGettingValuesDGP = false;




bool CheckMinGasPrice(std::vector<EthTransactionParams> &etps, const uint64_t &minGasPrice)
{
    for (EthTransactionParams &etp : etps)
    {
        if (etp.gasPrice < dev::u256(minGasPrice))
            return false;
    }
    return true;
}

valtype
GetSenderAddress(const CTransaction &tx, const CCoinsViewCache *coinsView, const std::vector<CTransaction> *blockTxs)
{
    CScript script;
    bool scriptFilled = false; 

    
    if (blockTxs)
    {
        LogPrintf("tx vin size %d\n", tx.vin.size());
        if (tx.vin.size() > 0)
            LogPrintf("tx vin[0] hash %s, n %d", tx.vin[0].prevout.hash.GetHex().c_str(),
                    tx.vin[0].prevout.n);
        
        LogPrintf("vtx size is %d\n\n", blockTxs->size());
        for (auto btx : *blockTxs)
        {
            LogPrintf("vtx vin size: %d, vout size %d\n", btx.vin.size(), btx.vout.size());
            LogPrintf("vtx vin[0] hash %s\n", btx.GetHash().GetHex().c_str());
            if (btx.GetHash() == tx.vin[0].prevout.hash)
            {LogPrintf("Find match");
                script = btx.vout[tx.vin[0].prevout.n].scriptPubKey;
                scriptFilled = true;
                break;
            }LogPrintf("Not match");
        }
    }LogPrintf("GetSenderAddress, scriptFilled: %d, coinsView %d\n", scriptFilled, NULL == coinsView);
    if (!scriptFilled && coinsView)
    {
        
        script = coinsView->AccessCoins(tx.vin[0].prevout.hash)->vout[tx.vin[0].prevout.n].scriptPubKey;
        scriptFilled = true;
    }LogPrintf("GetSenderAddress, script: %s\n", script.ToString().c_str());
    if (!scriptFilled)
    {

        CTransaction txPrevout;
        uint256 hashBlock;
        LogPrintf("before GetTransaction\n");
        if (GetTransaction(tx.vin[0].prevout.hash, txPrevout, hashBlock, true))
        {
            script = txPrevout.vout[tx.vin[0].prevout.n].scriptPubKey;
        }
        else
        {
            LogPrint("Error ","fetching transaction details of tx %s. This will probably cause more errors",
                     tx.vin[0].prevout.hash.ToString().c_str());
            return valtype();
        }
    }LogPrintf("before ExtractDestination\n");

    CTxDestination addressBit;
    txnouttype txType = TX_NONSTANDARD;
    if (ExtractDestination(script, addressBit, &txType))
    {
        LogPrintf(" ExtractDestination done\n");
        if ((txType == TX_PUBKEY || txType == TX_PUBKEYHASH) &&
                addressBit.type() == typeid(CKeyID))
        {
            CKeyID senderAddress(boost::get<CKeyID>(addressBit));
            return valtype(senderAddress.begin(), senderAddress.end());
        }
    }LogPrintf(" return type 0\n");
    
    return valtype();
}


UniValue vmLogToJSON(const ResultExecute &execRes, const CTransaction &tx, const CBlock &block)
{
    UniValue result(UniValue::VOBJ);


    int height = chainActive.Tip()->nHeight;

    if (tx != CTransaction())
        result.push_back(Pair("txid", tx.GetHash().GetHex()));
    result.push_back(Pair("address", execRes.execRes.newAddress.hex()));
    if (block.GetHash() != CBlock().GetHash())
    {
        result.push_back(Pair("time", block.GetBlockTime()));
        result.push_back(Pair("blockhash", block.GetHash().GetHex()));
        result.push_back(Pair("blockheight", height + 1));

    } else
    {
        result.push_back(Pair("time", GetAdjustedTime()));
        result.push_back(Pair("blockheight", height));
    }
    UniValue logEntries(UniValue::VARR);
    dev::eth::LogEntries logs = execRes.txRec.log();
    for (dev::eth::LogEntry log : logs)
    {
        UniValue logEntrie(UniValue::VOBJ);
        logEntrie.push_back(Pair("address", log.address.hex()));
        UniValue topics(UniValue::VARR);
        for (dev::h256 l : log.topics)
        {
            UniValue topicPair(UniValue::VOBJ);
            topicPair.push_back(Pair("raw", l.hex()));
            topics.push_back(topicPair);
            
        }
        UniValue dataPair(UniValue::VOBJ);
        dataPair.push_back(Pair("raw", HexStr(log.data)));
        logEntrie.push_back(Pair("data", dataPair));
        logEntrie.push_back(Pair("topics", topics));
        logEntries.push_back(logEntrie);
    }
    result.push_back(Pair("entries", logEntries));
    return result;
}

void writeVMlog(const std::vector<ResultExecute> &res, const CTransaction &tx = CTransaction(),
                const CBlock &block = CBlock())
{
    boost::filesystem::path tesraDir = GetDataDir() / "vmExecLogs.json";
    std::stringstream ss;
    if (fIsVMlogFile)
    {
        ss << ",";
    } else
    {
        std::ofstream file(tesraDir.string(), std::ios::out | std::ios::app);
        file << "{\"logs\":[]}";
        file.close();
    }

    for (size_t i = 0; i < res.size(); i++)
    {
        ss << vmLogToJSON(res[i], tx, block).write();
        if (i != res.size() - 1)
        {
            ss << ",";
        } else
        {
            ss << "]}";
        }
    }

    std::ofstream file(tesraDir.string(), std::ios::in | std::ios::out);
    file.seekp(-2, std::ios::end);
    file << ss.str();
    file.close();
    fIsVMlogFile = true;
}



std::vector<ResultExecute> CallContract(const dev::Address &addrContract, std::vector<unsigned char> opcode,
                                        const dev::Address &sender = dev::Address(), uint64_t gasLimit = 0)
{
    CBlock block;
    CMutableTransaction tx;

    CBlockIndex *pTip = chainActive.Tip();

    CBlockIndex *pblockindex = mapBlockIndex[pTip->GetBlockHash()];

    ReadBlockFromDisk(block, pblockindex);
    block.nTime = GetAdjustedTime();
    block.vtx.erase(block.vtx.begin() + 1, block.vtx.end());

    TesraDGP tesraDGP(globalState.get(), fGettingValuesDGP);
    uint64_t blockGasLimit = tesraDGP.getBlockGasLimit(pTip->nHeight + 1);

    if (gasLimit == 0)
    {
        gasLimit = blockGasLimit - 1;
    }
    dev::Address senderAddress =
            sender == dev::Address() ? dev::Address("ffffffffffffffffffffffffffffffffffffffff") : sender;
    tx.vout.push_back(
                CTxOut(0, CScript() << OP_DUP << OP_HASH160 << senderAddress.asBytes() << OP_EQUALVERIFY << OP_CHECKSIG));
    block.vtx.push_back(CTransaction(tx));

    TesraTransaction callTransaction(0, 1, dev::u256(gasLimit), addrContract, opcode, dev::u256(0));
    callTransaction.forceSender(senderAddress);
    callTransaction.setVersion(VersionVM::GetEVMDefault());


    ByteCodeExec exec(block, std::vector<TesraTransaction>(1, callTransaction), blockGasLimit);
    exec.performByteCode(dev::eth::Permanence::Reverted);
    return exec.getResult();
}



bool ComponentInitialize()
{
    LogPrintStr("initialize CContract component");
    return true;
}

bool ContractInit()
{
    

    
    dev::g_logPost = [&](std::string const& s, char const* c){ LogPrintStr(s + '\n', true); };
    dev::g_logPost(std::string("\n\n\n\n\n\n\n\n\n\n"), NULL);
    
    

    if ((GetBoolArg("-dgpstorage", false) && GetBoolArg("-dgpevm", false)) ||
            (!GetBoolArg("-dgpstorage", false) && GetBoolArg("-dgpevm", false)) ||
            (!GetBoolArg("-dgpstorage", false) && !GetBoolArg("-dgpevm", false)))
    {
        fGettingValuesDGP = true;
    } else
    {
        fGettingValuesDGP = false;
    }


    LogPrintf("AppInitMain: fGettingValuesDGP = %d", fGettingValuesDGP);

    dev::eth::Ethash::init();
    boost::filesystem::path stateDir = GetDataDir() / CONTRACT_STATE_DIR;
    bool fStatus = boost::filesystem::exists(stateDir);
    const std::string dirTesra(stateDir.string());
    const dev::h256 hashDB(dev::sha3(dev::rlp("")));
    dev::eth::BaseState existstate = fStatus ? dev::eth::BaseState::PreExisting : dev::eth::BaseState::Empty;
    globalState = std::unique_ptr<TesraState>(
                new TesraState(dev::u256(0), TesraState::openDB(dirTesra, hashDB, dev::WithExisting::Trust), dirTesra,
                              existstate));

    
    
    

    pstorageresult = new StorageResults(stateDir.string());


    bool IsEnabled =  [&]()->bool{
            if(chainActive.Tip()== nullptr) return false;
            return chainActive.Tip()->IsContractEnabled();
}();

    if (IsEnabled)
    {
        CBlockIndex *pTip = chainActive.Tip();
        CBlock block;
        if (!ReadBlockFromDisk(block, pTip))
        {
            LogPrint("ReadBlockFromDisk ","failed at %d, hash=%s", pTip->nHeight,
                     pTip->GetBlockHash().ToString());
            assert(0);
            return false;
        } else
        {
            uint256 hashStateRoot;
            uint256 hashUTXORoot;
            if(block.GetVMState(hashStateRoot, hashUTXORoot) != RET_VM_STATE_OK)
            {
                assert(0);
                LogPrintStr("GetVMState failed");
                return false;
            }else {
                globalState->setRoot(uintToh256(hashStateRoot));
                globalState->setRootUTXO(uintToh256(hashUTXORoot));
            }
        }
    } else
    {

        globalState->setRoot(dev::sha3(dev::rlp("")));
        globalState->setRootUTXO(dev::sha3(dev::rlp("")));
        
        LogPrintf("globalState:%s globalUtxo:%s\n",h256Touint(dev::sha3(dev::rlp(""))).GetHex().c_str(),h256Touint(dev::sha3(dev::rlp(""))).GetHex().c_str());

    }

    globalState->db().commit();
    globalState->dbUtxo().commit();

    fRecordLogOpcodes = GetBoolArg("-record-log-opcodes", false);
    fIsVMlogFile = boost::filesystem::exists(GetDataDir() / "vmExecLogs.json");

    if (!fLogEvents)
    {
        pstorageresult->wipeResults();
    }

    return true;
}


bool ComponentStartup()
{
    LogPrintStr("starting CContract component\n");

    return true;
}

bool ComponentShutdown()
{
    LogPrintStr("shutdown CContract component");

    delete pstorageresult;
    pstorageresult = NULL;
    delete globalState.release();
    
    return true;
}

uint64_t GetMinGasPrice(int height)
{
    uint64_t minGasPrice = 1;

    bool IsEnabled =  [&]()->bool{

            if(chainActive.Tip()== nullptr) return false;
            return chainActive.Tip()->IsContractEnabled();
}();
    if (!IsEnabled)
    {
        return 0;
    }

    TesraDGP tesraDGP(globalState.get(), fGettingValuesDGP);
    
    minGasPrice = tesraDGP.getMinGasPrice(height);

    return minGasPrice;
}

uint64_t GetBlockGasLimit(int height)
{
    uint64_t blockGasLimit = 1;

    bool IsEnabled =  [&]()->bool{

            if(chainActive.Tip()== nullptr) return false;
            return chainActive.Tip()->IsContractEnabled();
}();
    if (!IsEnabled)
    {
        return 0;
    }

    TesraDGP tesraDGP(globalState.get(), fGettingValuesDGP);
    
    blockGasLimit = tesraDGP.getBlockGasLimit(height);

    return blockGasLimit;
}

uint32_t GetBlockSize(int height)
{
    uint32_t blockSize = 1;

    bool IsEnabled =  [&]()->bool{

            if(chainActive.Tip()== nullptr) return false;
            return chainActive.Tip()->IsContractEnabled();
}();
    if (!IsEnabled)
    {
        return 0;
    }

    TesraDGP tesraDGP(globalState.get(), fGettingValuesDGP);
    
    blockSize = tesraDGP.getBlockSize(height);

    return blockSize;
}

bool AddressInUse(string contractaddress)
{
    
    bool IsEnabled =  [&]()->bool{

            if(chainActive.Tip()== nullptr) return false;
            return chainActive.Tip()->IsContractEnabled();
}();
    if (!IsEnabled)
    {
        return false;
    }
    dev::Address addrAccount(contractaddress);
    return globalState->addressInUse(addrAccount);
}

bool CheckContractTx(const CTransaction tx,CAmount &nFees,
                     CAmount &nMinGasPrice, int &level,
                     string &errinfo, const CAmount nAbsurdFee, bool rawTx)
{
    dev::u256 txMinGasPrice = 0;


    int height = chainActive.Tip()->nHeight + 1;

    bool IsEnabled =  [&]()->bool{

            if(chainActive.Tip()== nullptr) return false;
            return chainActive.Tip()->IsContractEnabled();
}();
    if (!IsEnabled)
    {
        return false;
    }

    uint64_t minGasPrice = GetMinGasPrice(height);
    uint64_t blockGasLimit = GetBlockGasLimit(height);
    size_t count = 0;
    for (const CTxOut &o : tx.vout)
        count += o.scriptPubKey.HasOpCreate() || o.scriptPubKey.HasOpCall() ? 1 : 0;
    TesraTxConverter converter(tx, NULL);
    ExtractTesraTX resultConverter;
    LogPrintStr("CheckContractTx"); 
    if (!converter.extractionTesraTransactions(resultConverter))
    {
        level = 100;
        errinfo = "bad-tx-bad-contract-format";
        return false;
    }
    std::vector<TesraTransaction> tesraTransactions = resultConverter.first;
    std::vector<EthTransactionParams> tesraETP = resultConverter.second;

    dev::u256 sumGas = dev::u256(0);
    dev::u256 gasAllTxs = dev::u256(0);
    for (TesraTransaction tesraTransaction : tesraTransactions)
    {
        sumGas += tesraTransaction.gas() * tesraTransaction.gasPrice();

        if (sumGas > dev::u256(INT64_MAX))
        {
            level = 100;
            errinfo = "bad-tx-gas-stipend-overflow";
            return false;
        }

        if (sumGas > dev::u256(nFees))
        {
            level = 100;
            errinfo = "bad-txns-fee-notenough";
            return false;
        }

        if (txMinGasPrice != 0)
        {
            txMinGasPrice = std::min(txMinGasPrice, tesraTransaction.gasPrice());
        } else
        {
            txMinGasPrice = tesraTransaction.gasPrice();
        }
        VersionVM v = tesraTransaction.getVersion();
        if (v.format != 0)
        {
            level = 100;
            errinfo = "bad-tx-version-format";
            return false;
        }
        if (v.rootVM != 1)
        {
            level = 100;
            errinfo = "bad-tx-version-rootvm";
            return false;
        }
        if (v.vmVersion != 0)
        {
            level = 100;
            errinfo = "bad-tx-version-vmversion";
            return false;
        }
        if (v.flagOptions != 0)
        {
            level = 100;
            errinfo = "bad-tx-version-flags";
            return false;
        }

        
        if (tesraTransaction.gas() < GetBoolArg("-minmempoolgaslimit", MEMPOOL_MIN_GAS_LIMIT))
        {
            level = 100;
            errinfo = "bad-tx-too-little-mempool-gas";
            return false;
        }

        
        if (tesraTransaction.gas() < MINIMUM_GAS_LIMIT && v.rootVM != 0)
        {
            level = 100;
            errinfo = "bad-tx-too-little-gas";
            return false;
        }

        if (tesraTransaction.gas() > UINT32_MAX)
        {
            level = 100;
            errinfo = "bad-tx-too-much-gas";
            return false;
        }

        gasAllTxs += tesraTransaction.gas();
        if (gasAllTxs > dev::u256(blockGasLimit))
        {
            level = 1;
            errinfo = "bad-txns-gas-exceeds-blockgaslimit";
            return false;
        }

        
        if (v.rootVM != 0 && (uint64_t)tesraTransaction.gasPrice() < minGasPrice)
        {
            level = 100;
            errinfo = "bad-tx-low-gas-price";
            return false;
        }
    }

    if (!CheckMinGasPrice(tesraETP, minGasPrice))
    {
        level = 100;
        errinfo = "bad-txns-small-gasprice";
        return false;
    }

    if (count > tesraTransactions.size())
    {
        level = 100;
        errinfo = "bad-txns-incorrect-format";
        return false;
    }

    if (rawTx && nAbsurdFee && dev::u256(nFees) > dev::u256(nAbsurdFee) + sumGas)
    {
        level = REJECT_HIGHFEE;
        errinfo = "absurdly-high-fee" + strprintf("%d > %d", nFees, nAbsurdFee);
        return false;
    }

    nMinGasPrice = CAmount(txMinGasPrice);

    nFees -= CAmount(sumGas);

    return true;
}

bool RunContractTx(CTransaction tx, CCoinsViewCache *v, CBlock *pblock,
                   uint64_t minGasPrice,
                   uint64_t hardBlockGasLimit,
                   uint64_t softBlockGasLimit,
                   uint64_t txGasLimit,
                   uint64_t usedGas,
                   ByteCodeExecResult &testExecResult)
{
    bool IsEnabled =  [&]()->bool{

            if(chainActive.Tip()== nullptr) return false;
            return chainActive.Tip()->IsContractEnabled();
}();
    if (!IsEnabled)
    {
        return false;
    }

    TesraTxConverter convert(tx, v, &pblock->vtx);

    ExtractTesraTX resultConverter;
    LogPrintStr("RunContractTx"); 
    if (!convert.extractionTesraTransactions(resultConverter))
    {
        
        
        return false;
    }
    std::vector<TesraTransaction> tesraTransactions = resultConverter.first;
    dev::u256 txGas = 0;
    for (TesraTransaction tesraTransaction : tesraTransactions)
    {
        txGas += tesraTransaction.gas();
        if (txGas > txGasLimit)
        {
            
            return false;
        }

        if (usedGas + tesraTransaction.gas() > softBlockGasLimit)
        {
            
            return false;
        }
        if (tesraTransaction.gasPrice() < minGasPrice)
        {
            
            return false;
        }
    }
    
    ByteCodeExec exec(*pblock, tesraTransactions, hardBlockGasLimit);
    if (!exec.performByteCode())
    {
        
        return false;
    }

    if (!exec.processingResults(testExecResult))
    {
        return false;
    }
    return true;
}

const std::map<std::uint32_t, std::string> exceptionMap =
{
    {0,  "None"},
    {1,  "Unknown"},
    {2,  "BadRLP"},
    {3,  "InvalidFormat"},
    {4,  "OutOfGasIntrinsic"},
    {5,  "InvalidSignature"},
    {6,  "InvalidNonce"},
    {7,  "NotEnoughCash"},
    {8,  "OutOfGasBase"},
    {9,  "BlockGasLimitReached"},
    {10, "BadInstruction"},
    {11, "BadJumpDestination"},
    {12, "OutOfGas"},
    {13, "OutOfStack"},
    {14, "StackUnderflow"},
    {15, "CreateWithValue"},
};

uint32_t GetExcepted(dev::eth::TransactionException status)
{
    uint32_t index = 0;
    if (status == dev::eth::TransactionException::None)
    {
        index = 0;
    } else if (status == dev::eth::TransactionException::Unknown)
    {
        index = 1;
    } else if (status == dev::eth::TransactionException::BadRLP)
    {
        index = 2;
    } else if (status == dev::eth::TransactionException::InvalidFormat)
    {
        index = 3;
    } else if (status == dev::eth::TransactionException::OutOfGasIntrinsic)
    {
        index = 4;
    } else if (status == dev::eth::TransactionException::InvalidSignature)
    {
        index = 5;
    } else if (status == dev::eth::TransactionException::InvalidNonce)
    {
        index = 6;
    } else if (status == dev::eth::TransactionException::NotEnoughCash)
    {
        index = 7;
    } else if (status == dev::eth::TransactionException::OutOfGasBase)
    {
        index = 8;
    } else if (status == dev::eth::TransactionException::BlockGasLimitReached)
    {
        index = 9;
    } else if (status == dev::eth::TransactionException::BadInstruction)
    {
        index = 10;
    } else if (status == dev::eth::TransactionException::BadJumpDestination)
    {
        index = 11;
    } else if (status == dev::eth::TransactionException::OutOfGas)
    {
        index = 12;
    } else if (status == dev::eth::TransactionException::OutOfStack)
    {
        index = 13;
    } else if (status == dev::eth::TransactionException::StackUnderflow)
    {
        index = 14;
    } else if (status == dev::eth::TransactionException::CreateWithValue)
    {
        index = 15;
    }
    auto it = exceptionMap.find(index);
    if (it != exceptionMap.end())
    {
        return it->first;
    } else
    {
        return 0;
    }
}

string GetExceptedInfo(uint32_t index)
{
    bool IsEnabled =  [&]()->bool{

            if(chainActive.Tip()== nullptr) return false;
            return chainActive.Tip()->IsContractEnabled();
}();
    if (!IsEnabled)
    {
        return "";
    }
    auto it = exceptionMap.find(index);
    if (it != exceptionMap.end())
    {
        return it->second;
    } else
    {
        return "";
    }
}
extern UniValue blockToJSON(const CBlock& block, const CBlockIndex* blockindex, bool txDetails);

bool ContractTxConnectBlock(CTransaction tx, uint32_t transactionIndex, CCoinsViewCache *v,
                            const CBlock &block,
                            int nHeight,
                            ByteCodeExecResult &bcer,
                            bool bLogEvents,
                            bool fJustCheck,
                            std::map<dev::Address, std::pair<CHeightTxIndexKey, std::vector<uint256>>> &heightIndexes,
                            int &level, string &errinfo,uint64_t &countCumulativeGasUsed,uint64_t &blockGasUsed)
{
    CBlockIndex* pblockindex = chainActive.Tip();

    UniValue blockJson = blockToJSON(block, pblockindex, true);

    
    

    if (!block.IsContractEnabled())
    {
        return false;
    }

    uint64_t minGasPrice = GetMinGasPrice(nHeight + 1);
    uint64_t blockGasLimit = GetBlockGasLimit(nHeight + 1);

    
    

    TesraTxConverter convert(tx, v, &block.vtx);

    ExtractTesraTX resultConvertQtumTX;
    
    
    if (!convert.extractionTesraTransactions(resultConvertQtumTX))
    {
        level = 100;
        errinfo = "bad-tx-bad-contract-format";
        return false;
    }
    if (!CheckMinGasPrice(resultConvertQtumTX.second, minGasPrice))
    {
        level = 100;
        errinfo = "bad-tx-low-gas-price";
        return false;
    }

    dev::u256 gasAllTxs = dev::u256(0);
    LogPrintf("ContractTxConnectBlock() : before ByteCodeExec\n");
    ByteCodeExec exec(block, resultConvertQtumTX.first, blockGasLimit);
    LogPrintf("ContractTxConnectBlock() : after ByteCodeExec\n");
    
    
    
    bool nonZeroVersion = false;
    dev::u256 sumGas = dev::u256(0);
    CAmount nTxFee = v->GetValueIn(tx) - tx.GetValueOut();
    for (TesraTransaction &qtx : resultConvertQtumTX.first)
    {
        sumGas += qtx.gas() * qtx.gasPrice();

        if (sumGas > dev::u256(INT64_MAX))
        {
            level = 100;
            errinfo = "bad-tx-gas-stipend-overflow";
            return false;
        }

        if (sumGas > dev::u256(nTxFee))
        {
            level = 100;
            errinfo = "bad-txns-fee-notenough";
            return false;
        }

        VersionVM v = qtx.getVersion();
        if (v.format != 0)
        {
            level = 100;
            errinfo = "bad-tx-version-format";
            return false;
        }
        if (v.rootVM != 0)
        {
            nonZeroVersion = true;
        } else
        {
            if (nonZeroVersion)
            {
                
                level = 100;
                errinfo = "bad-tx-mixed-zero-versions";
                return false;
            }
        }
        if (!(v.rootVM == 0 || v.rootVM == 1))
        {
            level = 100;
            errinfo = "bad-tx-version-rootvm";
            return false;
        }
        if (v.vmVersion != 0)
        {
            level = 100;
            errinfo = "bad-tx-version-vmversion";
            return false;
        }
        if (v.flagOptions != 0)
        {
            level = 100;
            errinfo = "bad-tx-version-flags";
            return false;
        }
        
        if (qtx.gas() < MINIMUM_GAS_LIMIT && v.rootVM != 0)
        {
            level = 100;
            errinfo = "bad-tx-too-little-gas";
            return false;
        }
        if (qtx.gas() > UINT32_MAX)
        {
            level = 100;
            errinfo = "bad-tx-too-much-gas";
            return false;
        }
        gasAllTxs += qtx.gas();
        if (gasAllTxs > dev::u256(blockGasLimit))
        {
            level = 1;
            errinfo = "bad-txns-gas-exceeds-blockgaslimit";
            return false;
        }
        
        if (v.rootVM != 0 && (uint64_t)qtx.gasPrice() < minGasPrice)
        {
            level = 100;
            errinfo = "bad-tx-low-gas-price";
            return false;
        }
    }

    if (!nonZeroVersion)
    {
        
        if (!tx.HasOpSpend())
        {
            level = 100;
            errinfo = "bad-tx-improper-version-0";
            return false;
        }
    }

    LogPrintf("ContractTxConnectBlock() : before exec.performByteCode\n");
    if (!exec.performByteCode(dev::eth::Permanence::Committed,nHeight))
    {LogPrintf("ContractTxConnectBlock() : after exec.performByteCode fail\n");
        level = 100;
        errinfo = "bad-tx-unknown-error";
        return false;
    }LogPrintf("ContractTxConnectBlock() : after exec.performByteCode ok\n");

    std::vector<ResultExecute> resultExec(exec.getResult());
    if (!exec.processingResults(bcer))
    {
        level = 100;
        errinfo = "bad-vm-exec-processing";
        return false;
    }

    countCumulativeGasUsed += bcer.usedGas;
    std::vector<TransactionReceiptInfo> tri;
    if (bLogEvents)
    {
        for (size_t k = 0; k < resultConvertQtumTX.first.size(); k++)
        {
            dev::Address key = resultExec[k].execRes.newAddress;
            if (!heightIndexes.count(key))
            {
                heightIndexes[key].first = CHeightTxIndexKey(nHeight, resultExec[k].execRes.newAddress);
            }
            heightIndexes[key].second.push_back(tx.GetHash());
            uint32_t excepted = GetExcepted(resultExec[k].execRes.excepted);
            tri.push_back(
                        TransactionReceiptInfo{block.GetHash(), uint32_t(nHeight), tx.GetHash(), uint32_t(transactionIndex),
                                               resultConvertQtumTX.first[k].from(), resultConvertQtumTX.first[k].to(),
                                               countCumulativeGasUsed, uint64_t(resultExec[k].execRes.gasUsed),
                                               resultExec[k].execRes.newAddress, resultExec[k].txRec.log(),
                                               excepted});
        }

        pstorageresult->addResult(uintToh256(tx.GetHash()), tri);
    }

    blockGasUsed += bcer.usedGas;
    if (blockGasUsed > blockGasLimit)
    {
        level = 1000;
        errinfo = "bad-blk-gaslimit";
        return false;
    }

    
    if (fRecordLogOpcodes && !fJustCheck)
    {
        writeVMlog(resultExec, tx, block);
    }

    for (ResultExecute &re: resultExec)
    {
        if (re.execRes.newAddress != dev::Address() && !fJustCheck){
            LogPrint("Address : ","%s :", std::string(re.execRes.newAddress.hex()));
            dev::g_logPost(std::string("Address : " + re.execRes.newAddress.hex()), NULL);
        }
    }
    return true;
}

void GetState(uint256 &hashStateRoot, uint256 &hashUTXORoot)
{
    bool IsEnabled =  [&]()->bool{

            if(chainActive.Tip()== nullptr) return false;
            return chainActive.Tip()->IsContractEnabled();
}();
    if (!IsEnabled)
    {
        return;
    }
    dev::h256 oldHashStateRoot(globalState->rootHash()); 
    dev::h256 oldHashUTXORoot(globalState->rootHashUTXO()); 

    hashStateRoot = h256Touint(oldHashStateRoot);
    hashUTXORoot = h256Touint(oldHashUTXORoot);
}

void UpdateState(uint256 hashStateRoot, uint256 hashUTXORoot)
{
    bool IsEnabled =  [&]()->bool{

            if(chainActive.Tip()== nullptr) return false;
            return chainActive.Tip()->IsContractEnabled();
}();
    if (!IsEnabled)
    {
        return;
    }

    if (hashStateRoot.IsNull() || hashUTXORoot.IsNull())
    {
        return;
    }
    globalState->setRoot(uintToh256(hashStateRoot));
    globalState->setRootUTXO(uintToh256(hashUTXORoot));
}

void DeleteResults(std::vector<CTransaction> const &txs)
{
    bool IsEnabled =  [&]()->bool{

            if(chainActive.Tip()== nullptr) return false;
            return chainActive.Tip()->IsContractEnabled();
}();
    if (!IsEnabled)
    {
        return;
    }
    pstorageresult->deleteResults(txs);
}

std::vector<TransactionReceiptInfo> GetResult(uint256 const &hashTx)
{
    bool IsEnabled =  [&]()->bool{

            if(chainActive.Tip()== nullptr) return false;
            return chainActive.Tip()->IsContractEnabled();
}();
    if (!IsEnabled)
    {
        return std::vector<TransactionReceiptInfo>();
    }
    return pstorageresult->getResult(uintToh256(hashTx));
}

void CommitResults()
{
    bool IsEnabled =  [&]()->bool{

            if(chainActive.Tip()== nullptr) return false;
            return chainActive.Tip()->IsContractEnabled();
}();
    if (!IsEnabled)
    {
        return;
    }
    pstorageresult->commitResults();
}

void ClearCacheResult()
{
    bool IsEnabled =  [&]()->bool{

            if(chainActive.Tip()== nullptr) return false;
            return chainActive.Tip()->IsContractEnabled();
}();
    if (!IsEnabled)
    {
        return;
    }
    pstorageresult->clearCacheResult();
}

std::map<dev::h256, std::pair<dev::u256, dev::u256>> GetStorageByAddress(string address)
{
    bool IsEnabled =  [&]()->bool{

            if(chainActive.Tip()== nullptr) return false;
            return chainActive.Tip()->IsContractEnabled();
}();
    if (!IsEnabled)
    {
        return std::map<dev::h256, std::pair<dev::u256, dev::u256>>();
    }

    dev::Address addrAccount(address);
    auto storage(globalState->storage(addrAccount));
    return storage;
};

void SetTemporaryState(uint256 hashStateRoot, uint256 hashUTXORoot)
{

    bool IsEnabled =  [&]()->bool{

            if(chainActive.Tip()== nullptr) return false;
            return chainActive.Tip()->IsContractEnabled();
}();
    if (!IsEnabled)
    {
        return;
    }
    if (hashStateRoot.IsNull() || hashUTXORoot.IsNull())
    {
        return;
    }
    TemporaryState ts(globalState);
    ts.SetRoot(uintToh256(hashStateRoot), uintToh256(hashUTXORoot));
}

std::unordered_map<dev::h160, dev::u256> GetContractList()
{
    bool IsEnabled =  [&]()->bool{

            if(chainActive.Tip()== nullptr) return false;
            return chainActive.Tip()->IsContractEnabled();
}();
    if (!IsEnabled)
    {
        return std::unordered_map<dev::h160, dev::u256>();
    }
    auto map = globalState->addresses();
    return map;
};


CAmount GetContractBalance(dev::h160 address)
{
    bool IsEnabled =  [&]()->bool{

            if(chainActive.Tip()== nullptr) return false;
            return chainActive.Tip()->IsContractEnabled();
}();
    if (!IsEnabled)
    {
        return CAmount(0);
    }
    return CAmount(globalState->balance(address));
}

std::vector<uint8_t> GetContractCode(dev::Address address)
{
    bool IsEnabled =  [&]()->bool{

            if(chainActive.Tip()== nullptr) return false;
            return chainActive.Tip()->IsContractEnabled();
}();
    if (!IsEnabled)
    {
        return std::vector<uint8_t>();
    }
    return globalState->code(address);
}

bool GetContractVin(dev::Address address, dev::h256 &hash, uint32_t &nVout, dev::u256 &value,
                    uint8_t &alive)
{
    bool ret = false;
    bool IsEnabled =  [&]()->bool{

            if(chainActive.Tip()== nullptr) return false;
            return chainActive.Tip()->IsContractEnabled();
}();
    if (!IsEnabled)
    {
        return ret;
    }
    std::unordered_map<dev::Address, Vin> vins = globalState->vins();
    if (vins.count(address))
    {
        hash = vins[address].hash;
        nVout = vins[address].nVout;
        value = vins[address].value;
        alive = vins[address].alive;
        ret = true;
    }
    return ret;
}

UniValue executionResultToJSON(const dev::eth::ExecutionResult &exRes)
{
    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("gasUsed", CAmount(exRes.gasUsed)));
    std::stringstream ss;
    ss << exRes.excepted;
    result.push_back(Pair("excepted", ss.str()));
    result.push_back(Pair("newAddress", exRes.newAddress.hex()));
    result.push_back(Pair("output", HexStr(exRes.output)));
    result.push_back(Pair("codeDeposit", static_cast<int32_t>(exRes.codeDeposit)));
    result.push_back(Pair("gasRefunded", CAmount(exRes.gasRefunded)));
    result.push_back(Pair("depositSize", static_cast<int32_t>(exRes.depositSize)));
    result.push_back(Pair("gasForDeposit", CAmount(exRes.gasForDeposit)));
    return result;
}

UniValue transactionReceiptToJSON(const dev::eth::TransactionReceipt &txRec)
{
    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("stateRoot", txRec.stateRoot().hex()));
    result.push_back(Pair("gasUsed", CAmount(txRec.gasUsed())));
    result.push_back(Pair("bloom", txRec.bloom().hex()));
    UniValue logEntries(UniValue::VARR);
    dev::eth::LogEntries logs = txRec.log();
    for (dev::eth::LogEntry log : logs)
    {
        UniValue logEntrie(UniValue::VOBJ);
        logEntrie.push_back(Pair("address", log.address.hex()));
        UniValue topics(UniValue::VARR);
        for (dev::h256 l : log.topics)
        {
            topics.push_back(l.hex());
        }
        logEntrie.push_back(Pair("topics", topics));
        logEntrie.push_back(Pair("data", HexStr(log.data)));
        logEntries.push_back(logEntrie);
    }
    result.push_back(Pair("log", logEntries));
    return result;
}

void RPCCallContract(UniValue &result, const string addrContract, std::vector<unsigned char> opcode,
                     string sender, uint64_t gasLimit)
{
    bool IsEnabled =  [&]()->bool{

            if(chainActive.Tip()== nullptr) return false;
            return chainActive.Tip()->IsContractEnabled();
}();
    if (!IsEnabled)
    {
        return;
    }

    dev::Address addrAccount(addrContract);
    dev::Address senderAddress(sender);

    std::vector<ResultExecute> execResults = CallContract(addrAccount, opcode, senderAddress, gasLimit);
    if (fRecordLogOpcodes)
    {
        writeVMlog(execResults);
    }
    result.push_back(Pair("executionResult", executionResultToJSON(execResults[0].execRes)));
    result.push_back(Pair("transactionReceipt", transactionReceiptToJSON(execResults[0].txRec)));
}


dev::eth::EnvInfo ByteCodeExec::BuildEVMEnvironment(int nHeight){
    dev::eth::EnvInfo env;
    CBlockIndex* tip = chainActive.Tip();
    

    

    if(nHeight == 0)
    {
        env.setNumber(dev::u256(tip->nHeight + 1));
       
    }
    else
    {
        env.setNumber(dev::u256(nHeight));
    }




    env.setTimestamp(dev::u256(block.nTime));
    env.setDifficulty(dev::u256(block.nBits));









    dev::eth::LastHashes lh;
    lh.resize(256);
    for(int i=0;i<256;i++){
        if(!tip)
            break;
        lh[i]= uintToh256(*tip->phashBlock);
        tip = tip->pprev;
    }
    env.setLastHashes(std::move(lh));
    env.setGasLimit(blockGasLimit);

    if(block.IsProofOfStake()){
        if(block.GetBlockHeader().nVersion < SMART_CONTRACT_VERSION)
            env.setAuthor(EthAddrFromScript(block.vtx[1].vout[1].scriptPubKey));
        else
            env.setAuthor(EthAddrFromScript(block.vtx[1].vout[2].scriptPubKey));
    }else {
        env.setAuthor(EthAddrFromScript(block.vtx[0].vout[0].scriptPubKey));
    }

    return env;
}



bool ByteCodeExec::performByteCode(dev::eth::Permanence type, int nHeight)
{
    for (TesraTransaction &tx : txs)
    {
        
        
        if (tx.getVersion().toRaw() != VersionVM::GetEVMDefault().toRaw())
        {
            
            return false;
        }
        

        dev::eth::EnvInfo envInfo(BuildEVMEnvironment(nHeight));

        std::unique_ptr<dev::eth::SealEngineFace> se(dev::eth::ChainParams(dev::eth::genesisInfo(dev::eth::Network::HomesteadTest)).createSealEngine());


        if (!tx.isCreation() && !globalState->addressInUse(tx.receiveAddress()))
        {
           
            dev::eth::ExecutionResult execRes;
            execRes.excepted = dev::eth::TransactionException::Unknown;
            result.push_back(ResultExecute{execRes, dev::eth::TransactionReceipt(dev::h256(), dev::u256(),
                                           dev::eth::LogEntries()),
                                           CTransaction()});
            continue;
        }
        

        

        ResultExecute res_ = globalState->execute(envInfo, *se.get(), tx, type, OnOpFunc());


        

        result.push_back(res_);
    }
    globalState->db().commit();
    globalState->dbUtxo().commit();
    
    return true;
}

bool ByteCodeExec::processingResults(ByteCodeExecResult &resultBCE)
{
    for (size_t i = 0; i < result.size(); i++)
    {
        uint64_t gasUsed = (uint64_t)result[i].execRes.gasUsed;
        if (result[i].execRes.excepted != dev::eth::TransactionException::None)
        {
            
            if (txs[i].value() > 0)
            { 
                CMutableTransaction tx;
                tx.vin.push_back(CTxIn(h256Touint(txs[i].getHashWith()), txs[i].getNVout(), CScript() << OP_SPEND));
                CScript script(CScript() << OP_DUP << OP_HASH160 << txs[i].sender().asBytes() << OP_EQUALVERIFY
                               << OP_CHECKSIG);
                tx.vout.push_back(CTxOut(CAmount(txs[i].value()), script));
                resultBCE.valueTransfers.push_back(CTransaction(tx));

               

            }
            resultBCE.usedGas += gasUsed;



        } else
        {



            if (txs[i].gas() > UINT64_MAX ||
                    result[i].execRes.gasUsed > UINT64_MAX ||
                    txs[i].gasPrice() > UINT64_MAX)
            {
                return false;
            }
            uint64_t gas = (uint64_t)txs[i].gas();
            uint64_t gasPrice = (uint64_t)txs[i].gasPrice();

            resultBCE.usedGas += gasUsed;
            int64_t amount = (gas - gasUsed) * gasPrice;








            if (amount < 0)
            {
                return false;
            }
            if (amount > 0)
            {
                CScript script(CScript() << OP_DUP << OP_HASH160 << txs[i].sender().asBytes() << OP_EQUALVERIFY
                               << OP_CHECKSIG);
                resultBCE.refundOutputs.push_back(CTxOut(amount, script));
                resultBCE.refundSender += amount;
            }
        }
        if (result[i].tx != CTransaction())
        {
            LogPrint("processingResults ","%d", i); 
            resultBCE.valueTransfers.push_back(result[i].tx);
        }
    }
    return true;
}


dev::Address ByteCodeExec::EthAddrFromScript(const CScript &script)
{
    CTxDestination addressBit;
    txnouttype txType = TX_NONSTANDARD;
    if (ExtractDestination(script, addressBit, &txType))
    {
        if ((txType == TX_PUBKEY || txType == TX_PUBKEYHASH) &&
                addressBit.type() == typeid(CKeyID))
        {
            CKeyID addressKey(boost::get<CKeyID>(addressBit));
            std::vector<unsigned char> addr(addressKey.begin(), addressKey.end());
            LogPrint("ByteCodeExec::EthAddrFromScript ","%s", HexStr(addr.begin(), addr.end())); 
            return dev::Address(addr);
        }
    }
    
    return dev::Address();
}


bool TesraTxConverter::extractionTesraTransactions(ExtractTesraTX &tesratx)
{
    std::vector<TesraTransaction> resultTX;
    std::vector<EthTransactionParams> resultETP;
    for (size_t i = 0; i < txBit.vout.size(); i++)
    {
        if (txBit.vout[i].scriptPubKey.HasOpCreate() || txBit.vout[i].scriptPubKey.HasOpCall())
        {
            if (receiveStack(txBit.vout[i].scriptPubKey))
            {
                EthTransactionParams params;
                if (parseEthTXParams(params))
                {
                    LogPrintStr("extractionTesraTransactions\n"); 
                    resultTX.push_back(createEthTX(params, i));
                    resultETP.push_back(params);
                    LogPrintStr("createEthTX and push back\n");
                } else
                {LogPrintf("parseEthTXParams return failed \n");
                    return false;
                }
            } else
            {LogPrintf("receiveStack return failed \n");
                return false;
            }
        }
    }
    tesratx = std::make_pair(resultTX, resultETP);
    return true;
}

bool TesraTxConverter::receiveStack(const CScript &scriptPubKey)
{
    EvalScript(stack, scriptPubKey, SCRIPT_EXEC_BYTE_CODE, BaseSignatureChecker(),  nullptr);
    LogPrintf("receiveStack stack.empty(): %d \n", stack.empty());
    if (stack.empty())
        return false;

    CScript scriptRest(stack.back().begin(), stack.back().end());
    stack.pop_back();

    opcode = (opcodetype)(*scriptRest.begin());
    LogPrintf("receiveStack stack.size(): %d \n", stack.size());
    if ((opcode == OP_CREATE && stack.size() < 4) || (opcode == OP_CALL && stack.size() < 5))
    {
        stack.clear();
        return false;
    }

    return true;
}

bool TesraTxConverter::parseEthTXParams(EthTransactionParams &params)
{
    try
    {
        dev::Address receiveAddress;
        valtype vecAddr;
        if (opcode == OP_CALL)
        {
            vecAddr = stack.back();
            stack.pop_back();
            receiveAddress = dev::Address(vecAddr);
        }
        if (stack.size() < 4)
            return false;

        if (stack.back().size() < 1)
        {
            return false;
        }
        valtype code(stack.back());
        stack.pop_back();
        uint64_t gasPrice = CScriptNum::vch_to_uint64(stack.back());
        stack.pop_back();
        uint64_t gasLimit = CScriptNum::vch_to_uint64(stack.back());
        stack.pop_back();
        if (gasPrice > INT64_MAX || gasLimit > INT64_MAX)
        {
            return false;
        }
        
        if (gasPrice != 0 && gasLimit > INT64_MAX / gasPrice)
        {
            
            return false;
        }
        if (stack.back().size() > 4)
        {
            return false;
        }
        VersionVM version = VersionVM::fromRaw((uint32_t)CScriptNum::vch_to_uint64(stack.back()));
        stack.pop_back();
        params.version = version;
        params.gasPrice = dev::u256(gasPrice);
        params.receiveAddress = receiveAddress;
        params.code = code;
        params.gasLimit = dev::u256(gasLimit);
        return true;
    }
    catch (const scriptnum_error &err)
    {
        LogPrintStr("Incorrect parameters to VM.");
        return false;
    }
}

TesraTransaction TesraTxConverter::createEthTX(const EthTransactionParams &etp, uint32_t nOut)
{
    TesraTransaction txEth;LogPrintStr("createEthTX txEth\n");
    if (etp.receiveAddress == dev::Address() && opcode != OP_CALL)
    {
        txEth = TesraTransaction(txBit.vout[nOut].nValue, etp.gasPrice, etp.gasLimit, etp.code, dev::u256(0));
    } else
    {
        txEth = TesraTransaction(txBit.vout[nOut].nValue, etp.gasPrice, etp.gasLimit, etp.receiveAddress, etp.code,
                                dev::u256(0));
    }
    LogPrintStr("createEthTX before GetSenderAddress\n");
    valtype types = GetSenderAddress(txBit, view, blockTransactions);
    LogPrintStr("createEthTX after GetSenderAddress, type: \n");
    dev::Address sender(GetSenderAddress(txBit, view, blockTransactions));
    txEth.forceSender(sender);
    txEth.setHashWith(uintToh256(txBit.GetHash()));
    txEth.setNVout(nOut);
    txEth.setVersion(etp.version);

    return txEth;
}


