

#include "bignum.h"
#include "sync.h"
#include "net.h"
#include "key.h"
#include "util.h"
#include "script.h"
#include "base58.h"
#include "protocol.h"
#include "activestormnode.h"
#include "stormnodeman.h"
#include "spork.h"
#include <boost/lexical_cast.hpp>
#include "stormnodeman.h"

using namespace std;
using namespace boost;

std::map<uint256, CStormnodeScanningError> mapStormnodeScanningErrors;
CStormnodeScanning snscan;

/* 
    Stormnode - Proof of Service 

    -- What it checks

    1.) Making sure Stormnodes have their ports open
    2.) Are responding to requests made by the network

    -- How it works

    When a block comes in, DoStormnodePOS is executed if the client is a 
    stormnode. Using the deterministic ranking algorithm up to 1% of the stormnode 
    network is checked each block. 

    A port is opened from Stormnode A to Stormnode B, if successful then nothing happens. 
    If there is an error, a CStormnodeScanningError object is propagated with an error code.
    Errors are applied to the Stormnodes and a score is incremented within the stormnode object,
    after a threshold is met, the stormnode goes into an error state. Each cycle the score is 
    decreased, so if the stormnode comes back online it will return to the list. 

    Stormnodes in a error state do not receive payment. 

    -- Future expansion

    We want to be able to prove the nodes have many qualities such as a specific CPU speed, bandwidth,
    and dedicated storage. E.g. We could require a full node be a computer running 2GHz with 10GB of space.

*/

void ProcessMessageStormnodePOS(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    if(fLiteMode) return; //disable all sandstorm/stormnode related functionality
    if(!IsSporkActive(SPORK_7_STORMNODE_SCANNING)) return;
    if(IsInitialBlockDownload()) return;

    if (strCommand == "snse") //Stormnode Scanning Error
    {

        CDataStream vMsg(vRecv);
        CStormnodeScanningError snse;
        vRecv >> snse;

        CInv inv(MSG_STORMNODE_SCANNING_ERROR, snse.GetHash());
        pfrom->AddInventoryKnown(inv);

        if(mapStormnodeScanningErrors.count(snse.GetHash())){
            return;
        }
        mapStormnodeScanningErrors.insert(make_pair(snse.GetHash(), snse));

        if(!snse.IsValid())
        {
            LogPrintf("StormnodePOS::snse - Invalid object\n");   
            return;
        }

        CStormnode* psnA = snodeman.Find(snse.vinStormnodeA);
        if(psnA == NULL) return;
        if(psnA->protocolVersion < MIN_STORMNODE_POS_PROTO_VERSION) return;

        int nBlockHeight = pindexBest->nHeight;
        if(nBlockHeight - snse.nBlockHeight > 10){
            LogPrintf("StormnodePOS::snse - Too old\n");
            return;   
        }

        // Lowest stormnodes in rank check the highest each block
        int a = snodeman.GetStormnodeRank(snse.vinStormnodeA, snse.nBlockHeight, MIN_STORMNODE_POS_PROTO_VERSION);
        if(a == -1 || a > GetCountScanningPerBlock())
        {
            LogPrintf("StormnodePOS::snse - StormnodeA ranking is too high\n");
            return;
        }

        int b = snodeman.GetStormnodeRank(snse.vinStormnodeB, snse.nBlockHeight, MIN_STORMNODE_POS_PROTO_VERSION, false);
        if(b == -1 || b < snodeman.CountStormnodesAboveProtocol(MIN_STORMNODE_POS_PROTO_VERSION)-GetCountScanningPerBlock())
         {
            LogPrintf("StormnodePOS::snse - StormnodeB ranking is too low\n");
            return;
        }

        if(!snse.SignatureValid()){
            LogPrintf("StormnodePOS::snse - Bad stormnode message\n");
            return;
        }

        CStormnode* psnB = snodeman.Find(snse.vinStormnodeB);
        if(psnB == NULL) return;

        if(fDebug) LogPrintf("ProcessMessageStormnodePOS::snse - nHeight %d StormnodeA %s StormnodeB %s\n", snse.nBlockHeight, psnA->addr.ToString().c_str(), psnB->addr.ToString().c_str());

        psnB->ApplyScanningError(snse);
        snse.Relay();
    }
}

// Returns how many stormnodes are allowed to scan each block
int GetCountScanningPerBlock()
{
    return std::max(1, snodeman.CountStormnodesAboveProtocol(MIN_STORMNODE_POS_PROTO_VERSION)/100);
}


