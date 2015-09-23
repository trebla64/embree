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

#define EMBREE_FLOAT8

namespace embree
{
  /* 8-wide AVX float type */
  template<>
  struct vfloat<8>
  {
    typedef vboolf8 Bool;
    typedef vint8   Int;
    typedef vfloat8 Float;

    enum  { size = 8 };                 // number of SIMD elements
    union { __m256 m256; float v[8]; }; // data

    ////////////////////////////////////////////////////////////////////////////////
    /// Constructors, Assignment & Cast Operators
    ////////////////////////////////////////////////////////////////////////////////

    __forceinline vfloat            ( ) {}
    __forceinline vfloat            ( const vfloat8& other ) { m256 = other.m256; }
    __forceinline vfloat8& operator=( const vfloat8& other ) { m256 = other.m256; return *this; }

    __forceinline vfloat( const __m256  a ) : m256(a) {}
    __forceinline operator const __m256&( void ) const { return m256; }
    __forceinline operator       __m256&( void )       { return m256; }

    __forceinline explicit vfloat( const vfloat4& a                   ) : m256(_mm256_insertf128_ps(_mm256_castps128_ps256(a),a,1)) {}
    __forceinline          vfloat( const vfloat4& a, const vfloat4& b ) : m256(_mm256_insertf128_ps(_mm256_castps128_ps256(a),b,1)) {}

    __forceinline explicit vfloat( const char* const a ) : m256(_mm256_loadu_ps((const float*)a)) {}
    __forceinline          vfloat( const float&      a ) : m256(_mm256_broadcast_ss(&a)) {}
    __forceinline          vfloat( float a, float b) : m256(_mm256_set_ps(b, a, b, a, b, a, b, a)) {}
    __forceinline          vfloat( float a, float b, float c, float d ) : m256(_mm256_set_ps(d, c, b, a, d, c, b, a)) {}
    __forceinline          vfloat( float a, float b, float c, float d, float e, float f, float g, float h ) : m256(_mm256_set_ps(h, g, f, e, d, c, b, a)) {}

    __forceinline explicit vfloat( const __m256i a ) : m256(_mm256_cvtepi32_ps(a)) {}

    ////////////////////////////////////////////////////////////////////////////////
    /// Constants
    ////////////////////////////////////////////////////////////////////////////////

    __forceinline vfloat( ZeroTy   ) : m256(_mm256_setzero_ps()) {}
    __forceinline vfloat( OneTy    ) : m256(_mm256_set1_ps(1.0f)) {}
    __forceinline vfloat( PosInfTy ) : m256(_mm256_set1_ps(pos_inf)) {}
    __forceinline vfloat( NegInfTy ) : m256(_mm256_set1_ps(neg_inf)) {}
    __forceinline vfloat( StepTy   ) : m256(_mm256_set_ps(7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f, 0.0f)) {}
    __forceinline vfloat( NaNTy    ) : m256(_mm256_set1_ps(nan)) {}

    ////////////////////////////////////////////////////////////////////////////////
    /// Loads and Stores
    ////////////////////////////////////////////////////////////////////////////////

    static __forceinline vfloat8 broadcast( const void* const a ) {
      return _mm256_broadcast_ss((float*)a); 
    }

    static __forceinline const vfloat8 broadcast4f(const void* ptr) { // FIXME: vfloat4 input type?
      return _mm256_broadcast_ps((__m128*)ptr); 
    }

    static __forceinline vfloat8 load( const unsigned char* const ptr ) {
#if defined(__AVX2__)
      return _mm256_cvtepi32_ps(_mm256_cvtepu8_epi32(_mm_loadu_si128((__m128i*)ptr)));
#else
      return vfloat8(vfloat4::load(ptr),vfloat4::load(ptr+4));
#endif
    }
      
    static __forceinline vfloat8 load( const float* const a) {
      return _mm256_load_ps(a); 
    }
    
