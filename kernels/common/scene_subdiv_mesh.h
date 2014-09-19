// ======================================================================== //
// Copyright 2009-2014 Intel Corporation                                    //
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

#include "common/geometry.h"
#include "common/buffer.h"


namespace embree
{
  class SubdivFace;
  class SubdivHalfEdge;
  class SubdivVertex;

  class SubdivMesh : public Geometry
  {
  public:
    SubdivMesh(Scene* parent, RTCGeometryFlags flags, size_t numFaces, size_t numEdges, size_t numVertices, size_t numTimeSteps);

    void enabling();
    void disabling();
    void setMask (unsigned mask);
    void setBuffer(RTCBufferType type, void* ptr, size_t offset, size_t stride);
    void* map(RTCBufferType type);
    void unmap(RTCBufferType type);
    void setUserData (void* ptr, bool ispc);
    void immutable ();
    bool verify ();

    unsigned int mask;                //!< for masking out geometry
    unsigned int numTimeSteps;        //!< number of time steps (1 or 2)  

    size_t numFaces;                  //!< number of faces
    size_t numEdges;                  //!< number of edges
    size_t numVertices;               //!< number of vertices
    size_t numHalfEdges;              //!< number of half-edges

    size_t size() const { return numFaces; };

    
    class HalfEdge
    {
    public:
      unsigned int vtx_index;
      unsigned int start_halfedge_id : 30;
      unsigned int local_halfedge_id :  2;
      unsigned int opposite_index;

      bool hasOpposite() const {
        return opposite_index != (unsigned int)-1;
      };

      HalfEdge *opposite(HalfEdge *halfEdges) const { 
        assert( opposite_index != (unsigned int)-1 );
        return &halfEdges[opposite_index]; 
      };

      unsigned int getStartVertexIndex(HalfEdge *halfEdges) const { 
        return vtx_index; 
      };
      
      HalfEdge *next(HalfEdge *halfEdges) const {
        return &halfEdges[ start_halfedge_id + ((local_halfedge_id+1)%4) ];
      };

      HalfEdge *prev(HalfEdge *halfEdges) const {
        return &halfEdges[ start_halfedge_id + ((local_halfedge_id+3)%4) ];
      };

      unsigned int getEndVertexIndex(HalfEdge *halfEdges) const {
        return next(halfEdges)->vtx_index;
      };
      
      HalfEdge *base() const {
	return (HalfEdge *)this - (start_halfedge_id + local_halfedge_id);
      };
    };


  private:
    BufferT<Vec3fa> vertices[2];      //!< vertex array

    
    /*! Indices of the vertices composing each face, provided by the application */
    BufferT<unsigned int> vertexIndices;

    /*! Offsets into the vertexIndices array indexed by face, provided by the application */
    BufferT<unsigned int> vertexOffsets;


    HalfEdge *halfEdges;

  public:

    /*! Coordinates of the vertex at the given index in the mesh. */
    __forceinline const Vec3fa &getVertexPosition(unsigned int index, const unsigned int t = 0) const { return vertices[t][index]; }

    __forceinline const Vec3fa *getVertexPositionPtr(const unsigned int t = 0) const { return &vertices[t][0]; }

    __forceinline const HalfEdge &getHalfEdgeForQuad(unsigned int q, const unsigned int i=0) const { return halfEdges[q*4+i]; }


    __forceinline const Vec3fa &getVertexPositionForHalfEdge(const HalfEdge &e) const { 
      return getVertexPosition( e.vtx_index );
    }

    __forceinline const Vec3fa &getVertexPositionForQuad(unsigned int q, const unsigned int i=0) const { 
      return getVertexPositionForHalfEdge( getHalfEdgeForQuad(q,i) );
    }


    void initializeHalfEdgeStructures ();

    /*! calculates the bounds of the quad associated with the half-edge */
    __forceinline BBox3fa bounds_quad(HalfEdge &e) const 
    {
      HalfEdge *p = &e;
      BBox3fa b = getVertexPositionForHalfEdge(*p);
      p = p->next( halfEdges );
      b.extend( getVertexPositionForHalfEdge(*p) );
      p = p->next( halfEdges );
      b.extend( getVertexPositionForHalfEdge(*p) );
      p = p->next( halfEdges );
      b.extend( getVertexPositionForHalfEdge(*p) );
      return b;
    }

    /*! calculates the bounds of the 1-ring associated with the vertex of the half-edge */
    __forceinline BBox3fa bounds_1ring(HalfEdge &e) const 
    {
      BBox3fa b = empty;
      HalfEdge *p = &e;
      do {
	/*! get bounds for the adjacent quad */
	b.extend( bounds_quad( *p ) );
	/*! continue with next adjacent edge. */
	p = p->opposite( halfEdges );
	p = p->next( halfEdges );
      } while( p != &e);
      
      return b;
    }

    /*! calculates the bounds of the i'th subdivision patch */
    __forceinline BBox3fa bounds(size_t i) const 
    {
      BBox3fa b = empty;
      for (size_t j=0;j<4;j++)
	b.extend( bounds_1ring(halfEdges[i*4+j]) );
      return b;
    }

  };

  __forceinline std::ostream &operator<<(std::ostream &o, const SubdivMesh::HalfEdge &h)
    {
      o << "vtx_index " << h.vtx_index << " start_halfedge_id " << h.start_halfedge_id << " local_halfedge_id " << h.local_halfedge_id << " opposite_index " << h.opposite_index;
      return o;
    } 


};
