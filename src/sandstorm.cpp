// Copyright (c) 2014-2015 The Darkcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sandstorm.h"
#include "sandstorm-relay.h"
#include "main.h"
#include "init.h"
#include "util.h"
#include "stormnodeman.h"
#include "instantx.h"
#include "ui_interface.h"

#include <algorithm>

#include <openssl/rand.h>

#include <boost/algorithm/string/replace.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/lexical_cast.hpp>

using namespace std;
using namespace boost;

CCriticalSection cs_sandstorm;

/** The main object for accessing Sandstorm */
CSandstormPool sandStormPool;
/** A helper object for signing messages from Stormnodes */
CSandStormSigner sandStormSigner;
/** The current Sandstorms in progress on the network */
std::vector<CSandstormQueue> vecSandstormQueue;
/** Keep track of the used Stormnodes */
std::vector<CTxIn> vecStormnodesUsed;
// keep track of the scanning errors I've seen
map<uint256, CSandstormBroadcastTx> mapSandstormBroadcastTxes;
//
CActiveStormnode activeStormnode;

// count peers we've requested the list from
int RequestedStormNodeList = 0;

/* *** BEGIN SANDSTORM MAGIC  **********
    Copyright 2014, Darkcoin Developers
        eduffield - evan@darkcoin.io
*/

void CSandstormPool::ProcessMessageSandstorm(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    if(fLiteMode) return; //disable all Sandstorm/Stormnode related functionality

    if (strCommand == "ssa") { //SandStorm Accept Into Pool

        if (pfrom->nVersion < MIN_SANDSTORM_PROTO_VERSION) {
            std::string strError = _("Incompatible version.");
            LogPrintf("ssa -- incompatible version! \n");
            pfrom->PushMessage("sssu", sessionID, GetState(), GetEntriesCount(), STORMNODE_REJECTED, strError);

            return;
        }

        if(!fStormNode){
            std::string strError = _("This is not a Stormnode.");
            LogPrintf("ssa -- not a Stormnode! \n");
            pfrom->PushMessage("sssu", sessionID, GetState(), GetEntriesCount(), STORMNODE_REJECTED, strError);

            return;
        }

        int nDenom;
        CTransaction txCollateral;
        vRecv >> nDenom >> txCollateral;

        std::string error = "";
        CStormnode* psn = snodeman.Find(activeStormnode.vin);
        if(psn == NULL)
        {
            std::string strError = _("Not in the Stormnode list.");
            pfrom->PushMessage("sssu", sessionID, GetState(), GetEntriesCount(), STORMNODE_REJECTED, strError);
            return;
        }

        if(sessionUsers == 0) {
            if(psn->nLastSsq != 0 &&
                psn->nLastSsq + snodeman.CountStormnodesAboveProtocol(MIN_SANDSTORM_PROTO_VERSION)/5 > snodeman.nSsqCount){
                LogPrintf("ssa -- last ssq too recent, must wait. %s \n", pfrom->addr.ToString().c_str());                
                std::string strError = _("Last Sandstorm was too recent.");
                pfrom->PushMessage("sssu", sessionID, GetState(), GetEntriesCount(), STORMNODE_ACCEPTED, error);
                return;
            }
        }

        if(!IsCompatibleWithSession(nDenom, txCollateral, error))
        {
            LogPrintf("ssa -- not compatible with existing transactions! \n");
            pfrom->PushMessage("sssu", sessionID, GetState(), GetEntriesCount(), STORMNODE_REJECTED, error);
            return;
        } else {
            LogPrintf("ssa -- is compatible, please submit! \n");
            pfrom->PushMessage("sssu", sessionID, GetState(), GetEntriesCount(), STORMNODE_ACCEPTED, error);
            return;
        }
    } else if (strCommand == "ssq") { //SandStorm Queue
        TRY_LOCK(cs_sandstorm, lockRecv);
        if(!lockRecv) return;

        if (pfrom->nVersion < MIN_SANDSTORM_PROTO_VERSION) {
            return;
        }

        CSandstormQueue ssq;
        vRecv >> ssq;


        CService addr;
        if(!ssq.GetAddress(addr)) return;
        if(!ssq.CheckSignature()) return;

        if(ssq.IsExpired()) return;

        CStormnode* psn = snodeman.Find(ssq.vin);
        if(psn == NULL) return;

        // if the queue is ready, submit if we can
        if(ssq.ready) {
            if(!pSubmittedToStormnode) return;
            if((CNetAddr)pSubmittedToStormnode->addr != (CNetAddr)addr){
                LogPrintf("ssq - message doesn't match current Stormnode - %s != %s\n", pSubmittedToStormnode->addr.ToString().c_str(), addr.ToString().c_str());
                 return;
            }

            if(state == POOL_STATUS_QUEUE){
                if (fDebug)  LogPrintf("Sandstorm queue is ready - %s\n", addr.ToString().c_str());
                PrepareSandstormDenominate();
            }
         } else {
            BOOST_FOREACH(CSandstormQueue q, vecSandstormQueue){
                if(q.vin == ssq.vin) return;
            }

            if(fDebug) LogPrintf("ssq last %d last2 %d count %d\n", psn->nLastSsq, psn->nLastSsq + snodeman.size()/5, snodeman.nSsqCount);
             //don't allow a few nodes to dominate the queuing process
            if(psn->nLastSsq != 0 &&
                psn->nLastSsq + snodeman.CountStormnodesAboveProtocol(MIN_SANDSTORM_PROTO_VERSION)/5 > snodeman.nSsqCount){
                if(fDebug) LogPrintf("ssq -- Stormnode sending too many ssq messages. %s \n", psn->addr.ToString().c_str());
                return;
            }
            snodeman.nSsqCount++;
            psn->nLastSsq = snodeman.nSsqCount;
            psn->allowFreeTx = true;

            if(fDebug) LogPrintf("ssq - new Sandstorm queue object - %s\n", addr.ToString().c_str());
            vecSandstormQueue.push_back(ssq);
            ssq.Relay();
            ssq.time = GetTime();
        }

    } else if (strCommand == "ssi") { //SandStorm vIn
        std::string error = "";
        if (pfrom->nVersion < MIN_SANDSTORM_PROTO_VERSION) {
            LogPrintf("ssi -- incompatible version! \n");
            error = _("Incompatible version.");
            pfrom->PushMessage("sssu", sessionID, GetState(), GetEntriesCount(), STORMNODE_REJECTED, error);
 
            return;
        }

        if(!fStormNode){
            LogPrintf("ssi -- not a Stormnode! \n");
            error = _("This is not a Stormnode.");
            pfrom->PushMessage("sssu", sessionID, GetState(), GetEntriesCount(), STORMNODE_REJECTED, error);
 
            return;
        }

        std::vector<CTxIn> in;
        int64_t nAmount;
        CTransaction txCollateral;
        std::vector<CTxOut> out;
        vRecv >> in >> nAmount >> txCollateral >> out;

        //do we have enough users in the current session?
        if(!IsSessionReady()){
            LogPrintf("ssi -- session not complete! \n");
            error = _("Session not complete!");
            pfrom->PushMessage("sssu", sessionID, GetState(), GetEntriesCount(), STORMNODE_REJECTED, error);
            return;
        }

        //do we have the same denominations as the current session?
        if(!IsCompatibleWithEntries(out))
        {
            LogPrintf("ssi -- not compatible with existing transactions! \n");
            error = _("Not compatible with existing transactions.");
            pfrom->PushMessage("sssu", sessionID, GetState(), GetEntriesCount(), STORMNODE_REJECTED, error);
            return;
        }

        //check it like a transaction
        {
            int64_t nValueIn = 0;
            int64_t nValueOut = 0;
            bool missingTx = false;

            CValidationState state;
            CTransaction tx;

            BOOST_FOREACH(CTxOut o, out){
                nValueOut += o.nValue;
                tx.vout.push_back(o);

                if(o.scriptPubKey.size() != 25){
                    LogPrintf("ssi - non-standard pubkey detected! %s\n", o.scriptPubKey.ToString().c_str());
                    error = _("Non-standard public key detected.");
                    pfrom->PushMessage("sssu", sessionID, GetState(), GetEntriesCount(), STORMNODE_REJECTED, error);
                    return;
                }
                if(!o.scriptPubKey.IsNormalPaymentScript()){
                    LogPrintf("ssi - invalid script! %s\n", o.scriptPubKey.ToString().c_str());
                    error = _("Invalid script detected.");
                    pfrom->PushMessage("sssu", sessionID, GetState(), GetEntriesCount(), STORMNODE_REJECTED, error);
                    return;
                }
            }

            BOOST_FOREACH(const CTxIn i, in){
                tx.vin.push_back(i);

                if(fDebug) LogPrintf("ssi -- tx in %s\n", i.ToString().c_str());

                CTransaction tx2;
                uint256 hash;
                //if(GetTransaction(i.prevout.hash, tx2, hash, true)){
        if(GetTransaction(i.prevout.hash, tx2, hash)){
                    if(tx2.vout.size() > i.prevout.n) {
                        nValueIn += tx2.vout[i.prevout.n].nValue;
                    }
                } else{
                    missingTx = true;
                }
            }

            if (nValueIn > SANDSTORM_POOL_MAX) {
                LogPrintf("ssi -- more than Sandstorm pool max! %s\n", tx.ToString().c_str());
                error = _("Value more than Sandstorm pool maximum allows.");
                pfrom->PushMessage("sssu", sessionID, GetState(), GetEntriesCount(), STORMNODE_REJECTED, error);
                return;
            }

            if(!missingTx){
                if (nValueIn-nValueOut > nValueIn*.01) {
                    LogPrintf("ssi -- fees are too high! %s\n", tx.ToString().c_str());
                    error = _("Transaction fees are too high.");
                    pfrom->PushMessage("sssu", sessionID, GetState(), GetEntriesCount(), STORMNODE_REJECTED, error);
                    return;
                }
            } else {
                LogPrintf("ssi -- missing input tx! %s\n", tx.ToString().c_str());
                error = _("Missing input transaction information.");
                pfrom->PushMessage("sssu", sessionID, GetState(), GetEntriesCount(), STORMNODE_REJECTED, error);
                return;
            }

            if(!AcceptableInputs(mempool, tx, false, false)){
                LogPrintf("ssi -- transaction not valid! \n");
                error = _("Transaction not valid.");
                pfrom->PushMessage("sssu", sessionID, GetState(), GetEntriesCount(), STORMNODE_REJECTED, error);
                return;
            }
        }

        if(AddEntry(in, nAmount, txCollateral, out, error)){
            pfrom->PushMessage("sssu", sessionID, GetState(), GetEntriesCount(), STORMNODE_ACCEPTED, error);
            Check();

            RelayStatus(sessionID, GetState(), GetEntriesCount(), STORMNODE_RESET);
        } else {
            pfrom->PushMessage("sssu", sessionID, GetState(), GetEntriesCount(), STORMNODE_REJECTED, error);
        }
    
    } else if (strCommand == "sssu") { //SandStorm status update
        if (pfrom->nVersion < MIN_SANDSTORM_PROTO_VERSION) 
        {
            return;
        }

        if(!pSubmittedToStormnode) return;
        if((CNetAddr)pSubmittedToStormnode->addr != (CNetAddr)pfrom->addr){
            //LogPrintf("sssu - message doesn't match current Stormnode - %s != %s\n", pSubmittedToStormnode->addr.ToString().c_str(), pfrom->addr.ToString().c_str());
            return;
        }

        int sessionIDMessage;
        int state;
        int entriesCount;
        int accepted;
        std::string error;
        vRecv >> sessionIDMessage >> state >> entriesCount >> accepted >> error;

        if(fDebug) LogPrintf("sssu - state: %i entriesCount: %i accepted: %i error: %s \n", state, entriesCount, accepted, error.c_str());

        if((accepted != 1 && accepted != 0) && sessionID != sessionIDMessage){
            LogPrintf("sssu - message doesn't match current Sandstorm session %d %d\n", sessionID, sessionIDMessage);
            return;
        }

        StatusUpdate(state, entriesCount, accepted, error, sessionIDMessage);

    } else if (strCommand == "sss") { //SandStorm Sign Final Tx
        
        if (pfrom->nVersion < MIN_SANDSTORM_PROTO_VERSION) {
            return;
        }

        vector<CTxIn> sigs;
        vRecv >> sigs;

        bool success = false;
        int count = 0;

        BOOST_FOREACH(const CTxIn item, sigs)
        {
            if(AddScriptSig(item)) success = true;
            if(fDebug) LogPrintf(" -- sigs count %d %d\n", (int)sigs.size(), count);
            count++;
        }

        if(success){
        sandStormPool.Check();
            RelayStatus(sandStormPool.sessionID, sandStormPool.GetState(), sandStormPool.GetEntriesCount(), STORMNODE_RESET);
        }
    } else if (strCommand == "ssf") { //SandStorm Final tx
        if (pfrom->nVersion < MIN_SANDSTORM_PROTO_VERSION) {
            return;
        }

        if(!pSubmittedToStormnode) return;
        if((CNetAddr)pSubmittedToStormnode->addr != (CNetAddr)pfrom->addr){
            //LogPrintf("ssc - message doesn't match current Stormnode - %s != %s\n", pSubmittedToStormnode->addr.ToString().c_str(), pfrom->addr.ToString().c_str());
            return;
        }

        int sessionIDMessage;
        CTransaction txNew;
        vRecv >> sessionIDMessage >> txNew;

        if(sessionID != sessionIDMessage){
            if (fDebug) LogPrintf("ssf - message doesn't match current Sandstorm session %d %d\n", sessionID, sessionIDMessage);
            return;
        }

        //check to see if input is spent already? (and probably not confirmed)
        SignFinalTransaction(txNew, pfrom);
    
    } else if (strCommand == "ssc") { //SandStorm Complete
    
        if (pfrom->nVersion < MIN_SANDSTORM_PROTO_VERSION) {
            return;
        }

        if(!pSubmittedToStormnode) return;
        if((CNetAddr)pSubmittedToStormnode->addr != (CNetAddr)pfrom->addr){
            //LogPrintf("ssc - message doesn't match current Stormnode - %s != %s\n", pSubmittedToStormnode->addr.ToString().c_str(), pfrom->addr.ToString().c_str());
            return;
        }

        int sessionIDMessage;
        bool error;
        std::string lastMessage;
        vRecv >> sessionIDMessage >> error >> lastMessage;

        if(sessionID != sessionIDMessage){
            if (fDebug) LogPrintf("ssc - message doesn't match current Sandstorm session %d %d\n", sandStormPool.sessionID, sessionIDMessage);
            return;
        }

        sandStormPool.CompletedTransaction(error, lastMessage);
    }

}

