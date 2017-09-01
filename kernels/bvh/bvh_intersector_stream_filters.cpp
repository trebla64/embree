// ======================================================================== //
// Copyright 2009-2017 Intel Corporation                                    //
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

#include "bvh_intersector_stream_filters.h"
#include "bvh_intersector_stream.h"

namespace embree
{
  namespace isa
  {
    static const size_t MAX_RAYS_PER_OCTANT = 8*sizeof(size_t);

    static_assert(MAX_RAYS_PER_OCTANT <= MAX_INTERNAL_STREAM_SIZE, "maximal internal stream size exceeded");

    __forceinline void RayStream::filterAOS(Scene* scene, RTCRay* _rayN, size_t N, size_t stride, IntersectContext* context, bool intersect)
    {
#if 1
      RayStreamAOS rayN(_rayN);
      for (size_t i = 0; i < N; i += VSIZEX)
      {
        const vintx vi = vintx(int(i)) + vintx(step);
        vboolx valid = vi < vintx(int(N));
        const vintx offset = vi * int(stride);

        RayK<VSIZEX> ray = rayN.getRayByOffset<VSIZEX>(valid, offset);
        valid &= ray.tnear <= ray.tfar;

        if (intersect)
          scene->intersect(valid, ray, context);
        else
          scene->occluded(valid, ray, context);

        rayN.setHitByOffset<VSIZEX>(valid, offset, ray, intersect);
      }

#else

      Ray* __restrict__ rayN = (Ray*)_rayN;
      __aligned(64) Ray* octants[8][MAX_RAYS_PER_OCTANT];
      unsigned int rays_in_octant[8];

      for (size_t i=0;i<8;i++) rays_in_octant[i] = 0;
      size_t inputRayID = 0;

      while(1)
      {
        int cur_octant = -1;
        /* sort rays into octants */
        for (;inputRayID<N;)
        {
          Ray &ray = *(Ray*)((char*)rayN + inputRayID * stride);
          /* filter out invalid rays */
          if (unlikely(ray.tnear > ray.tfar)) { inputRayID++; continue; }
          if (unlikely(!intersect && ray.geomID == 0)) { inputRayID++; continue; } // ignore already occluded rays

#if defined(EMBREE_IGNORE_INVALID_RAYS)
          if (unlikely(!ray.valid())) {  inputRayID++; continue; }
#endif

          const unsigned int octantID = movemask(vfloat4(ray.dir) < 0.0f) & 0x7;

          assert(octantID < 8);
          octants[octantID][rays_in_octant[octantID]++] = &ray;
          inputRayID++;
          if (unlikely(rays_in_octant[octantID] == MAX_RAYS_PER_OCTANT))
          {
            cur_octant = octantID;
            break;
          }
        }
        /* need to flush rays in octant ? */
        if (unlikely(cur_octant == -1))
          for (int i=0;i<8;i++)
            if (rays_in_octant[i])
            {
              cur_octant = i;
              break;
            }

        /* all rays traced ? */
        if (unlikely(cur_octant == -1))
          break;

        
        Ray** rays = &octants[cur_octant][0];
        const size_t numOctantRays = rays_in_octant[cur_octant];

        /* special codepath for very small number of rays per octant */
        if (numOctantRays == 1)
        {
          if (intersect) scene->intersect((RTCRay&)*rays[0],context);
          else           scene->occluded ((RTCRay&)*rays[0],context);
        }        
        /* codepath for large number of rays per octant */
        else
        {
          /* incoherent ray stream code path */
          if (intersect) scene->intersectN((RTCRay**)rays,numOctantRays,context);
          else           scene->occludedN ((RTCRay**)rays,numOctantRays,context);
        }
        rays_in_octant[cur_octant] = 0;
      }

#endif
    }

