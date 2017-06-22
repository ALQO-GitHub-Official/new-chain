// Copyright (c) 2017 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef PIVX_ZEROCOIN_H
#define PIVX_ZEROCOIN_H
#include "libzerocoin/bignum.h"
#include "serialize.h"

class CZerocoinMint
{
private:
    int64_t denominationAsInt;
    int nHeight;
    CBigNum value;
    CBigNum randomness;
    CBigNum serialNumber;
    uint256 txid;
    bool isUsed;

public:
    CZerocoinMint()
    {
        SetNull();
    }

    CZerocoinMint(int denomination, CBigNum value, CBigNum randomness, CBigNum serialNumber, bool isUsed)
    {
        this->denominationAsInt = denomination;
        this->value = value;
        this->randomness = randomness;
        this->serialNumber = serialNumber;
        this->isUsed = isUsed;
    }

    void SetNull()
    {
        isUsed = false;
        randomness = 0;
        value = 0;
        denominationAsInt = -1;
        nHeight = -1;
        txid = 0;
    }

    uint256 GetHash() const;

    CBigNum GetValue() const { return value; }
    void SetValue(CBigNum value){ this->value = value; }
    int64_t GetDenominationAsInt() const { return denominationAsInt; }
    int64_t GetDenominationAsAmount() const;
    void SetDenomination(int denomination){ this->denominationAsInt = denomination; }
    int GetHeight() const { return nHeight; }
    void SetHeight(int nHeight){ this->nHeight = nHeight; }
    bool IsUsed() const { return this->isUsed; }
    void SetUsed(bool isUsed){ this->isUsed = isUsed; }
    CBigNum GetRandomness() const{ return randomness; }
    void SetRandomness(CBigNum rand){ this->randomness = rand; }
    CBigNum GetSerialNumber() const { return serialNumber; }
    void SetSerialNumber(CBigNum serial){ this->serialNumber = serial; }
    uint256 GetTxHash() const { return this->txid; }
    void SetTxHash(uint256 txid) { this->txid = txid; }

    inline bool operator <(const CZerocoinMint& a) const { return GetHeight() < a.GetHeight(); }

    CZerocoinMint(const CZerocoinMint& other) {
        denominationAsInt = other.GetDenominationAsInt();
        nHeight = other.GetHeight();
        value = other.GetValue();
        randomness = other.GetRandomness();
        serialNumber = other.GetSerialNumber();
        txid = other.GetTxHash();
        isUsed = other.IsUsed();
    }
    
    // Copy another CZerocoinMint
    inline CZerocoinMint& operator=(const CZerocoinMint& other) {
        denominationAsInt = other.GetDenominationAsInt();
        nHeight = other.GetHeight();
        value = other.GetValue();
        randomness = other.GetRandomness();
        serialNumber = other.GetSerialNumber();
        txid = other.GetTxHash();
        isUsed = other.IsUsed();
        return *this;
    }
    
    // why 6 below (SPOCK)
    inline bool checkUnused(int denom, int Height) const {
        if (IsUsed() == false && GetDenominationAsInt() == denominationAsInt && GetRandomness() != 0 && GetSerialNumber() != 0 && GetHeight() != -1 && GetHeight() != INT_MAX && GetHeight() >= 1 && (GetHeight() + 6 <= Height)) {
            return true;
        } else {
            return false;
        }
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(isUsed);
        READWRITE(randomness);
        READWRITE(serialNumber);
        READWRITE(value);
        READWRITE(denominationAsInt);
        READWRITE(nHeight);
        READWRITE(txid);
    };
};

class CZerocoinSpend
{
private:
    CBigNum coinSerial;
    uint256 hashTx;
    CBigNum pubCoin;
    int denomination;
    unsigned int nAccumulatorChecksum;

public:
    CZerocoinSpend()
    {
        SetNull();
    }

    CZerocoinSpend(CBigNum coinSerial, uint256 hashTx, CBigNum pubCoin, int denomination, unsigned int nAccumulatorChecksum)
    {
        this->coinSerial = coinSerial;
        this->hashTx = hashTx;
        this->pubCoin = pubCoin;
        this->denomination = denomination;
        this->nAccumulatorChecksum = nAccumulatorChecksum;
    }

    void SetNull()
    {
        coinSerial = 0;
        hashTx = 0;
        pubCoin = 0;
        denomination = -1;
    }

    CBigNum GetSerial() const { return coinSerial; }
    uint256 GetTxHash() const { return hashTx; }
    CBigNum GetPubCoin() const { return pubCoin; }
    int GetDenomination() const { return denomination; }
    unsigned int GetAccumulatorChecksum() const { return this->nAccumulatorChecksum; }
    uint256 GetHash() const;
 
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(coinSerial);
        READWRITE(hashTx);
        READWRITE(pubCoin);
        READWRITE(denomination);
        READWRITE(nAccumulatorChecksum);
    };
};

#endif //PIVX_ZEROCOIN_H
