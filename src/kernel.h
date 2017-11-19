// Copyright (c) 2012-2013 The PPCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef PPCOIN_KERNEL_H
#define PPCOIN_KERNEL_H

#include "consensus/params.h"
#include "primitives/block.h"

// MODIFIER_INTERVAL_RATIO:
// ratio of group interval length between the last group and the first group
static const int MODIFIER_INTERVAL_RATIO = 3;

int64_t GetWeight(int64_t nIntervalBeginning, int64_t nIntervalEnd, const Consensus::Params& params);
bool ComputeNextStakeModifier(const CBlockIndex* pindexCurrent, uint64_t& nStakeModifier, bool& fGeneratedStakeModifier);
bool CheckStakeTimeKernelHash(unsigned int nBits, const CBlock& blockFrom, unsigned int nTxPrevOffset, const CTransaction& txPrev, const COutPoint& prevout, unsigned int nTimeTx, uint256& hashProofOfStake, uint256& targetProofOfStake, CBlockIndex* pindexPrev, bool fPrintProofOfStake, const Consensus::Params& params);
bool CheckProofOfStake(const CTransaction& tx, unsigned int nBits, uint256& hashProofOfStake, uint256& targetProofOfStake, const Consensus::Params& params);
bool CheckCoinStakeTimestamp(int64_t nTimeBlock, int64_t nTimeTx);
unsigned int GetStakeModifierChecksum(const CBlockIndex* pindex);
bool CheckStakeModifierCheckpoints(int nHeight, unsigned int nStakeModifierChecksum);
double GetAverageStakeWeight(CBlockIndex* pindexPrev, const Consensus::Params& params);
int64_t GetStakeModifierSelectionIntervalSection(int nSection, const Consensus::Params& params);
int64_t GetStakeModifierSelectionInterval(const Consensus::Params& params);
int64_t GetStakeTimeFactoredWeight(int64_t timeWeight, int64_t bnCoinDayWeight, CBlockIndex* pindexPrev, const Consensus::Params& params);
double GetPoSKernelPS(CBlockIndex* pindexPrev);

// This is needed because the foreach macro can't get over the comma in pair<t1, t2>
#define PAIRTYPE(t1, t2)    std::pair<t1, t2>

#endif // PPCOIN_KERNEL_H
