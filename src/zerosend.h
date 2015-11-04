// Copyright (c) 2014-2015 The Darkcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ZEROSEND_H
#define ZEROSEND_H

//#include "primitives/transaction.h"
#include "main.h"
#include "sync.h"
#include "blanknode.h"
#include "activeblanknode.h"
#include "blanknodeman.h"
#include "blanknode-payments.h"
#include "zerosend-relay.h"

class CTxIn;
class CZerosendPool;
class CZeroSendSigner;
class CBlankNodeVote;
class CFantomAddress;
class CZerosendQueue;
class CZerosendBroadcastTx;
class CActiveBlanknode;

// pool states for mixing
#define POOL_MAX_TRANSACTIONS                  3 // wait for X transactions to merge and publish
#define POOL_MAX_TRANSACTIONS_TESTNET          2 // wait for X transactions to merge and publish
#define POOL_STATUS_UNKNOWN                    0 // waiting for update
#define POOL_STATUS_IDLE                       1 // waiting for update
#define POOL_STATUS_QUEUE                      2 // waiting in a queue
#define POOL_STATUS_ACCEPTING_ENTRIES          3 // accepting entries
#define POOL_STATUS_FINALIZE_TRANSACTION       4 // master node will broadcast what it accepted
#define POOL_STATUS_SIGNING                    5 // check inputs/outputs, sign final tx
#define POOL_STATUS_TRANSMISSION               6 // transmit transaction
#define POOL_STATUS_ERROR                      7 // error
#define POOL_STATUS_SUCCESS                    8 // success

// status update message constants
#define BLANKNODE_ACCEPTED                    1
#define BLANKNODE_REJECTED                    0
#define BLANKNODE_RESET                       -1

#define ZEROSEND_QUEUE_TIMEOUT                 240
#define ZEROSEND_SIGNING_TIMEOUT               120

// used for anonymous relaying of inputs/outputs/sigs
#define ZEROSEND_RELAY_IN                 1
#define ZEROSEND_RELAY_OUT                2
#define ZEROSEND_RELAY_SIG                3

extern CZerosendPool zeroSendPool;
extern CZeroSendSigner zeroSendSigner;
extern std::vector<CZerosendQueue> vecZerosendQueue;
extern std::string strBlankNodePrivKey;
extern map<uint256, CZerosendBroadcastTx> mapZerosendBroadcastTxes;
extern CActiveBlanknode activeBlanknode;

// get the Zerosend chain depth for a given input
int GetInputZerosendRounds(CTxIn in, int rounds=0);


// Holds an Zerosend input
class CTxSSIn : public CTxIn
{
public:
    bool fHasSig;
    int nSentTimes; //times we've sent this anonymously 

    CTxSSIn(const CTxIn& in)
    {
        prevout = in.prevout;
        scriptSig = in.scriptSig;
        prevPubKey = in.prevPubKey;
        nSequence = in.nSequence;
        nSentTimes = 0;
        fHasSig = false;
    }
};

/** Holds a Zerosend output
 */
class CTxSSOut : public CTxOut
{
public:
    int nSentTimes; //times we've sent this anonymously 

    CTxSSOut(const CTxOut& out)
    {
        nValue = out.nValue;
        nRounds = out.nRounds;
        scriptPubKey = out.scriptPubKey;
        nSentTimes = 0;
    }
};

// A clients transaction in the zerosend pool
class CZeroSendEntry
{
public:
    bool isSet;
    std::vector<CTxSSIn> sev;
    std::vector<CTxSSOut> vout;
    int64_t amount;
    CTransaction collateral;
    CTransaction txSupporting;
    int64_t addedTime;

    CZeroSendEntry()
    {
        isSet = false;
        collateral = CTransaction();
        amount = 0;
    }

    bool Add(const std::vector<CTxIn> vinIn, int64_t amountIn, const CTransaction collateralIn, const std::vector<CTxOut> voutIn)
    {
        if(isSet){return false;}

        BOOST_FOREACH(const CTxIn& in, vinIn)
            sev.push_back(in);

        BOOST_FOREACH(const CTxOut& out, voutIn)
            vout.push_back(out);

        amount = amountIn;
        collateral = collateralIn;
        isSet = true;
        addedTime = GetTime();

        return true;
    }

    bool AddSig(const CTxIn& vin)
    {
        BOOST_FOREACH(CTxSSIn& s, sev) {
            if(s.prevout == vin.prevout && s.nSequence == vin.nSequence){
                if(s.fHasSig){return false;}
                s.scriptSig = vin.scriptSig;
                s.prevPubKey = vin.prevPubKey;
                s.fHasSig = true;

                return true;
            }
        }

        return false;
    }

    bool IsExpired()
    {
        return (GetTime() - addedTime) > ZEROSEND_QUEUE_TIMEOUT;// 120 seconds
    }
};


//
// A currently inprogress zerosend merge and denomination information
//
class CZerosendQueue
{
public:
    CTxIn vin;
    int64_t time;
    int nDenom;
    bool ready; //ready for submit
    std::vector<unsigned char> vchSig;
    
