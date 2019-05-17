



#include "main.h"
#include "random.h"
#include "util.h"

#include <vector>

#include <boost/test/unit_test.hpp>

#define SKIPLIST_LENGTH 300000

BOOST_AUTO_TEST_SUITE(skiplist_tests)

BOOST_AUTO_TEST_CASE(skiplist_test)
{
    std::vector<CBlockIndex> vIndex(SKIPLIST_LENGTH);

    for (int i=0; i<SKIPLIST_LENGTH; i++) {
        vIndex[i].nHeight = i;
        vIndex[i].pprev = (i == 0) ? NULL : &vIndex[i - 1];
        vIndex[i].BuildSkip();
    }

    for (int i=0; i<SKIPLIST_LENGTH; i++) {
        if (i > 0) {
            BOOST_CHECK(vIndex[i].pskip == &vIndex[vIndex[i].pskip->nHeight]);
            BOOST_CHECK(vIndex[i].pskip->nHeight < i);
        } else {
            BOOST_CHECK(vIndex[i].pskip == NULL);
        }
    }

    for (int i=0; i < 1000; i++) {
        int from = insecure_rand() % (SKIPLIST_LENGTH - 1);
        int to = insecure_rand() % (from + 1);

        BOOST_CHECK(vIndex[SKIPLIST_LENGTH - 1].GetAncestor(from) == &vIndex[from]);
        BOOST_CHECK(vIndex[from].GetAncestor(to) == &vIndex[to]);
        BOOST_CHECK(vIndex[from].GetAncestor(0) == &vIndex[0]);
    }
}

BOOST_AUTO_TEST_CASE(getlocator_test)
{
    
    std::vector<uint256> vHashMain(100000);
    std::vector<CBlockIndex> vBlocksMain(100000);
    for (unsigned int i=0; i<vBlocksMain.size(); i++) {
        vHashMain[i] = i; 
        vBlocksMain[i].nHeight = i;
        vBlocksMain[i].pprev = i ? &vBlocksMain[i - 1] : NULL;
        vBlocksMain[i].phashBlock = &vHashMain[i];
        vBlocksMain[i].BuildSkip();
        BOOST_CHECK_EQUAL((int)vBlocksMain[i].GetBlockHash().GetLow64(), vBlocksMain[i].nHeight);
        BOOST_CHECK(vBlocksMain[i].pprev == NULL || vBlocksMain[i].nHeight == vBlocksMain[i].pprev->nHeight + 1);
    }

    
    std::vector<uint256> vHashSide(50000);
    std::vector<CBlockIndex> vBlocksSide(50000);
    for (unsigned int i=0; i<vBlocksSide.size(); i++) {
        vHashSide[i] = i + 50000 + (uint256(1) << 128); 
        vBlocksSide[i].nHeight = i + 50000;
        vBlocksSide[i].pprev = i ? &vBlocksSide[i - 1] : &vBlocksMain[49999];
        vBlocksSide[i].phashBlock = &vHashSide[i];
        vBlocksSide[i].BuildSkip();
        BOOST_CHECK_EQUAL((int)vBlocksSide[i].GetBlockHash().GetLow64(), vBlocksSide[i].nHeight);
        BOOST_CHECK(vBlocksSide[i].pprev == NULL || vBlocksSide[i].nHeight == vBlocksSide[i].pprev->nHeight + 1);
    }

    
    CChain chain;
    chain.SetTip(&vBlocksMain.back());

    
    for (int n=0; n<100; n++) {
        int r = insecure_rand() % 150000;
        CBlockIndex* tip = (r < 100000) ? &vBlocksMain[r] : &vBlocksSide[r - 100000];
        CBlockLocator locator = chain.GetLocator(tip);

        
        BOOST_CHECK(locator.vHave.front() == tip->GetBlockHash());
        BOOST_CHECK(locator.vHave.back() == vBlocksMain[0].GetBlockHash());

        
        for (unsigned int i = 1; i < 12 && i < locator.vHave.size() - 1; i++) {
            BOOST_CHECK_EQUAL(locator.vHave[i].GetLow64(), tip->nHeight - i);
        }

        
        unsigned int dist = 2;
        for (unsigned int i = 12; i < locator.vHave.size() - 1; i++) {
            BOOST_CHECK_EQUAL(locator.vHave[i - 1].GetLow64() - locator.vHave[i].GetLow64(), dist);
            dist *= 2;
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
