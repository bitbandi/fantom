// Copyright (c) 2014-2015 The Darkcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SANDSTORM_H
#define SANDSTORM_H

//#include "primitives/transaction.h"
#include "main.h"
#include "sync.h"
#include "stormnode.h"
#include "activestormnode.h"
#include "stormnodeman.h"
#include "sandstorm-relay.h"

class CTxIn;
class CSandstormPool;
class CSandStormSigner;
class CStormNodeVote;
class CDarkSilkAddress;
class CSandstormQueue;
class CSandstormBroadcastTx;
class CActiveStormnode;

// pool states for mixing
#define POOL_MAX_TRANSACTIONS                  1 // wait for X transactions to merge and publish
#define POOL_MAX_TRANSACTIONS_TESTNET          1 // wait for X transactions to merge and publish
#define POOL_STATUS_UNKNOWN                    0 // waiting for update
#define POOL_STATUS_IDLE                       1 // waiting for update
#define POOL_STATUS_QUEUE                      2 // waiting in a queue
#define POOL_STATUS_ACCEPTING_ENTRIES          3 // accepting entries
#define POOL_STATUS_FINALIZE_TRANSACTION       4 // stormnode will broadcast what it accepted
#define POOL_STATUS_SIGNING                    5 // check inputs/outputs, sign final tx
#define POOL_STATUS_TRANSMISSION               6 // transmit transaction
#define POOL_STATUS_ERROR                      7 // error
#define POOL_STATUS_SUCCESS                    8 // success

// status update message constants
#define STORMNODE_ACCEPTED                    1
#define STORMNODE_REJECTED                    0
#define STORMNODE_RESET                       -1

#define SANDSTORM_QUEUE_TIMEOUT                 120
#define SANDSTORM_SIGNING_TIMEOUT               60

// used for anonymous relaying of inputs/outputs/sigs
#define SANDSTORM_RELAY_IN                 1
#define SANDSTORM_RELAY_OUT                2
#define SANDSTORM_RELAY_SIG                3

extern CSandstormPool sandStormPool;
extern CSandStormSigner sandStormSigner;
extern std::vector<CSandstormQueue> vecSandstormQueue;
extern std::string strStormNodePrivKey;
extern map<uint256, CSandstormBroadcastTx> mapSandstormBroadcastTxes;
extern CActiveStormnode activeStormnode;

// get the Sandstorm chain depth for a given input
int GetInputSandstormRounds(CTxIn in, int rounds=0);


// Holds an Sandstorm input
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