int randomizeList (int i) { return std::rand()%i;}

// Recursively determine the rounds of a given input (How deep is the Sandstorm chain for a given input)
int GetInputSandstormRounds(CTxIn in, int rounds)
{
    static std::map<uint256, CWalletTx> mDenomWtxes;

    if(rounds >= 17) return rounds;

    uint256 hash = in.prevout.hash;
    unsigned int nout = in.prevout.n;

    CWalletTx wtx;
    if(pwalletMain->GetTransaction(hash, wtx))
    {
        std::map<uint256, CWalletTx>::const_iterator mdwi = mDenomWtxes.find(hash);
        // not known yet, let's add it
        if(mdwi == mDenomWtxes.end())
        {
            if(fDebug) LogPrintf("GetInputSandstormRounds INSERTING %s\n", hash.ToString());
            mDenomWtxes[hash] = wtx;
        }
        // found and it's not an initial value, just return it
        else if(mDenomWtxes[hash].vout[nout].nRounds != -10)
        {
            return mDenomWtxes[hash].vout[nout].nRounds;
        }
        // bounds check
        if(nout >= wtx.vout.size())
        {
            mDenomWtxes[hash].vout[nout].nRounds = -4;
            if(fDebug) LogPrintf("GetInputSandstormRounds UPDATED   %s %3d %d\n", hash.ToString(), nout, mDenomWtxes[hash].vout[nout].nRounds);
            return mDenomWtxes[hash].vout[nout].nRounds;
        }

        mDenomWtxes[hash].vout[nout].nRounds = -3;
        if(pwalletMain->IsCollateralAmount(wtx.vout[nout].nValue))
        {
            mDenomWtxes[hash].vout[nout].nRounds = -3;
            if(fDebug) LogPrintf("GetInputSandstormRounds UPDATED   %s %3d %d\n", hash.ToString(), nout, mDenomWtxes[hash].vout[nout].nRounds);
            return mDenomWtxes[hash].vout[nout].nRounds;
        }

        mDenomWtxes[hash].vout[nout].nRounds = -2;
        if(/*rounds == 0 && */!pwalletMain->IsDenominatedAmount(wtx.vout[nout].nValue)) //NOT DENOM
        {
            mDenomWtxes[hash].vout[nout].nRounds = -2;
            if(fDebug) LogPrintf("GetInputSandstormRounds UPDATED   %s %3d %d\n", hash.ToString(), nout, mDenomWtxes[hash].vout[nout].nRounds);
            return mDenomWtxes[hash].vout[nout].nRounds;
        }

        bool fAllDenoms = true;
        BOOST_FOREACH(CTxOut out, wtx.vout)
        {
            fAllDenoms = fAllDenoms && pwalletMain->IsDenominatedAmount(out.nValue);
        }
        // this one is denominated but there is another non-denominated output found in the same tx
        if(!fAllDenoms)
        {
            mDenomWtxes[hash].vout[nout].nRounds = 0;
            if(fDebug) LogPrintf("GetInputSandstormRounds UPDATED   %s %3d %d\n", hash.ToString(), nout, mDenomWtxes[hash].vout[nout].nRounds);
            return mDenomWtxes[hash].vout[nout].nRounds;
        }
        int nShortest = -10; // an initial value, should be no way to get this by calculations
        bool fDenomFound = false;
        // only denoms here so let's look up
        BOOST_FOREACH(CTxIn in2, wtx.vin)
        {
            if(pwalletMain->IsMine(in2))
            {
                int n = GetInputSandstormRounds(in2, rounds+1);
                // denom found, find the shortest chain or initially assign nShortest with the first found value
                if(n >= 0 && (n < nShortest || nShortest == -10))
                {
                    nShortest = n;
                    fDenomFound = true;
                }
            }
        }
        mDenomWtxes[hash].vout[nout].nRounds = fDenomFound
                ? nShortest + 1 // good, we a +1 to the shortest one
                : 0;            // too bad, we are the fist one in that chain
        if(fDebug) LogPrintf("GetInputSandstormRounds UPDATED   %s %3d %d\n", hash.ToString(), nout, mDenomWtxes[hash].vout[nout].nRounds);
        return mDenomWtxes[hash].vout[nout].nRounds;
    }

    return rounds-1;
}

void CSandstormPool::Reset(){
    cachedLastSuccess = 0;
    vecStormnodesUsed.clear();
    UnlockCoins();
    SetNull();
}

void CSandstormPool::SetNull(bool clearEverything){
    finalTransaction.vin.clear();
    finalTransaction.vout.clear();

    entries.clear();

    state = POOL_STATUS_IDLE;

    lastTimeChanged = GetTimeMillis();

    entriesCount = 0;
    lastEntryAccepted = 0;
    countEntriesAccepted = 0;
    lastNewBlock = 0;

    sessionUsers = 0;
    sessionDenom = 0;
    sessionFoundStormnode = false;
    vecSessionCollateral.clear();
    txCollateral = CTransaction();

    if(clearEverything){
        myEntries.clear();
        sessionID = 0;
    }

    // -- seed random number generator (used for ordering output lists)
    unsigned int seed = 0;
    GetRandBytes((unsigned char*)&seed, sizeof(seed));
    std::srand(seed);
}

bool CSandstormPool::SetCollateralAddress(std::string strAddress){
    CDarkSilkAddress address;
    if (!address.SetString(strAddress))
    {
        LogPrintf("CSandstormPool::SetCollateralAddress - Invalid SandStorm collateral address\n");

        return false;
    }
    collateralPubKey = GetScriptForDestination(address.Get());
    return true;
}

//
// Unlock coins after Sandstorm fails or succeeds
//
void CSandstormPool::UnlockCoins(){
    BOOST_FOREACH(CTxIn v, lockedCoins)
        pwalletMain->UnlockCoin(v.prevout);

    lockedCoins.clear();
}