    CZerosendQueue()
    {
        nDenom = 0;
        vin = CTxIn();
        time = 0;
        vchSig.clear();
        ready = false;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(nDenom);
        READWRITE(vin);
        READWRITE(time);
        READWRITE(ready);
        READWRITE(vchSig);
    )

    bool GetAddress(CService &addr)
    {
        CBlanknode* psn = snodeman.Find(vin);
        if(psn != NULL)
        {
            addr = psn->addr;
            return true;
        }
        return false;
    }

    bool GetProtocolVersion(int &protocolVersion)
    {
        CBlanknode* psn = snodeman.Find(vin);
        if(psn != NULL)
        {
            protocolVersion = psn->protocolVersion;
            return true;
        }
        return false;
    }

    bool Sign();
    bool Relay();

    bool IsExpired()
    {
        return (GetTime() - time) > ZEROSEND_QUEUE_TIMEOUT;// 120 seconds
    }

    bool CheckSignature();

};

// store zerosend tx signature information
class CZerosendBroadcastTx
{
public:
    CTransaction tx;
    CTxIn vin;
    vector<unsigned char> vchSig;
    int64_t sigTime;
};

//
// Helper object for signing and checking signatures
//
class CZeroSendSigner
{
public:
    bool IsVinAssociatedWithPubkey(CTxIn& vin, CPubKey& pubkey);
    bool SetKey(std::string strSecret, std::string& errorMessage, CKey& key, CPubKey& pubkey);
    bool SignMessage(std::string strMessage, std::string& errorMessage, std::vector<unsigned char>& vchSig, CKey key);
    bool VerifyMessage(CPubKey pubkey, std::vector<unsigned char>& vchSig, std::string strMessage, std::string& errorMessage);
};

//
// Used to keep track of current status of zerosend pool
//
class CZerosendPool
{
public:
    // clients entries
    std::vector<CZeroSendEntry> myEntries;
    // blanknode entries
    std::vector<CZeroSendEntry> entries;
    // the finalized transaction ready for signing
    CTransaction finalTransaction;

    int64_t lastTimeChanged;
    int64_t lastAutoDenomination;

    unsigned int state;
    unsigned int entriesCount;
    unsigned int lastEntryAccepted;
    unsigned int countEntriesAccepted;

    // where collateral should be made out to
    CScript collateralPubKey;

    std::vector<CTxIn> lockedCoins;

    uint256 BlankNodeBlockHash;

    std::string lastMessage;
    bool completedTransaction;
    bool unitTest;
    CBlanknode* pSubmittedToBlanknode;

    int sessionID;
    int sessionDenom; //Users must submit an denom matching this
    int sessionUsers; //N Users have said they'll join
    bool sessionFoundBlanknode; //If we've found a compatible blanknode
    std::vector<CTransaction> vecSessionCollateral;

    int cachedLastSuccess;
    int cachedNumBlocks; //used for the overview screen
    int minBlockSpacing; //required blocks between mixes
    CTransaction txCollateral;

    int64_t lastNewBlock;

    //debugging data
    std::string strAutoDenomResult;

    CZerosendPool()
    {
        /* ZeroSend uses collateral addresses to trust parties entering the pool
            to behave themselves. If they don't it takes their money. */

        cachedLastSuccess = 0;
        cachedNumBlocks = 0;
        unitTest = false;
        txCollateral = CTransaction();
        minBlockSpacing = 1;
        lastNewBlock = 0;


        SetNull();
    }

    /** Process a Zerosend message using the Zerosend protocol
     * \param pfrom
     * \param strCommand lower case command string; valid values are:
     *        Command  | Description
     *        -------- | -----------------
     *        ssa      | Zerosend Acceptable
     *        ssc      | Zerosend Complete
     *        ssf      | Zerosend Final tx
     *        ssi      | Zerosend vIn
     *        ssq      | Zerosend Queue
     *        sss      | Zerosend Signal Final Tx
     *        sssu     | Zerosend status update
     *        sssub    | Zerosend Subscribe To
     * \param vRecv
     */
    void ProcessMessageZerosend(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);

    void InitCollateralAddress(){
        std::string strAddress = "";
        if(Params().NetworkID() == CChainParams::MAIN) {
            strAddress = "D7FBJNGDmEsU5wx2m3xw85N8kRgCqA8S7L";
        }
        SetCollateralAddress(strAddress);
    }

    void SetMinBlockSpacing(int minBlockSpacingIn){
        minBlockSpacing = minBlockSpacingIn;
    }

    bool SetCollateralAddress(std::string strAddress);
    void Reset();

    void SetNull(bool clearEverything=false);

    void UnlockCoins();

    bool IsNull() const
    {
        return (state == POOL_STATUS_ACCEPTING_ENTRIES && entries.empty() && myEntries.empty());
    }

    int GetState() const
    {
        return state;
    }

    int GetEntriesCount() const
    {
        if(fBlankNode){
            return entries.size();
        } else {
            return entriesCount;
        }
    }

    int GetLastEntryAccepted() const
    {
        return lastEntryAccepted;
    }

