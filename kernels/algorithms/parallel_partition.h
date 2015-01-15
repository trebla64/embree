// ======================================================================== //
// Copyright 2009-2015 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

#include "common/default.h"

#define DBG_PART(x) x

namespace embree
{
  template<typename T, size_t BLOCK_SIZE>
  class __aligned(64) parallel_partition
    {
    private:

      static const size_t SERIAL_THRESHOLD = 16;

      AlignedAtomicCounter64 blockID;
      
      size_t N;
      size_t blocks;
      T* array;

      enum {
        NEED_LEFT_BLOCK           = 1,
        NEED_RIGHT_BLOCK          = 2
      };

      /* do we need a left block? */
      __forceinline bool needLeftBlock(const size_t mode)  { return (mode & NEED_LEFT_BLOCK)  == NEED_LEFT_BLOCK; }

      /* do we need a right block? */
      __forceinline bool needRightBlock(const size_t mode) { return (mode & NEED_RIGHT_BLOCK) == NEED_RIGHT_BLOCK; }

      /* do we need both blocks? */
      __forceinline bool needBothBlocks(const size_t mode) { return needLeftBlock(mode) && needRightBlock(mode); }
      
      /* get left/right atomic block id */
      __forceinline size_t getBlockID(const size_t mode)
      {
        size_t v = 0;
        if (needLeftBlock(mode))  v |= 1;
        if (needRightBlock(mode)) v |= (size_t)1 << 32;
        size_t val = blockID.add(v);
        return val;
      }

      /* get left index from block id */
      __forceinline size_t getLeftBlockIndex(const size_t id) { return id & 0xffffffff; }

      /* get right index from block id */
      __forceinline size_t getRightBlockIndex(const size_t id) { return id >> 32; }

      /* get left array index from block index */
      __forceinline void getLeftArrayIndex(const size_t blockIndex, size_t &begin, size_t &end) 
      { 
        begin = blockIndex * BLOCK_SIZE; 
        end   = begin + BLOCK_SIZE; 
      }

      /* get right array index from block index */
      __forceinline void getRightArrayIndex(const size_t blockIndex, size_t &begin, size_t &end) 
      { 
        begin = (blocks-1-blockIndex) * BLOCK_SIZE; 
        end   = begin + BLOCK_SIZE; 
      }

      /* is block id valid? */
      __forceinline bool validBlockID(const size_t id)
      {
        const size_t numLeftBlocks  = getLeftBlockIndex(id) + 1;
        const size_t numRightBlocks = getRightBlockIndex(id) + 1;
        return numLeftBlocks+numRightBlocks <= blocks;
      }

      /* serial partitioning */
      size_t serialPartitioning(const size_t begin, const size_t end, const T pivot)
      {
        T* l = array + begin;
        T* r = array + end - 1;

        while(1)
          {
            while (likely(l <= r && *l < pivot)) 
              ++l;
            while (likely(l <= r && *r >= pivot)) 
              --r;
            if (r<l) break;

            std::swap(*l,*r);
            l++; r--;
          }
      
        return l - array;        
      }

      /* neutralize left and right block */
      size_t neutralizeBlocks(size_t &left_begin,
                               const size_t &left_end,
                               size_t &right_begin,
                               const size_t &right_end,
                               const T pivot)
      {
        while(left_begin < left_end && right_begin < right_end)
          {
            while(array[left_begin] < pivot)
              {
                left_begin++;
                if (left_begin >= left_end) break;
              }

            while(array[right_begin] >= pivot)
              {
                right_begin++;
                if (right_begin >= right_end) break;
              }

            if (unlikely(left_begin == left_end || right_begin == right_end)) break;

            std::swap(array[left_begin++],array[right_begin++]);
          }

        size_t mode = 0;
        if (unlikely(left_begin == left_end))
          mode |= NEED_LEFT_BLOCK;

        if (unlikely(right_begin == right_end))
          mode |= NEED_RIGHT_BLOCK;

        assert(mode != 0);
        return mode;
      }

      /* check left part of array */
      void checkLeft(const size_t begin, const size_t end, const T pivot)
      {
        for (size_t i=begin;i<end;i++)
          if (array[i] >= pivot)
            {
              DBG_PRINT(i);
              DBG_PRINT(array[i]);
              DBG_PRINT(pivot);
              FATAL("partition error on left side");
            }
      }

      /* check right part of array */
      void checkRight(const size_t begin, const size_t end, const T pivot)
      {
        for (size_t i=begin;i<end;i++)
          if (array[i] < pivot)
            {
              DBG_PRINT(i);
              DBG_PRINT(array[i]);
              DBG_PRINT(pivot);
              FATAL("partition error on right side");
            }
      }

    public:

    parallel_partition(T *array, size_t N) : array(array), N(N)
        {
          blockID.reset();
          blocks = N/BLOCK_SIZE;
        }

      size_t parition(const T pivot)
      {
        if (N <= SERIAL_THRESHOLD)
          {
            size_t mid = serialPartitioning(0,N,pivot);
            DBG_PRINT( mid );
            DBG_PART(
                     checkLeft(0,mid,pivot);
                     checkRight(mid,N,pivot);
                     );
            return mid;
          }

        size_t mode = NEED_LEFT_BLOCK | NEED_RIGHT_BLOCK;
        
        size_t left_begin  = (size_t)-1;
        size_t left_end    = (size_t)-1;
        size_t right_begin = (size_t)-1;
        size_t right_end   = (size_t)-1;

        size_t maxLeftBlock  = (size_t)-1;
        size_t maxRightBlock = (size_t)-1;

        while(1)
          {
            size_t id = getBlockID(mode);
            if (!validBlockID(id)) break;

            /* need a left block? */
            if (needLeftBlock(mode))
              {
                const size_t blockIndex = getLeftBlockIndex(id);
                getLeftArrayIndex(blockIndex,left_begin,left_end);                
                maxLeftBlock = blockIndex;
              }

            /* need a right block? */
            if (needRightBlock(mode))
              {
                const size_t blockIndex = getRightBlockIndex(id);
                getRightArrayIndex(blockIndex,right_begin,right_end);
                maxRightBlock = blockIndex;
              }

            DBG_PART(
                     DBG_PRINT(left_begin);
                     DBG_PRINT(left_end);
                     DBG_PRINT(right_begin);
                     DBG_PRINT(right_end);
                     );
            
            assert(left_begin  < left_end);
            assert(right_begin < right_end);

            assert(left_end <= right_begin);

            mode = neutralizeBlocks(left_begin,left_end,right_begin,right_end,pivot);
          }

        assert(left_end <= right_begin);

        DBG_PART(
                 DBG_PRINT("CLEANUP");
                 DBG_PRINT(left_begin);
                 DBG_PRINT(left_end);
                 DBG_PRINT(right_begin);
                 DBG_PRINT(right_end);
                 );

        const size_t local_mid = serialPartitioning(left_begin,right_end,pivot);
        const size_t mid = left_begin + mid;
        DBG_PART(
                 DBG_PRINT( local_mid );
                 DBG_PRINT( mid );
                 checkLeft(0,mid,pivot);
                 checkRight(mid,N,pivot);
                 );

        return mid;
      }

    };

};
