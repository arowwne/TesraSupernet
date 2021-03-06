









#include "keystore.h"
#include "main.h"
#include "net.h"
#include "pow.h"
#include "script/sign.h"
#include "serialize.h"
#include "util.h"

#include <stdint.h>

#include <boost/assign/list_of.hpp> 
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/foreach.hpp>
#include <boost/test/unit_test.hpp>


extern bool AddOrphanTx(const CTransaction& tx, NodeId peer);
extern void EraseOrphansFor(NodeId peer);
extern unsigned int LimitOrphanTxSize(unsigned int nMaxOrphans);
struct COrphanTx {
    CTransaction tx;
    NodeId fromPeer;
};
extern std::map<uint256, COrphanTx> mapOrphanTransactions;
extern std::map<uint256, std::set<uint256> > mapOrphanTransactionsByPrev;

CService ip(uint32_t i)
{
    struct in_addr s;
    s.s_addr = i;
    return CService(CNetAddr(s), Params().GetDefaultPort());
}

BOOST_AUTO_TEST_SUITE(DoS_tests)

BOOST_AUTO_TEST_CASE(DoS_banning)
{
    CNode::ClearBanned();
    CAddress addr1(ip(0xa0b0c001));
    CNode dummyNode1(INVALID_SOCKET, addr1, "", true);
    dummyNode1.nVersion = 1;
    Misbehaving(dummyNode1.GetId(), 100); 
    SendMessages(&dummyNode1, false);
    BOOST_CHECK(CNode::IsBanned(addr1));
    BOOST_CHECK(!CNode::IsBanned(ip(0xa0b0c001|0x0000ff00))); 

    CAddress addr2(ip(0xa0b0c002));
    CNode dummyNode2(INVALID_SOCKET, addr2, "", true);
    dummyNode2.nVersion = 1;
    Misbehaving(dummyNode2.GetId(), 50);
    SendMessages(&dummyNode2, false);
    BOOST_CHECK(!CNode::IsBanned(addr2)); 
    BOOST_CHECK(CNode::IsBanned(addr1));  
    Misbehaving(dummyNode2.GetId(), 50);
    SendMessages(&dummyNode2, false);
    BOOST_CHECK(CNode::IsBanned(addr2));
}

BOOST_AUTO_TEST_CASE(DoS_banscore)
{
    CNode::ClearBanned();
    mapArgs["-banscore"] = "111"; 
    CAddress addr1(ip(0xa0b0c001));
    CNode dummyNode1(INVALID_SOCKET, addr1, "", true);
    dummyNode1.nVersion = 1;
    Misbehaving(dummyNode1.GetId(), 100);
    SendMessages(&dummyNode1, false);
    BOOST_CHECK(!CNode::IsBanned(addr1));
    Misbehaving(dummyNode1.GetId(), 10);
    SendMessages(&dummyNode1, false);
    BOOST_CHECK(!CNode::IsBanned(addr1));
    Misbehaving(dummyNode1.GetId(), 1);
    SendMessages(&dummyNode1, false);
    BOOST_CHECK(CNode::IsBanned(addr1));
    mapArgs.erase("-banscore");
}

BOOST_AUTO_TEST_CASE(DoS_bantime)
{
    CNode::ClearBanned();
    int64_t nStartTime = GetTime();
    SetMockTime(nStartTime); 

    CAddress addr(ip(0xa0b0c001));
    CNode dummyNode(INVALID_SOCKET, addr, "", true);
    dummyNode.nVersion = 1;

    Misbehaving(dummyNode.GetId(), 100);
    SendMessages(&dummyNode, false);
    BOOST_CHECK(CNode::IsBanned(addr));

    SetMockTime(nStartTime+60*60);
    BOOST_CHECK(CNode::IsBanned(addr));

    SetMockTime(nStartTime+60*60*24+1);
    BOOST_CHECK(!CNode::IsBanned(addr));
}

CTransaction RandomOrphan()
{
    std::map<uint256, COrphanTx>::iterator it;
    it = mapOrphanTransactions.lower_bound(GetRandHash());
    if (it == mapOrphanTransactions.end())
        it = mapOrphanTransactions.begin();
    return it->second.tx;
}

BOOST_AUTO_TEST_CASE(DoS_mapOrphans)
{
    CKey key;
    key.MakeNewKey(true);
    CBasicKeyStore keystore;
    keystore.AddKey(key);

    
    for (int i = 0; i < 50; i++)
    {
        CMutableTransaction tx;
        tx.vin.resize(1);
        tx.vin[0].prevout.n = 0;
        tx.vin[0].prevout.hash = GetRandHash();
        tx.vin[0].scriptSig << OP_1;
        tx.vout.resize(1);
        tx.vout[0].nValue = 1*CENT;
        tx.vout[0].scriptPubKey = GetScriptForDestination(key.GetPubKey().GetID());

        AddOrphanTx(tx, i);
    }

    
    for (int i = 0; i < 50; i++)
    {
        CTransaction txPrev = RandomOrphan();

        CMutableTransaction tx;
        tx.vin.resize(1);
        tx.vin[0].prevout.n = 0;
        tx.vin[0].prevout.hash = txPrev.GetHash();
        tx.vout.resize(1);
        tx.vout[0].nValue = 1*CENT;
        tx.vout[0].scriptPubKey = GetScriptForDestination(key.GetPubKey().GetID());
        SignSignature(keystore, txPrev, tx, 0);

        AddOrphanTx(tx, i);
    }

    
    for (int i = 0; i < 10; i++)
    {
        CTransaction txPrev = RandomOrphan();

        CMutableTransaction tx;
        tx.vout.resize(1);
        tx.vout[0].nValue = 1*CENT;
        tx.vout[0].scriptPubKey = GetScriptForDestination(key.GetPubKey().GetID());
        tx.vin.resize(500);
        for (unsigned int j = 0; j < tx.vin.size(); j++)
        {
            tx.vin[j].prevout.n = j;
            tx.vin[j].prevout.hash = txPrev.GetHash();
        }
        SignSignature(keystore, txPrev, tx, 0);
        
        
        for (unsigned int j = 1; j < tx.vin.size(); j++)
            tx.vin[j].scriptSig = tx.vin[0].scriptSig;

        BOOST_CHECK(!AddOrphanTx(tx, i));
    }

    
    for (NodeId i = 0; i < 3; i++)
    {
        size_t sizeBefore = mapOrphanTransactions.size();
        EraseOrphansFor(i);
        BOOST_CHECK(mapOrphanTransactions.size() < sizeBefore);
    }

    
    LimitOrphanTxSize(40);
    BOOST_CHECK(mapOrphanTransactions.size() <= 40);
    LimitOrphanTxSize(10);
    BOOST_CHECK(mapOrphanTransactions.size() <= 10);
    LimitOrphanTxSize(0);
    BOOST_CHECK(mapOrphanTransactions.empty());
    BOOST_CHECK(mapOrphanTransactionsByPrev.empty());
}

BOOST_AUTO_TEST_SUITE_END()