    int GetCountEntriesAccepted() const
    {
        return countEntriesAccepted;
    }

    int GetMyTransactionCount() const
    {
        return myEntries.size();
    }

    void UpdateState(unsigned int newState)
    {
        if (fBlankNode && (newState == POOL_STATUS_ERROR || newState == POOL_STATUS_SUCCESS)){
            LogPrintf("CZerosendPool::UpdateState() - Can't set state to ERROR or SUCCESS as a blanknode. \n");
            return;
        }

        LogPrintf("CZerosendPool::UpdateState() == %d | %d \n", state, newState);
        if(state != newState){
            lastTimeChanged = GetTimeMillis();
            if(fBlankNode) {
                RelayStatus(zeroSendPool.sessionID, zeroSendPool.GetState(), zeroSendPool.GetEntriesCount(), BLANKNODE_RESET);
            }
        }
        state = newState;
    }

    int GetMaxPoolTransactions()
    {   
        //if we're on testnet, just use two transactions per merge
        if(Params().NetworkID() == CChainParams::TESTNET) return POOL_MAX_TRANSACTIONS_TESTNET;
        
        //use the production amount
        return POOL_MAX_TRANSACTIONS;
    }

    //Do we have enough users to take entries?
    bool IsSessionReady(){
        return sessionUsers >= GetMaxPoolTransactions();
    }

    // Are these outputs compatible with other client in the pool?
    bool IsCompatibleWithEntries(std::vector<CTxOut>& vout);
    // Is this amount compatible with other client in the pool?
    bool IsCompatibleWithSession(int64_t nAmount, CTransaction txCollateral, std::string& strReason);

    // Passively run Zerosend in the background according to the configuration in settings (only for Qt)
    bool DoAutomaticDenominating(bool fDryRun=false, bool ready=false);
    bool PrepareZerosendDenominate();


    // check for process in Zerosend
    void Check();
    void CheckFinalTransaction();
    // charge fees to bad actors
    void ChargeFees();
    // rarely charge fees to pay miners
    void ChargeRandomFees();
    void CheckTimeout();
    void CheckForCompleteQueue();
    // check to make sure a signature matches an input in the pool
    bool SignatureValid(const CScript& newSig, const CTxIn& newVin);
    // if the collateral is valid given by a client
    bool IsCollateralValid(const CTransaction& txCollateral);
    // add a clients entry to the pool
    bool AddEntry(const std::vector<CTxIn>& newInput, const int64_t& nAmount, const CTransaction& txCollateral, const std::vector<CTxOut>& newOutput, std::string& error);

    
    // add signature to a vin
    bool AddScriptSig(const CTxIn& newVin);
    
    // are all inputs signed?
    bool SignaturesComplete();
    
    // as a client, send a transaction to a blanknode to start the denomination process
    void SendZerosendDenominate(std::vector<CTxIn>& vin, std::vector<CTxOut>& vout, int64_t amount);
    
    // get blanknode updates about the progress of zerosend
    bool StatusUpdate(int newState, int newEntriesCount, int newAccepted, std::string& error, int newSessionID=0);

    // as a client, check and sign the final transaction
    bool SignFinalTransaction(CTransaction& finalTransactionNew, CNode* node);

    // get the last valid block hash for a given modulus
    bool GetLastValidBlockHash(uint256& hash, int mod=1, int nBlockHeight=0);
    
    // process a new block
    void NewBlock();
    
    void CompletedTransaction(bool error, std::string lastMessageNew);
    
    void ClearLastMessage();
    
    // used for liquidity providers
    bool SendRandomPaymentToSelf();
   
   // split up large inputs or make fee sized inputs
    bool MakeCollateralAmounts();
    
    bool CreateDenominated(int64_t nTotalValue);
    
    // get the denominations for a list of outputs (returns a bitshifted integer)
    int GetDenominations(const std::vector<CTxOut>& vout, bool fRandDenom = false);
    int GetDenominations(const std::vector<CTxSSOut>& vout);
    
    void GetDenominationsToString(int nDenom, std::string& strDenom);
    
    // get the denominations for a specific amount of fantom.
    int GetDenominationsByAmount(int64_t nAmount, int nDenomTarget=0);
    int GetDenominationsByAmounts(std::vector<int64_t>& vecAmount, bool fRandDenom = false);


    //
    // Relay Zerosend Messages
    //

    void RelayFinalTransaction(const int sessionID, const CTransaction& txNew);
    void RelaySignaturesAnon(std::vector<CTxIn>& vin);
    void RelayInAnon(std::vector<CTxIn>& vin, std::vector<CTxOut>& vout);
    void RelayIn(const std::vector<CTxSSIn>& vin, const int64_t& nAmount, const CTransaction& txCollateral, const std::vector<CTxSSOut>& vout);
    void RelayStatus(const int sessionID, const int newState, const int newEntriesCount, const int newAccepted, const std::string error="");
    void RelayCompletedTransaction(const int sessionID, const bool error, const std::string errorMessage);
};

void ThreadCheckZeroSendPool();

#endif
