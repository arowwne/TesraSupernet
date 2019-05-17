



#include "libzerocoin/Denominations.h"
#include "amount.h"
#include "chainparams.h"
#include "coincontrol.h"
#include "main.h"
#include "wallet.h"
#include "walletdb.h"
#include "txdb.h"
#include <boost/test/unit_test.hpp>
#include <iostream>

using namespace libzerocoin;


BOOST_AUTO_TEST_SUITE(zerocoin_transactions_tests)

static CWallet cWallet("unlocked.dat");

BOOST_AUTO_TEST_CASE(zerocoin_spend_test)
{
    SelectParams(CBaseChainParams::MAIN);
    ZerocoinParams *ZCParams = Params().Zerocoin_Params();
    (void)ZCParams;

    bool fFirstRun;
    cWallet.LoadWallet(fFirstRun);
    CMutableTransaction tx;
    CWalletTx* wtx = new CWalletTx(&cWallet, tx);
    bool fMintChange=true;
    bool fMinimizeChange=true;
    std::vector<CZerocoinSpend> vSpends;
    std::vector<CZerocoinMint> vMints;
    CAmount nAmount = COIN;
    int nSecurityLevel = 100;

    CZerocoinSpendReceipt receipt;
    cWallet.SpendZerocoin(nAmount, nSecurityLevel, *wtx, receipt, vMints, fMintChange, fMinimizeChange);

    BOOST_CHECK_MESSAGE(receipt.GetStatus() == ZULO_TRX_FUNDS_PROBLEMS, "Failed Invalid Amount Check");

    nAmount = 1;
    CZerocoinSpendReceipt receipt2;
    cWallet.SpendZerocoin(nAmount, nSecurityLevel, *wtx, receipt2, vMints, fMintChange, fMinimizeChange);

    
    

    BOOST_CHECK_MESSAGE(receipt2.GetStatus() == ZULO_TRX_FUNDS_PROBLEMS, "Failed Invalid Amount Check");

}

BOOST_AUTO_TEST_SUITE_END()