    __forceinline void RayStream::filterAOP(Scene* scene, RTCRay** _rayN, size_t N, IntersectContext* context, bool intersect)
    {
      Ray** __restrict__ rayN = (Ray**)_rayN;

#if 1

      /* fallback to packets */
      for (size_t i = 0; i < N; i += VSIZEX)
      {
        const size_t n = min(N - i, size_t(VSIZEX));
        vboolx valid = vintx(step) < vintx(int(n));
        RayK<VSIZEX> ray;

        /* gather rays */
        for (size_t k = 0; k < n; k++)
        {
          Ray* __restrict__ ray_k = rayN[i + k];

          ray.org.x[k]  = ray_k->org.x;
          ray.org.y[k]  = ray_k->org.y;
          ray.org.z[k]  = ray_k->org.z;
          ray.dir.x[k]  = ray_k->dir.x;
          ray.dir.y[k]  = ray_k->dir.y;
          ray.dir.z[k]  = ray_k->dir.z;
          ray.tnear[k]  = ray_k->tnear;
          ray.tfar[k]   = ray_k->tfar;
          ray.time[k]   = ray_k->time;
          ray.mask[k]   = ray_k->mask;
          ray.instID[k] = ray_k->instID;
        }

        ray.geomID = RTC_INVALID_GEOMETRY_ID;

        /* filter out invalid rays */
        valid &= ray.tnear <= ray.tfar;

        /* intersect packet */
        if (intersect)
          scene->intersect(valid, ray, context);
        else
          scene->occluded (valid, ray, context);

        /* scatter hits */
        for (size_t k = 0; k < n; k++)
        {
          Ray* __restrict__ ray_k = rayN[i + k];;

          ray_k->geomID = ray.geomID[k];
          if (intersect && ray.geomID[k] != RTC_INVALID_GEOMETRY_ID)
          {
            ray_k->tfar   = ray.tfar[k];
            ray_k->Ng.x   = ray.Ng.x[k];
            ray_k->Ng.y   = ray.Ng.y[k];
            ray_k->Ng.z   = ray.Ng.z[k];
            ray_k->u      = ray.u[k];
            ray_k->v      = ray.v[k];
            ray_k->primID = ray.primID[k];
            ray_k->instID = ray.instID[k];
          }
        }
      }

#else

      __aligned(64) Ray* octants[8][MAX_RAYS_PER_OCTANT];
      unsigned int rays_in_octant[8];

      for (size_t i=0;i<8;i++) rays_in_octant[i] = 0;
      size_t inputRayID = 0;

      while(1)
      {
        int cur_octant = -1;
        /* sort rays into octants */
        for (;inputRayID<N;)
        {
          Ray &ray = *rayN[inputRayID];
          /* filter out invalid rays */
          if (unlikely(ray.tnear > ray.tfar)) { inputRayID++; continue; }
          if (unlikely(!intersect && ray.geomID == 0)) { inputRayID++; continue; } // ignore already occluded rays

#if defined(EMBREE_IGNORE_INVALID_RAYS)
          if (unlikely(!ray.valid())) {  inputRayID++; continue; }
#endif

          const unsigned int octantID = movemask(vfloat4(ray.dir) < 0.0f) & 0x7;

          assert(octantID < 8);
          octants[octantID][rays_in_octant[octantID]++] = &ray;
          inputRayID++;
          if (unlikely(rays_in_octant[octantID] == MAX_RAYS_PER_OCTANT))
          {
            cur_octant = octantID;
            break;
          }
        }
        /* need to flush rays in octant ? */
        if (unlikely(cur_octant == -1))
          for (int i=0;i<8;i++)
            if (rays_in_octant[i])
            {
              cur_octant = i;
              break;
            }

        /* all rays traced ? */
        if (unlikely(cur_octant == -1))
          break;

        
        Ray** rays = &octants[cur_octant][0];
        const size_t numOctantRays = rays_in_octant[cur_octant];

        /* special codepath for very small number of rays per octant */
        if (numOctantRays == 1)
        {
          if (intersect) scene->intersect((RTCRay&)*rays[0],context);
          else           scene->occluded ((RTCRay&)*rays[0],context);
        }        
        /* codepath for large number of rays per octant */
        else
        {
          /* incoherent ray stream code path */
          if (intersect) scene->intersectN((RTCRay**)rays,numOctantRays,context);
          else           scene->occludedN ((RTCRay**)rays,numOctantRays,context);
        }
        rays_in_octant[cur_octant] = 0;
      }

#endif
    }

