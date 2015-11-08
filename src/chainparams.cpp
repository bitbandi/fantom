// Copyright (c) 2010-2015 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin developers
// Copyright (c) 2015 DuckYeah! (Ahmad Akhtar Ul Islam A Kazi)
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "assert.h"

#include "chainparams.h"
#include "main.h"
#include "util.h"

#include <boost/assign/list_of.hpp>

using namespace boost::assign;

struct SeedSpec6 {
    uint8_t addr[16];
    uint16_t port;
};

#include "chainparamsseeds.h"

//
// Main network
//

// Convert the pnSeeds array into usable address objects.
static void convertSeeds(std::vector<CAddress> &vSeedsOut, const unsigned int *data, unsigned int count, int port)
{
    // It'll only connect to one or two seed nodes because once it connects,
    // it'll get a pile of addresses with newer timestamps.
    // Seed nodes are given a random 'last seen time' of between one and two
    // weeks ago.
    const int64_t nOneWeek = 7*24*60*60;
    for (unsigned int k = 0; k < count; ++k)
    {
        struct in_addr ip;
        unsigned int i = data[k], t;
        
        // -- convert to big endian
        t =   (i & 0x000000ff) << 24u
            | (i & 0x0000ff00) << 8u
            | (i & 0x00ff0000) >> 8u
            | (i & 0xff000000) >> 24u;
        
        memcpy(&ip, &t, sizeof(ip));
        
        CAddress addr(CService(ip, port));
        addr.nTime = GetTime()-GetRand(nOneWeek)-nOneWeek;
        vSeedsOut.push_back(addr);
    }
}

class CMainParams : public CChainParams {
public:
    CMainParams() {
        // The message start string is designed to be unlikely to occur in normal data.
        // The characters are rarely used upper ASCII, not valid as UTF-8, and produce
        // a large 4-byte int at any alignment.
        pchMessageStart[0] = 0x1f;
        pchMessageStart[1] = 0x22;
        pchMessageStart[2] = 0x05;
        pchMessageStart[3] = 0x31;
        vAlertPubKey = ParseHex("");
        nDefaultPort = 31000;
        nRPCPort = 31500;
        bnProofOfWorkLimit = CBigNum(~uint256(0) >> 12); // PoW starting difficulty = 0.0002441

        // Build the genesis block. Note that the output of the genesis coinbase cannot
        // be spent as it did not originally exist in the database.
        
        const char* pszTimestamp = "Prince Harry meets Obamas, gives royal push for Invictus Games for Troops";
        CTransaction txNew;
		txNew.nTime = 1446489000;
        txNew.vin.resize(1);
        txNew.vout.resize(1);
        txNew.vin[0].scriptSig = CScript() << 0 << CBigNum(42) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
        txNew.vout[0].nValue = 8250 * COIN;
        txNew.vout[0].scriptPubKey = CScript() << ParseHex("0435807e4107407bbe2894c27cdc12b73c81c692a705bcefe57bf048a9fb9b071c173f39ca15735981a47eca5e2504fb812fc36c9f4fe4565a62c5ad7523996dd6") << OP_CHECKSIG; // FmTLDsQnLuUtg2AjCQ3Nv1K8eEqzEhDjb1
        genesis.vtx.push_back(txNew);
        genesis.hashPrevBlock = 0;
        genesis.hashMerkleRoot = genesis.BuildMerkleTree();
        genesis.nVersion = 1;
        genesis.nTime    = 1446489000; //Change to current UNIX Time of generated genesis
        genesis.nBits    = bnProofOfWorkLimit.GetCompact();
        genesis.nNonce   = 4222;

        hashGenesisBlock = genesis.GetHash(); 
        
        assert(hashGenesisBlock == uint256("0x00023849a06b0457577455af0636c8ff17386c18b86a86b6f2fb3595b414f627"));
        assert(genesis.hashMerkleRoot == uint256("0x6ef161b6a21f9b9fdf67ddf63e6e7c644331aff731a7b6ba0efd919d5575214c"));
        
        hashGenesisBlock = uint256("0x00023849a06b0457577455af0636c8ff17386c18b86a86b6f2fb3595b414f627");
        	        
        base58Prefixes[PUBKEY_ADDRESS] = list_of(36);                     //Fantom addresses start with 'D'
        base58Prefixes[SCRIPT_ADDRESS] = list_of(75);                     //Fantom script addresses start with '5'
        base58Prefixes[SECRET_KEY] =     list_of(76);                    //Fantom private keys start with 'y'              
        base58Prefixes[EXT_PUBLIC_KEY] = list_of(0x88)(0x52)(0xFE)(0xC8); //Fantom BIP32 pubkeys start with 'drks'
        base58Prefixes[EXT_SECRET_KEY] = list_of(0x88)(0x52)(0x9C)(0x2C); //Fantom BIP32 prvkeys start with 'drky'

        convertSeeds(vFixedSeeds, pnSeed, ARRAYLEN(pnSeed), nDefaultPort);

        nLastPOWBlock = 8500000;
    }