    static __forceinline void store(void* ptr, const vfloat8& f ) {
      return _mm256_store_ps((float*)ptr,f);
    }

    static __forceinline vfloat8 loadu( const void* const a) {
      return _mm256_loadu_ps((float*)a); 
    }
    
    static __forceinline void storeu(void* ptr, const vfloat8& f ) {
      return _mm256_storeu_ps((float*)ptr,f);
    }

    static __forceinline void store( const vboolf8& mask, void* ptr, const vfloat8& f ) {
      return _mm256_maskstore_ps((float*)ptr,(__m256i)mask,f);
    }
    
    static __forceinline void storeu( const vboolf8& mask, void* ptr, const vfloat8& f ) {
      return _mm256_storeu_ps((float*)ptr,_mm256_blendv_ps(_mm256_loadu_ps((float*)ptr),f,mask));
    }
    
#if defined (__AVX2__)
    static __forceinline vfloat8 load_nt(void* ptr) {
      return _mm256_castsi256_ps(_mm256_stream_load_si256((__m256i*)ptr));
    }
#endif
    
    static __forceinline void store_nt(float* ptr, const vfloat8& v) {
      _mm256_stream_ps(ptr,v);
    }

    static __forceinline void store ( const vboolf8& mask, void* ptr, const vint8& ofs, const vfloat8& v, const int scale = 1 )
    {
      if (likely(mask[0])) *(float*)(((char*)ptr)+scale*ofs[0]) = v[0];
      if (likely(mask[1])) *(float*)(((char*)ptr)+scale*ofs[1]) = v[1];
      if (likely(mask[2])) *(float*)(((char*)ptr)+scale*ofs[2]) = v[2];
      if (likely(mask[3])) *(float*)(((char*)ptr)+scale*ofs[3]) = v[3];
      if (likely(mask[4])) *(float*)(((char*)ptr)+scale*ofs[4]) = v[4];
      if (likely(mask[5])) *(float*)(((char*)ptr)+scale*ofs[5]) = v[5];
      if (likely(mask[6])) *(float*)(((char*)ptr)+scale*ofs[6]) = v[6];
      if (likely(mask[7])) *(float*)(((char*)ptr)+scale*ofs[7]) = v[7];
    }
    static __forceinline void store ( const vboolf8& mask, char* ptr, const vint8& ofs, const vfloat8& v ) {
      store(mask,ptr,ofs,v,1);
    }
    static __forceinline void store ( const vboolf8& mask, float* ptr, const vint8& ofs, const vfloat8& v ) {
      store(mask,ptr,ofs,v,4);
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// Array Access
    ////////////////////////////////////////////////////////////////////////////////

    __forceinline const float& operator []( const size_t i ) const { assert(i < 8); return v[i]; }
    __forceinline       float& operator []( const size_t i )       { assert(i < 8); return v[i]; }
  };


  ////////////////////////////////////////////////////////////////////////////////
  /// Unary Operators
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline const vfloat8 cast      (const vint8& a   ) { return _mm256_castsi256_ps(a); }
  __forceinline const vint8 cast      (const vfloat8& a   ) { return _mm256_castps_si256(a); }
  __forceinline const vfloat8 operator +( const vfloat8& a ) { return a; }
  __forceinline const vfloat8 operator -( const vfloat8& a ) {
    const __m256 mask = _mm256_castsi256_ps(_mm256_set1_epi32(0x80000000)); 
    return _mm256_xor_ps(a.m256, mask); 
  }
  __forceinline const vfloat8 abs  ( const vfloat8& a ) {
    const __m256 mask = _mm256_castsi256_ps(_mm256_set1_epi32(0x7fffffff));
    return _mm256_and_ps(a.m256, mask); 
  }
  __forceinline const vfloat8 sign    ( const vfloat8& a ) { return _mm256_blendv_ps(vfloat8(one), -vfloat8(one), _mm256_cmp_ps(a, vfloat8(zero), _CMP_NGE_UQ )); }
  __forceinline const vfloat8 signmsk ( const vfloat8& a ) { return _mm256_and_ps(a.m256,_mm256_castsi256_ps(_mm256_set1_epi32(0x80000000))); }

