// ======================================================================== //
// Copyright 2009-2019 Intel Corporation                                    //
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

#if defined(EMBREE_DPCPP_SUPPORT)
#include "common.h"
#include "AABB.h"
#include "AABB3f.h"

/* ====== BVH16 config ====== */

#define BINS 16
#define BVH_LEAF_MASK        8
#define BVH_INVALID_NODE     3
#define BVH_NODE_N          16
#define BVH_NODE_N_LOG       4

/* ====== QUANTIZATION config ====== */

#define QUANT_BITS            8
#define QUANT_MIN             0
#define QUANT_MAX             255
#define QUANT_MAX_MANT        (255.0f/256.0f)

namespace embree
{
  namespace gpu
  {
    
    struct BVHBase
    {
      AABB3f bounds; 
      unsigned long rootNodeOffset; 
      unsigned int nodeDataStart;
      unsigned int nodeDataCur;
      unsigned int leafDataStart;
      unsigned int leafDataCur;
      unsigned int proceduralDataStart;
      unsigned int proceduralDataCur;
      /* unsigned int backPointerDataStart; */
      /* unsigned int backPointerDataEnd; */
    };

    /* ======================================================================== */
    /* ============================== BVH NODES =============================== */
    /* ======================================================================== */

    struct BVHNodeN
    {              
      uint offset[BVH_NODE_N];  
      uint parent[BVH_NODE_N]; 
      float lower_x[BVH_NODE_N]; 
      float upper_x[BVH_NODE_N]; 
      float lower_y[BVH_NODE_N]; 
      float upper_y[BVH_NODE_N]; 
      float lower_z[BVH_NODE_N]; 
      float upper_z[BVH_NODE_N]; 

      inline void initBVHNodeN(uint slotID)
      {
	const float pos_inf =  INFINITY;
	const float neg_inf = -INFINITY;	
	offset[slotID]  =  (uint)(-1);  
	parent[slotID]  =  (uint)(-1); 
	lower_x[slotID] =  pos_inf; 
	upper_x[slotID] =  neg_inf;
	lower_y[slotID] =  pos_inf; 
	upper_y[slotID] =  neg_inf;
	lower_z[slotID] =  pos_inf; 
	upper_z[slotID] =  neg_inf;  
      }


      inline void setBVHNodeN(const struct AABB &aabb, uint slot)
      {
	lower_x[slot] = aabb.lower.x();
	lower_y[slot] = aabb.lower.y();
	lower_z[slot] = aabb.lower.z();
	upper_x[slot] = aabb.upper.x();
	upper_y[slot] = aabb.upper.y();
	upper_z[slot] = aabb.upper.z();
      }

      inline void setBVHNodeN_offset(const struct AABB &aabb, const uint _offset, const uint _parent, uint slot)
      {
	offset[slot] = _offset;
	parent[slot] = _parent;  
	lower_x[slot] = aabb.lower.x();
	lower_y[slot] = aabb.lower.y();
	lower_z[slot] = aabb.lower.z();
	upper_x[slot] = aabb.upper.x();
	upper_y[slot] = aabb.upper.y();
	upper_z[slot] = aabb.upper.z();
      }


    };

    inline const cl::sycl::stream &operator<<(const cl::sycl::stream &out, const BVHNodeN& node) {
      for (uint i=0;i<BVH_NODE_N;i++)
	{
	  out << " i " << i << " offset " << node.offset[i] << " lower_x " << node.lower_x[i] << " upper_x " << node.upper_x[i] << " lower_y " << node.lower_y[i] << " upper_y " << node.upper_y[i] << " lower_z " << node.lower_z[i] << " upper_z " << node.upper_z[i] << cl::sycl::endl;
	}      
      return out; 
    }

    struct QBVHNodeN
    {
      uint   offset[BVH_NODE_N];
      float4 org;
      float4 scale;

      /* special layout requires less block loads */
      struct {  
	cl::sycl::uchar lower_z;
	cl::sycl::uchar upper_z;    
      } bounds_z[BVH_NODE_N];

      struct {
	cl::sycl::uchar lower_x;
	cl::sycl::uchar upper_x;
	cl::sycl::uchar lower_y;
	cl::sycl::uchar upper_y;
      } bounds_xy[BVH_NODE_N];
      
      /* uchar lower_x[BVH_NODE_N]; */
      /* uchar upper_x[BVH_NODE_N]; */
      /* uchar lower_y[BVH_NODE_N]; */
      /* uchar upper_y[BVH_NODE_N]; */
      /* uchar lower_z[BVH_NODE_N]; */
      /* uchar upper_z[BVH_NODE_N]; */
      