void CStormnodeScanning::CleanStormnodeScanningErrors()
{
    if(pindexBest == NULL) return;

    std::map<uint256, CStormnodeScanningError>::iterator it = mapStormnodeScanningErrors.begin();

    while(it != mapStormnodeScanningErrors.end()) {
        if(GetTime() > it->second.nExpiration){ //keep them for an hour
            LogPrintf("Removing old stormnode scanning error %s\n", it->second.GetHash().ToString().c_str());

            mapStormnodeScanningErrors.erase(it++);
        } else {
            it++;
        }
    }

}

// Check other stormnodes to make sure they're running correctly
void CStormnodeScanning::DoStormnodePOSChecks()
{
    if(!fStormNode) return;
    if(fLiteMode) return; //disable all sandstorm/stormnode related functionality
    if(!IsSporkActive(SPORK_7_STORMNODE_SCANNING)) return;
    if(IsInitialBlockDownload()) return;

    int nBlockHeight = pindexBest->nHeight-5;

    int a = snodeman.GetStormnodeRank(activeStormnode.vin, nBlockHeight, MIN_STORMNODE_POS_PROTO_VERSION);
    if(a == -1 || a > GetCountScanningPerBlock()){
        // we don't need to do anything this block
        return;
    }

    // The lowest ranking nodes (Stormnode A) check the highest ranking nodes (Stormnode B)
    CStormnode* psn = snodeman.GetStormnodeByRank(snodeman.CountStormnodesAboveProtocol(MIN_STORMNODE_POS_PROTO_VERSION)-a, nBlockHeight, MIN_STORMNODE_POS_PROTO_VERSION, false);
    if(psn == NULL) return;

    // -- first check : Port is open

    if(!ConnectNode((CAddress)psn->addr, NULL, true)){
        // we couldn't connect to the node, let's send a scanning error
        CStormnodeScanningError snse(activeStormnode.vin, psn->vin, SCANNING_ERROR_NO_RESPONSE, nBlockHeight);
        snse.Sign();
        mapStormnodeScanningErrors.insert(make_pair(snse.GetHash(), snse));
        snse.Relay();
    }

    // success
    CStormnodeScanningError snse(activeStormnode.vin, psn->vin, SCANNING_SUCCESS, nBlockHeight);
    snse.Sign();
    mapStormnodeScanningErrors.insert(make_pair(snse.GetHash(), snse));
    snse.Relay();
}

bool CStormnodeScanningError::SignatureValid()
{
    std::string errorMessage;
    std::string strMessage = vinStormnodeA.ToString() + vinStormnodeB.ToString() + 
        boost::lexical_cast<std::string>(nBlockHeight) + boost::lexical_cast<std::string>(nErrorType);

    CStormnode* psn = snodeman.Find(vinStormnodeA);

    if(psn == NULL)
    {
        LogPrintf("CStormnodeScanningError::SignatureValid() - Unknown Stormnode\n");
        return false;
    }

    CScript pubkey;
    pubkey.SetDestination(psn->pubkey2.GetID());
    CTxDestination address1;
    ExtractDestination(pubkey, address1);
    CFantomAddress address2(address1);

    if(!sandStormSigner.VerifyMessage(psn->pubkey2, vchStormNodeSignature, strMessage, errorMessage)) {
        LogPrintf("CStormnodeScanningError::SignatureValid() - Verify message failed\n");
        return false;
    }

    return true;
}

bool CStormnodeScanningError::Sign()
{
    std::string errorMessage;

    CKey key2;
    CPubKey pubkey2;
    std::string strMessage = vinStormnodeA.ToString() + vinStormnodeB.ToString() + 
        boost::lexical_cast<std::string>(nBlockHeight) + boost::lexical_cast<std::string>(nErrorType);

    if(!sandStormSigner.SetKey(strStormNodePrivKey, errorMessage, key2, pubkey2))
    {
        LogPrintf("CStormnodeScanningError::Sign() - ERROR: Invalid stormnodeprivkey: '%s'\n", errorMessage.c_str());
        return false;
    }

    CScript pubkey;
    pubkey.SetDestination(pubkey2.GetID());
    CTxDestination address1;
    ExtractDestination(pubkey, address1);
    CFantomAddress address2(address1);
    //LogPrintf("signing pubkey2 %s \n", address2.ToString().c_str());

    if(!sandStormSigner.SignMessage(strMessage, errorMessage, vchStormNodeSignature, key2)) {
        LogPrintf("CStormnodeScanningError::Sign() - Sign message failed");
        return false;
    }

    if(!sandStormSigner.VerifyMessage(pubkey2, vchStormNodeSignature, strMessage, errorMessage)) {
        LogPrintf("CStormnodeScanningError::Sign() - Verify message failed");
        return false;
    }

    return true;
}

void CStormnodeScanningError::Relay()
{
    CInv inv(MSG_STORMNODE_SCANNING_ERROR, GetHash());

    vector<CInv> vInv;
    vInv.push_back(inv);
    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes){
        pnode->PushMessage("inv", vInv);
    }
}