  __forceinline const vfloat8 rcp  ( const vfloat8& a ) {
    const vfloat8 r   = _mm256_rcp_ps(a.m256);
#if defined(__AVX2__)
    return _mm256_mul_ps(r,_mm256_fnmadd_ps(r, a, vfloat8(2.0f)));
#else
    return _mm256_mul_ps(r,_mm256_sub_ps(vfloat8(2.0f), _mm256_mul_ps(r, a)));
#endif
  }
  __forceinline const vfloat8 sqr  ( const vfloat8& a ) { return _mm256_mul_ps(a,a); }
  __forceinline const vfloat8 sqrt ( const vfloat8& a ) { return _mm256_sqrt_ps(a.m256); }
  __forceinline const vfloat8 rsqrt( const vfloat8& a ) {
    const vfloat8 r = _mm256_rsqrt_ps(a.m256);
    return _mm256_add_ps(_mm256_mul_ps(_mm256_set1_ps(1.5f), r), _mm256_mul_ps(_mm256_mul_ps(_mm256_mul_ps(a, _mm256_set1_ps(-0.5f)), r), _mm256_mul_ps(r, r))); 
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// Binary Operators
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline const vfloat8 operator +( const vfloat8& a, const vfloat8& b ) { return _mm256_add_ps(a.m256, b.m256); }
  __forceinline const vfloat8 operator +( const vfloat8& a, const float   b ) { return a + vfloat8(b); }
  __forceinline const vfloat8 operator +( const float   a, const vfloat8& b ) { return vfloat8(a) + b; }

  __forceinline const vfloat8 operator -( const vfloat8& a, const vfloat8& b ) { return _mm256_sub_ps(a.m256, b.m256); }
  __forceinline const vfloat8 operator -( const vfloat8& a, const float   b ) { return a - vfloat8(b); }
  __forceinline const vfloat8 operator -( const float   a, const vfloat8& b ) { return vfloat8(a) - b; }

  __forceinline const vfloat8 operator *( const vfloat8& a, const vfloat8& b ) { return _mm256_mul_ps(a.m256, b.m256); }
  __forceinline const vfloat8 operator *( const vfloat8& a, const float    b ) { return a * vfloat8(b); }
  __forceinline const vfloat8 operator *( const float    a, const vfloat8& b ) { return vfloat8(a) * b; }

  __forceinline const vfloat8 operator /( const vfloat8& a, const vfloat8& b ) { return _mm256_div_ps(a.m256, b.m256); }
  __forceinline const vfloat8 operator /( const vfloat8& a, const float    b ) { return a / vfloat8(b); }
  __forceinline const vfloat8 operator /( const float    a, const vfloat8& b ) { return vfloat8(a) / b; }

  __forceinline const vfloat8 operator^( const vfloat8& a, const vfloat8& b ) { return _mm256_xor_ps(a.m256,b.m256); }
  __forceinline const vfloat8 operator^( const vfloat8& a, const vint8&   b ) { return _mm256_xor_ps(a.m256,_mm256_castsi256_ps(b.m256)); }

  __forceinline const vfloat8 operator&( const vfloat8& a, const vfloat8& b ) { return _mm256_and_ps(a.m256,b.m256); }

  __forceinline const vfloat8 min( const vfloat8& a, const vfloat8& b ) { return _mm256_min_ps(a.m256, b.m256); }
  __forceinline const vfloat8 min( const vfloat8& a, const float    b ) { return _mm256_min_ps(a.m256, vfloat8(b)); }
  __forceinline const vfloat8 min( const float    a, const vfloat8& b ) { return _mm256_min_ps(vfloat8(a), b.m256); }

  __forceinline const vfloat8 max( const vfloat8& a, const vfloat8& b ) { return _mm256_max_ps(a.m256, b.m256); }
  __forceinline const vfloat8 max( const vfloat8& a, const float    b ) { return _mm256_max_ps(a.m256, vfloat8(b)); }
  __forceinline const vfloat8 max( const float    a, const vfloat8& b ) { return _mm256_max_ps(vfloat8(a), b.m256); }

#if defined (__AVX2__)
    __forceinline vfloat8 mini(const vfloat8& a, const vfloat8& b) {
      const vint8 ai = _mm256_castps_si256(a);
      const vint8 bi = _mm256_castps_si256(b);
      const vint8 ci = _mm256_min_epi32(ai,bi);
      return _mm256_castsi256_ps(ci);
    }
    __forceinline vfloat8 maxi(const vfloat8& a, const vfloat8& b) {
      const vint8 ai = _mm256_castps_si256(a);
      const vint8 bi = _mm256_castps_si256(b);
      const vint8 ci = _mm256_max_epi32(ai,bi);
      return _mm256_castsi256_ps(ci);
    }
#endif

  ////////////////////////////////////////////////////////////////////////////////
  /// Ternary Operators
  ////////////////////////////////////////////////////////////////////////////////

#if defined(__AVX2__)
  __forceinline const vfloat8 madd  ( const vfloat8& a, const vfloat8& b, const vfloat8& c) { return _mm256_fmadd_ps(a,b,c); }
  __forceinline const vfloat8 msub  ( const vfloat8& a, const vfloat8& b, const vfloat8& c) { return _mm256_fmsub_ps(a,b,c); }
  __forceinline const vfloat8 nmadd ( const vfloat8& a, const vfloat8& b, const vfloat8& c) { return _mm256_fnmadd_ps(a,b,c); }
  __forceinline const vfloat8 nmsub ( const vfloat8& a, const vfloat8& b, const vfloat8& c) { return _mm256_fnmsub_ps(a,b,c); }
#else
  __forceinline const vfloat8 madd  ( const vfloat8& a, const vfloat8& b, const vfloat8& c) { return a*b+c; }
  __forceinline const vfloat8 msub  ( const vfloat8& a, const vfloat8& b, const vfloat8& c) { return a*b-c; }
  __forceinline const vfloat8 nmadd ( const vfloat8& a, const vfloat8& b, const vfloat8& c) { return -a*b+c;}
  __forceinline const vfloat8 nmsub ( const vfloat8& a, const vfloat8& b, const vfloat8& c) { return -a*b-c; }
#endif

  ////////////////////////////////////////////////////////////////////////////////
  /// Assignment Operators
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline vfloat8& operator +=( vfloat8& a, const vfloat8& b ) { return a = a + b; }
  __forceinline vfloat8& operator +=( vfloat8& a, const float    b ) { return a = a + b; }

  __forceinline vfloat8& operator -=( vfloat8& a, const vfloat8& b ) { return a = a - b; }
  __forceinline vfloat8& operator -=( vfloat8& a, const float    b ) { return a = a - b; }

  __forceinline vfloat8& operator *=( vfloat8& a, const vfloat8& b ) { return a = a * b; }
  __forceinline vfloat8& operator *=( vfloat8& a, const float    b ) { return a = a * b; }

  __forceinline vfloat8& operator /=( vfloat8& a, const vfloat8& b ) { return a = a / b; }
  __forceinline vfloat8& operator /=( vfloat8& a, const float    b ) { return a = a / b; }

  ////////////////////////////////////////////////////////////////////////////////
  /// Comparison Operators + Select
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline const vboolf8 operator ==( const vfloat8& a, const vfloat8& b ) { return _mm256_cmp_ps(a.m256, b.m256, _CMP_EQ_OQ ); }
  __forceinline const vboolf8 operator ==( const vfloat8& a, const float    b ) { return _mm256_cmp_ps(a.m256, vfloat8(b), _CMP_EQ_OQ ); }
  __forceinline const vboolf8 operator ==( const float    a, const vfloat8& b ) { return _mm256_cmp_ps(vfloat8(a), b.m256, _CMP_EQ_OQ ); }

  __forceinline const vboolf8 operator !=( const vfloat8& a, const vfloat8& b ) { return _mm256_cmp_ps(a.m256, b.m256, _CMP_NEQ_OQ); }
  __forceinline const vboolf8 operator !=( const vfloat8& a, const float    b ) { return _mm256_cmp_ps(a.m256, vfloat8(b), _CMP_NEQ_OQ); }
  __forceinline const vboolf8 operator !=( const float    a, const vfloat8& b ) { return _mm256_cmp_ps(vfloat8(a), b.m256, _CMP_NEQ_OQ); }

  __forceinline const vboolf8 operator < ( const vfloat8& a, const vfloat8& b ) { return _mm256_cmp_ps(a.m256, b.m256, _CMP_LT_OQ ); }
  __forceinline const vboolf8 operator < ( const vfloat8& a, const float    b ) { return _mm256_cmp_ps(a.m256, vfloat8(b), _CMP_LT_OQ ); }
  __forceinline const vboolf8 operator < ( const float    a, const vfloat8& b ) { return _mm256_cmp_ps(vfloat8(a), b.m256, _CMP_LT_OQ ); }

  __forceinline const vboolf8 operator >=( const vfloat8& a, const vfloat8& b ) { return _mm256_cmp_ps(a.m256, b.m256, _CMP_GE_OQ); }
  __forceinline const vboolf8 operator >=( const vfloat8& a, const float    b ) { return _mm256_cmp_ps(a.m256, vfloat8(b), _CMP_GE_OQ); }
  __forceinline const vboolf8 operator >=( const float    a, const vfloat8& b ) { return _mm256_cmp_ps(vfloat8(a), b.m256, _CMP_GE_OQ); }

  __forceinline const vboolf8 operator > ( const vfloat8& a, const vfloat8& b ) { return _mm256_cmp_ps(a.m256, b.m256, _CMP_GT_OQ); }
  __forceinline const vboolf8 operator > ( const vfloat8& a, const float    b ) { return _mm256_cmp_ps(a.m256, vfloat8(b), _CMP_GT_OQ); }
  __forceinline const vboolf8 operator > ( const float    a, const vfloat8& b ) { return _mm256_cmp_ps(vfloat8(a), b.m256, _CMP_GT_OQ); }

  __forceinline const vboolf8 operator <=( const vfloat8& a, const vfloat8& b ) { return _mm256_cmp_ps(a.m256, b.m256, _CMP_LE_OQ ); }
  __forceinline const vboolf8 operator <=( const vfloat8& a, const float    b ) { return _mm256_cmp_ps(a.m256, vfloat8(b), _CMP_LE_OQ ); }
  __forceinline const vboolf8 operator <=( const float    a, const vfloat8& b ) { return _mm256_cmp_ps(vfloat8(a), b.m256, _CMP_LE_OQ ); }
  
  __forceinline const vfloat8 select( const vboolf8& m, const vfloat8& t, const vfloat8& f ) {
    return _mm256_blendv_ps(f, t, m); 
  }

#if defined(__clang__) || defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  __forceinline const vfloat8 select(const int m, const vfloat8& t, const vfloat8& f) {
    return select(vboolf8(m), t, f); // workaround for clang and Microsoft compiler bugs
  }
#else
  __forceinline const vfloat8 select( const int m, const vfloat8& t, const vfloat8& f ) {
    return _mm256_blend_ps(f, t, m);
  }
#endif