      inline static void init(cl::sycl::intel::sub_group &sg,
			      QBVHNodeN &node,			      
			      AABB *childrenAABB,
			      uint numChildren)
      {
	AABB child;
	const uint subgroupLocalID = sg.get_local_id()[0];
	if (subgroupLocalID < numChildren)
	  child = childrenAABB[subgroupLocalID];
	else
	  child.init();
	AABB aabb = child.sub_group_reduce(sg);
	const float4 minF = aabb.lower;
	const float4 maxF = aabb.upper;      
	const float4 diff = aabb.size()*(1.0f+2.0f*FLT_MIN);
	float4 decode_scale = diff / (float4)(QUANT_MAX);
	
	decode_scale = float4(decode_scale.x() == 0.0f ? 2.0f*FLT_MIN : decode_scale.x(),
			      decode_scale.y() == 0.0f ? 2.0f*FLT_MIN : decode_scale.y(),
			      decode_scale.z() == 0.0f ? 2.0f*FLT_MIN : decode_scale.z(),
			      0.0f);
	  
	float4 encode_scale = (float4)(QUANT_MAX) / diff;
	encode_scale = float4(diff.x() > 0.0f ? diff.x() : 0.0f,
			      diff.y() > 0.0f ? diff.y() : 0.0f,
			      diff.z() > 0.0f ? diff.z() : 0.0f,
			      0.0f);
	
	if (subgroupLocalID < BVH_NODE_N)
	{

	  int m_valid = subgroupLocalID < numChildren;
	  
	  const float4 lower = child.lower;
	  const float4 upper = child.upper;
	  
	  float4 lowerf = floor((lower - minF)*encode_scale);
	  float4 upperf =  ceil((upper - minF)*encode_scale);

	  int4 ilower = max(lowerf.convert<int,cl::sycl::rounding_mode::rtn>(),int4(QUANT_MIN));
	  int4 iupper = min(upperf.convert<int,cl::sycl::rounding_mode::rtp>(),int4(QUANT_MAX));

	  /* lower/upper correction */
	  int4 m_lower_correction = (cl::sycl::fma(ilower.convert<float,cl::sycl::rounding_mode::rtn>(),decode_scale,minF)) > lower;
	  int4 m_upper_correction = (cl::sycl::fma(iupper.convert<float,cl::sycl::rounding_mode::rtp>(),decode_scale,minF)) < upper;

	  //ilower = max((m_lower_correction ? ilower-1 : ilower),QUANT_MIN);
	  //iupper = min((m_upper_correction ? iupper+1 : iupper),QUANT_MAX);
	  
	  ilower = max(cl::sycl::select(ilower,ilower-1,m_lower_correction),QUANT_MIN);
	  iupper = min(cl::sycl::select(iupper,iupper+1,m_upper_correction),QUANT_MAX);	  

	  /* disable invalid lanes */	  
	  //ilower = m_valid ? ilower : (int4)QUANT_MAX;
	  //iupper = m_valid ? iupper : (int4)QUANT_MIN;

	  ilower = cl::sycl::select((int4)QUANT_MAX,ilower,m_valid);
	  iupper = cl::sycl::select((int4)QUANT_MIN,iupper,m_valid);
	  uchar4 clower = ilower.convert<cl::sycl::uchar,cl::sycl::rounding_mode::rtz>();
	  uchar4 cupper = iupper.convert<cl::sycl::uchar,cl::sycl::rounding_mode::rtz>();
	  
	  node.offset[subgroupLocalID] = -1;
	  node.bounds_xy[subgroupLocalID].lower_x = clower.x();
	  node.bounds_xy[subgroupLocalID].lower_y = clower.y();
	  node.bounds_z [subgroupLocalID].lower_z = clower.z();
	  node.bounds_xy[subgroupLocalID].upper_x = cupper.x();
	  node.bounds_xy[subgroupLocalID].upper_y = cupper.y();
	  node.bounds_z [subgroupLocalID].upper_z = cupper.z();	  
	  node.org   = minF;
	  node.scale = decode_scale;	  
	}
      }
    };

    
    inline const cl::sycl::stream &operator<<(const cl::sycl::stream &out, const QBVHNodeN& node) {
      for (uint i=0;i<BVH_NODE_N;i++)
	{
	  out << " i " << i
	      << " offset  " << node.offset[i]
	      << " lower_x " << (int)node.bounds_xy[i].lower_x
	      << " upper_x " << (int)node.bounds_xy[i].upper_x
	      << " lower_y " << (int)node.bounds_xy[i].lower_y
	      << " upper_y " << (int)node.bounds_xy[i].upper_y
	      << " lower_z " << (int)node.bounds_z[i] .lower_z
	      << " upper_z " << (int)node.bounds_z[i] .upper_z;
	}      
      return out; 
    }

  };
};

#endif
