// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chain.h"
#include "chainparams.h"
#include "validation.h"

#include "post.h"
#include "kernel.h"
#include "timedata.h"
#include "util.h"

/*
* SolarCoin: PoW / PoST
*/

// get current inflation rate using average stake weight ~1.5-2.5% (measure of liquidity) PoST
double GetCurrentInflationRate(double nAverageWeight)
{
    double inflationRate = (17*(log(nAverageWeight/20)))/100;

    return inflationRate;
}

// get current interest rate by targeting for network stake dependent inflation rate PoST
double GetCurrentInterestRate(CBlockIndex* pindexPrev, const Consensus::Params& params)
{
    double interestRate = 0;
    int twoPercentIntHeight = params.TWO_PERCENT_INT_HEIGHT;
    int twoPercentInt = params.TWO_PERCENT_INT;

    // Fixed interest rate after PoW + 1000
    if (pindexPrev->nHeight > twoPercentIntHeight)
    {
        interestRate = twoPercentInt;
    } else {
        double nAverageWeight = GetAverageStakeWeight(pindexPrev);
        double inflationRate = GetCurrentInflationRate(nAverageWeight) / 100;
        // Bug fix: Should be "GetCurrentCoinSupply(pindexPrev) * COIN", but this code is no longer executed.
        interestRate = ((inflationRate * GetCurrentCoinSupply(pindexPrev, params)) / nAverageWeight) * 100;

        // Cap interest rate (must use the 2.0.2 interest rate value)
        if (interestRate > 10.0)
            interestRate = 10.0;
    }

    return interestRate;
}

// Get the current coin supply / COIN
int64_t GetCurrentCoinSupply(CBlockIndex* pindexPrev, const Consensus::Params& params)
{
    // removed addition of 1.35 SLR / block after 835000 + 1000
    if (pindexPrev->nHeight > params.TWO_PERCENT_INT_HEIGHT)
        if (pindexPrev->nHeight >= params.FORK_HEIGHT_2)
            // Bug fix: pindexPrev->nMoneySupply is an int64_t that has overflowed and is now negative.
            // Use the real coin supply + expected growth rate since twoPercentIntHeight from granting.
            return ((pindexPrev->nMoneySupply - (98000000000 * COIN)) / COIN) + (int64_t)((double)(pindexPrev->nHeight - params.TWO_PERCENT_INT_HEIGHT) * params.COIN_SUPPLY_GROWTH_RATE);
        else
            return params.INITIAL_COIN_SUPPLY;
    else
        return (params.INITIAL_COIN_SUPPLY + ((pindexPrev->nHeight - params.LAST_POW_BLOCK) * params.COIN_SUPPLY_GROWTH_RATE));
}

// Get the block rate for one hour
int GetBlockRatePerHour(const Consensus::Params& params)
{
    int nRate = 0;
    CBlockIndex* pindex = chainActive.Tip();
    int64_t nTargetTime = GetAdjustedTime() - 3600;

    while (pindex && pindex->pprev && pindex->nTime > nTargetTime) {
        nRate += 1;
        pindex = pindex->pprev;
    }
    if (nRate < params.nTargetSpacing / 2)
        LogPrintf("GetBlockRatePerHour: Warning, block rate (%d) is less than half of nTargetSpacing=%ld.\n", nRate, params.nTargetSpacing);
    return nRate;
}

// Stakers coin reward based on coin stake time factor and targeted inflation rate PoST
int64_t GetProofOfStakeTimeReward(int64_t nStakeTime, int64_t nFees, CBlockIndex* pindexPrev, const Consensus::Params& params)
{
    int64_t nInterestRate = GetCurrentInterestRate(pindexPrev, params)*CENT;
    int64_t nSubsidy = nStakeTime * nInterestRate * 33 / (365 * 33 + 8);

    if (fDebug && gArgs.GetBoolArg("-printcreation", false))
        LogPrintf("GetProofOfStakeTimeReward(): create=%s nStakeTime=%ld\n", FormatMoney(nSubsidy).c_str(), nStakeTime);

    return nSubsidy + nFees;
}

