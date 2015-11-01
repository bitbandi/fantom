// Copyright(c) 2015 - Ahmad Akhtar Ul Islam Ansaruddin A Kazi
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef AUTO_ADJUST_PROT_H
#define AUTO_ADJUST_PROT_H

#include "core.h"
#include "bignum.h"
#include "sync.h"
#include "txmempool.h"
#include "net.h"
#include "script.h"
#include "scrypt.h"

#include <list>

/**
 * The Purpose of this file is to help the Network's
 * Auto Adjustment Protocol Function, It uses a mix
 * of psudo-random and non-random variable in key
 * places of the currencies network, this file may
 * part play the role of a chainparams styled file
 * which should (will) be curbed in future versions.
 * 
 * This should not be changed without forks... Thank You.
 */

int64_t GenerateProofOfWorkVars(int64_t nFees, uint256 prevHash);
// int64_t GenerateBlockTimeVars;
int64_t GenerateRetargetVars(int64_t sysDiff, int64_t randomThought, int64_t sysRTFormula);
int64_t GenerateProofOfStakeVars(int64_t nFees, int64_t nPFormula, int64_t curCoinHoldings);

#endif