//
// Check the Sandstorm progress and send client updates if a Stormnode
//
void CSandstormPool::Check()
{
    if(fDebug && fStormNode) LogPrintf("CSandstormPool::Check() - entries count %lu\n", entries.size());

    //printf("CSandstormPool::Check() %d - %d - %d\n", state, anonTx.CountEntries(), GetTimeMillis()-lastTimeChanged);
   
    // If entries is full, then move on to the next phase
    if(state == POOL_STATUS_ACCEPTING_ENTRIES && (int)entries.size() >= GetMaxPoolTransactions())
    {
        if(fDebug) LogPrintf("CSandstormPool::Check() -- TRYING TRANSACTION \n");
        UpdateState(POOL_STATUS_FINALIZE_TRANSACTION);
    }

    // create the finalized transaction for distribution to the clients
    if(state == POOL_STATUS_FINALIZE_TRANSACTION) {
        if(fDebug) LogPrintf("CSandstormPool::Check() -- FINALIZE TRANSACTIONS\n");
        UpdateState(POOL_STATUS_SIGNING);

        if (fStormNode) {
            CTransaction txNew;
            // make our new transaction
            for(unsigned int i = 0; i < entries.size(); i++){
                BOOST_FOREACH(const CTxOut& v, entries[i].vout)
                    txNew.vout.push_back(v);

                BOOST_FOREACH(const CTxSSIn& s, entries[i].sev)
                    txNew.vin.push_back(s);
            }

            // shuffle the outputs for improved anonymity
            std::random_shuffle ( txNew.vin.begin(),  txNew.vin.end(),  randomizeList);
            std::random_shuffle ( txNew.vout.begin(), txNew.vout.end(), randomizeList);



            if(fDebug) LogPrintf("Transaction 1: %s\n", txNew.ToString().c_str());
            finalTransaction = txNew;

            // request signatures from clients
            RelayFinalTransaction(sessionID, finalTransaction);
        }
    }

    // If we have all of the signatures, try to compile the transaction
    if(state == POOL_STATUS_SIGNING && SignaturesComplete()) {
        if(fDebug) LogPrintf("CSandstormPool::Check() -- SIGNING\n");
        UpdateState(POOL_STATUS_TRANSMISSION);

        CheckFinalTransaction();
    }

    // reset if we're here for 10 seconds
    if((state == POOL_STATUS_ERROR || state == POOL_STATUS_SUCCESS) && GetTimeMillis()-lastTimeChanged >= 10000) {
        if(fDebug) LogPrintf("CSandstormPool::Check() -- RESETTING MESSAGE \n");
        SetNull(true);
        if(fStormNode) RelayStatus(sandStormPool.sessionID, sandStormPool.GetState(), sandStormPool.GetEntriesCount(), STORMNODE_RESET);
        UnlockCoins();
    }
}

void CSandstormPool::CheckFinalTransaction()
{
    CWalletTx txNew = CWalletTx(pwalletMain, finalTransaction);

    LOCK2(cs_main, pwalletMain->cs_wallet);
    {
        if (fStormNode) { //only the main node is master atm
            if(fDebug) LogPrintf("Transaction 2: %s\n", txNew.ToString().c_str());

            // See if the transaction is valid
            if (!txNew.AcceptToMemoryPool(false))
            {
                LogPrintf("CSandstormPool::Check() - CommitTransaction : Error: Transaction not valid\n");
                SetNull();
                pwalletMain->Lock();

                // not much we can do in this case
                UpdateState(POOL_STATUS_ACCEPTING_ENTRIES);

                RelayCompletedTransaction(sessionID, true, "Transaction not valid, please try again");
                return;
            }

            LogPrintf("CSandstormPool::Check() -- IS MASTER -- TRANSMITTING SandStorm\n");

            // sign a message

            int64_t sigTime = GetAdjustedTime();
            std::string strMessage = txNew.GetHash().ToString() + boost::lexical_cast<std::string>(sigTime);
            std::string strError = "";
            std::vector<unsigned char> vchSig;
            CKey key2;
            CPubKey pubkey2;

            if(!sandStormSigner.SetKey(strStormNodePrivKey, strError, key2, pubkey2))
            {
                LogPrintf("CSandstormPool::Check() - ERROR: Invalid Stormnodeprivkey: '%s'\n", strError.c_str());
                return;
            }

            if(!sandStormSigner.SignMessage(strMessage, strError, vchSig, key2)) {
                LogPrintf("CSandstormPool::Check() - Sign message failed\n");
                return;
            }

            if(!sandStormSigner.VerifyMessage(pubkey2, vchSig, strMessage, strError)) {
                LogPrintf("CSandstormPool::Check() - Verify message failed\n");
                return;
            }

            string txHash = txNew.GetHash().ToString().c_str();
            if(fDebug) LogPrintf("CSandstormPool::Check() -- txHash %d \n", txHash);
            if(!mapSandstormBroadcastTxes.count(txNew.GetHash())){
                CSandstormBroadcastTx dstx;
                dstx.tx = txNew;
                dstx.vin = activeStormnode.vin;
                dstx.vchSig = vchSig;
                dstx.sigTime = sigTime;

                mapSandstormBroadcastTxes.insert(make_pair(txNew.GetHash(), dstx));
            }

            // Broadcast the transaction to the network
            txNew.fTimeReceivedIsTxTime = true;
            txNew.RelayWalletTransaction();

            // Tell the clients it was successful
            RelayCompletedTransaction(sessionID, false, _("Transaction created successfully."));

            // Randomly charge clients
            ChargeRandomFees();

            // Reset
            if(fDebug) LogPrintf("CSandstormPool::Check() -- COMPLETED -- RESETTING \n");
            SetNull(true);
            UnlockCoins();
            if(fStormNode) RelayStatus(sandStormPool.sessionID, sandStormPool.GetState(), sandStormPool.GetEntriesCount(), STORMNODE_RESET);
            pwalletMain->Lock();
        }
    }   
}

//
// Charge clients a fee if they're abusive
//
// Why bother? Sandstorm uses collateral to ensure abuse to the process is kept to a minimum.
// The submission and signing stages in Sandstorm are completely separate. In the cases where
// a client submits a transaction then refused to sign, there must be a cost. Otherwise they
// would be able to do this over and over again and bring the mixing to a hault.
//
// How does this work? Messages to Stormnodes come in via "ssi", these require a valid collateral
// transaction for the client to be able to enter the pool. This transaction is kept by the Stormnode
// until the transaction is either complete or fails.
//
void CSandstormPool::ChargeFees(){
    if(fStormNode) {
        //we don't need to charge collateral for every offence.
        int offences = 0;
        int r = rand()%100;
        if(r > 33) return;

        if(state == POOL_STATUS_ACCEPTING_ENTRIES){
            BOOST_FOREACH(const CTransaction& txCollateral, vecSessionCollateral) {
                bool found = false;
                BOOST_FOREACH(const CSandStormEntry& v, entries) {
                    if(v.collateral == txCollateral) {
                        found = true;
                    }
                }

                // This queue entry didn't send us the promised transaction
                if(!found){
                    LogPrintf("CSandstormPool::ChargeFees -- found uncooperative node (didn't send transaction). Found offence.\n");
                    offences++;
                }
            }
        }

        if(state == POOL_STATUS_SIGNING) {
            // who didn't sign?
            BOOST_FOREACH(const CSandStormEntry v, entries) {
                BOOST_FOREACH(const CTxSSIn s, v.sev) {
                    if(!s.fHasSig){
                        LogPrintf("CSandstormPool::ChargeFees -- found uncooperative node (didn't sign). Found offence\n");
                         offences++;
                    }
                }
            }
        }

        r = rand()%100;
        int target = 0;

        //mostly offending?
        if(offences >= POOL_MAX_TRANSACTIONS-1 && r > 33) return;

        //everyone is an offender? That's not right
        if(offences >= POOL_MAX_TRANSACTIONS) return;

        //charge one of the offenders randomly
        if(offences > 1) target = 50;

        //pick random client to charge
        r = rand()%100;

        if(state == POOL_STATUS_ACCEPTING_ENTRIES){
            BOOST_FOREACH(const CTransaction& txCollateral, vecSessionCollateral) {
                bool found = false;
                BOOST_FOREACH(const CSandStormEntry& v, entries) {
                    if(v.collateral == txCollateral) {
                        found = true;
                    }
                }

                // This queue entry didn't send us the promised transaction
                if(!found && r > target){
                    LogPrintf("CSandstormPool::ChargeFees -- found uncooperative node (didn't send transaction). charging fees.\n");
 
                    CWalletTx wtxCollateral = CWalletTx(pwalletMain, txCollateral);

                    // Broadcast
                    if (!wtxCollateral.AcceptToMemoryPool(false))
                    {
                        // This must not fail. The transaction has already been signed and recorded.
                        LogPrintf("CSandstormPool::ChargeFees() : Error: Transaction not valid");
                    }
                    wtxCollateral.RelayWalletTransaction();
                    return;
                }
            }
        }

        if(state == POOL_STATUS_SIGNING) {
            // who didn't sign?
            BOOST_FOREACH(const CSandStormEntry v, entries) {
                BOOST_FOREACH(const CTxSSIn s, v.sev) {
                    if(!s.fHasSig && r > target){
                        LogPrintf("CSandstormPool::ChargeFees -- found uncooperative node (didn't sign). charging fees.\n");
 
                        CWalletTx wtxCollateral = CWalletTx(pwalletMain, v.collateral);

                        // Broadcast
                        if (!wtxCollateral.AcceptToMemoryPool(true))
                        {
                            // This must not fail. The transaction has already been signed and recorded.
                            LogPrintf("CSandstormPool::ChargeFees() : Error: Transaction not valid");
                        }
                        wtxCollateral.RelayWalletTransaction();
                        return;
                    }
                }
            }
        }
    }
}

// charge the collateral randomly
//  - Sandstorm is completely free, to pay miners we randomly pay the collateral of users.
void CSandstormPool::ChargeRandomFees(){
    if(fStormNode) {
        int i = 0;

        BOOST_FOREACH(const CTransaction& txCollateral, vecSessionCollateral) {
            int r = rand()%1000;

            /*
                Collateral Fee Charges:

                Being that SandStorm has "no fees" we need to have some kind of cost associated
                with using it to stop abuse. Otherwise it could serve as an attack vector and
                allow endless transaction that would bloat DarkSilk and make it unusable. To
                stop these kinds of attacks 1 in 50 successful transactions are charged. This
                adds up to a cost of 0.002DRK per transaction on average.
            */
            if(r <= 20)
            {
                LogPrintf("CSandstormPool::ChargeRandomFees -- charging random fees. %u\n", i);
 
                CWalletTx wtxCollateral = CWalletTx(pwalletMain, txCollateral);

                // Broadcast
                if (!wtxCollateral.AcceptToMemoryPool(true))
                {
                    // This must not fail. The transaction has already been signed and recorded.
                    LogPrintf("CSandstormPool::ChargeRandomFees() : Error: Transaction not valid");
                }
                wtxCollateral.RelayWalletTransaction();
            }
        }
    }
}