// Target adjustment V2
static unsigned int GetNextTargetRequiredV1(const CBlockIndex* pindexLast, bool fProofOfStake, const Consensus::Params& params)
{
    arith_uint256 bnTargetLimit = fProofOfStake ? UintToArith256(params.posLimit) : UintToArith256(params.powLimit);
    
    if (pindexLast == nullptr)
        return bnTargetLimit.GetCompact(); // genesis block

    const CBlockIndex* pindexPrev = GetLastBlockIndex(pindexLast, fProofOfStake);
    if (pindexPrev->pprev == nullptr)
        return bnTargetLimit.GetCompact(); // first block
    const CBlockIndex* pindexPrevPrev = GetLastBlockIndex(pindexPrev->pprev, fProofOfStake);
    if (pindexPrevPrev->pprev == nullptr)
        return bnTargetLimit.GetCompact(); // second block

    int64_t nActualSpacing = pindexPrev->GetBlockTime() - pindexPrevPrev->GetBlockTime();

    // ppcoin: target change every block
    // ppcoin: retarget with exponential moving toward target spacing
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexPrev->nBits);
    int64_t nInterval = params.DifficultyAdjustmentInterval_V1();
    bnNew *= ((nInterval - 1) * params.nTargetSpacing + nActualSpacing + nActualSpacing);
    bnNew /= ((nInterval + 1) * params.nTargetSpacing);

    if (bnNew > bnTargetLimit)
        bnNew = bnTargetLimit;

    return bnNew.GetCompact();
}

// Target adjustment V2
unsigned int GetNextTargetRequiredV2(const CBlockIndex* pindexLast, bool fProofOfStake, const Consensus::Params& params)
{
    arith_uint256 bnTargetLimit = fProofOfStake ? UintToArith256(params.posLimit) : UintToArith256(params.powLimit);
    
    if (pindexLast == nullptr)
        return bnTargetLimit.GetCompact(); // genesis block

    const CBlockIndex* pindexPrev = GetLastBlockIndex(pindexLast, fProofOfStake);
    if (pindexPrev->pprev == nullptr)
        return bnTargetLimit.GetCompact(); // first block
    const CBlockIndex* pindexPrevPrev = GetLastBlockIndex(pindexPrev->pprev, fProofOfStake);
    if (pindexPrevPrev->pprev == nullptr)
        return bnTargetLimit.GetCompact(); // second block

    int64_t nActualSpacing = pindexPrev->GetBlockTime() - pindexPrevPrev->GetBlockTime();
    if (nActualSpacing < 0)
        nActualSpacing = params.nTargetSpacing;

    // ppcoin: target change every block
    // ppcoin: retarget with exponential moving toward target spacing
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexPrev->nBits);
    int64_t nInterval = params.DifficultyAdjustmentInterval_V2();
    bnNew *= ((nInterval - 1) * params.nTargetSpacing + nActualSpacing + nActualSpacing);
    bnNew /= ((nInterval + 1) * params.nTargetSpacing);

    if (bnNew <= 0 || bnNew > bnTargetLimit)
        bnNew = bnTargetLimit;

    return bnNew.GetCompact();
}

// PoST Target adjustment
unsigned int GetNextTargetRequired(const CBlockIndex* pindexLast, bool fProofOfStake, const Consensus::Params& params)
{
    int DiffMode = (pindexLast->nHeight+1 >= 310000 || fTestNet ? 2 : 1);

    //LogPrintf("DEBUG: DiffMode = %d\n", DiffMode);
    if (DiffMode == 1) {
        return GetNextTargetRequiredV1(pindexLast, fProofOfStake, params);
    } else {
        return GetNextTargetRequiredV2(pindexLast, fProofOfStake, params);
    }
}