    void RayStream::filterSOACoherent(Scene* scene, char* rayData, size_t streams, size_t stream_offset, IntersectContext* context, bool intersect)
    {
      /* all valid accels need to have a intersectN/occludedN */
      bool chunkFallback = scene->isRobust() || !scene->accels.validIsecN();

      /* check for common octant */
      if (unlikely(!chunkFallback))
      {
        vfloatx min_x(pos_inf), max_x(neg_inf);
        vfloatx min_y(pos_inf), max_y(neg_inf);
        vfloatx min_z(pos_inf), max_z(neg_inf);
        vboolx all_active(true);
        for (size_t s=0; s<streams; s++)
        {
          const size_t offset = s*stream_offset;
          RayK<VSIZEX> &ray = *(RayK<VSIZEX>*)(rayData + offset);
          min_x = min(min_x,ray.dir.x);
          min_y = min(min_y,ray.dir.y);
          min_z = min(min_z,ray.dir.z);
          max_x = max(max_x,ray.dir.x);
          max_y = max(max_y,ray.dir.y);
          max_z = max(max_z,ray.dir.z);
          all_active &= ray.tnear <= ray.tfar;
#if defined(EMBREE_IGNORE_INVALID_RAYS)
          all_active &= ray.valid();
#endif
        }
        const bool commonOctant =
          (all(max_x < vfloatx(zero)) || all(min_x >= vfloatx(zero))) &&
          (all(max_y < vfloatx(zero)) || all(min_y >= vfloatx(zero))) &&
          (all(max_z < vfloatx(zero)) || all(min_z >= vfloatx(zero)));

        /* fallback to chunk in case of non-common octants */
        chunkFallback |= !commonOctant || !all(all_active);
      }

      /* fallback to chunk if necessary */
      if (unlikely(chunkFallback))
      {
        for (size_t s=0; s<streams; s++)
        {
          const size_t offset = s*stream_offset;
          RayK<VSIZEX> &ray = *(RayK<VSIZEX>*)(rayData + offset);
          vboolx valid = ray.tnear <= ray.tfar;
          if (intersect) scene->intersect(valid,ray,context);
          else           scene->occluded (valid,ray,context);
        }
        return;
      }

      static const size_t MAX_COHERENT_RAY_PACKETS = MAX_RAYS_PER_OCTANT / VSIZEX;
      __aligned(64) RayK<VSIZEX> *rays_ptr[MAX_RAYS_PER_OCTANT / VSIZEX];

      /* set input layout to SOA */
      context->setInputSOA(VSIZEX);
      size_t numStreams = 0;

      for (size_t s=0; s<streams; s++)
      {
        const size_t offset = s*stream_offset;
        RayK<VSIZEX> &ray = *(RayK<VSIZEX>*)(rayData + offset);
        rays_ptr[numStreams++] = &ray;
        /* trace as stream */
        if (unlikely(numStreams == MAX_COHERENT_RAY_PACKETS))
        {
          const size_t size = numStreams*VSIZEX;
          if (intersect)
            scene->intersectN((RTCRay**)rays_ptr,size,context);
          else
            scene->occludedN((RTCRay**)rays_ptr,size,context);
          numStreams = 0;
        }
      }
      /* flush remaining streams */
      if (unlikely(numStreams))
      {
        const size_t size = numStreams*VSIZEX;
        if (intersect)
          scene->intersectN((RTCRay**)rays_ptr,size,context);
        else
          scene->occludedN((RTCRay**)rays_ptr,size,context);
      }
    }