    virtual const CBlock& GenesisBlock() const { return genesis; }
    virtual Network NetworkID() const { return CChainParams::MAIN; }

    virtual const vector<CAddress>& FixedSeeds() const {
        return vFixedSeeds;
    }
protected:
    CBlock genesis;
    vector<CAddress> vFixedSeeds;
};
static CMainParams mainParams;


//
// Testnet
//

class CTestNetParams : public CMainParams {
public:
    CTestNetParams() {
        // The message start string is designed to be unlikely to occur in normal data.
        // The characters are rarely used upper ASCII, not valid as UTF-8, and produce
        // a large 4-byte int at any alignment.
        pchMessageStart[0] = 0x1f;
        pchMessageStart[1] = 0x22;
        pchMessageStart[2] = 0x05;
        pchMessageStart[3] = 0x30;
        bnProofOfWorkLimit = CBigNum(~uint256(0) >> 12); // PoW starting difficulty = 0.0002441
        vAlertPubKey = ParseHex("");
        nDefaultPort = 31750;
        nRPCPort = 31800;
        strDataDir = "testnet";

        // Modify the testnet genesis block so the timestamp is valid for a later start.
        genesis.nTime = 1446489500;
        genesis.nBits  = bnProofOfWorkLimit.GetCompact(); 
        genesis.nNonce = 0;
        hashGenesisBlock = genesis.GetHash(); 
        
		vFixedSeeds.clear();
        vSeeds.clear();

        base58Prefixes[PUBKEY_ADDRESS] = list_of(36);                     //Fantom addresses start with 'D'
        base58Prefixes[SCRIPT_ADDRESS] = list_of(10);                     //Fantom script addresses start with '5'
        base58Prefixes[SECRET_KEY] =     list_of(140);                    //Fantom private keys start with 'y'              
        base58Prefixes[EXT_PUBLIC_KEY] = list_of(0x02)(0xFE)(0x52)(0x7D); //Fantom BIP32 pubkeys start with 'drks'
        base58Prefixes[EXT_SECRET_KEY] = list_of(0x02)(0xFE)(0x52)(0x8C); //Fantom BIP32 prvkeys start with 'drky'
		
		convertSeeds(vFixedSeeds, pnTestnetSeed, ARRAYLEN(pnTestnetSeed), nDefaultPort);

        nLastPOWBlock = 100;
    }

    virtual Network NetworkID() const { return CChainParams::TESTNET; }
};
static CTestNetParams testNetParams;

static CChainParams *pCurrentParams = &mainParams;

const CChainParams &Params() {
    return *pCurrentParams;
}

void SelectParams(CChainParams::Network network) {
    switch (network) {
        case CChainParams::MAIN:
            pCurrentParams = &mainParams;
            break;
        case CChainParams::TESTNET:
            pCurrentParams = &testNetParams;
            break;
        default:
            assert(false && "Unimplemented network");
            return;
    }
}

bool SelectParamsFromCommandLine() {
    
    bool fTestNet = GetBoolArg("-testnet", false);
    
    if (fTestNet) {
        SelectParams(CChainParams::TESTNET);
    } else {
        SelectParams(CChainParams::MAIN);
    }
    return true;
}
