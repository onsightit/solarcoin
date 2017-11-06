// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pow.h"
#include "chain.h"
#include "primitives/block.h"
#include "util.h"

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

// PoST Target adjustment
/* TODO: Convert this code
static unsigned int GetNextTargetRequired(const CBlockIndex* pindexLast, bool fProofOfStake, const Consensus::Params& params)
{
    arith_uint256 bnTargetLimit = UintToArith256(params.posLimit);
    
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
        nActualSpacing = nTargetSpacing;

    // ppcoin: target change every block
    // ppcoin: retarget with exponential moving toward target spacing
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexPrev->nBits);
    int64_t nInterval = nTargetTimespan / nTargetSpacing;
    bnNew *= ((nInterval - 1) * nTargetSpacing + nActualSpacing + nActualSpacing);
    bnNew /= ((nInterval + 1) * nTargetSpacing);

    if (bnNew <= 0 || bnNew > bnTargetLimit)
        bnNew = bnTargetLimit;

    return bnNew.GetCompact();
}
*/

unsigned int static KimotoGravityWell(const CBlockIndex* pindexLast, uint64_t TargetBlocksSpacingSeconds, uint64_t PastBlocksMin, uint64_t PastBlocksMax, const Consensus::Params& params) {

    arith_uint256 bnProofOfWorkLimit = UintToArith256(params.powLimit);
    arith_uint256 bnStartDiff        = (bnProofOfWorkLimit >> 6);

    if (pindexLast->nHeight+1 == 160 && !fTestNet) {
        LogPrintf("DEBUG: Height 160: %08x %s\n", bnStartDiff.GetCompact(), ArithToUint256(bnStartDiff).ToString().c_str());
        return bnStartDiff.GetCompact();
    }

    const CBlockIndex *BlockLastSolved  = pindexLast;
    const CBlockIndex *BlockReading = pindexLast;
    uint64_t  PastBlocksMass  = 0;
    int64_t   PastRateActualSeconds   = 0;
    int64_t   PastRateTargetSeconds   = 0;
    double  PastRateAdjustmentRatio = double(1);
    arith_uint256 PastDifficultyAverage;
    arith_uint256 PastDifficultyAveragePrev;
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

        if (i == 1) {
            PastDifficultyAverage.SetCompact(BlockReading->nBits);
        } else {
            PastDifficultyAverage = ((arith_uint256().SetCompact(BlockReading->nBits) - PastDifficultyAveragePrev) / i) + PastDifficultyAveragePrev;
        }

        PastDifficultyAveragePrev = PastDifficultyAverage;
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
        if (BlockReading->pprev == NULL) {
            assert(BlockReading);
            break;
        }
        BlockReading = BlockReading->pprev;
    }

    arith_uint256 bnNew(PastDifficultyAverage);
    LogPrintf("DEBUG: bnNew(PastDifficultyAverage): %08x %s\n", bnNew.GetCompact(), ArithToUint256(bnNew).ToString().c_str());
    if (PastRateActualSeconds != 0 && PastRateTargetSeconds != 0) {
        bnNew *= PastRateActualSeconds;
        LogPrintf("DEBUG: bnNew *= PastRateActualSeconds: %08x PastRateActualSeconds: %lu\n", bnNew.GetCompact(), PastRateActualSeconds);
        bnNew /= PastRateTargetSeconds;
        LogPrintf("DEBUG: bnNew /= PastRateTargetSeconds: %08x PastRateTargetSeconds: %lu\n", bnNew.GetCompact(), PastRateTargetSeconds);
    }
    // TODO: bnNew should NOT be greater than bnPoWLimit
    if (bnNew > bnProofOfWorkLimit) {
        LogPrintf("DEBUG: %08x %s\n", bnNew.GetCompact(), ArithToUint256(bnNew).ToString().c_str());
        bnNew = bnProofOfWorkLimit;
        LogPrintf("DEBUG: bnNew set to bnProofOfWorkLimit.\n");
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

        LogPrintf("DEBUG: nTargetTimespan = %lu    nInterval = %ld\n", nTargetTimespan, nInterval);
        return pindexLast->nBits;
    }

    // Litecoin: This fixes an issue where a 51% attack can change difficulty at will.
    // Go back the full period unless it's the first retarget after genesis. Code courtesy of Art Forz
    int blockstogoback = nInterval-1;
    if ((pindexLast->nHeight+1) != nInterval)
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
    int DiffMode = (pindexLast->nHeight+1 >= 310000 ? 2 : 1);

    LogPrintf("DEBUG: DiffMode = %d\n", DiffMode);
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

    // DEBUG
    LogPrintf("DEBUG: CheckProofOfWork: nBits=%08x bnTarget=%s\n", nBits, bnTarget.ToString().c_str());
    
        // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}