    __forceinline void RayStream::filterSOA(Scene* scene, char* rayData, size_t N, size_t streams, size_t stream_offset, IntersectContext* context, bool intersect)
    {
      const size_t rayDataAlignment = (size_t)rayData       % (VSIZEX*sizeof(float));
      const size_t offsetAlignment  = (size_t)stream_offset % (VSIZEX*sizeof(float));
#if 1

      /* fast path for packets with the correct width and data alignment */
      if (likely(N == VSIZEX  &&
                 !rayDataAlignment &&
                 !offsetAlignment))
      {
#if defined(__AVX__) && ENABLE_COHERENT_STREAM_PATH == 1
        if (unlikely(isCoherent(context->user->flags)))
          {
            filterSOACoherent(scene, rayData, streams, stream_offset, context, intersect);
            return;
          }
#endif

        for (size_t s = 0; s < streams; s++)
        {
          const size_t offset = s * stream_offset;
          RayK<VSIZEX>& ray = *(RayK<VSIZEX>*)(rayData + offset);
          const vboolx valid = (ray.tnear <= ray.tfar);          
          if (intersect)
            scene->intersect(valid, ray, context);
          else
            scene->occluded(valid, ray, context);
        }
      }
      else
      {
        /* this is a very slow fallback path but it's extremely unlikely to be hit */
        for (size_t s = 0; s < streams; s++)
        {          
          const size_t offset = s * stream_offset;
          RayPacketAOS rayN(rayData + offset, N);
          RayK<VSIZEX> ray;
          for (size_t i = 0; i < N; i++)
          {
            /* invalidate all lanes */
            ray.tnear = 0.0f;
            ray.tfar  = neg_inf;
            /* extract single ray and copy data to first packet lane */
            rayN.getRayByIndex(ray,0,i);
            const vboolx valid = (ray.tnear <= ray.tfar);          
            if (intersect)
            {
              scene->intersect(valid, ray, context);
              rayN.setHitByIndex(i,ray,0,true);
            }
            else
            {
              scene->occluded(valid, ray, context);
              rayN.setHitByIndex(i,ray,0,false);
            }
          }
        }
      }

#else

      /* otherwise use stream intersector */
      RayPacketAOS rayN(rayData, N);
      __aligned(64) Ray rays[MAX_RAYS_PER_OCTANT];
      __aligned(64) Ray *rays_ptr[MAX_RAYS_PER_OCTANT];
      
      size_t octants[8][MAX_RAYS_PER_OCTANT];
      unsigned int rays_in_octant[8];

      for (size_t i=0;i<8;i++) rays_in_octant[i] = 0;

      size_t soffset = 0;

      for (size_t s=0;s<streams;s++,soffset+=stream_offset)
      {
        // todo: use SIMD width to compute octants
        for (size_t i=0;i<N;i++)
        {
          /* global + local offset */
          const size_t offset = soffset + sizeof(float) * i;

          if (unlikely(!rayN.isValidByOffset(offset))) continue;

#if defined(EMBREE_IGNORE_INVALID_RAYS)
          __aligned(64) Ray ray = rayN.getRayByOffset(offset);
          if (unlikely(!ray.valid())) continue; 
#endif

          const size_t octantID = rayN.getOctantByOffset(offset);

          assert(octantID < 8);
          octants[octantID][rays_in_octant[octantID]++] = offset;
        
          if (unlikely(rays_in_octant[octantID] == MAX_RAYS_PER_OCTANT))
          {
            for (size_t j=0;j<MAX_RAYS_PER_OCTANT;j++)
            {
              rays_ptr[j] = &rays[j]; // rays_ptr might get reordered for occludedN
              rays[j] = rayN.getRayByOffset(octants[octantID][j]);
            }

            if (intersect)
              scene->intersectN((RTCRay**)rays_ptr,MAX_RAYS_PER_OCTANT,context);
            else
              scene->occludedN((RTCRay**)rays_ptr,MAX_RAYS_PER_OCTANT,context);

            for (size_t j=0;j<MAX_RAYS_PER_OCTANT;j++)
              rayN.setHitByOffset(octants[octantID][j],rays[j],intersect);
            
            rays_in_octant[octantID] = 0;
          }
        }        
      }

      /* flush remaining rays per octant */
      for (size_t i=0;i<8;i++)
        if (rays_in_octant[i])
        {
          for (size_t j=0;j<rays_in_octant[i];j++)
          {
            rays_ptr[j] = &rays[j]; // rays_ptr might get reordered for occludedN
            rays[j] = rayN.getRayByOffset(octants[i][j]);
          }

          if (intersect)
            scene->intersectN((RTCRay**)rays_ptr,rays_in_octant[i],context);
          else
            scene->occludedN((RTCRay**)rays_ptr,rays_in_octant[i],context);        

          for (size_t j=0;j<rays_in_octant[i];j++)
            rayN.setHitByOffset(octants[i][j],rays[j],intersect);
        }

#endif
    }