// PoW Difficulty adjustment
unsigned int static KimotoGravityWell(const CBlockIndex* pindexLast, uint64_t TargetBlocksSpacingSeconds, uint64_t PastBlocksMin, uint64_t PastBlocksMax, const Consensus::Params& params) {

    arith_uint256 bnProofOfWorkLimit = UintToArith256(params.powLimit);
    arith_uint256 bnStartDiff        = (bnProofOfWorkLimit >> 6);

    if (pindexLast->nHeight+1 == 160 && !fTestNet) {
        //LogPrintf("DEBUG: Height 160: %08x %s\n", bnStartDiff.GetCompact(), ArithToUint256(bnStartDiff).ToString().c_str());
        return bnStartDiff.GetCompact();
    }

    const CBlockIndex *BlockLastSolved  = pindexLast;
    const CBlockIndex *BlockReading = pindexLast;
    arith_uint256 PastDifficultyAverage = arith_uint256().SetCompact(0);
    uint64_t  PastBlocksMass  = 0;
    int64_t   PastRateActualSeconds   = 0;
    int64_t   PastRateTargetSeconds   = 0;
    double  PastRateAdjustmentRatio = double(1);
    double  EventHorizonDeviation;
    double  EventHorizonDeviationFast;
    double  EventHorizonDeviationSlow;

    if (BlockLastSolved == nullptr || BlockLastSolved->nHeight == 0 || (uint64_t)BlockLastSolved->nHeight < PastBlocksMin) {
        return bnProofOfWorkLimit.GetCompact();
    }

    int64_t LatestBlockTime = BlockLastSolved->GetBlockTime();

    for (unsigned int i = 1; BlockReading && BlockReading->nHeight > 0; i++) {
        if (PastBlocksMax > 0 && i > PastBlocksMax) {
            break;
        }
        PastBlocksMass++;

        PastDifficultyAverage += arith_uint256().SetCompact(BlockReading->nBits);

        PastRateActualSeconds   = BlockLastSolved->GetBlockTime() - BlockReading->GetBlockTime();

        if (LatestBlockTime < BlockReading->GetBlockTime()) {
            if (BlockReading->nHeight > 16000) { // HARD Fork block number
                LatestBlockTime = BlockReading->GetBlockTime();
            }
        }
        PastRateActualSeconds   = LatestBlockTime - BlockReading->GetBlockTime();
        PastRateTargetSeconds   = TargetBlocksSpacingSeconds * PastBlocksMass;
        PastRateAdjustmentRatio = double(1);

        if (PastRateActualSeconds < 0) {
            PastRateActualSeconds = 0;
        }
        if (BlockReading->nHeight > 16000) { // HARD Fork block number
            if (PastRateActualSeconds < 1) {
                PastRateActualSeconds = 1;
            }
        } else {
            if (PastRateActualSeconds < 0) {
                PastRateActualSeconds = 0;
            }
        }
        if (PastRateActualSeconds != 0 && PastRateTargetSeconds != 0) {
            PastRateAdjustmentRatio = double(PastRateTargetSeconds) / double(PastRateActualSeconds);
        }
        EventHorizonDeviation   = 1 + (0.7084 * pow((double(PastBlocksMass)/double(144)), -1.228));
        EventHorizonDeviationFast   = EventHorizonDeviation;
        EventHorizonDeviationSlow   = 1 / EventHorizonDeviation;

        if (PastBlocksMass >= PastBlocksMin) {
            if ((PastRateAdjustmentRatio <= EventHorizonDeviationSlow) || (PastRateAdjustmentRatio >= EventHorizonDeviationFast)) {
                assert(BlockReading);
                break;
            }
        }
        if (BlockReading->pprev == nullptr) {
            assert(BlockReading);
            break;
        }
        BlockReading = BlockReading->pprev;
    }

    if (PastBlocksMass > 0) {
        PastDifficultyAverage = PastDifficultyAverage / PastBlocksMass;
    } else {
        PastDifficultyAverage = bnProofOfWorkLimit;
    }

    arith_uint256 bnNew = PastDifficultyAverage;
    //LogPrintf("DEBUG: bnNew=%08x %s\n", bnNew.GetCompact(), ArithToUint256(bnNew).ToString().c_str());
    if (PastRateActualSeconds != 0 && PastRateTargetSeconds != 0) {
        bnNew *= PastRateActualSeconds;
        bnNew /= PastRateTargetSeconds;
    }
    // TODO: bnNew should NOT be greater than bnPoWLimit
    if (bnNew > bnProofOfWorkLimit) {
        bnNew = bnProofOfWorkLimit;
    }

    /// debug print
    LogPrintf("Difficulty Retarget - Gravity Well\n");
    LogPrintf("PastRateAdjustmentRatio = %g\n", PastRateAdjustmentRatio);
    LogPrintf("Before: %08x %s\n", BlockLastSolved->nBits, ArithToUint256(arith_uint256().SetCompact(BlockLastSolved->nBits)).ToString().c_str());
    LogPrintf("After: %08x %s\n", bnNew.GetCompact(), ArithToUint256(bnNew).ToString().c_str());

    return bnNew.GetCompact();
}

