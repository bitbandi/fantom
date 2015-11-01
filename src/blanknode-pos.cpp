

#include "bignum.h"
#include "sync.h"
#include "net.h"
#include "key.h"
#include "util.h"
#include "script.h"
#include "base58.h"
#include "protocol.h"
#include "activeblanknode.h"
#include "blanknodeman.h"
#include "spork.h"
#include <boost/lexical_cast.hpp>
#include "blanknodeman.h"

using namespace std;
using namespace boost;

std::map<uint256, CBlanknodeScanningError> mapBlanknodeScanningErrors;
CBlanknodeScanning snscan;

/* 
    Blanknode - Proof of Service 

    -- What it checks

    1.) Making sure Blanknodes have their ports open
    2.) Are responding to requests made by the network

    -- How it works

    When a block comes in, DoBlanknodePOS is executed if the client is a 
    blanknode. Using the deterministic ranking algorithm up to 1% of the blanknode 
    network is checked each block. 

    A port is opened from Blanknode A to Blanknode B, if successful then nothing happens. 
    If there is an error, a CBlanknodeScanningError object is propagated with an error code.
    Errors are applied to the Blanknodes and a score is incremented within the blanknode object,
    after a threshold is met, the blanknode goes into an error state. Each cycle the score is 
    decreased, so if the blanknode comes back online it will return to the list. 

    Blanknodes in a error state do not receive payment. 

    -- Future expansion

    We want to be able to prove the nodes have many qualities such as a specific CPU speed, bandwidth,
    and dedicated storage. E.g. We could require a full node be a computer running 2GHz with 10GB of space.

*/

void ProcessMessageBlanknodePOS(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    if(fLiteMode) return; //disable all zerosend/blanknode related functionality
    if(!IsSporkActive(SPORK_7_BLANKNODE_SCANNING)) return;
    if(IsInitialBlockDownload()) return;

    if (strCommand == "snse") //Blanknode Scanning Error
    {

        CDataStream vMsg(vRecv);
        CBlanknodeScanningError snse;
        vRecv >> snse;

        CInv inv(MSG_BLANKNODE_SCANNING_ERROR, snse.GetHash());
        pfrom->AddInventoryKnown(inv);

        if(mapBlanknodeScanningErrors.count(snse.GetHash())){
            return;
        }
        mapBlanknodeScanningErrors.insert(make_pair(snse.GetHash(), snse));

        if(!snse.IsValid())
        {
            LogPrintf("BlanknodePOS::snse - Invalid object\n");   
            return;
        }

        CBlanknode* psnA = snodeman.Find(snse.vinBlanknodeA);
        if(psnA == NULL) return;
        if(psnA->protocolVersion < MIN_BLANKNODE_POS_PROTO_VERSION) return;

        int nBlockHeight = pindexBest->nHeight;
        if(nBlockHeight - snse.nBlockHeight > 10){
            LogPrintf("BlanknodePOS::snse - Too old\n");
            return;   
        }

        // Lowest blanknodes in rank check the highest each block
        int a = snodeman.GetBlanknodeRank(snse.vinBlanknodeA, snse.nBlockHeight, MIN_BLANKNODE_POS_PROTO_VERSION);
        if(a == -1 || a > GetCountScanningPerBlock())
        {
            LogPrintf("BlanknodePOS::snse - BlanknodeA ranking is too high\n");
            return;
        }

        int b = snodeman.GetBlanknodeRank(snse.vinBlanknodeB, snse.nBlockHeight, MIN_BLANKNODE_POS_PROTO_VERSION, false);
        if(b == -1 || b < snodeman.CountBlanknodesAboveProtocol(MIN_BLANKNODE_POS_PROTO_VERSION)-GetCountScanningPerBlock())
         {
            LogPrintf("BlanknodePOS::snse - BlanknodeB ranking is too low\n");
            return;
        }

        if(!snse.SignatureValid()){
            LogPrintf("BlanknodePOS::snse - Bad blanknode message\n");
            return;
        }

        CBlanknode* psnB = snodeman.Find(snse.vinBlanknodeB);
        if(psnB == NULL) return;

        if(fDebug) LogPrintf("ProcessMessageBlanknodePOS::snse - nHeight %d BlanknodeA %s BlanknodeB %s\n", snse.nBlockHeight, psnA->addr.ToString().c_str(), psnB->addr.ToString().c_str());

        psnB->ApplyScanningError(snse);
        snse.Relay();
    }
}

// Returns how many blanknodes are allowed to scan each block
int GetCountScanningPerBlock()
{
    return std::max(1, snodeman.CountBlanknodesAboveProtocol(MIN_BLANKNODE_POS_PROTO_VERSION)/100);
}