    void RayStream::filterSOPCoherent(Scene* scene, const RTCRayNp& _rayN, size_t N, IntersectContext* context, bool intersect)
    {
      RayStreamSOP& rayN = *(RayStreamSOP*)&_rayN;

      /* all valid accels need to have a intersectN/occludedN */
      bool chunkFallback = scene->isRobust() || !scene->accels.validIsecN();

      /* check for common octant */
      if (unlikely(!chunkFallback))
      {
        vfloatx min_x(pos_inf), max_x(neg_inf);
        vfloatx min_y(pos_inf), max_y(neg_inf);
        vfloatx min_z(pos_inf), max_z(neg_inf);
        vboolx all_active(true);
        for (size_t i = 0; i < N; i += VSIZEX)
        {
          const vintx vi = vintx(int(i)) + vintx(step);
          const vboolx valid = vi < vintx(int(N));
          const size_t offset = sizeof(float) * i;

          const Vec3vfx dir = rayN.getDirByOffset(valid, offset);

          min_x = min(min_x, dir.x);
          min_y = min(min_y, dir.y);
          min_z = min(min_z, dir.z);
          max_x = max(max_x, dir.x);
          max_y = max(max_y, dir.y);
          max_z = max(max_z, dir.z);

          vboolx active = rayN.isValidByOffset(valid, offset);
#if defined(EMBREE_IGNORE_INVALID_RAYS)
          __aligned(64) Ray ray = rayN.getRayByOffset(offset);
          active &= ray.valid();
#endif
          all_active = select(valid, all_active & active, all_active);
        }
        const bool commonOctant =
          (all(max_x < vfloatx(zero)) || all(min_x >= vfloatx(zero))) &&
          (all(max_y < vfloatx(zero)) || all(min_y >= vfloatx(zero))) &&
          (all(max_z < vfloatx(zero)) || all(min_z >= vfloatx(zero)));

        /* fallback to chunk in case of non-common octants */
        chunkFallback |= !commonOctant || !all(all_active);
      }

      /* fallback to chunk if necessary */
      if (unlikely(chunkFallback))
      {
        for (size_t i = 0; i < N; i += VSIZEX)
        {
          const vintx vi = vintx(int(i)) + vintx(step);
          vboolx valid = vi < vintx(int(N));
          const size_t offset = sizeof(float) * i;

          RayK<VSIZEX> ray = rayN.getRayByOffset<VSIZEX>(valid, offset);
          valid &= ray.tnear <= ray.tfar;
          if (intersect)
            scene->intersect(valid, ray, context);
          else
            scene->occluded (valid, ray, context);
          rayN.setHitByOffset<VSIZEX>(valid, offset, ray, intersect);
        }
        return;
      }

      static const size_t MAX_COHERENT_RAY_PACKETS = MAX_RAYS_PER_OCTANT / VSIZEX;
      __aligned(64) RayK<VSIZEX> rays[MAX_COHERENT_RAY_PACKETS];
      __aligned(64) RayK<VSIZEX>* rays_ptr[MAX_COHERENT_RAY_PACKETS];

      /* set input layout to SOA */
      context->setInputSOA(VSIZEX);

      for (size_t i = 0; i < N; i += MAX_COHERENT_RAY_PACKETS * VSIZEX)
      {
        const size_t size = min(N-i, MAX_COHERENT_RAY_PACKETS * VSIZEX);

        /* convert from SOP to SOA */
        for (size_t j = 0; j < size; j += VSIZEX)
        {
          const vintx vi = vintx(int(i+j)) + vintx(step);
          const vboolx valid = vi < vintx(int(N));
          const size_t offset = sizeof(float) * (i+j);
          const size_t packetID = j / VSIZEX;

          rays[packetID] = rayN.getRayByOffset(valid, offset);
          rays_ptr[packetID] = &rays[packetID]; // rays_ptr might get reordered for occludedN
        }

        /* trace as stream */
        if (intersect)
          scene->intersectN((RTCRay**)rays_ptr, size, context);
        else
          scene->occludedN((RTCRay**)rays_ptr, size, context);

        /* convert from SOA to SOP */
        for (size_t j = 0; j < size; j += VSIZEX)
        {
          const vintx vi = vintx(int(i+j)) + vintx(step);
          const vboolx valid = vi < vintx(int(N));
          const size_t offset = sizeof(float) * (i+j);
          const size_t packetID = j / VSIZEX;

          rayN.setHitByOffset(valid, offset, rays[packetID], intersect);
        }
      }
    }