unsigned int static GetNextWorkRequired_V1(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();
    bool fTestNet = false; // TODO: params.fTestNet;

    // Genesis block
    if (pindexLast == nullptr)
        return nProofOfWorkLimit;

    unsigned int nInterval;
    unsigned int nTargetTimespan;

    if (pindexLast->nHeight+1 < params.nHeight_Version2)
    {
        nInterval = params.DifficultyAdjustmentInterval_V1();
        nTargetTimespan = params.nTargetTimespan_Version1;
    }
    else
    {
        nInterval = params.DifficultyAdjustmentInterval_V2();
        nTargetTimespan = params.nTargetTimespan_Version2;
    }

    // Only change once per interval
    if ((pindexLast->nHeight+1) % nInterval != 0)
    {
        // Special difficulty rule for testnet:
        if (fTestNet)
        {
            // If the new block's timestamp is more than 2* 10 minutes
            // then allow mining of a min-difficulty block.
            if (pblock->nTime > pindexLast->nTime + params.nTargetSpacing * 2)
                return nProofOfWorkLimit;
            else
            {
                // Return the last non-special-min-difficulty-rules-block
                const CBlockIndex* pindex = pindexLast;
                while (pindex->pprev && pindex->nHeight % nInterval != 0 && pindex->nBits == nProofOfWorkLimit)
                    pindex = pindex->pprev;
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

    // Litecoin: This fixes an issue where a 51% attack can change difficulty at will.
    // Go back the full period unless it's the first retarget after genesis. Code courtesy of Art Forz
    int blockstogoback = nInterval-1;
    if ((unsigned int)(pindexLast->nHeight+1) != nInterval)
        blockstogoback = nInterval;

    // Go back by what we want to be 14 days worth of blocks
    const CBlockIndex* pindexFirst = pindexLast;
    for (int i = 0; pindexFirst && i < blockstogoback; i++)
        pindexFirst = pindexFirst->pprev;
    assert(pindexFirst);

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - pindexFirst->GetBlockTime();
    LogPrintf("  nActualTimespan = %ld  before bounds\n", nActualTimespan);
    if (nActualTimespan < nTargetTimespan/4)
        nActualTimespan = nTargetTimespan/4;
    if (nActualTimespan > nTargetTimespan*4)
        nActualTimespan = nTargetTimespan*4;

    // Retarget
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= nTargetTimespan;

    arith_uint256 bnProofOfWorkLimit = arith_uint256().SetCompact(nProofOfWorkLimit);

    if (bnNew > bnProofOfWorkLimit)
        bnNew = bnProofOfWorkLimit;

    /// debug print
    LogPrintf("GetNextWorkRequired RETARGET\n");
    LogPrintf("nTargetTimespan = %ld    nActualTimespan = %ld\n", nTargetTimespan, nActualTimespan);
    LogPrintf("Before: %08x  %s\n", pindexLast->nBits, ArithToUint256(arith_uint256().SetCompact(pindexLast->nBits)).ToString().c_str());
    LogPrintf("After:  %08x  %s\n", bnNew.GetCompact(), ArithToUint256(bnNew).ToString().c_str());

    return bnNew.GetCompact();
}

unsigned int static GetNextWorkRequired_V2(const CBlockIndex* pindexLast, const Consensus::Params& params)
{
    // Kimoto Gravity Well (Patched for Time-warp exploit 4/5/14)
    static const int64_t  BlocksTargetSpacing = 60; // 1 minute
    unsigned int TimeDaySeconds  = 60 * 60 * 24;
    int64_t   PastSecondsMin  = TimeDaySeconds * 0.1;
    int64_t   PastSecondsMax  = TimeDaySeconds * 2.8;
    uint64_t  PastBlocksMin   = PastSecondsMin / BlocksTargetSpacing;
    uint64_t  PastBlocksMax   = PastSecondsMax / BlocksTargetSpacing;

    if (fTestNet && gArgs.GetBoolArg("-zerogravity", false))
        return UintToArith256(params.powLimit).GetCompact();
    else
        return KimotoGravityWell(pindexLast, BlocksTargetSpacing, PastBlocksMin, PastBlocksMax, params);
}

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    int DiffMode = (pindexLast->nHeight+1 >= 310000 || fTestNet ? 2 : 1);

    //LogPrintf("DEBUG: DiffMode = %d\n", DiffMode);
    if (DiffMode == 1) {
        return GetNextWorkRequired_V1(pindexLast, pblock, params); // nHeight_Version2 = 208440 (less than 310000)
    } else {
        return GetNextWorkRequired_V2(pindexLast, params);
    }
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    //LogPrintf("DEBUG: CheckProofOfWork: nBits=%08x bnTarget=%s\n", nBits, bnTarget.ToString().c_str());

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}

// ppcoin: find last block index up to pindex
const CBlockIndex* GetLastBlockIndex(const CBlockIndex* pindex, bool fProofOfStake)
{
    while (pindex && pindex->pprev && (pindex->IsProofOfStake() != fProofOfStake))
        pindex = pindex->pprev;
    return pindex;
}

/// Used in tests
unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    if (nActualTimespan < params.nTargetTimespan_Version1/4)
        nActualTimespan = params.nTargetTimespan_Version1/4;
    if (nActualTimespan > params.nTargetTimespan_Version1*4)
        nActualTimespan = params.nTargetTimespan_Version1*4;

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew;
    arith_uint256 bnOld;
    bnNew.SetCompact(pindexLast->nBits);
    bnOld = bnNew;
    // Solarcoin: intermediate uint256 can overflow by 1 bit
    bool fShift = bnNew.bits() > bnPowLimit.bits() - 1;
    if (fShift)
        bnNew >>= 1;
    bnNew *= nActualTimespan;
    bnNew /= params.nTargetTimespan_Version1;
    if (fShift)
        bnNew <<= 1;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}
