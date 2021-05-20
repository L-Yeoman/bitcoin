// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow.h>

#include <arith_uint256.h>
#include <chain.h>
#include <primitives/block.h>
#include <uint256.h>
//设定工作量难度
unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);
    //极限难度值
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    // Only change once per difficulty adjustment interval
    //距离上一次难度调整是否过了2016个区块，如果没有就沿用当前区块链顶点区块的难度值
    if ((pindexLast->nHeight+1) % params.DifficultyAdjustmentInterval() != 0)
    {
        if (params.fPowAllowMinDifficultyBlocks)
        {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 2* 10 minutes
            // then allow mining of a min-difficulty block.
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing*2)
                return nProofOfWorkLimit;
            else
            {
                // Return the last non-special-min-difficulty-rules-block
                const CBlockIndex* pindex = pindexLast;
                while (pindex->pprev && pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 && pindex->nBits == nProofOfWorkLimit)
                    pindex = pindex->pprev;
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

    // Go back by what we want to be 14 days worth of blocks
    //超过2016个区块，需要重新调整一次难度
    int nHeightFirst = pindexLast->nHeight - (params.DifficultyAdjustmentInterval()-1);
    assert(nHeightFirst >= 0);
    const CBlockIndex* pindexFirst = pindexLast->GetAncestor(nHeightFirst);
    assert(pindexFirst);

    return CalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), params);
}
//参考文章：https://zhuanlan.zhihu.com/p/128097006
/**
 * 调整挖矿难度的方法
 * 比特币挖矿成功公式：H(block header) <= target
 * target以指数形式存在，用十六进制表示，共8位，前2位为指数，后6位为系数
 * target通过bits得来，如：0x171320bc，指数为0x17，系数为：0x1320bc
 * 计算公式：难度值（target） = 系数*2^(8*(指数-3))
 * 难度值（target） = 0x1320bc * 2^(8 * (0x17 - 3))
*/
unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Limit adjustment step
    //计算最近的2016个区块实际花费了多长时间
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    //将实际时间限制在3.5天-8周内，低于3.5天的按3.5天算，高于8周的按8周算
    if (nActualTimespan < params.nPowTargetTimespan/4)
        nActualTimespan = params.nPowTargetTimespan/4;
    if (nActualTimespan > params.nPowTargetTimespan*4)
        nActualTimespan = params.nPowTargetTimespan*4;

    // Retarget
    //设定目标难度值
    /**
     * 设新的难度值为T，旧的难度之为o，最近2016个区块实际耗费时间为Dur
     * 系统期望生成2016个区块的时间是20160分钟，也就是10分钟产生一个
     * T = o*Dur/20160
     * 变换得  T/o = Dur/20160
     * 假如Dur小于20160，也就是最近2016个区块得实际时间小于期望值（区块出的太快）
     * 则目标难度target变小，从而求解难度变大（target值越小，挖矿难度越大，因为哈希值可落的范围会越小，反之值越大，挖矿难度就越小）
     * 如果最近2016个区块的实际时间大于20160（区块出的太慢），则target变大，难度变小
    */
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);//旧的目标难度
    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}
/**
 * 计算工作量，判断是否挖矿成功
 * 先拿到当前区块的hash，比较是否大于目标难度值
 * 如果小于目标难度值则完成工作，表示挖矿成功
 * 否则需要把区块的nonce值+1，然后再次计算区块头hash
 * 如此循环直到找到满足条件的hash
*/
bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}