void CBlanknodeScanning::CleanBlanknodeScanningErrors()
{
    if(pindexBest == NULL) return;

    std::map<uint256, CBlanknodeScanningError>::iterator it = mapBlanknodeScanningErrors.begin();

    while(it != mapBlanknodeScanningErrors.end()) {
        if(GetTime() > it->second.nExpiration){ //keep them for an hour
            LogPrintf("Removing old blanknode scanning error %s\n", it->second.GetHash().ToString().c_str());

            mapBlanknodeScanningErrors.erase(it++);
        } else {
            it++;
        }
    }

}

// Check other blanknodes to make sure they're running correctly
void CBlanknodeScanning::DoBlanknodePOSChecks()
{
    if(!fBlankNode) return;
    if(fLiteMode) return; //disable all zerosend/blanknode related functionality
    if(!IsSporkActive(SPORK_7_BLANKNODE_SCANNING)) return;
    if(IsInitialBlockDownload()) return;

    int nBlockHeight = pindexBest->nHeight-5;

    int a = snodeman.GetBlanknodeRank(activeBlanknode.vin, nBlockHeight, MIN_BLANKNODE_POS_PROTO_VERSION);
    if(a == -1 || a > GetCountScanningPerBlock()){
        // we don't need to do anything this block
        return;
    }

    // The lowest ranking nodes (Blanknode A) check the highest ranking nodes (Blanknode B)
    CBlanknode* psn = snodeman.GetBlanknodeByRank(snodeman.CountBlanknodesAboveProtocol(MIN_BLANKNODE_POS_PROTO_VERSION)-a, nBlockHeight, MIN_BLANKNODE_POS_PROTO_VERSION, false);
    if(psn == NULL) return;

    // -- first check : Port is open

    if(!ConnectNode((CAddress)psn->addr, NULL, true)){
        // we couldn't connect to the node, let's send a scanning error
        CBlanknodeScanningError snse(activeBlanknode.vin, psn->vin, SCANNING_ERROR_NO_RESPONSE, nBlockHeight);
        snse.Sign();
        mapBlanknodeScanningErrors.insert(make_pair(snse.GetHash(), snse));
        snse.Relay();
    }

    // success
    CBlanknodeScanningError snse(activeBlanknode.vin, psn->vin, SCANNING_SUCCESS, nBlockHeight);
    snse.Sign();
    mapBlanknodeScanningErrors.insert(make_pair(snse.GetHash(), snse));
    snse.Relay();
}

bool CBlanknodeScanningError::SignatureValid()
{
    std::string errorMessage;
    std::string strMessage = vinBlanknodeA.ToString() + vinBlanknodeB.ToString() + 
        boost::lexical_cast<std::string>(nBlockHeight) + boost::lexical_cast<std::string>(nErrorType);

    CBlanknode* psn = snodeman.Find(vinBlanknodeA);

    if(psn == NULL)
    {
        LogPrintf("CBlanknodeScanningError::SignatureValid() - Unknown Blanknode\n");
        return false;
    }

    CScript pubkey;
    pubkey.SetDestination(psn->pubkey2.GetID());
    CTxDestination address1;
    ExtractDestination(pubkey, address1);
    CFantomAddress address2(address1);

    if(!zeroSendSigner.VerifyMessage(psn->pubkey2, vchBlankNodeSignature, strMessage, errorMessage)) {
        LogPrintf("CBlanknodeScanningError::SignatureValid() - Verify message failed\n");
        return false;
    }

    return true;
}

bool CBlanknodeScanningError::Sign()
{
    std::string errorMessage;

    CKey key2;
    CPubKey pubkey2;
    std::string strMessage = vinBlanknodeA.ToString() + vinBlanknodeB.ToString() + 
        boost::lexical_cast<std::string>(nBlockHeight) + boost::lexical_cast<std::string>(nErrorType);

    if(!zeroSendSigner.SetKey(strBlankNodePrivKey, errorMessage, key2, pubkey2))
    {
        LogPrintf("CBlanknodeScanningError::Sign() - ERROR: Invalid blanknodeprivkey: '%s'\n", errorMessage.c_str());
        return false;
    }

    CScript pubkey;
    pubkey.SetDestination(pubkey2.GetID());
    CTxDestination address1;
    ExtractDestination(pubkey, address1);
    CFantomAddress address2(address1);
    //LogPrintf("signing pubkey2 %s \n", address2.ToString().c_str());

    if(!zeroSendSigner.SignMessage(strMessage, errorMessage, vchBlankNodeSignature, key2)) {
        LogPrintf("CBlanknodeScanningError::Sign() - Sign message failed");
        return false;
    }

    if(!zeroSendSigner.VerifyMessage(pubkey2, vchBlankNodeSignature, strMessage, errorMessage)) {
        LogPrintf("CBlanknodeScanningError::Sign() - Verify message failed");
        return false;
    }

    return true;
}

void CBlanknodeScanningError::Relay()
{
    CInv inv(MSG_BLANKNODE_SCANNING_ERROR, GetHash());

    vector<CInv> vInv;
    vInv.push_back(inv);
    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes){
        pnode->PushMessage("inv", vInv);
    }
}