//
// Check for various timeouts (queue objects, Sandstorm, etc)
//
void CSandstormPool::CheckTimeout(){
    if(!fEnableSandstorm && !fStormNode) return;

    // catching hanging sessions
    if(!fStormNode) {
        if(state == POOL_STATUS_TRANSMISSION) {
            if(fDebug) LogPrintf("CSandstormPool::CheckTimeout() -- Session complete -- Running Check()\n");
            Check();
        }
    }

    // check Sandstorm queue objects for timeouts
    int c = 0;
    vector<CSandstormQueue>::iterator it;
    for(it=vecSandstormQueue.begin();it<vecSandstormQueue.end();it++){
        if((*it).IsExpired()){
            if(fDebug) LogPrintf("CSandstormPool::CheckTimeout() : Removing expired queue entry - %d\n", c);
            vecSandstormQueue.erase(it);
            break;
        }
        c++;
    }

    int addLagTime = 0;
    if(!fStormNode) addLagTime = 10000; //if we're the client, give the server a few extra seconds before resetting.

    if(state == POOL_STATUS_ACCEPTING_ENTRIES || state == POOL_STATUS_QUEUE){
        c = 0;

        // if it's a Stormnode, the entries are stored in "entries", otherwise they're stored in myEntries
        std::vector<CSandStormEntry> *vec = &myEntries;
        if(fStormNode) vec = &entries;

        // check for a timeout and reset if needed
        vector<CSandStormEntry>::iterator it2;
        for(it2=vec->begin();it2<vec->end();it2++){
            if((*it2).IsExpired()){
                if(fDebug) LogPrintf("CSandstormPool::CheckTimeout() : Removing expired entry - %d\n", c);
                vec->erase(it2);
                if(entries.size() == 0 && myEntries.size() == 0){
                    SetNull(true);
                    UnlockCoins();
                }
                if(fStormNode){
                    RelayStatus(sandStormPool.sessionID, sandStormPool.GetState(), sandStormPool.GetEntriesCount(), STORMNODE_RESET);
                }
                break;
            }
            c++;
        }

        if(GetTimeMillis()-lastTimeChanged >= (SANDSTORM_QUEUE_TIMEOUT*1000)+addLagTime){
            lastTimeChanged = GetTimeMillis();

            SetNull(true);
        }
    } else if(GetTimeMillis()-lastTimeChanged >= (SANDSTORM_QUEUE_TIMEOUT*1000)+addLagTime){
        if(fDebug) LogPrintf("CSandstormPool::CheckTimeout() -- Session timed out (30s) -- resetting\n");
        SetNull();
        UnlockCoins();

        UpdateState(POOL_STATUS_ERROR);
        lastMessage = _("Session timed out, please resubmit.");
    }

    if(state == POOL_STATUS_SIGNING && GetTimeMillis()-lastTimeChanged >= (SANDSTORM_SIGNING_TIMEOUT*1000)+addLagTime ) {
            if(fDebug) LogPrintf("CSandstormPool::CheckTimeout() -- Session timed out -- restting\n");
            ChargeFees();
            SetNull();
            UnlockCoins();
            //add my transactions to the new session

            UpdateState(POOL_STATUS_ERROR);
            lastMessage = _("Signing timed out, please resubmit.");
    }
}

//
// Check for complete queue
//
void CSandstormPool::CheckForCompleteQueue(){
    if(!fEnableSandstorm && !fStormNode) return;

    /* Check to see if we're ready for submissions from clients */
    // 
    // After receiving multiple ssa messages, the queue will switch to "accepting entries"
    // which is the active state right before merging the transaction
    //
    if(state == POOL_STATUS_QUEUE && sessionUsers == GetMaxPoolTransactions()) {
        UpdateState(POOL_STATUS_ACCEPTING_ENTRIES);

        CSandstormQueue ssq;
        ssq.nDenom = sessionDenom;
        ssq.vin = activeStormnode.vin;
        ssq.time = GetTime();
        ssq.ready = true;
        ssq.Sign();
        ssq.Relay();
    }
}

// check to see if the signature is valid
bool CSandstormPool::SignatureValid(const CScript& newSig, const CTxIn& newVin){
    CTransaction txNew;
    txNew.vin.clear();
    txNew.vout.clear();

    int found = -1;
    CScript sigPubKey = CScript();
    unsigned int i = 0;

    BOOST_FOREACH(CSandStormEntry& e, entries) {
        BOOST_FOREACH(const CTxOut& out, e.vout)
            txNew.vout.push_back(out);

        BOOST_FOREACH(const CTxSSIn& s, e.sev){
            txNew.vin.push_back(s);

            if(s == newVin){
                 found = i;
                sigPubKey = s.prevPubKey;
            }
            i++;
        }
    }

    if(found >= 0){ //might have to do this one input at a time?
        int n = found;
        txNew.vin[n].scriptSig = newSig;
        if(fDebug) LogPrintf("CSandstormPool::SignatureValid() - Sign with sig %s\n", newSig.ToString().substr(0,24).c_str());
        if (!VerifyScript(txNew.vin[n].scriptSig, sigPubKey, SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_STRICTENC, SignatureChecker(txNew, i))){
            if(fDebug) LogPrintf("CSandstormPool::SignatureValid() - Signing - Error signing input %u\n", n);
            return false;
        }
    }

    if(fDebug) LogPrintf("CSandstormPool::SignatureValid() - Signing - Successfully validated input\n");
    return true;
}

// check to make sure the collateral provided by the client is valid
bool CSandstormPool::IsCollateralValid(const CTransaction& txCollateral){
    if(txCollateral.vout.size() < 1) return false;
    if(txCollateral.nLockTime != 0) return false;

    int64_t nValueIn = 0;
    int64_t nValueOut = 0;
    bool missingTx = false;

    BOOST_FOREACH(const CTxOut o, txCollateral.vout){
        nValueOut += o.nValue;

        if(!o.scriptPubKey.IsNormalPaymentScript()){
            LogPrintf ("CSandstormPool::IsCollateralValid - Invalid Script %s\n", txCollateral.ToString().c_str());
            return false;
        }
    }

    BOOST_FOREACH(const CTxIn i, txCollateral.vin){
        CTransaction tx2;
        uint256 hash;
        //if(GetTransaction(i.prevout.hash, tx2, hash, true)){
    if(GetTransaction(i.prevout.hash, tx2, hash)){
            if(tx2.vout.size() > i.prevout.n) {
                nValueIn += tx2.vout[i.prevout.n].nValue;
            }
        } else{
            missingTx = true;
        }
    }

    if(missingTx){
        if(fDebug) LogPrintf ("CSandstormPool::IsCollateralValid - Unknown inputs in collateral transaction - %s\n", txCollateral.ToString().c_str());
        return false;
    }

    //collateral transactions are required to pay out SANDSTORM_COLLATERAL as a fee to the miners
    if(nValueIn - nValueOut < SANDSTORM_COLLATERAL) {
        if(fDebug) LogPrintf ("CSandstormPool::IsCollateralValid - did not include enough fees in transaction %d\n%s\n", nValueOut-nValueIn, txCollateral.ToString().c_str());
        return false;
    }

    if(fDebug) LogPrintf("CSandstormPool::IsCollateralValid %s\n", txCollateral.ToString().c_str());
 
    CValidationState state;
    if(!AcceptableInputs(mempool, txCollateral, false, false)){
        if(fDebug) LogPrintf ("CSandstormPool::IsCollateralValid - didn't pass IsAcceptable\n");
        return false;
    }

    return true;
}


//
// Add a clients transaction to the pool
//
bool CSandstormPool::AddEntry(const std::vector<CTxIn>& newInput, const int64_t& nAmount, const CTransaction& txCollateral, const std::vector<CTxOut>& newOutput, std::string& error){
     if (!fStormNode) return false;

    BOOST_FOREACH(CTxIn in, newInput) {
        if (in.prevout.IsNull() || nAmount < 0) {
            if(fDebug) LogPrintf ("CSandstormPool::AddEntry - input not valid!\n");
            error = _("Input is not valid.");
            sessionUsers--;
            return false;
        }
    }

    if (!IsCollateralValid(txCollateral)){
        if(fDebug) LogPrintf ("CSandstormPool::AddEntry - collateral not valid!\n");
        error = _("Collateral is not valid.");
        sessionUsers--;
        return false;
    }

    if((int)entries.size() >= GetMaxPoolTransactions()){
        if(fDebug) LogPrintf ("CSandstormPool::AddEntry - entries is full!\n");
        error = _("Entries are full.");
        sessionUsers--;
        return false;
    }

    BOOST_FOREACH(CTxIn in, newInput) {
        if(fDebug) LogPrintf("looking for vin -- %s\n", in.ToString().c_str());
        BOOST_FOREACH(const CSandStormEntry& v, entries) {
            BOOST_FOREACH(const CTxSSIn& s, v.sev){
                if((CTxIn)s == in) {
                    if(fDebug) LogPrintf ("CSandstormPool::AddEntry - found in vin\n");
                     error = _("Already have that input.");
                    sessionUsers--;
                    return false;
                }
            }
        }
    }

    CSandStormEntry v;
    v.Add(newInput, nAmount, txCollateral, newOutput);
    entries.push_back(v);

    if(fDebug) LogPrintf("CSandstormPool::AddEntry -- adding %s\n", newInput[0].ToString().c_str());
    error = "";

    return true;
}

bool CSandstormPool::AddScriptSig(const CTxIn& newVin){
    if(fDebug) LogPrintf("CSandstormPool::AddScriptSig -- new sig  %s\n", newVin.scriptSig.ToString().substr(0,24).c_str());


    BOOST_FOREACH(const CSandStormEntry& v, entries) {
        BOOST_FOREACH(const CTxSSIn& s, v.sev){
            if(s.scriptSig == newVin.scriptSig) {
                printf("CSandstormPool::AddScriptSig - already exists \n");
                return false;
            }
        }
    }

    if(!SignatureValid(newVin.scriptSig, newVin)){
        if(fDebug) LogPrintf("CSandstormPool::AddScriptSig - Invalid Sig\n");
        return false;
    }

    if(fDebug) LogPrintf("CSandstormPool::AddScriptSig -- sig %s\n", newVin.ToString().c_str());

    BOOST_FOREACH(CTxIn& vin, finalTransaction.vin){
        if(newVin.prevout == vin.prevout && vin.nSequence == newVin.nSequence){
            vin.scriptSig = newVin.scriptSig;
            vin.prevPubKey = newVin.prevPubKey;
            if(fDebug) LogPrintf("CSandStromPool::AddScriptSig -- adding to finalTransaction  %s\n", newVin.scriptSig.ToString().substr(0,24).c_str());
        }
    }
    for(unsigned int i = 0; i < entries.size(); i++){
        if(entries[i].AddSig(newVin)){
            if(fDebug) LogPrintf("CSandStormPool::AddScriptSig -- adding  %s\n", newVin.scriptSig.ToString().substr(0,24).c_str());
            return true;
        }
    }

    LogPrintf("CSandstormPool::AddScriptSig -- Couldn't set sig!\n" );
    return false;
}