  __forceinline vfloat8  lerp(const vfloat8& a, const vfloat8& b, const vfloat8& t) {
#if defined(__AVX2__)
    return madd(t, b, madd(-t, a, a));
#else
    return a + t*(b-a);
#endif
  }

  __forceinline bool isvalid ( const vfloat8& v ) {
    return all((v > vfloat8(-FLT_LARGE)) & (v < vfloat8(+FLT_LARGE)));
  }

  __forceinline bool is_finite ( const vfloat8& a ) {
    return all((a >= vfloat8(-FLT_MAX) & (a <= vfloat8(+FLT_MAX))));
  }

  __forceinline bool is_finite ( const vboolf8& valid, const vfloat8& a ) {
    return all(valid, (a >= vfloat8(-FLT_MAX) & (a <= vfloat8(+FLT_MAX))));
  }
      
  ////////////////////////////////////////////////////////////////////////////////
  /// Rounding Functions
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline const vfloat8 floor     ( const vfloat8& a ) { return _mm256_round_ps(a, _MM_FROUND_TO_NEG_INF    ); }
  __forceinline const vfloat8 ceil      ( const vfloat8& a ) { return _mm256_round_ps(a, _MM_FROUND_TO_POS_INF    ); }
  __forceinline const vfloat8 trunc     ( const vfloat8& a ) { return _mm256_round_ps(a, _MM_FROUND_TO_ZERO       ); }
  __forceinline const vfloat8 frac      ( const vfloat8& a ) { return a-floor(a); }