/** Holds a Sandstorm output
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

// A clients transaction in the sandstorm pool
class CSandStormEntry
{
public:
    bool isSet;
    std::vector<CTxSSIn> sev;
    std::vector<CTxSSOut> vout;
    int64_t amount;
    CTransaction collateral;
    CTransaction txSupporting;
    int64_t addedTime;

    CSandStormEntry()
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
        return (GetTime() - addedTime) > SANDSTORM_QUEUE_TIMEOUT;// 120 seconds
    }
};


//
// A currently inprogress sandstorm merge and denomination information
//
class CSandstormQueue
{
public:
    CTxIn vin;
    int64_t time;
    int nDenom;
    bool ready; //ready for submit
    std::vector<unsigned char> vchSig;
    
    CSandstormQueue()
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
        CStormnode* psn = snodeman.Find(vin);
        if(psn != NULL)
        {
            addr = psn->addr;
            return true;
        }
        return false;
    }

    bool GetProtocolVersion(int &protocolVersion)
    {
        CStormnode* psn = snodeman.Find(vin);
        if(psn)
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
        return (GetTime() - time) > SANDSTORM_QUEUE_TIMEOUT;// 120 seconds
    }

    bool CheckSignature();

};

// store sandstorm tx signature information
class CSandstormBroadcastTx
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
class CSandStormSigner
{
public:
    bool IsVinAssociatedWithPubkey(CTxIn& vin, CPubKey& pubkey);
    bool SetKey(std::string strSecret, std::string& errorMessage, CKey& key, CPubKey& pubkey);
    bool SignMessage(std::string strMessage, std::string& errorMessage, std::vector<unsigned char>& vchSig, CKey key);
    bool VerifyMessage(CPubKey pubkey, std::vector<unsigned char>& vchSig, std::string strMessage, std::string& errorMessage);
};

//
// Used to keep track of current status of sandstorm pool
//
class CSandstormPool
{
public:
    // clients entries
    std::vector<CSandStormEntry> myEntries;
    // stormnode entries
    std::vector<CSandStormEntry> entries;
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

    uint256 StormNodeBlockHash;

    std::string lastMessage;
    bool completedTransaction;
    bool unitTest;
    CStormnode* pSubmittedToStormnode;

    int sessionID;
    int sessionDenom; //Users must submit an denom matching this
    int sessionUsers; //N Users have said they'll join
    bool sessionFoundStormnode; //If we've found a compatible stormnode
    int64_t sessionTotalValue; //used for autoDenom
    std::vector<CTransaction> vecSessionCollateral;

    int cachedLastSuccess;
    int cachedNumBlocks; //used for the overview screen
    int minBlockSpacing; //required blocks between mixes
    CTransaction txCollateral;

    int64_t lastNewBlock;

    //debugging data
    std::string strAutoDenomResult;

    CSandstormPool()
    {
        /* SandStorm uses collateral addresses to trust parties entering the pool
            to behave themselves. If they don't it takes their money. */

        cachedLastSuccess = 0;
        cachedNumBlocks = 0;
        unitTest = false;
        txCollateral = CTransaction();
        minBlockSpacing = 1;
        lastNewBlock = 0;


        SetNull();
    }

    /** Process a Sandstorm message using the Sandstorm protocol
     * \param pfrom
     * \param strCommand lower case command string; valid values are:
     *        Command  | Description
     *        -------- | -----------------
     *        ssa      | Sandstorm Acceptable
     *        ssc      | Sandstorm Complete
     *        ssf      | Sandstorm Final tx
     *        ssi      | Sandstorm vIn
     *        ssq      | Sandstorm Queue
     *        sss      | Sandstorm Signal Final Tx
     *        sssu     | Sandstorm status update
     *        sssub    | Sandstorm Subscribe To
     * \param vRecv
     */
    void ProcessMessageSandstorm(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);

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
        if(fStormNode){
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
        if (fStormNode && (newState == POOL_STATUS_ERROR || newState == POOL_STATUS_SUCCESS)){
            LogPrintf("CSandstormPool::UpdateState() - Can't set state to ERROR or SUCCESS as a stormnode. \n");
            return;
        }

        LogPrintf("CSandstormPool::UpdateState() == %d | %d \n", state, newState);
        if(state != newState){
            lastTimeChanged = GetTimeMillis();
            if(fStormNode) {
                RelayStatus(sandStormPool.sessionID, sandStormPool.GetState(), sandStormPool.GetEntriesCount(), STORMNODE_RESET);
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

    // Passively run Sandstorm in the background according to the configuration in settings (only for Qt)
    bool DoAutomaticDenominating(bool fDryRun=false, bool ready=false);
    bool PrepareSandstormDenominate();


    // check for process in Sandstorm
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
    
    // as a client, send a transaction to a stormnode to start the denomination process
    void SendSandstormDenominate(std::vector<CTxIn>& vin, std::vector<CTxOut>& vout, int64_t amount);
    
    // get stormnode updates about the progress of sandstorm
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
    int GetDenominations(const std::vector<CTxOut>& vout);
    int GetDenominations(const std::vector<CTxSSOut>& vout);
    
    void GetDenominationsToString(int nDenom, std::string& strDenom);
    
    // get the denominations for a specific amount of darksilk.
    int GetDenominationsByAmount(int64_t nAmount, int nDenomTarget=0);
    int GetDenominationsByAmounts(std::vector<int64_t>& vecAmount);


    //
    // Relay Sandstorm Messages
    //

    void RelayFinalTransaction(const int sessionID, const CTransaction& txNew);
    void RelaySignaturesAnon(std::vector<CTxIn>& vin);
    void RelayInAnon(std::vector<CTxIn>& vin, std::vector<CTxOut>& vout);
    void RelayIn(const std::vector<CTxSSIn>& vin, const int64_t& nAmount, const CTransaction& txCollateral, const std::vector<CTxSSOut>& vout);
    void RelayStatus(const int sessionID, const int newState, const int newEntriesCount, const int newAccepted, const std::string error="");
    void RelayCompletedTransaction(const int sessionID, const bool error, const std::string errorMessage);
};

void ThreadCheckSandStormPool();

#endif