// check to make sure everything is signed
bool CSandstormPool::SignaturesComplete(){

    BOOST_FOREACH(const CSandStormEntry& v, entries) {
        BOOST_FOREACH(const CTxSSIn& s, v.sev){
            if(!s.fHasSig) return false;
        }
    }
    return true;
}

//
// Execute a Sandstorm denomination via a Stormnode.
// This is only ran from clients
//
void CSandstormPool::SendSandstormDenominate(std::vector<CTxIn>& vin, std::vector<CTxOut>& vout, int64_t amount){
    if(sandStormPool.txCollateral == CTransaction()){
        LogPrintf ("CSandstormPool:SendSandstormDenominate() - Sandstorm collateral not set");
        return;
    }

    // lock the funds we're going to use
    BOOST_FOREACH(CTxIn in, txCollateral.vin)
        lockedCoins.push_back(in);

    BOOST_FOREACH(CTxIn in, vin)
        lockedCoins.push_back(in);

    //BOOST_FOREACH(CTxOut o, vout)
    //    LogPrintf(" vout - %s\n", o.ToString().c_str());


    // we should already be connected to a Stormnode
    if(!sessionFoundStormnode){
        LogPrintf("CSandstormPool::SendSandstormDenominate() - No Stormnode has been selected yet.\n");
        UnlockCoins();
        SetNull(true);
        return;
    }

    if (!CheckDiskSpace())
        return;

    if(fStormNode) {
        LogPrintf("CSandstormPool::SendSandstormDenominate() - SandStorm from a Stormnode is not supported currently.\n");
        return;
    }

    UpdateState(POOL_STATUS_ACCEPTING_ENTRIES);

    LogPrintf("CSandstormPool::SendSandstormDenominate() - Added transaction to pool.\n");
 
    ClearLastMessage();

    //check it against the memory pool to make sure it's valid
    {
        int64_t nValueOut = 0;

        CValidationState state;
        CTransaction tx;

        BOOST_FOREACH(const CTxOut& o, vout){
            nValueOut += o.nValue;
            tx.vout.push_back(o);
        }

        BOOST_FOREACH(const CTxIn& i, vin){
            tx.vin.push_back(i);

            if(fDebug) LogPrintf("ssi -- tx in %s\n", i.ToString().c_str());
        }

        LogPrintf("Submitting tx %s\n", tx.ToString().c_str());

        
        if(!AcceptableInputs(mempool, tx, false)){
            LogPrintf("ssi -- transaction not valid! %s \n", tx.ToString().c_str());
            return;
        }
    }

    // store our entry for later use
    CSandStormEntry e;
    e.Add(vin, amount, txCollateral, vout);
    myEntries.push_back(e);


    RelayIn(myEntries[0].sev, myEntries[0].amount, txCollateral, myEntries[0].vout);
    Check();
}

// Incoming message from Stormnode updating the progress of Sandstorm
//    newAccepted:  -1 mean's it'n not a "transaction accepted/not accepted" message, just a standard update
//                  0 means transaction was not accepted
//                  1 means transaction was accepted

bool CSandstormPool::StatusUpdate(int newState, int newEntriesCount, int newAccepted, std::string& error, int newSessionID){
    if(fStormNode) return false;
    if(state == POOL_STATUS_ERROR || state == POOL_STATUS_SUCCESS) return false;

    UpdateState(newState);
    entriesCount = newEntriesCount;

    if(error.size() > 0) strAutoDenomResult = _("Stormnode:") + " " + error;

    if(newAccepted != -1) {
        lastEntryAccepted = newAccepted;
        countEntriesAccepted += newAccepted;
        if(newAccepted == 0){
            UpdateState(POOL_STATUS_ERROR);
            lastMessage = error;
        }

        if(newAccepted == 1 && newSessionID != 0) {
            sessionID = newSessionID;
            LogPrintf("CSandstormPool::StatusUpdate - set sessionID to %d\n", sessionID);
             sessionFoundStormnode = true;
        }
    }

    if(newState == POOL_STATUS_ACCEPTING_ENTRIES){
        if(newAccepted == 1){
            LogPrintf("CSandstormPool::StatusUpdate - entry accepted! \n");
            sessionFoundStormnode = true;
            //wait for other users. Stormnode will report when ready
            UpdateState(POOL_STATUS_QUEUE);
        } else if (newAccepted == 0 && sessionID == 0 && !sessionFoundStormnode) {
            LogPrintf("CSandstormPool::StatusUpdate - entry not accepted by Stormnode \n");
            UnlockCoins();
            UpdateState(POOL_STATUS_ACCEPTING_ENTRIES);
            DoAutomaticDenominating(); //try another Stormnode
        }
        if(sessionFoundStormnode) return true;
    }

    return true;
}

//
// After we receive the finalized transaction from the Stormnode, we must
// check it to make sure it's what we want, then sign it if we agree.
// If we refuse to sign, it's possible we'll be charged collateral
//
bool CSandstormPool::SignFinalTransaction(CTransaction& finalTransactionNew, CNode* node){
    if(fStormNode) return false;

    finalTransaction = finalTransactionNew;
    LogPrintf("CSandstormPool::SignFinalTransaction %s\n", finalTransaction.ToString().c_str());
 
    vector<CTxIn> sigs;

    //make sure my inputs/outputs are present, otherwise refuse to sign
    BOOST_FOREACH(const CSandStormEntry e, myEntries) {
        BOOST_FOREACH(const CTxSSIn s, e.sev) {
            /* Sign my transaction and all outputs */
            int mine = -1;
            CScript prevPubKey = CScript();
            CTxIn vin = CTxIn();

            for(unsigned int i = 0; i < finalTransaction.vin.size(); i++){
                if(finalTransaction.vin[i] == s){
                    mine = i;
                    prevPubKey = s.prevPubKey;
                    vin = s;
                }
            }

            if(mine >= 0){ //might have to do this one input at a time?
                
                int foundOutputs = 0;
                int64_t nValue1 = 0;
                int64_t nValue2 = 0;

                for(unsigned int i = 0; i < finalTransaction.vout.size(); i++){
                    BOOST_FOREACH(const CTxOut& o, e.vout) {
                       string Ftx = finalTransaction.vout[i].scriptPubKey.ToString().c_str();
                       string Otx = o.scriptPubKey.ToString().c_str();
                        if(Ftx == Otx){
                            if(fDebug) LogPrintf("CSandstormPool::SignFinalTransaction - foundOutputs = %d \n", foundOutputs);
                            foundOutputs++;
                            nValue1 += finalTransaction.vout[i].nValue;
                        }
                    }
                }

                BOOST_FOREACH(const CTxOut o, e.vout)
                    nValue2 += o.nValue;

                int targetOuputs = e.vout.size();
                if(foundOutputs < targetOuputs || nValue1 != nValue2) {
                    // in this case, something went wrong and we'll refuse to sign. It's possible we'll be charged collateral. But that's
                    // better then signing if the transaction doesn't look like what we wanted.
                    LogPrintf("CSandstormPool::Sign - My entries are not correct! Refusing to sign. %d entries %d target. \n", foundOutputs, targetOuputs);
                    
                    return false;
                }
                
                if(fDebug) LogPrintf("CSandstormPool::Sign - Signing my input %i\n", mine);
                if(!SignSignature(*pwalletMain, prevPubKey, finalTransaction, mine, int(SIGHASH_ALL|SIGHASH_ANYONECANPAY))) { // changes scriptSig
                    if(fDebug) LogPrintf("CSandstormPool::Sign - Unable to sign my own transaction! \n");
                    // not sure what to do here, it will timeout...?
                }

                sigs.push_back(finalTransaction.vin[mine]);
                if(fDebug) LogPrintf(" -- sss %d %d %s\n", mine, (int)sigs.size(), finalTransaction.vin[mine].scriptSig.ToString().c_str());                    
            }

        }

        if(fDebug) LogPrintf("CSandstormPool::Sign - txNew:\n%s", finalTransaction.ToString().c_str());
    }

   // push all of our signatures to the Stormnode
   if(sigs.size() > 0 && node != NULL)
       node->PushMessage("sss", sigs);

    return true;
}

void CSandstormPool::NewBlock()
{
    if(fDebug) LogPrintf("CSandstormPool::NewBlock \n");

    //we we're processing lots of blocks, we'll just leave
    if(GetTime() - lastNewBlock < 10) return;
    lastNewBlock = GetTime();

    sandStormPool.CheckTimeout();

    if(!fEnableSandstorm) return;

    if(!fStormNode){
        //denominate all non-denominated inputs every 25 minutes.
        if(pindexBest->nHeight % 10 == 0) UnlockCoins();
    }
}

// Sandstorm transaction was completed (failed or successed)
void CSandstormPool::CompletedTransaction(bool error, std::string lastMessageNew)
{
    if(fStormNode) return;

    if(error){
        LogPrintf("CompletedTransaction -- error \n");
        UpdateState(POOL_STATUS_ERROR);
        Check();
        UnlockCoins();
    } else {
        LogPrintf("CompletedTransaction -- success \n");
        UpdateState(POOL_STATUS_SUCCESS);

        myEntries.clear();
        UnlockCoins();
        if(!fStormNode) SetNull(true);

        // To avoid race conditions, we'll only let SS run once per block
        cachedLastSuccess = pindexBest->nHeight;
    }
    lastMessage = lastMessageNew;

    completedTransaction = true;
}

void CSandstormPool::ClearLastMessage()
{
    lastMessage = "";
}