  ////////////////////////////////////////////////////////////////////////////////
  /// Movement/Shifting/Shuffling Functions
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline vfloat8 unpacklo( const vfloat8& a, const vfloat8& b ) { return _mm256_unpacklo_ps(a.m256, b.m256); }
  __forceinline vfloat8 unpackhi( const vfloat8& a, const vfloat8& b ) { return _mm256_unpackhi_ps(a.m256, b.m256); }

  template<size_t i> __forceinline const vfloat8 shuffle( const vfloat8& a ) {
    return _mm256_permute_ps(a, _MM_SHUFFLE(i, i, i, i));
  }

  template<size_t i0, size_t i1> __forceinline const vfloat8 shuffle128( const vfloat8& a ) {
    return _mm256_permute2f128_ps(a, a, (i1 << 4) | (i0 << 0));
  }

  template<size_t i0, size_t i1> __forceinline const vfloat8 shuffle128( const vfloat8& a,  const vfloat8& b) {
    return _mm256_permute2f128_ps(a, b, (i1 << 4) | (i0 << 0));
  }

  template<size_t i0, size_t i1, size_t i2, size_t i3> __forceinline const vfloat8 shuffle( const vfloat8& a ) {
    return _mm256_permute_ps(a, _MM_SHUFFLE(i3, i2, i1, i0));
  }

