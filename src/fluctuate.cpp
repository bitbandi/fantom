// Copyright (c) 2009-2015 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin developers
// Copyright (c) 2015 DuckYeah! (Ahmad Akhtar Ul Islam A Kazi)
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/lexical_cast.hpp>

#include "chainparams.h"
#include "init.h"
#include "kernel.h"
#include "net.h"
#include "txdb.h"
#include "txmempool.h"

using namespace std;
using namespace boost;

double ConvertBitsToDouble(unsigned int nBits)
{
    int nShift = (nBits >> 24) & 0xff;

    double dDiff =
        (double)0x0000ffff / (double)(nBits & 0x00ffffff);

    while (nShift < 29)
    {
        dDiff *= 256.0;
        nShift++;
    }
    while (nShift > 29)
    {
        dDiff /= 256.0;
        nShift--;
    }

    return dDiff;
}

int static generateMTRandom(unsigned int s, int range)
{
    boost::mt19937 gen(s);
    boost::uniform_int<> dist(1, range);
    return dist(gen);
}

int64_t GenerateProofOfWorkVars(int64_t nFees, uint256 prevHash)
{
	std::string cseed_str = prevHash.ToString().substr(7,7);
    const char* cseed = cseed_str.c_str();
    long seed = hex2long(cseed);
    int rand = generateMTRandom(seed, 9);
    int par = generateMTRandom(seed, 99);
	int chip = generateMTRandom(seed, 999);
	int sqoc = generateMTRandom(seed, 4);
	int dDiff = 5;
	
    int64_t nSubsidy = 0;;
    int64_t nVars = ((rand + 1) / chip) * nFees / (par / dDiff);
    
    if (nVars > 100 && nVars < 2) {nSubsidy = rand ^ 4;}
    else {nSubsidy = nVars;}
}

int64_t GenerateBlockTimeVars = dDiff ^ 2 / 25200; // This must be under 60 Seconds