//
// Passively run Sandstorm in the background to anonymize funds based on the given configuration.
//
// This does NOT run by default for daemons, only for Qt.
//
bool CSandstormPool::DoAutomaticDenominating(bool fDryRun, bool ready)
{
    LOCK(cs_sandstorm);

    if(IsInitialBlockDownload()) return false;

    if(fStormNode) return false;
    if(state == POOL_STATUS_ERROR || state == POOL_STATUS_SUCCESS) return false;

    if(pindexBest->nHeight - cachedLastSuccess < minBlockSpacing) {
        LogPrintf("CSandstormPool::DoAutomaticDenominating - Last successful Sandstorm action was too recent\n");
        strAutoDenomResult = _("Last successful Sandstorm action was too recent.");
        return false;
    }
    if(!fEnableSandstorm) {
        if(fDebug) LogPrintf("CSandstormPool::DoAutomaticDenominating - Sandstorm is disabled\n");
        strAutoDenomResult = _("Sandstorm is disabled.");
        return false;
    }

    if (!fDryRun && pwalletMain->IsLocked()){
        strAutoDenomResult = _("Wallet is locked.");
        return false;
    }

    if(sandStormPool.GetState() != POOL_STATUS_ERROR && sandStormPool.GetState() != POOL_STATUS_SUCCESS){
        if(sandStormPool.GetMyTransactionCount() > 0){
            return true;
        }
    }

    if(snodeman.size() == 0){
        if(fDebug) LogPrintf("CSandstormPool::DoAutomaticDenominating - No Stormnodes detected\n");
        strAutoDenomResult = _("No Stormnodes detected.");
        return false;
    }

    // ** find the coins we'll use
    std::vector<CTxIn> vCoins;    int64_t nValueMin = CENT;
    int64_t nValueIn = 0;

    // should not be less than fees in SANDSTORM_COLLATERAL + few (lets say 5) smallest denoms
    int64_t nLowestDenom = SANDSTORM_COLLATERAL + sandStormDenominations[sandStormDenominations.size() - 1]*5;

    // if there are no SS collateral inputs yet
    if(!pwalletMain->HasCollateralInputs())
        // should have some additional amount for them
        nLowestDenom += SANDSTORM_COLLATERAL*4;

    int64_t nBalanceNeedsAnonymized = nAnonymizeDarkSilkAmount*COIN - pwalletMain->GetAnonymizedBalance();

    // if balanceNeedsAnonymized is more than pool max, take the pool max
    if(nBalanceNeedsAnonymized > SANDSTORM_POOL_MAX) nBalanceNeedsAnonymized = SANDSTORM_POOL_MAX;

    // if balanceNeedsAnonymized is more than non-anonymized, take non-anonymized
    int64_t nBalanceNotYetAnonymized = pwalletMain->GetBalance() - pwalletMain->GetAnonymizedBalance();
    if(nBalanceNeedsAnonymized > nBalanceNotYetAnonymized) nBalanceNeedsAnonymized = nBalanceNotYetAnonymized;

    if(nBalanceNeedsAnonymized < nLowestDenom)
    {
        LogPrintf("DoAutomaticDenominating : No funds detected in need of denominating \n");
        strAutoDenomResult = _("No funds detected in need of denominating.");
        return false;
    }

    if (fDebug) LogPrintf("DoAutomaticDenominating : nLowestDenom=%d, nBalanceNeedsAnonymized=%d\n", nLowestDenom, nBalanceNeedsAnonymized);

    // select coins that should be given to the pool
    if (!pwalletMain->SelectCoinsDark(nValueMin, nBalanceNeedsAnonymized, vCoins, nValueIn, 0, nSandstormRounds))
    {
        nValueIn = 0;
        vCoins.clear();

        if (pwalletMain->SelectCoinsDark(nValueMin, 9999999*COIN, vCoins, nValueIn, -2, 0))
        {
            if(!fDryRun) return CreateDenominated(nBalanceNeedsAnonymized);
            return true;
        } else {
            LogPrintf("DoAutomaticDenominating : Can't denominate - no compatible inputs left\n");
            strAutoDenomResult = _("Can't denominate: no compatible inputs left.");
            return false;
        }

    }

    //check to see if we have the collateral sized inputs, it requires these
    if(!pwalletMain->HasCollateralInputs()){
        if(!fDryRun) MakeCollateralAmounts();
        return true;
    }

    if(fDryRun) return true;

    std::vector<CTxOut> vOut;

    // initial phase, find a Stormnode
    if(!sessionFoundStormnode){
        int nUseQueue = rand()%100;
        UpdateState(POOL_STATUS_ACCEPTING_ENTRIES);

        sessionTotalValue = pwalletMain->GetTotalValue(vCoins);

        //randomize the amounts we mix
        if(sessionTotalValue > nBalanceNeedsAnonymized) sessionTotalValue = nBalanceNeedsAnonymized;

        double fSilkSubmitted = (sessionTotalValue / CENT);
        LogPrintf("Submitting Sandstorm for %f DRKSLK CENT - sessionTotalValue %d\n", fSilkSubmitted, sessionTotalValue);

        if(pwalletMain->GetDenominatedBalance(true, true) > 0){ //get denominated unconfirmed inputs
            LogPrintf("DoAutomaticDenominating -- Found unconfirmed denominated outputs, will wait till they confirm to continue.\n");
            strAutoDenomResult = _("Found unconfirmed denominated outputs, will wait till they confirm to continue.");
            return false;
        }

        //check our collateral
        if(txCollateral != CTransaction()){
            if(!IsCollateralValid(txCollateral)) {
                txCollateral = CTransaction();
                LogPrintf("DoAutomaticDenominating -- Invalid collateral, resetting.\n");
            }
        }

        //don't use the queues all of the time for mixing
        if(nUseQueue > 33){

            // Look through the queues and see if anything matches
            BOOST_FOREACH(CSandstormQueue& ssq, vecSandstormQueue){
                CService addr;
                if(ssq.time == 0) continue;

                if(!ssq.GetAddress(addr)) continue;
                if(ssq.IsExpired()) continue;

                int protocolVersion;
                if(!ssq.GetProtocolVersion(protocolVersion)) continue;
                if(protocolVersion < MIN_SANDSTORM_PROTO_VERSION) continue;

                //non-denom's are incompatible
                if((ssq.nDenom & (1 << 4))) continue;

                //don't reuse Stormnodes
                BOOST_FOREACH(CTxIn usedVin, vecStormnodesUsed){
                    if(ssq.vin == usedVin) {
                        continue;
                    }
                }

                std::vector<CTxIn> vTempCoins;
                std::vector<COutput> vTempCoins2;
                // Try to match their denominations if possible
                if (!pwalletMain->SelectCoinsByDenominations(ssq.nDenom, nValueMin, nBalanceNeedsAnonymized, vTempCoins, vTempCoins2, nValueIn, 0, nSandstormRounds)){
                    LogPrintf("DoAutomaticDenominating - Couldn't match denominations %d\n", ssq.nDenom);
                    continue;
                }

                // connect to Stormnode and submit the queue request
                if(ConnectNode((CAddress)addr, NULL, true)){
                    

                    CNode* pNode = FindNode(addr);
                    if(pNode)
                    {
                        std::string strReason;
                        if(txCollateral == CTransaction()){
                            if(!pwalletMain->CreateCollateralTransaction(txCollateral, strReason)){
                                LogPrintf("DoAutomaticDenominating -- ssa error:%s\n", strReason.c_str());
                                return false;
                            }
                        }

                        CStormnode* psn = snodeman.Find(ssq.vin);
                        if(psn == NULL)
                        {
                            LogPrintf("DoAutomaticDenominating --- ssq vin %s is not in stormnode list!", ssq.vin.ToString());
                            continue;
                        }
                        pSubmittedToStormnode = psn;
                        vecStormnodesUsed.push_back(ssq.vin);
                        sessionDenom = ssq.nDenom;

                        pNode->PushMessage("ssa", sessionDenom, txCollateral);
                        LogPrintf("DoAutomaticDenominating --- connected (from queue), sending ssa for %d %d - %s\n", sessionDenom, GetDenominationsByAmount(sessionTotalValue), pNode->addr.ToString().c_str());
                        strAutoDenomResult = "";
                        return true;
                    }
                } else {
                    LogPrintf("DoAutomaticDenominating --- error connecting \n");
                    strAutoDenomResult = _("Error connecting to Stormnode.");
                    ssq.time = 0; //remove node
                    return DoAutomaticDenominating();
                }
            }
        }

        int i = 0;

        // otherwise, try one randomly
        while(i < 10)
        {   
            CStormnode* psn = snodeman.FindRandom();
            if(psn == NULL)
            {
                LogPrintf("DoAutomaticDenominating --- Stormnode list is empty!\n");
                return false;
            }
            //don't reuse Stormnodes
            BOOST_FOREACH(CTxIn usedVin, vecStormnodesUsed) {
                if(psn->vin == usedVin){
                    i++;
                    continue;
                }
            }
            if(psn->protocolVersion < MIN_SANDSTORM_PROTO_VERSION) {
                i++;
                continue;
            }

            if(psn->nLastSsq != 0 &&
                psn->nLastSsq + snodeman.CountStormnodesAboveProtocol(MIN_SANDSTORM_PROTO_VERSION)/5 > snodeman.nSsqCount){
                i++;
                continue;
            }

            lastTimeChanged = GetTimeMillis();
            LogPrintf("DoAutomaticDenominating -- attempt %d connection to Stormnode %s\n", i, psn->addr.ToString().c_str());
            if(ConnectNode((CAddress)psn->addr, NULL, true)) {
                LOCK(cs_vNodes);
                BOOST_FOREACH(CNode* pnode, vNodes)
                {
                    if((CNetAddr)pnode->addr != (CNetAddr)psn->addr) continue;

                    std::string strReason;
                    if(txCollateral == CTransaction()){
                        if(!pwalletMain->CreateCollateralTransaction(txCollateral, strReason)){
                            LogPrintf("DoAutomaticDenominating -- create collateral error:%s\n", strReason.c_str());
                            return false;
                        }
                    }

                    pSubmittedToStormnode = psn;
                    vecStormnodesUsed.push_back(psn->vin);

                    std::vector<int64_t> vecAmounts;
                    pwalletMain->ConvertList(vCoins, vecAmounts);
                    sessionDenom = GetDenominationsByAmounts(vecAmounts);

                    pnode->PushMessage("ssa", sessionDenom, txCollateral);
                    LogPrintf("DoAutomaticDenominating --- connected, sending ssa for %d - denom %d\n", sessionDenom, GetDenominationsByAmount(sessionTotalValue));
                    strAutoDenomResult = "";
                    return true;
                }
            } else {
                i++;
                continue;
            }
        }

        strAutoDenomResult = _("No compatible Stormnode found.");
        return false;
    }

    strAutoDenomResult = "";
    if(!ready) return true;

    if(sessionDenom == 0) return true;

    return false;
}


bool CSandstormPool::PrepareSandstormDenominate()
{
    // Submit transaction to the pool if we get here, use sessionDenom so we use the same amount of money
    std::string strError = pwalletMain->PrepareSandstormDenominate(0, nSandstormRounds);
    LogPrintf("DoAutomaticDenominating : Running Sandstorm denominate. Return '%s'\n", strError.c_str());

    if(strError == "") return true;

    strAutoDenomResult = strError;
    LogPrintf("DoAutomaticDenominating : Error running denominate, %s\n", strError.c_str());
    return false;
}

