// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_TXDB_H
#define BITCOIN_TXDB_H

#include <coins.h>
#include <dbwrapper.h>
#include <chain.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

class BitAssetObj;
class BitAsset;
class CBlockIndex;
class CCoinsViewDBCursor;
class SidechainObj;
class SidechainDeposit;
class SidechainTransfer;
class SidechainWithdrawal;
class SidechainWithdrawalBundle;
class uint256;

//! No need to periodic flush if at least this much space still available.
static constexpr int MAX_BLOCK_COINSDB_USAGE = 10;
//! -dbcache default (MiB)
static const int64_t nDefaultDbCache = 450;
//! -dbbatchsize default (bytes)
static const int64_t nDefaultDbBatchSize = 16 << 20;
//! max. -dbcache (MiB)
static const int64_t nMaxDbCache = sizeof(void*) > 4 ? 16384 : 1024;
//! min. -dbcache (MiB)
static const int64_t nMinDbCache = 4;
//! Max memory allocated to block tree DB specific cache, if no -txindex (MiB)
static const int64_t nMaxBlockDBCache = 2;
//! Max memory allocated to block tree DB specific cache, if -txindex (MiB)
// Unlike for the UTXO database, for the txindex scenario the leveldb cache make
// a meaningful difference: https://github.com/bitcoin/bitcoin/pull/8273#issuecomment-229601991
static const int64_t nMaxBlockDBAndTxIndexCache = 1024;
//! Max memory allocated to coin DB specific cache (MiB)
static const int64_t nMaxCoinsDBCache = 8;

struct CDiskTxPos : public CDiskBlockPos
{
    unsigned int nTxOffset; // after header

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(*static_cast<CDiskBlockPos*>(this));
        READWRITE(VARINT(nTxOffset));
    }

    CDiskTxPos(const CDiskBlockPos &blockIn, unsigned int nTxOffsetIn) : CDiskBlockPos(blockIn.nFile, blockIn.nPos), nTxOffset(nTxOffsetIn) {
    }

    CDiskTxPos() {
        SetNull();
    }

    void SetNull() {
        CDiskBlockPos::SetNull();
        nTxOffset = 0;
    }
};

/** CCoinsView backed by the coin database (chainstate/) */
class CCoinsViewDB final : public CCoinsView
{
protected:
    CDBWrapper db;
public:
    explicit CCoinsViewDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

    bool GetCoin(const COutPoint &outpoint, Coin &coin) const override;
    bool HaveCoin(const COutPoint &outpoint) const override;
    uint256 GetBestBlock() const override;
    std::vector<uint256> GetHeadBlocks() const override;
    bool BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock) override;
    CCoinsViewCursor *Cursor() const override;

    //! Attempt to update from an older database format. Returns whether an error occurred.
    bool Upgrade();
    size_t EstimateSize() const override;
};

/** Specialization of CCoinsViewCursor to iterate over a CCoinsViewDB */
class CCoinsViewDBCursor: public CCoinsViewCursor
{
public:
    ~CCoinsViewDBCursor() {}

    bool GetKey(COutPoint &key) const override;
    bool GetValue(Coin &coin) const override;
    unsigned int GetValueSize() const override;

    bool Valid() const override;
    void Next() override;

private:
    CCoinsViewDBCursor(CDBIterator* pcursorIn, const uint256 &hashBlockIn):
        CCoinsViewCursor(hashBlockIn), pcursor(pcursorIn) {}
    std::unique_ptr<CDBIterator> pcursor;
    std::pair<char, COutPoint> keyTmp;

    friend class CCoinsViewDB;
};

/** Access to the block database (blocks/index/) */
class CBlockTreeDB : public CDBWrapper
{
public:
    explicit CBlockTreeDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

    CBlockTreeDB(const CBlockTreeDB&) = delete;
    CBlockTreeDB& operator=(const CBlockTreeDB&) = delete;

    bool WriteBatchSync(const std::vector<std::pair<int, const CBlockFileInfo*> >& fileInfo, int nLastFile, const std::vector<const CBlockIndex*>& blockinfo);
    bool ReadBlockFileInfo(int nFile, CBlockFileInfo &info);
    bool ReadLastBlockFile(int &nFile);
    bool WriteReindexing(bool fReindexing);
    bool ReadReindexing(bool &fReindexing);
    bool ReadTxIndex(const uint256 &txid, CDiskTxPos &pos);
    bool WriteTxIndex(const std::vector<std::pair<uint256, CDiskTxPos> > &vect);
    bool WriteFlag(const std::string &name, bool fValue);
    bool ReadFlag(const std::string &name, bool &fValue);
    bool LoadBlockIndexGuts(const Consensus::Params& consensusParams, std::function<CBlockIndex*(const uint256&, const uint256&)> insertBlockIndex);
};

/** Access to the sidechain database (blocks/sidechain/) */
class CSidechainTreeDB : public CDBWrapper
{
public:
    CSidechainTreeDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);
    bool WriteSidechainIndex(const std::vector<std::pair<uint256, const SidechainObj *> > &list);
    bool WriteWithdrawalUpdate(const std::vector<SidechainWithdrawal>& vWithdrawal);
    bool WriteWithdrawalBundleUpdate(const SidechainWithdrawalBundle& withdrawalBundle);
    bool WriteLastWithdrawalBundleHash(const uint256& hash);

    bool GetWithdrawal(const uint256 & /* Withdrawal ID */, SidechainWithdrawal &withdrawal);
    bool GetWithdrawalBundle(const uint256 & /* Withdrawal Bundle ID */, SidechainWithdrawalBundle &withdrawalBundle);
    bool GetDeposit(const uint256 & /* Deposit ID */, SidechainDeposit &deposit);
    bool HaveDeposits();
    bool HaveDepositNonAmount(const uint256& hashNonAmount);
    bool GetLastDeposit(SidechainDeposit& deposit);
    bool GetLastWithdrawalBundleHash(uint256& hash);

    bool HaveWithdrawalBundle(const uint256& hashWithdrawalBundle) const;

    std::vector<SidechainWithdrawal> GetWithdrawals(const uint8_t & /* nSidechain */);
    std::vector<SidechainWithdrawalBundle> GetWithdrawalBundles(const uint8_t & /* nSidechain */);
    std::vector<SidechainDeposit> GetDeposits(const uint8_t & /* nSidechain */);
};

/** Access to the BitAsset database (blocks/BitAssets/) */
class BitAssetDB : public CDBWrapper
{
public:
    BitAssetDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);
    bool WriteBitAssets(const std::vector<BitAsset>& vAsset);

    std::vector<BitAsset> GetAssets();

    bool GetLastAssetID(uint32_t& nID);
    bool WriteLastAssetID(const uint32_t nID);
    bool GetAsset(const uint32_t nID, BitAsset& asset);

    bool RemoveAsset(const uint32_t nID);
};


#endif // BITCOIN_TXDB_H
