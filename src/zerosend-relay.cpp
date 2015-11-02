
#include "zerosend-relay.h"


CZeroSendRelay::CZeroSendRelay()
{
    vinBlanknode = CTxIn();
    nBlockHeight = 0;
    nRelayType = 0;
    in = CTxIn();
    out = CTxOut();
}

CZeroSendRelay::CZeroSendRelay(CTxIn& vinBlanknodeIn, vector<unsigned char>& vchSigIn, int nBlockHeightIn, int nRelayTypeIn, CTxIn& in2, CTxOut& out2)
{
    vinBlanknode = vinBlanknodeIn;
    vchSig = vchSigIn;
    nBlockHeight = nBlockHeightIn;
    nRelayType = nRelayTypeIn;
    in = in2;
    out = out2;
}

std::string CZeroSendRelay::ToString()
{
    std::ostringstream info;

    info << "vin: " << vinBlanknode.ToString() <<
        " nBlockHeight: " << (int)nBlockHeight <<
        " nRelayType: "  << (int)nRelayType <<
        " in " << in.ToString() <<
        " out " << out.ToString();
        
    return info.str();   
}

bool CZeroSendRelay::Sign(std::string strSharedKey)
{
    std::string strMessage = in.ToString() + out.ToString();

    CKey key2;
    CPubKey pubkey2;
    std::string errorMessage = "";

    if(!zeroSendSigner.SetKey(strSharedKey, errorMessage, key2, pubkey2))
    {
        LogPrintf("CZeroSendRelay()::Sign - ERROR: Invalid shared key: '%s'\n", errorMessage.c_str());
        return false;
    }

    if(!zeroSendSigner.SignMessage(strMessage, errorMessage, vchSig2, key2)) 
    {
        LogPrintf("CZeroSendRelay():Sign - Sign message failed\n");
        return false;
    }

    if(!zeroSendSigner.VerifyMessage(pubkey2, vchSig2, strMessage, errorMessage)) 
    {
        LogPrintf("CZeroSendRelay():Sign - Verify message failed\n");
        return false;
    }

    return true;
}

bool CZeroSendRelay::VerifyMessage(std::string strSharedKey)
{
    std::string strMessage = in.ToString() + out.ToString();

    CKey key2;
    CPubKey pubkey2;
    std::string errorMessage = "";

    if(!zeroSendSigner.SetKey(strSharedKey, errorMessage, key2, pubkey2))
    {
        LogPrintf("CZeroSendRelay()::VerifyMessage - ERROR: Invalid shared key: '%s'\n", errorMessage.c_str());
         return false;
    }

    if(!zeroSendSigner.VerifyMessage(pubkey2, vchSig2, strMessage, errorMessage)) {
        LogPrintf("CZeroSendRelay()::VerifyMessage - Verify message failed\n");
        return false;    }

    return true;
}

void CZeroSendRelay::Relay()
{
    int nCount = std::min(snodeman.CountEnabled(), 20);
    int nRank1 = (rand() % nCount)+1; 
    int nRank2 = (rand() % nCount)+1; 

    //keep picking another second number till we get one that doesn't match
    while(nRank1 == nRank2) nRank2 = (rand() % nCount)+1;

    //printf("rank 1 - rank2 %d %d \n", nRank1, nRank2);

    //relay this message through 2 separate nodes for redundancy
    RelayThroughNode(nRank1);
    RelayThroughNode(nRank2);
}

void CZeroSendRelay::RelayThroughNode(int nRank)
{
    CBlanknode* psn = snodeman.GetBlanknodeByRank(nRank, nBlockHeight, MIN_ZEROSEND_PROTO_VERSION);

    if(psn != NULL){
        //printf("RelayThroughNode %s\n", psn->addr.ToString().c_str());
        if(ConnectNode((CAddress)psn->addr, NULL, true)){
            //printf("Connected\n");
            CNode* pNode = FindNode(psn->addr);
            if(pNode)
            {
                //printf("Found\n");
                pNode->PushMessage("ssr", (*this));
                return;
            }
        }
    } else {
        //printf("RelayThroughNode NULL\n");
    }
} 
