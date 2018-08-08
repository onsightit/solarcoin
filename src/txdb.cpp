// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "txdb.h"

#include "chainparams.h"
#include "validation.h"
#include "hash.h"
#include "pow.h"
#include "uint256.h"

#include <stdint.h>

#include <boost/thread.hpp>

using namespace std;

static const char DB_COINS = 'c';
static const char DB_BLOCK_FILES = 'f';
static const char DB_TXINDEX = 't';
static const char DB_BLOCK_INDEX = 'b';

static const char DB_BEST_BLOCK = 'B';
static const char DB_FLAG = 'F';
static const char DB_REINDEX_FLAG = 'R';
static const char DB_LAST_BLOCK = 'l';


CCoinsViewDB::CCoinsViewDB(size_t nCacheSize, bool fMemory, bool fWipe) : db(GetDataDir() / "chainstate", nCacheSize, fMemory, fWipe, true) 
{
}

bool CCoinsViewDB::GetCoins(const uint256 &txid, CCoins &coins) const {
    return db.Read(std::make_pair(DB_COINS, txid), coins);
}

bool CCoinsViewDB::HaveCoins(const uint256 &txid) const {
    return db.Exists(std::make_pair(DB_COINS, txid));
}

uint256 CCoinsViewDB::GetBestBlock() const {
    uint256 hashBestChain;
    if (!db.Read(DB_BEST_BLOCK, hashBestChain))
        return uint256();
    return hashBestChain;
}

bool CCoinsViewDB::BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock) {
    CDBBatch batch(db);
    size_t count = 0;
    size_t changed = 0;
    for (CCoinsMap::iterator it = mapCoins.begin(); it != mapCoins.end();) {
        if (it->second.flags & CCoinsCacheEntry::DIRTY) {
            if (it->second.coins.IsPruned())
                batch.Erase(std::make_pair(DB_COINS, it->first));
            else
                batch.Write(std::make_pair(DB_COINS, it->first), it->second.coins);
            changed++;
        }
        count++;
        CCoinsMap::iterator itOld = it++;
        mapCoins.erase(itOld);
    }
    if (!hashBlock.IsNull())
        batch.Write(DB_BEST_BLOCK, hashBlock);

    LogPrint("coindb", "Committing %u changed transactions (out of %u) to coin database...\n", (unsigned int)changed, (unsigned int)count);
    return db.WriteBatch(batch);
}

CBlockTreeDB::CBlockTreeDB(size_t nCacheSize, bool fMemory, bool fWipe) : CDBWrapper(GetDataDir() / "blocks" / "index", nCacheSize, fMemory, fWipe) {
}

bool CBlockTreeDB::ReadBlockFileInfo(int nFile, CBlockFileInfo &info) {
    return Read(std::make_pair(DB_BLOCK_FILES, nFile), info);
}

bool CBlockTreeDB::WriteReindexing(bool fReindexing) {
    if (fReindexing)
        return Write(DB_REINDEX_FLAG, '1');
    else
        return Erase(DB_REINDEX_FLAG);
}

bool CBlockTreeDB::ReadReindexing(bool &fReindexing) {
    fReindexing = Exists(DB_REINDEX_FLAG);
    return true;
}

bool CBlockTreeDB::ReadLastBlockFile(int &nFile) {
    return Read(DB_LAST_BLOCK, nFile);
}

CCoinsViewCursor *CCoinsViewDB::Cursor() const
{
    CCoinsViewDBCursor *i = new CCoinsViewDBCursor(const_cast<CDBWrapper*>(&db)->NewIterator(), GetBestBlock());
    /* It seems that there are no "const iterators" for LevelDB.  Since we
       only need read operations on it, use a const-cast to get around
       that restriction.  */
    i->pcursor->Seek(DB_COINS);
    // Cache key of first record
    i->pcursor->GetKey(i->keyTmp);
    return i;
}

bool CCoinsViewDBCursor::GetKey(uint256 &key) const
{
    // Return cached key
    if (keyTmp.first == DB_COINS) {
        key = keyTmp.second;
        return true;
    }
    return false;
}

bool CCoinsViewDBCursor::GetValue(CCoins &coins) const
{
    return pcursor->GetValue(coins);
}

unsigned int CCoinsViewDBCursor::GetValueSize() const
{
    return pcursor->GetValueSize();
}

bool CCoinsViewDBCursor::Valid() const
{
    return keyTmp.first == DB_COINS;
}

void CCoinsViewDBCursor::Next()
{
    pcursor->Next();
    if (!pcursor->Valid() || !pcursor->GetKey(keyTmp))
        keyTmp.first = 0; // Invalidate cached key after last record so that Valid() and GetKey() return false
}

bool CBlockTreeDB::WriteBatchSync(const std::vector<std::pair<int, const CBlockFileInfo*> >& fileInfo, int nLastFile, const std::vector<const CBlockIndex*>& blockinfo) {
    CDBBatch batch(*this);
    for (std::vector<std::pair<int, const CBlockFileInfo*> >::const_iterator it=fileInfo.begin(); it != fileInfo.end(); it++) {
        batch.Write(std::make_pair(DB_BLOCK_FILES, it->first), *it->second);
    }
    batch.Write(DB_LAST_BLOCK, nLastFile);
    for (std::vector<const CBlockIndex*>::const_iterator it=blockinfo.begin(); it != blockinfo.end(); it++) {
        batch.Write(std::make_pair(DB_BLOCK_INDEX, (*it)->GetBlockHash()), CDiskBlockIndex(*it));
    }
    return WriteBatch(batch, true);
}