  template<size_t i0, size_t i1, size_t i2, size_t i3> __forceinline const vfloat8 shuffle( const vfloat8& a, const vfloat8& b ) {
    return _mm256_shuffle_ps(a, b, _MM_SHUFFLE(i3, i2, i1, i0));
  }

  template<> __forceinline const vfloat8 shuffle<0, 0, 2, 2>( const vfloat8& b ) { return _mm256_moveldup_ps(b); }
  template<> __forceinline const vfloat8 shuffle<1, 1, 3, 3>( const vfloat8& b ) { return _mm256_movehdup_ps(b); }
  template<> __forceinline const vfloat8 shuffle<0, 1, 0, 1>( const vfloat8& b ) { return _mm256_castpd_ps(_mm256_movedup_pd(_mm256_castps_pd(b))); }

  __forceinline const vfloat8 broadcast(const float* ptr) { return _mm256_broadcast_ss(ptr); }
  template<size_t i> __forceinline const vfloat8 insert (const vfloat8& a, const vfloat4& b) { return _mm256_insertf128_ps (a,b,i); }
  template<size_t i> __forceinline const vfloat4 extract   (const vfloat8& a               ) { return _mm256_extractf128_ps(a  ,i); }
  template<>         __forceinline const vfloat4 extract<0>(const vfloat8& a               ) { return _mm256_castps256_ps128(a); }