bool CSandstormPool::SendRandomPaymentToSelf()
{
    int64_t nBalance = pwalletMain->GetBalance();
    int64_t nPayment = (nBalance*0.35) + (rand() % nBalance);

    if(nPayment > nBalance) nPayment = nBalance-(0.1*COIN);

    // make our change address
    CReserveKey reservekey(pwalletMain);

    CScript scriptChange;
    CPubKey vchPubKey;
    assert(reservekey.GetReservedKey(vchPubKey)); // should never fail, as we just unlocked
    scriptChange = GetScriptForDestination(vchPubKey.GetID());

    CWalletTx wtx;
    int64_t nFeeRet = 0;
    std::string strFail = "";
    vector< pair<CScript, int64_t> > vecSend;

    // ****** Add fees ************ /
    vecSend.push_back(make_pair(scriptChange, nPayment));

    CCoinControl *coinControl=NULL;
    int32_t nChangePos;
    bool success = pwalletMain->CreateTransaction(vecSend, wtx, reservekey, nFeeRet, nChangePos, strFail, coinControl, ONLY_DENOMINATED);
    if(!success){
        LogPrintf("SendRandomPaymentToSelf: Error - %s\n", strFail.c_str());
        return false;
    }

    pwalletMain->CommitTransaction(wtx, reservekey);

    LogPrintf("SendRandomPaymentToSelf Success: tx %s\n", wtx.GetHash().GetHex().c_str());

    return true;
}

// Split up large inputs or create fee sized inputs
bool CSandstormPool::MakeCollateralAmounts()
{
    // make our change address
    CReserveKey reservekey(pwalletMain);

    CScript scriptChange;
    CPubKey vchPubKey;
    assert(reservekey.GetReservedKey(vchPubKey)); // should never fail, as we just unlocked
    scriptChange= GetScriptForDestination(vchPubKey.GetID());

    CWalletTx wtx;
    int64_t nFeeRet = 0;
    std::string strFail = "";
    vector< pair<CScript, int64_t> > vecSend;

    vecSend.push_back(make_pair(scriptChange, SANDSTORM_COLLATERAL*4));

    CCoinControl *coinControl=NULL;
    int32_t nChangePos;
    // try to use non-denominated and not sn-like funds
    bool success = pwalletMain->CreateTransaction(vecSend, wtx, reservekey,
            nFeeRet, nChangePos, strFail, coinControl, ONLY_NONDENOMINATED_NOTSN);
    if(!success){
        // if we failed (most likeky not enough funds), try to use denominated instead -
        // SN-like funds should not be touched in any case and we can't mix denominated without collaterals anyway
        success = pwalletMain->CreateTransaction(vecSend, wtx, reservekey,
                nFeeRet, nChangePos, strFail, coinControl, ONLY_DENOMINATED);
        if(!success){
            LogPrintf("MakeCollateralAmounts: Error - %s\n", strFail.c_str());
            return false;
        }
    }

    // use the same cachedLastSuccess as for SS mixinx to prevent race
    if(pwalletMain->CommitTransaction(wtx, reservekey))
        cachedLastSuccess = pindexBest->nHeight;

    LogPrintf("MakeCollateralAmounts Success: tx %s\n", wtx.GetHash().GetHex().c_str());

    return true;
}

// Create denominations
bool CSandstormPool::CreateDenominated(int64_t nTotalValue)
{
    // make our change address
    CReserveKey reservekey(pwalletMain);

    CScript scriptChange;
    CPubKey vchPubKey;
    assert(reservekey.GetReservedKey(vchPubKey)); // should never fail, as we just unlocked
    scriptChange = GetScriptForDestination(vchPubKey.GetID());

    CWalletTx wtx;
    int64_t nFeeRet = 0;
    std::string strFail = "";
    vector< pair<CScript, int64_t> > vecSend;
    int64_t nValueLeft = nTotalValue;

    // ****** Add collateral outputs ************ /
    if(!pwalletMain->HasCollateralInputs()) {
        vecSend.push_back(make_pair(scriptChange, SANDSTORM_COLLATERAL*4));
        nValueLeft -= SANDSTORM_COLLATERAL*4;
    }

    // ****** Add denoms ************ /
    BOOST_REVERSE_FOREACH(int64_t v, sandStormDenominations){
        int nOutputs = 0;

        // add each output up to 10 times until it can't be added again
        while(nValueLeft - v >= SANDSTORM_COLLATERAL && nOutputs <= 10) {
            CScript scriptChange;
            CPubKey vchPubKey;
            //use a unique change address
            assert(reservekey.GetReservedKey(vchPubKey)); // should never fail, as we just unlocked
            scriptChange= GetScriptForDestination(vchPubKey.GetID());
            reservekey.KeepKey();

            vecSend.push_back(make_pair(scriptChange, v));

            //increment outputs and subtract denomination amount
            nOutputs++;
            nValueLeft -= v;
            LogPrintf("CreateDenominated1 %d\n", nValueLeft);
        }

        if(nValueLeft == 0) break;
    }
    LogPrintf("CreateDenominated2 %d\n", nValueLeft);

    // if we have anything left over, it will be automatically send back as change - there is no need to send it manually

    CCoinControl *coinControl=NULL;
    int32_t nChangePos;
    bool success = pwalletMain->CreateTransaction(vecSend, wtx, reservekey,
            nFeeRet, nChangePos, strFail, coinControl, ONLY_NONDENOMINATED_NOTSN);
    if(!success){
        LogPrintf("CreateDenominated: Error - %s\n", strFail.c_str());
        return false;
    }

    // use the same cachedLastSuccess as for SS mixinx to prevent race
    if(pwalletMain->CommitTransaction(wtx, reservekey))
        cachedLastSuccess = pindexBest->nHeight;

    LogPrintf("CreateDenominated Success: tx %s\n", wtx.GetHash().GetHex().c_str());

    return true;
}

bool CSandstormPool::IsCompatibleWithEntries(std::vector<CTxOut>& vout)
{   
    if(GetDenominations(vout) == 0) return false;

    BOOST_FOREACH(const CSandStormEntry v, entries) {
        LogPrintf(" IsCompatibleWithEntries %d %d\n", GetDenominations(vout), GetDenominations(v.vout));
/*
        BOOST_FOREACH(CTxOut o1, vout)
            LogPrintf(" vout 1 - %s\n", o1.ToString().c_str());

        BOOST_FOREACH(CTxOut o2, v.vout)
            LogPrintf(" vout 2 - %s\n", o2.ToString().c_str());
*/
        if(GetDenominations(vout) != GetDenominations(v.vout)) return false;
    }

    return true;
}

bool CSandstormPool::IsCompatibleWithSession(int64_t nDenom, CTransaction txCollateral, std::string& strReason)
{
    if(nDenom == 0) return false;
    
    LogPrintf("CSandstormPool::IsCompatibleWithSession - sessionDenom %d sessionUsers %d\n", sessionDenom, sessionUsers);

    if (!unitTest && !IsCollateralValid(txCollateral)){
        if(fDebug) LogPrintf ("CSandstormPool::IsCompatibleWithSession - collateral not valid!\n");
        strReason = _("Collateral not valid.");
        return false;
    }

    if(sessionUsers < 0) sessionUsers = 0;

    if(sessionUsers == 0) {
        sessionID = 1 + (rand() % 999999);
        sessionDenom = nDenom;
        sessionUsers++;
        lastTimeChanged = GetTimeMillis();
        entries.clear();

        if(!unitTest){
            //broadcast that I'm accepting entries, only if it's the first entry through
            CSandstormQueue ssq;
            ssq.nDenom = nDenom;
            ssq.vin = activeStormnode.vin;
            ssq.time = GetTime();
            ssq.Sign();
            ssq.Relay();
        }

        UpdateState(POOL_STATUS_QUEUE);
        vecSessionCollateral.push_back(txCollateral);
        return true;
    }

    if((state != POOL_STATUS_ACCEPTING_ENTRIES && state != POOL_STATUS_QUEUE) || sessionUsers >= GetMaxPoolTransactions()){
        if((state != POOL_STATUS_ACCEPTING_ENTRIES && state != POOL_STATUS_QUEUE)) strReason = _("Incompatible mode.");
        if(sessionUsers >= GetMaxPoolTransactions()) strReason = _("Stormnode queue is full.");
        LogPrintf("CSandstormPool::IsCompatibleWithSession - incompatible mode, return false %d %d\n", state != POOL_STATUS_ACCEPTING_ENTRIES, sessionUsers >= GetMaxPoolTransactions());
        return false;
    }

    if(nDenom != sessionDenom) {
        strReason = _("No matching denominations found for mixing.");
        return false;
    }

    LogPrintf("CSandStormPool::IsCompatibleWithSession - compatible\n");

    sessionUsers++;
    lastTimeChanged = GetTimeMillis();
    vecSessionCollateral.push_back(txCollateral);

    return true;
}

//create a nice string to show the denominations
void CSandstormPool::GetDenominationsToString(int nDenom, std::string& strDenom){
    // Function returns as follows:
    //
    // bit 0 - 100DRK+1 ( bit on if present )
    // bit 1 - 10DRK+1
    // bit 2 - 1DRK+1
    // bit 3 - .1DRK+1
    // bit 3 - non-denom


    strDenom = "";

    if(nDenom & (1 << 0)) {
        if(strDenom.size() > 0) strDenom += "+";
        strDenom += "100";
    }

    if(nDenom & (1 << 1)) {
        if(strDenom.size() > 0) strDenom += "+";
        strDenom += "10";
    }

    if(nDenom & (1 << 2)) {
        if(strDenom.size() > 0) strDenom += "+";
        strDenom += "1";
    }

    if(nDenom & (1 << 3)) {
        if(strDenom.size() > 0) strDenom += "+";
        strDenom += "0.1";
    }
}

int CSandstormPool::GetDenominations(const std::vector<CTxSSOut>& vout){
    std::vector<CTxOut> vout2;

    BOOST_FOREACH(CTxSSOut out, vout)
        vout2.push_back(out);

    return GetDenominations(vout2);
}

// return a bitshifted integer representing the denominations in this list
int CSandstormPool::GetDenominations(const std::vector<CTxOut>& vout){
    std::vector<pair<int64_t, int> > denomUsed;

    // make a list of denominations, with zero uses
    BOOST_FOREACH(int64_t d, sandStormDenominations)
        denomUsed.push_back(make_pair(d, 0));

    // look for denominations and update uses to 1
    BOOST_FOREACH(CTxOut out, vout){
        bool found = false;
        BOOST_FOREACH (PAIRTYPE(int64_t, int)& s, denomUsed){
            if (out.nValue == s.first){
                s.second = 1;
                found = true;
            }
        }
        if(!found) return 0;
    }

    int denom = 0;
    int c = 0;
    // if the denomination is used, shift the bit on.
    // then move to the next
    BOOST_FOREACH (PAIRTYPE(int64_t, int)& s, denomUsed)
        denom |= s.second << c++;

    // Function returns as follows:
    //
    // bit 0 - 100DRK+1 ( bit on if present )
    // bit 1 - 10DRK+1
    // bit 2 - 1DRK+1
    // bit 3 - .1DRK+1

    return denom;
}