bool CBlockTreeDB::ReadTxIndex(const uint256 &txid, CDiskTxPos &pos) {
    return Read(std::make_pair(DB_TXINDEX, txid), pos);
}

bool CBlockTreeDB::WriteTxIndex(const std::vector<std::pair<uint256, CDiskTxPos> >&vect) {
    CDBBatch batch(*this);
    for (std::vector<std::pair<uint256,CDiskTxPos> >::const_iterator it=vect.begin(); it!=vect.end(); it++)
        batch.Write(std::make_pair(DB_TXINDEX, it->first), it->second);
    return WriteBatch(batch);
}

bool CBlockTreeDB::WriteFlag(const std::string &name, bool fValue) {
    return Write(std::make_pair(DB_FLAG, name), fValue ? '1' : '0');
}

bool CBlockTreeDB::ReadFlag(const std::string &name, bool &fValue) {
    char ch;
    if (!Read(std::make_pair(DB_FLAG, name), ch))
        return false;
    fValue = ch == '1';
    return true;
}

bool CBlockTreeDB::LoadBlockIndexGuts(boost::function<CBlockIndex*(const uint256&)> insertBlockIndex)
{
    std::unique_ptr<CDBIterator> pcursor(NewIterator());

    pcursor->Seek(std::make_pair(DB_BLOCK_INDEX, uint256()));

    // Load mapBlockIndex
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        std::pair<char, uint256> key;
        if (pcursor->GetKey(key) && key.first == DB_BLOCK_INDEX) {
            CDiskBlockIndex diskindex;
            if (pcursor->GetValue(diskindex)) {
                // Construct block index object
                CBlockIndex* pindexNew      = insertBlockIndex(diskindex.GetBlockHash());
                pindexNew->pprev            = insertBlockIndex(diskindex.hashPrev);
                pindexNew->nHeight          = diskindex.nHeight;
                pindexNew->nFile            = diskindex.nFile;
                pindexNew->nDataPos         = diskindex.nDataPos; // SolarCoin: was nBlockPos
                pindexNew->nUndoPos         = diskindex.nUndoPos;
                pindexNew->nVersion         = diskindex.nVersion;
                pindexNew->hashMerkleRoot   = diskindex.hashMerkleRoot;
                pindexNew->nTime            = diskindex.nTime;
                pindexNew->nBits            = diskindex.nBits;
                pindexNew->nNonce           = diskindex.nNonce;
                pindexNew->nStatus          = diskindex.nStatus;
                pindexNew->nTx              = diskindex.nTx;
                // SolarCoin: PoST
                pindexNew->nMint            = diskindex.nMint;
                pindexNew->nMoneySupply     = diskindex.nMoneySupply;
                pindexNew->nFlags           = diskindex.nFlags;
                pindexNew->nStakeModifier   = diskindex.nStakeModifier;
                pindexNew->prevoutStake     = diskindex.prevoutStake;
                pindexNew->nStakeTime       = diskindex.nStakeTime;
                pindexNew->hashProofOfStake = diskindex.hashProofOfStake;

                // SolarCoin: Disable PoW Sanity check while loading block index from disk.
                // We use the sha256 hash for the block index for performance reasons, which is recorded for later use.
                // CheckProofOfWork() uses the scrypt hash which is discarded after a block is accepted.
                // While it is technically feasible to verify the PoW, doing so takes several minutes as it
                // requires recomputing every PoW hash during every SolarCoin startup.
                // We opt instead to simply trust the data that is on your local disk.
                //if (!CheckProofOfWork(pindexNew->GetBlockHash(), pindexNew->nBits, Params().GetConsensus()))
                //    return error("LoadBlockIndex(): CheckProofOfWork failed: %s", pindexNew->ToString());

                // SolarCoin: CBlockIndex::IsProofOfStake is not valid during header download. Use height instead.
                if (pindexNew->nHeight > Params().GetConsensus().LAST_POW_BLOCK) {
                    // DEBUG: Show proofs, stake modifier mint and moneysupply of first 20 PoST blocks
                    if (pindexNew->nHeight <= Params().GetConsensus().LAST_POW_BLOCK + 20)
                        LogPrintf("DEBUG: txdb: nHeight=%d hashProofOfStake=%s nStakeTime=%d nStakeModifier=%d nMint=%s nMoneySupply=%s\n", pindexNew->nHeight, pindexNew->hashProofOfStake.ToString(), pindexNew->nStakeTime, pindexNew->nStakeModifier, FormatMoney(pindexNew->nMint).c_str(), FormatMoneySupply(pindexNew->nMoneySupply).c_str());
                } else {
                    // DEBUG: Show Mint, MoneySupply, Stake Modifier and hashProofOfStake of 20 PoW blocks
                    if (pindexNew->nHeight >= 4000 && pindexNew->nHeight < 4020)
                        LogPrintf("DEBUG: txdb: nHeight=%d nMint=%s nMoneySupply=%s nStakeModifier=%d hashProofOfStake=%s\n", pindexNew->nHeight, FormatMoney(pindexNew->nMint).c_str(), FormatMoneySupply(pindexNew->nMoneySupply).c_str(), pindexNew->nStakeModifier, pindexNew->hashProofOfStake.ToString());
                }
                // SolarCoin: build setStakeSeen
                if (pindexNew->IsProofOfStake())
                    setStakeSeen.insert(make_pair(pindexNew->prevoutStake, pindexNew->nStakeTime));

                pcursor->Next();
            } else {
                return error("LoadBlockIndex() : failed to read value");
            }
        } else {
            break;
        }
    }

    return true;
}