    void RayStream::filterSOP(Scene* scene, const RTCRayNp& _rayN, size_t N, IntersectContext* context, bool intersect)
    {
      /* use fast path for coherent ray mode */
#if defined(__AVX__) && ENABLE_COHERENT_STREAM_PATH == 1
      if (unlikely(isCoherent(context->user->flags)))
      {
        filterSOPCoherent(scene, _rayN, N, context, intersect);
        return;
      }
#endif
      
      /* otherwise use stream intersector */
      RayStreamSOP& rayN = *(RayStreamSOP*)&_rayN;

#if 1

      /* fallback to packets */
      for (size_t i = 0; i < N; i += VSIZEX)
      {
        const vintx vi = vintx(int(i)) + vintx(step);
        vboolx valid = vi < vintx(int(N));
        const size_t offset = sizeof(float) * i;

        RayK<VSIZEX> ray = rayN.getRayByOffset<VSIZEX>(valid, offset);

        /* filter out invalid rays */
        valid &= ray.tnear <= ray.tfar;

        if (intersect)
          scene->intersect(valid, ray, context);
        else
          scene->occluded (valid, ray, context);

        rayN.setHitByOffset<VSIZEX>(valid, offset, ray, intersect);
      }

#else

      size_t rayStartIndex = 0;

      __aligned(64) Ray rays[MAX_RAYS_PER_OCTANT];
      __aligned(64) Ray *rays_ptr[MAX_RAYS_PER_OCTANT];

      size_t octants[8][MAX_RAYS_PER_OCTANT];
      unsigned int rays_in_octant[8];

      for (size_t i=0;i<8;i++) rays_in_octant[i] = 0;

      {
        // todo: use SIMD width to compute octants
        for (size_t i=rayStartIndex;i<N;i++)
        {
          /* global + local offset */
          const size_t offset = sizeof(float) * i;

          if (unlikely(!rayN.isValidByOffset(offset))) continue;

#if defined(EMBREE_IGNORE_INVALID_RAYS)
          __aligned(64) Ray ray = rayN.getRayByOffset(offset);
          if (unlikely(!ray.valid())) continue; 
#endif

          const size_t octantID = rayN.getOctantByOffset(offset);

          assert(octantID < 8);
          octants[octantID][rays_in_octant[octantID]++] = offset;
        
          if (unlikely(rays_in_octant[octantID] == MAX_RAYS_PER_OCTANT))
          {
            for (size_t j=0;j<MAX_RAYS_PER_OCTANT;j++)
            {
              rays_ptr[j] = &rays[j]; // rays_ptr might get reordered for occludedN
              rays[j] = rayN.getRayByOffset(octants[octantID][j]);
            }

            if (intersect)
              scene->intersectN((RTCRay**)rays_ptr,MAX_RAYS_PER_OCTANT,context);
            else
              scene->occludedN((RTCRay**)rays_ptr,MAX_RAYS_PER_OCTANT,context);

            for (size_t j=0;j<MAX_RAYS_PER_OCTANT;j++)
              rayN.setHitByOffset(octants[octantID][j],rays[j],intersect);
            
            rays_in_octant[octantID] = 0;
          }
        }        
      }

      /* flush remaining rays per octant */
      for (size_t i=0;i<8;i++)
        if (rays_in_octant[i])
        {
          for (size_t j=0;j<rays_in_octant[i];j++)
          {
            rays_ptr[j] = &rays[j]; // rays_ptr might get reordered for occludedN
            rays[j] = rayN.getRayByOffset(octants[i][j]);
          }

          if (intersect)
            scene->intersectN((RTCRay**)rays_ptr,rays_in_octant[i],context);
          else
            scene->occludedN((RTCRay**)rays_ptr,rays_in_octant[i],context);        

          for (size_t j=0;j<rays_in_octant[i];j++)
            rayN.setHitByOffset(octants[i][j],rays[j],intersect);
        }

#endif

    }

    RayStreamFilterFuncs rayStreamFilterFuncs() {
      return RayStreamFilterFuncs(RayStream::filterAOS, RayStream::filterAOP, RayStream::filterSOA, RayStream::filterSOP);
    }
  };
};