  template<size_t i> __forceinline float fextract   (const vfloat8& a            ) { return _mm_cvtss_f32(_mm256_extractf128_ps(a  ,i)); }

  __forceinline vfloat8 assign( const vfloat4& a ) { return _mm256_castps128_ps256(a); }

#if defined (__AVX2__)
  __forceinline vfloat8 permute(const vfloat8 &a, const __m256i &index) {
    return _mm256_permutevar8x32_ps(a,index);
  }

  template<int i>
  __forceinline vfloat8 alignr(const vfloat8 &a, const vfloat8 &b) {
    return _mm256_castsi256_ps(_mm256_alignr_epi8(_mm256_castps_si256(a), _mm256_castps_si256(b), i));
  }  
#endif

#if defined (__AVX_I__)
  template<const int mode>
  __forceinline vint4 convert_to_hf16(const vfloat8 &a) {
    return _mm256_cvtps_ph(a,mode);
  }

  __forceinline vfloat8 convert_from_hf16(const vint4 &a) {
    return _mm256_cvtph_ps(a);
  }
#endif

  __forceinline vfloat4 broadcast4f( const vfloat8& a, const size_t k ) {
    return vfloat4::broadcast(&a[k]);
  }

  __forceinline vfloat8 broadcast8f( const vfloat8& a, const size_t k ) {
    return vfloat8::broadcast(&a[k]);
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// Transpose
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline void transpose(const vfloat8& r0, const vfloat8& r1, const vfloat8& r2, const vfloat8& r3, vfloat8& c0, vfloat8& c1, vfloat8& c2, vfloat8& c3)
  {
    vfloat8 l02 = unpacklo(r0,r2);
    vfloat8 h02 = unpackhi(r0,r2);
    vfloat8 l13 = unpacklo(r1,r3);
    vfloat8 h13 = unpackhi(r1,r3);
    c0 = unpacklo(l02,l13);
    c1 = unpackhi(l02,l13);
    c2 = unpacklo(h02,h13);
    c3 = unpackhi(h02,h13);
  }

  __forceinline void transpose(const vfloat8& r0, const vfloat8& r1, const vfloat8& r2, const vfloat8& r3, vfloat8& c0, vfloat8& c1, vfloat8& c2)
  {
    vfloat8 l02 = unpacklo(r0,r2);
    vfloat8 h02 = unpackhi(r0,r2);
    vfloat8 l13 = unpacklo(r1,r3);
    vfloat8 h13 = unpackhi(r1,r3);
    c0 = unpacklo(l02,l13);
    c1 = unpackhi(l02,l13);
    c2 = unpacklo(h02,h13);
  }

  __forceinline void transpose(const vfloat8& r0, const vfloat8& r1, const vfloat8& r2, const vfloat8& r3, const vfloat8& r4, const vfloat8& r5, const vfloat8& r6, const vfloat8& r7,
                               vfloat8& c0, vfloat8& c1, vfloat8& c2, vfloat8& c3, vfloat8& c4, vfloat8& c5, vfloat8& c6, vfloat8& c7)
  {
    vfloat8 h0,h1,h2,h3; transpose(r0,r1,r2,r3,h0,h1,h2,h3);
    vfloat8 h4,h5,h6,h7; transpose(r4,r5,r6,r7,h4,h5,h6,h7);
    c0 = shuffle128<0,2>(h0,h4);
    c1 = shuffle128<0,2>(h1,h5);
    c2 = shuffle128<0,2>(h2,h6);
    c3 = shuffle128<0,2>(h3,h7);
    c4 = shuffle128<1,3>(h0,h4);
    c5 = shuffle128<1,3>(h1,h5);
    c6 = shuffle128<1,3>(h2,h6);
    c7 = shuffle128<1,3>(h3,h7);
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// Reductions
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline const vfloat8 vreduce_min2(const vfloat8& v) { return min(v,shuffle<1,0,3,2>(v)); }
  __forceinline const vfloat8 vreduce_min4(const vfloat8& v) { vfloat8 v1 = vreduce_min2(v); return min(v1,shuffle<2,3,0,1>(v1)); }
  __forceinline const vfloat8 vreduce_min (const vfloat8& v) { vfloat8 v1 = vreduce_min4(v); return min(v1,shuffle128<1,0>(v1)); }

  __forceinline const vfloat8 vreduce_max2(const vfloat8& v) { return max(v,shuffle<1,0,3,2>(v)); }
  __forceinline const vfloat8 vreduce_max4(const vfloat8& v) { vfloat8 v1 = vreduce_max2(v); return max(v1,shuffle<2,3,0,1>(v1)); }
  __forceinline const vfloat8 vreduce_max (const vfloat8& v) { vfloat8 v1 = vreduce_max4(v); return max(v1,shuffle128<1,0>(v1)); }

  __forceinline const vfloat8 vreduce_add2(const vfloat8& v) { return v + shuffle<1,0,3,2>(v); }
  __forceinline const vfloat8 vreduce_add4(const vfloat8& v) { vfloat8 v1 = vreduce_add2(v); return v1 + shuffle<2,3,0,1>(v1); }
  __forceinline const vfloat8 vreduce_add (const vfloat8& v) { vfloat8 v1 = vreduce_add4(v); return v1 + shuffle128<1,0>(v1); }

  __forceinline float reduce_min(const vfloat8& v) { return _mm_cvtss_f32(extract<0>(vreduce_min(v))); }
  __forceinline float reduce_max(const vfloat8& v) { return _mm_cvtss_f32(extract<0>(vreduce_max(v))); }
  __forceinline float reduce_add(const vfloat8& v) { return _mm_cvtss_f32(extract<0>(vreduce_add(v))); }

  __forceinline size_t select_min(const vfloat8& v) { return __bsf(movemask(v == vreduce_min(v))); }
  __forceinline size_t select_max(const vfloat8& v) { return __bsf(movemask(v == vreduce_max(v))); }

  __forceinline size_t select_min(const vboolf8& valid, const vfloat8& v) { const vfloat8 a = select(valid,v,vfloat8(pos_inf)); return __bsf(movemask(valid & (a == vreduce_min(a)))); }
  __forceinline size_t select_max(const vboolf8& valid, const vfloat8& v) { const vfloat8 a = select(valid,v,vfloat8(neg_inf)); return __bsf(movemask(valid & (a == vreduce_max(a)))); }

  ////////////////////////////////////////////////////////////////////////////////
  /// Euclidian Space Operators
  ////////////////////////////////////////////////////////////////////////////////

  //__forceinline vfloat8 dot ( const vfloat8& a, const vfloat8& b ) {
  //  return vreduce_add4(a*b);
  //}

  __forceinline vfloat8 dot ( const vfloat8& a, const vfloat8& b ) {
    return _mm256_dp_ps(a,b,0x7F);
  }

  __forceinline vfloat8 cross ( const vfloat8& a, const vfloat8& b )
  {
    const vfloat8 a0 = a;
    const vfloat8 b0 = shuffle<1,2,0,3>(b);
    const vfloat8 a1 = shuffle<1,2,0,3>(a);
    const vfloat8 b1 = b;
    return shuffle<1,2,0,3>(msub(a0,b0,a1*b1));
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// Output Operators
  ////////////////////////////////////////////////////////////////////////////////

  inline std::ostream& operator<<(std::ostream& cout, const vfloat8& a) {
    return cout << "<" << a[0] << ", " << a[1] << ", " << a[2] << ", " << a[3] << ", " << a[4] << ", " << a[5] << ", " << a[6] << ", " << a[7] << ">";
  }
}