int CSandstormPool::GetDenominationsByAmounts(std::vector<int64_t>& vecAmount){
    CScript e = CScript();
    std::vector<CTxOut> vout1;

    // Make outputs by looping through denominations, from small to large
    BOOST_REVERSE_FOREACH(int64_t v, vecAmount){
        int nOutputs = 0;

        CTxOut o(v, e);
        vout1.push_back(o);
        nOutputs++;
    }

    return GetDenominations(vout1);
}

int CSandstormPool::GetDenominationsByAmount(int64_t nAmount, int nDenomTarget){
    CScript e = CScript();
    int64_t nValueLeft = nAmount;

    std::vector<CTxOut> vout1;

    // Make outputs by looping through denominations, from small to large
    BOOST_REVERSE_FOREACH(int64_t v, sandStormDenominations){
        if(nDenomTarget != 0){
            bool fAccepted = false;
            if((nDenomTarget & (1 << 0)) &&      v == ((100000*COIN)+100000000)) {fAccepted = true;}
            else if((nDenomTarget & (1 << 1)) && v == ((10000*COIN) +10000000)) {fAccepted = true;}
            else if((nDenomTarget & (1 << 2)) && v == ((1000*COIN)  +1000000)) {fAccepted = true;}
            else if((nDenomTarget & (1 << 3)) && v == ((100*COIN)   +100000)) {fAccepted = true;}
            else if((nDenomTarget & (1 << 4)) && v == ((10*COIN)    +10000)) {fAccepted = true;}
            else if((nDenomTarget & (1 << 5)) && v == ((1*COIN)     +1000)) {fAccepted = true;}
            else if((nDenomTarget & (1 << 6)) && v == ((.1*COIN)    +100)) {fAccepted = true;}
            if(!fAccepted) continue;
        }

        int nOutputs = 0;

        // add each output up to 10 times until it can't be added again
        while(nValueLeft - v >= 0 && nOutputs <= 10) {
            CTxOut o(v, e);
            vout1.push_back(o);
            nValueLeft -= v;
            nOutputs++;
        }
        LogPrintf("GetDenominationsByAmount --- %d nOutputs %d\n", v, nOutputs);
    }

    //add non-denom left overs as change
    if(nValueLeft > 0){
        CTxOut o(nValueLeft, e);
        vout1.push_back(o);
    }

    return GetDenominations(vout1);
}

bool CSandStormSigner::IsVinAssociatedWithPubkey(CTxIn& vin, CPubKey& pubkey){
    CScript payee2;
    payee2 = GetScriptForDestination(pubkey.GetID());

    CTransaction txVin;
    uint256 hash;
    //if(GetTransaction(vin.prevout.hash, txVin, hash, true)){
    if(GetTransaction(vin.prevout.hash, txVin, hash)){
        BOOST_FOREACH(CTxOut out, txVin.vout){
            if(out.nValue == 42000*COIN){
                if(out.scriptPubKey == payee2) return true;
            }
        }
    }

    return false;
}

bool CSandStormSigner::SetKey(std::string strSecret, std::string& errorMessage, CKey& key, CPubKey& pubkey){
    CDarkSilkSecret vchSecret;
    bool fGood = vchSecret.SetString(strSecret);

    if (!fGood) {
        errorMessage = _("Invalid private key.");
        return false;
    }

    key = vchSecret.GetKey();
    pubkey = key.GetPubKey();

    return true;
}

bool CSandStormSigner::SignMessage(std::string strMessage, std::string& errorMessage, vector<unsigned char>& vchSig, CKey key)
{
    CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    if (!key.SignCompact(ss.GetHash(), vchSig)) {
        errorMessage = _("Signing failed.");
        return false;
    }

    return true;
}

bool CSandStormSigner::VerifyMessage(CPubKey pubkey, vector<unsigned char>& vchSig, std::string strMessage, std::string& errorMessage)
{
    CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    CPubKey pubkey2;
    if (!pubkey2.RecoverCompact(ss.GetHash(), vchSig)) {
        errorMessage = _("Error recovering public key.");
        return false;
    }

    if (fDebug && pubkey2.GetID() != pubkey.GetID())
        LogPrintf("CSandStormSigner::VerifyMessage -- keys don't match: %s %s", pubkey2.GetID().ToString(), pubkey.GetID().ToString());

    return (pubkey2.GetID() == pubkey.GetID());
}

bool CSandstormQueue::Sign()
{
    if(!fStormNode) return false;

    std::string strMessage = vin.ToString() + boost::lexical_cast<std::string>(nDenom) + boost::lexical_cast<std::string>(time) + boost::lexical_cast<std::string>(ready);

    CKey key2;
    CPubKey pubkey2;
    std::string errorMessage = "";

    if(!sandStormSigner.SetKey(strStormNodePrivKey, errorMessage, key2, pubkey2))
    {
        LogPrintf("CSandstormQueue():Relay - ERROR: Invalid Stormnodeprivkey: '%s'\n", errorMessage.c_str());
        return false;
    }

    if(!sandStormSigner.SignMessage(strMessage, errorMessage, vchSig, key2)) {
        LogPrintf("CSandstormQueue():Relay - Sign message failed");
        return false;
    }

    if(!sandStormSigner.VerifyMessage(pubkey2, vchSig, strMessage, errorMessage)) {
        LogPrintf("CSandstormQueue():Relay - Verify message failed");
        return false;
    }

    return true;
}

bool CSandstormQueue::Relay()
{

    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes){
        // always relay to everyone
        pnode->PushMessage("ssq", (*this));
    }

    return true;
}

bool CSandstormQueue::CheckSignature()
{
    CStormnode* psn = snodeman.Find(vin);
    if(psn)
    {
        std::string strMessage = vin.ToString() + boost::lexical_cast<std::string>(nDenom) + boost::lexical_cast<std::string>(time) + boost::lexical_cast<std::string>(ready);
        std::string errorMessage = "";
        if(!sandStormSigner.VerifyMessage(psn->pubkey2, vchSig, strMessage, errorMessage)){
            return error("CSandstormQueue::CheckSignature() - Got bad Stormnode address signature %s \n", vin.ToString().c_str());
        }

        return true;
    }
    return false;
}

void CSandstormPool::RelayFinalTransaction(const int sessionID, const CTransaction& txNew)
{
    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes)
    {
        pnode->PushMessage("ssf", sessionID, txNew);
    }
}

void CSandstormPool::RelayIn(const std::vector<CTxSSIn>& vin, const int64_t& nAmount, const CTransaction& txCollateral, const std::vector<CTxSSOut>& vout)
{
    LOCK(cs_vNodes);

    std::vector<CTxIn> vin2;
    std::vector<CTxOut> vout2;

    BOOST_FOREACH(CTxSSIn in, vin)
        vin2.push_back(in);

    BOOST_FOREACH(CTxSSOut out, vout)
        vout2.push_back(out);

    BOOST_FOREACH(CNode* pnode, vNodes)
    {
        if(!pSubmittedToStormnode) return;
        if((CNetAddr)sandStormPool.pSubmittedToStormnode->addr != (CNetAddr)pnode->addr) continue;
        LogPrintf("RelayIn - found master, relaying message - %s \n", pnode->addr.ToString().c_str());
        pnode->PushMessage("ssi", vin2, nAmount, txCollateral, vout2);
    }
}

void CSandstormPool::RelayStatus(const int sessionID, const int newState, const int newEntriesCount, const int newAccepted, const std::string error)
{
    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes)
        pnode->PushMessage("sssu", sessionID, newState, newEntriesCount, newAccepted, error);
}

void CSandstormPool::RelayCompletedTransaction(const int sessionID, const bool error, const std::string errorMessage)
{
    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes)
        pnode->PushMessage("ssc", sessionID, error, errorMessage);
}

//TODO: Rename/move to core
void ThreadCheckSandStormPool()
{
    if(fLiteMode) return; //disable all Sandstorm/Stormnode related functionality
    if(IsInitialBlockDownload()) return;
    // Make this thread recognisable as the wallet flushing thread
    RenameThread("DarkSilk-Sandstorm");

    unsigned int c = 0;
    std::string errorMessage;

    while (true)
    {
        c++;

        MilliSleep(1000);
        //LogPrintf("ThreadCheckSandStormPool::check timeout\n");

        sandStormPool.CheckTimeout();
        sandStormPool.CheckForCompleteQueue();

        if(c % 60 == 0)
        {
            LOCK(cs_main);
            /*
                cs_main is required for doing CStormnode.Check because something
                is modifying the coins view without a mempool lock. It causes
                segfaults from this code without the cs_main lock.
            */
            snodeman.CheckAndRemove();
            snodeman.ProcessStormnodeConnections();
            stormnodePayments.CleanPaymentList();
            CleanTransactionLocksList();
        }

        if(c % STORMNODE_PING_SECONDS == 0) activeStormnode.ManageStatus();

        if(c % STORMNODES_DUMP_SECONDS == 0) DumpStormnodes();

        //try to sync the Stormnode list and payment list every 5 seconds from at least 3 nodes
        if(c % 5 == 0 && RequestedStormNodeList < 3){
            bool fIsInitialDownload = IsInitialBlockDownload();
            if(!fIsInitialDownload) {
                LOCK(cs_vNodes);
                BOOST_FOREACH(CNode* pnode, vNodes)
                {
                    if (pnode->nVersion >= MIN_SANDSTORM_PROTO_VERSION) {

                        //keep track of who we've asked for the list
                        if(pnode->HasFulfilledRequest("snsync")) continue;
                        pnode->FulfilledRequest("snsync");

                        LogPrintf("Successfully synced, asking for Stormnode list and payment list\n");

                        //request full sn list only if Stormnodes.dat was updated quite a long time ago
                        snodeman.SsegUpdate(pnode);                      
                        pnode->PushMessage("snget"); //sync payees
                        pnode->PushMessage("getsporks"); //get current network sporks
                        RequestedStormNodeList++;
                    }
                }
            }
        }

        if(c % 60 == 0){
            //if we've used 1/5 of the Stormnode list, then clear the list.
            if((int)vecStormnodesUsed.size() > (int)snodeman.size() / 5)
                vecStormnodesUsed.clear();
        }

        if(sandStormPool.GetState() == POOL_STATUS_IDLE && c % 6 == 0){
            sandStormPool.DoAutomaticDenominating();
        }
    }
}
