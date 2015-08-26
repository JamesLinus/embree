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

#include "bvh8_intersector8_test.h"
#include "../geometry/triangle4.h"
#include "../geometry/triangle8.h"
#include "../geometry/triangle8v.h"
#include "../geometry/intersector_iterators.h"
#include "../geometry/triangle_intersector_moeller.h"
#include "../geometry/triangle_intersector_pluecker.h"
#include "../geometry/triangle_intersector_pluecker2.h"

#define DBG(x) 

#define OLD_TRAVERSAL 0

namespace embree
{
  namespace isa
  { 

    template<typename PrimitiveIntersector8>
    __forceinline void BVH8Intersector8Test<PrimitiveIntersector8>::intersect1(const BVH8* bvh, NodeRef root, const size_t k, Precalculations& pre, Ray8& ray,const Vec3f8 &ray_org, const Vec3f8 &ray_dir, const Vec3f8 &ray_rdir, const float8 &ray_tnear, const float8 &ray_tfar, const Vec3i8& nearXYZ)
    {
#if defined (__AVX2__)
      /*! stack state */
      StackItemT<NodeRef> stack[stackSizeSingle];  //!< stack of nodes 
      StackItemT<NodeRef>* stackPtr = stack+2;        //!< current stack pointer
      //StackItemT<NodeRef>* stackEnd = stack+stackSizeSingle;
      stack[0].ptr  = BVH8::invalidNode;
      stack[0].dist = inf;
      stack[1].ptr  = root;
      stack[1].dist = neg_inf;
      
      /*! offsets to select the side that becomes the lower or upper bound */
      //const size_t nearX = nearXYZ.x[k];
      //const size_t nearY = nearXYZ.y[k];
      //const size_t nearZ = nearXYZ.z[k];

      /*! load the ray into SIMD registers */
      const Vec3f8 org (ray_org .x[k],ray_org .y[k],ray_org .z[k]);
      const Vec3f8 rdir(ray_rdir.x[k],ray_rdir.y[k],ray_rdir.z[k]);
      const Vec3f8 norg = -org;
      const Vec3f8 org_rdir(org*rdir);
      float8 rayNear(ray_tnear[k]), rayFar(ray_tfar[k]);
     
/* pop loop */

      while (true) //pop:
      {
        stackPtr--;
        NodeRef cur = stackPtr->ptr;

        /*! pop next node */
        //if (unlikely(stackPtr == stack)) break;
        //stackPtr--;
        
        /*! if popped node is too far, pop next one */
        //if (unlikely(*(float*)&stackPtr->dist > ray.tfar[k]))
        //continue;
        
        /* downtraversal loop */
        const float8 inactive_inf = float8(pos_inf);

        while (true)
        {
          /*! stop if we found a leaf */
          if (unlikely(cur.isLeaf())) break;
          STAT3(normal.trav_nodes,1,1,1);
          
          /*! single ray intersection with 4 boxes */
          const Node* node = cur.node();

#if OLD_TRAVERSAL == 1

          const size_t farX  = nearX ^ sizeof(float8), farY  = nearY ^ sizeof(float8), farZ  = nearZ ^ sizeof(float8);
          const float8 _tNearX = msub(load8f((const char*)node+nearX), rdir.x, org_rdir.x);
          const float8 _tNearY = msub(load8f((const char*)node+nearY), rdir.y, org_rdir.y);
          const float8 _tNearZ = msub(load8f((const char*)node+nearZ), rdir.z, org_rdir.z);
          const float8 _tFarX  = msub(load8f((const char*)node+farX ), rdir.x, org_rdir.x);
          const float8 _tFarY  = msub(load8f((const char*)node+farY ), rdir.y, org_rdir.y);
          const float8 _tFarZ  = msub(load8f((const char*)node+farZ ), rdir.z, org_rdir.z);

          const float8 tNearX  = _tNearX;
          const float8 tNearY  = _tNearY;
          const float8 tNearZ  = _tNearZ;

          const float8 tFarX   = _tFarX;
          const float8 tFarY   = _tFarY;
          const float8 tFarZ   = _tFarZ;

          const float8 tNear = maxi(maxi(tNearX,tNearY),maxi(tNearZ,rayNear));
          const float8 tFar  = mini(mini(tFarX ,tFarY ),mini(tFarZ ,rayFar ));
          const bool8 vmask = cast(tNear) > cast(tFar);
          size_t mask = movemask(vmask)^0xff;

#else
          const float8 _tNearX = msub(node->lower_x, rdir.x, org_rdir.x);
          const float8 _tNearY = msub(node->lower_y, rdir.y, org_rdir.y);
          const float8 _tNearZ = msub(node->lower_z, rdir.z, org_rdir.z);
          const float8 _tFarX  = msub(node->upper_x, rdir.x, org_rdir.x);
          const float8 _tFarY  = msub(node->upper_y, rdir.y, org_rdir.y);
          const float8 _tFarZ  = msub(node->upper_z, rdir.z, org_rdir.z);
          stackPtr--;
          const bool8  nactive = inactive_inf == node->lower_x;
          cur = stackPtr->ptr; 
          
          const float8 tNearX  = mini(_tNearX,_tFarX);
          const float8 tNearY  = mini(_tNearY,_tFarY);
          const float8 tNearZ  = mini(_tNearZ,_tFarZ);

          const float8 tFarX   = maxi(_tNearX,_tFarX);
          const float8 tFarY   = maxi(_tNearY,_tFarY);
          const float8 tFarZ   = maxi(_tNearZ,_tFarZ);

          const float8 tNear   = maxi(maxi(tNearX,tNearY),maxi(tNearZ,rayNear));
          const float8 tFar    = mini(mini(tFarX ,tFarY ),mini(tFarZ ,rayFar ));
          const bool8 vmask    = nactive | cast(tNear) > cast(tFar);

          size_t mask = movemask(vmask)^0xff;

#endif
          
          /*! if no child is hit, pop next node */
          if (unlikely(mask == 0)) continue;
            //goto pop;
          stackPtr++;

          /*! one child is hit, continue with that child */
          size_t r = __bscf(mask);
          if (likely(mask == 0)) {
            cur = node->child(r); 
            cur.prefetch();
            assert(cur != BVH8::emptyNode);
            continue;
          }
          
          /*! two children are hit, push far child, and continue with closer child */
          NodeRef c0 = node->child(r); c0.prefetch(); const unsigned int d0 = ((unsigned int*)&tNear)[r];
          r = __bscf(mask);
          NodeRef c1 = node->child(r); c1.prefetch(); const unsigned int d1 = ((unsigned int*)&tNear)[r];
          assert(c0 != BVH8::emptyNode);
          assert(c1 != BVH8::emptyNode);
          if (likely(mask == 0)) {
            assert(stackPtr < stackEnd); 
            if (d0 < d1) { stackPtr->ptr = c1; stackPtr->dist = d1; stackPtr++; cur = c0; continue; }
            else         { stackPtr->ptr = c0; stackPtr->dist = d0; stackPtr++; cur = c1; continue; }
          }
          
          /*! Here starts the slow path for 3 or 4 hit children. We push
           *  all nodes onto the stack to sort them there. */
          assert(stackPtr < stackEnd); 
          stackPtr->ptr = c0; stackPtr->dist = d0; stackPtr++;
          assert(stackPtr < stackEnd); 
          stackPtr->ptr = c1; stackPtr->dist = d1; stackPtr++;
          
          /*! three children are hit, push all onto stack and sort 3 stack items, continue with closest child */
          assert(stackPtr < stackEnd); 
          r = __bscf(mask);
          NodeRef c = node->child(r); c.prefetch(); unsigned int d = ((unsigned int*)&tNear)[r]; stackPtr->ptr = c; stackPtr->dist = d; stackPtr++;
          assert(c != BVH8::emptyNode);
          if (likely(mask == 0)) {
            sort(stackPtr[-1],stackPtr[-2],stackPtr[-3]);
            cur = (NodeRef) stackPtr[-1].ptr; stackPtr--;
            continue;
          }
          
	  /*! four children are hit, push all onto stack and sort 4 stack items, continue with closest child */
          r = __bscf(mask);
          c = node->child(r); c.prefetch(); d = *(unsigned int*)&tNear[r]; stackPtr->ptr = c; stackPtr->dist = d; stackPtr++;
	  if (likely(mask == 0)) {
	    sort(stackPtr[-1],stackPtr[-2],stackPtr[-3],stackPtr[-4]);
	    cur = (NodeRef) stackPtr[-1].ptr; stackPtr--;
	    continue;
	  }

	  /*! fallback case if more than 4 children are hit */
	  while (1)
	  {
	    r = __bscf(mask);
	    assert(stackPtr < stackEnd);
	    c = node->child(r); c.prefetch(); d = *(unsigned int*)&tNear[r]; stackPtr->ptr = c; stackPtr->dist = d; stackPtr++;
	    if (unlikely(mask == 0)) break;
	  }
	  
	  cur = (NodeRef) stackPtr[-1].ptr; stackPtr--;
	}
        
        /* return if stack is empty */
        if (unlikely(cur == BVH8::invalidNode)) break;

        /*! this is a leaf node */
	assert(cur != BVH8::emptyNode);
        STAT3(normal.trav_leaves,1,1,1);
        size_t num; Triangle* prim = (Triangle*) cur.leaf(num);

        const float old_tfar = ray.tfar[k];

        size_t lazy_node = 0;
        PrimitiveIntersector8::intersect(pre,ray,k,prim,num,bvh->scene,lazy_node);
        if (unlikely(ray.tfar[k] < old_tfar))
        {
          StackItemT<NodeRef>* new_stackPtr = stack+1;        //!< current stack pointer            
          for (StackItemT<NodeRef>*s = stack+1; s != stackPtr; s++)
            if (unlikely(*(float*)&s->dist < ray.tfar[k]))
              *new_stackPtr++ = *s;
          stackPtr = new_stackPtr;
        }
        rayFar = ray.tfar[k];

      }
#endif
    }
   

    struct __aligned(64) Context 
    {
      static const size_t stackSizeSingle = 64;

      Context() {}

      __forceinline void init(BVH8::NodeRef &root,
                              StackItemT<BVH8::NodeRef> *contextStack) 
      { 
        current       = root; 
        stack         = contextStack;
        stack[0].ptr  = BVH8::invalidNode;
        stack[0].dist = inf;
        stack[1].ptr  = root;
        stack[1].dist = inf;
        stackPtr      = stack + 2;
      } 
      
      __forceinline BVH8::NodeRef pop()
      {
        stackPtr--;
        current = stackPtr->ptr;
        return current;
      }

      __forceinline BVH8::NodeRef getCurrent()
      {
        return current;
      }

      __forceinline void prefetchNext()
      {
        stackPtr[-1].ptr.prefetch();
      }
      
      __forceinline void resetMask()
      {
        hit_mask = 0;
      }

      __forceinline bool terminated()
      {
        return current == BVH8::invalidNode;
      }
      
      __forceinline void intersectNode(BVH8::NodeRef &cur,
                                       const Vec3f8 &rdir8,
                                       const Vec3f8 &org_rdir8,
                                       const float8 &tnear8,
                                       const float8 &tfar8,
                                       const size_t k)
      {

#if defined(__AVX2__)
        STAT3(normal.trav_nodes,1,1,1);
        const BVH8::Node* node = cur.node();

#if 0
        int8 index(k);
        const Vec3f8 rdir(permute(rdir8.x,index),permute(rdir8.y,index),permute(rdir8.z,index));
        const Vec3f8 org_rdir(permute(org_rdir8.x,index),permute(org_rdir8.y,index),permute(org_rdir8.z,index));
#else
        const Vec3f8 rdir(rdir8.x[k],rdir8.y[k],rdir8.z[k]);
        const Vec3f8 org_rdir(org_rdir8.x[k],org_rdir8.y[k],org_rdir8.z[k]);
#endif

        const float8 _tNearX = msub(node->lower_x, rdir.x, org_rdir.x);
        const float8 _tNearY = msub(node->lower_y, rdir.y, org_rdir.y);
        const float8 _tNearZ = msub(node->lower_z, rdir.z, org_rdir.z);
        const float8 _tFarX  = msub(node->upper_x, rdir.x, org_rdir.x);
        const float8 _tFarY  = msub(node->upper_y, rdir.y, org_rdir.y);
        const float8 _tFarZ  = msub(node->upper_z, rdir.z, org_rdir.z);

        const float8 inactive_inf = float8(pos_inf);
        const bool8  nactive      = inactive_inf == node->lower_x;

        const float8 tNearX  = mini(_tNearX,_tFarX);
        const float8 tNearY  = mini(_tNearY,_tFarY);
        const float8 tNearZ  = mini(_tNearZ,_tFarZ);

        const float8 tFarX   = maxi(_tNearX,_tFarX);
        const float8 tFarY   = maxi(_tNearY,_tFarY);
        const float8 tFarZ   = maxi(_tNearZ,_tFarZ);

        const float8 rayNear(tnear8[k]);
        const float8 rayFar(tfar8[k]);

        const float8 tNear   = maxi(maxi(tNearX,tNearY),maxi(tNearZ,rayNear));
        const float8 tFar    = mini(mini(tFarX ,tFarY ),mini(tFarZ ,rayFar ));
        const bool8 vmask    = nactive | cast(tNear) > cast(tFar);
        const size_t mask    = movemask(vmask)^0xff;
        dist        = tNear;
        hit_mask    = mask;
#endif
      }


      __forceinline void sortNodesDirect(BVH8::NodeRef &cur)
      {
        const BVH8::Node* node = cur.node();
        size_t mask = hit_mask;
        assert(current.isNode());

        if (unlikely(mask == 0)) return;

        /*! one child is hit, continue with that child */
        size_t r = __bscf(mask);
        if (likely(mask == 0)) {
          cur = node->child(r); 
          //cur.prefetch();
          assert(cur != BVH8::emptyNode);
          stackPtr->ptr = cur;
          stackPtr++;
          return;
        }
          
        /*! two children are hit, push far child, and continue with closer child */
        BVH8::NodeRef c0 = node->child(r); 
        //c0.prefetch(); 
        const unsigned int d0 = ((unsigned int*)&dist)[r];
        r = __bscf(mask);
        BVH8::NodeRef c1 = node->child(r); 
        //c1.prefetch(); 
        const unsigned int d1 = ((unsigned int*)&dist)[r];
        assert(c0 != BVH8::emptyNode);
        assert(c1 != BVH8::emptyNode);
        if (likely(mask == 0)) {
          assert(stackPtr < stackEnd); 
          if (d0 < d1) { stackPtr->ptr = c1; stackPtr->dist = d1; stackPtr++; stackPtr->ptr = c0; stackPtr->dist = d0; stackPtr++; return; }
          else         { stackPtr->ptr = c0; stackPtr->dist = d0; stackPtr++; stackPtr->ptr = c1; stackPtr->dist = d1; stackPtr++; return; }
        }
          
        /*! Here starts the slow path for 3 or 4 hit children. We push
         *  all nodes onto the stack to sort them there. */
        assert(stackPtr < stackEnd); 
        stackPtr->ptr = c0; stackPtr->dist = d0; stackPtr++;
        assert(stackPtr < stackEnd); 
        stackPtr->ptr = c1; stackPtr->dist = d1; stackPtr++;
        
        /*! three children are hit, push all onto stack and sort 3 stack items, continue with closest child */
        assert(stackPtr < stackEnd); 
        r = __bscf(mask);
          BVH8::NodeRef c = node->child(r); 
          //c.prefetch(); 
          unsigned int d = ((unsigned int*)&dist)[r]; stackPtr->ptr = c; stackPtr->dist = d; stackPtr++;
          assert(c != BVH8::emptyNode);
          if (likely(mask == 0)) {
            sort(stackPtr[-1],stackPtr[-2],stackPtr[-3]);
            return;
          }
          
	  /*! four children are hit, push all onto stack and sort 4 stack items, continue with closest child */
          r = __bscf(mask);
          c = node->child(r); 
          //c.prefetch(); 
          d = *(unsigned int*)&dist[r]; stackPtr->ptr = c; stackPtr->dist = d; stackPtr++;
	  if (likely(mask == 0)) {
	    sort(stackPtr[-1],stackPtr[-2],stackPtr[-3],stackPtr[-4]);
	    return;
	  }

	  /*! fallback case if more than 4 children are hit */
	  while (1)
	  {
	    r = __bscf(mask);
	    assert(stackPtr < stackEnd);
	    c = node->child(r); 
            //c.prefetch(); 
            d = *(unsigned int*)&dist[r]; stackPtr->ptr = c; stackPtr->dist = d; stackPtr++;
	    if (unlikely(mask == 0)) break;
	  }
      }

      __forceinline void sortNodes()
      {
        size_t mask = hit_mask;
          assert(current.isNode());
          const size_t hits = __popcnt(mask);
          const BVH8::Node* node = current.node();

	  while (1)
	  {
	    size_t r = __bscf(mask);
            BVH8::NodeRef c = node->child(r); 
            unsigned int d  = *(unsigned int*)&dist[r]; 
            stackPtr->ptr   = c; 
            stackPtr->dist  = d; 
            stackPtr++;
	    if (unlikely(mask == 0)) break;
	  }
          
          if (unlikely(hits >= 2))
          {
            if (likely(hits == 2))
              sort(stackPtr[-1],stackPtr[-2]);              
            else if (likely(hits == 3))
              sort(stackPtr[-1],stackPtr[-2],stackPtr[-3]);              
            else if (likely(hits == 4))
              sort(stackPtr[-1],stackPtr[-2],stackPtr[-3],stackPtr[-4]);              
          }                    
        
      }

      __forceinline void compactStack(const float max_dist)
      {
        StackItemT<BVH8::NodeRef>* new_stackPtr = stack+1;        //!< current stack pointer
        for (StackItemT<BVH8::NodeRef>*s = stack+1; s != stackPtr; s++)
          if (unlikely(*(float*)&s->dist < max_dist))
            *new_stackPtr++ = *s;
        stackPtr = new_stackPtr;        
      }


      /////////////////////////////////////////////////

      BVH8::NodeRef current;
      size_t hit_mask;
      StackItemT<BVH8::NodeRef>* stack;
      StackItemT<BVH8::NodeRef>* stackPtr;
      
      float8 dist;            
    };
    
    template<typename PrimitiveIntersector8>    
    void BVH8Intersector8Test<PrimitiveIntersector8>::intersect(bool8* valid_i, BVH8* bvh, Ray8& ray)
    {
      Context context[8];
      StackItemT<BVH8::NodeRef>  contextStack[8][stackSizeSingle];  //!< stack of nodes 
      
      /* load ray */
      bool8 valid0 = *valid_i;
#if defined(RTCORE_IGNORE_INVALID_RAYS)
      valid0 &= ray.valid();
#endif
      assert(all(valid0,ray.tnear > -FLT_MIN));
      Vec3f8 ray_org = ray.org;
      Vec3f8 ray_dir = ray.dir;
      float8 ray_tnear = ray.tnear, ray_tfar  = ray.tfar;

      const Vec3f8 rdir = rcp_safe(ray_dir);
      const Vec3f8 org(ray_org), org_rdir = org * rdir;
      ray_tnear = select(valid0,ray_tnear,float8(pos_inf));
      ray_tfar  = select(valid0,ray_tfar ,float8(neg_inf));
      const float8 inf = float8(pos_inf);
      Precalculations pre(valid0,ray);

      /* compute near/far per ray */
      Vec3i8 nearXYZ;
      nearXYZ.x = select(rdir.x >= 0.0f,int8(0*(int)sizeof(float8)),int8(1*(int)sizeof(float8)));
      nearXYZ.y = select(rdir.y >= 0.0f,int8(2*(int)sizeof(float8)),int8(3*(int)sizeof(float8)));
      nearXYZ.z = select(rdir.z >= 0.0f,int8(4*(int)sizeof(float8)),int8(5*(int)sizeof(float8)));

        /* switch to single ray traversal */
      const bool8 active = ray_tnear < ray_tfar;

#if !defined(__WIN32__) || defined(__X86_64__)
      size_t bits = movemask(active);
      for (size_t i=__bsf(bits); bits!=0; bits=__blsr(bits), i=__bsf(bits)) {
        intersect1(bvh,bvh->root,i,pre,ray,ray_org,ray_dir,rdir,ray_tnear,ray_tfar,nearXYZ);
      }
#else
      size_t g_active = movemask(active);
      size_t bits = g_active;
      for (size_t i=__bsf(bits); bits!=0; bits=__blsr(bits), i=__bsf(bits)) 
        context[i].init(bvh->root,contextStack[i]);

#define DBG_TEST(x) 

      while(g_active != 0)
      {
        size_t m_intersect = 0;
        for (size_t bits = g_active, i=__bsf(bits); bits!=0; bits=__blsr(bits), i=__bsf(bits)) 
        {
          DBG_TEST(PRINT(i));
          context[i].resetMask(); //
          BVH8::NodeRef cur = context[i].pop();
          if (unlikely(cur.isLeaf()))
          {
            const size_t m_slot = (size_t)1 << i;
            if (unlikely(cur == BVH8::invalidNode))
            {
              DBG_TEST(PRINT2("TERMINATED",i));
              g_active ^= m_slot;
              continue;
            }
            m_intersect |= m_slot;
            cur.prefetch();
          }
          else
          {
            DBG_TEST(PRINT(i));
            assert( cur != BVH8::invalidNode );
            DBG_TEST(PRINT("NODE"));
            context[i].intersectNode(cur,rdir,org_rdir,ray_tnear,ray_tfar,i);
            context[i].sortNodesDirect(cur);      
            context[i].prefetchNext();
          }
        }

        if (unlikely(g_active == 0)) break;

        for (size_t bits = m_intersect, i=__bsf(bits); bits!=0; bits=__blsr(bits), i=__bsf(bits)) 
        {
          BVH8::NodeRef cur = context[i].getCurrent();
          DBG_TEST(PRINT(i));
          assert( cur != BVH8::invalidNode );
          STAT3(normal.trav_leaves,1,1,1);
          DBG_TEST(PRINT("LEAF"));
          size_t num; 
          Triangle* prim = (Triangle*) cur.leaf(num);
          const float old_tfar = ray.tfar[i];
          size_t lazy_node = 0;
          PrimitiveIntersector8::intersect(pre,ray,i,prim,num,bvh->scene,lazy_node);
          if (unlikely(ray.tfar[i] < old_tfar))
              context[i].compactStack(ray.tfar[i]);
          
        }

#if 0
        DBG_TEST(PRINT("SORT"));
        for (size_t bits = m_traversal, i=__bsf(bits); bits!=0; bits=__btc(bits,i), i=__bsf(bits)) 
        {
          if (unlikely(context[i].hit_mask))
          {
            DBG_TEST(PRINT2("SORT",i));
            context[i].sortNodes();          
          }
          context[i].prefetchNext();
        }
#endif        
        
      }
#endif
      AVX_ZERO_UPPER();
    }
    


    template<typename PrimitiveIntersector8>
    __forceinline bool BVH8Intersector8Test<PrimitiveIntersector8>::occluded1(const BVH8* bvh, NodeRef root, const size_t k, Precalculations& pre, Ray8& ray,const Vec3f8 &ray_org, const Vec3f8 &ray_dir, const Vec3f8 &ray_rdir, const float8 &ray_tnear, const float8 &ray_tfar, const Vec3i8& nearXYZ)
    {
#if defined (__AVX2__)

      /*! stack state */
      NodeRef stack[stackSizeSingle];  //!< stack of nodes that still need to get traversed
      NodeRef* stackPtr = stack+1;        //!< current stack pointer
      NodeRef* stackEnd = stack+stackSizeSingle;
      stack[0]  = root;
      
      /*! offsets to select the side that becomes the lower or upper bound */
      const size_t nearX = nearXYZ.x[k];
      const size_t nearY = nearXYZ.y[k];
      const size_t nearZ = nearXYZ.z[k];
      
      /*! load the ray into SIMD registers */
      const Vec3f8 org (ray_org .x[k],ray_org .y[k],ray_org .z[k]);
      const Vec3f8 rdir(ray_rdir.x[k],ray_rdir.y[k],ray_rdir.z[k]);
      const Vec3f8 norg = -org, org_rdir(org*rdir);
      const float8 rayNear(ray_tnear[k]), rayFar(ray_tfar[k]); 

      /* pop loop */
      while (true) pop:
      {
        /*! pop next node */
        if (unlikely(stackPtr == stack)) break;
        stackPtr--;
        NodeRef cur = (NodeRef) *stackPtr;
        
        /* downtraversal loop */
        while (true)
        {
          /*! stop if we found a leaf */
          if (unlikely(cur.isLeaf())) break;
          STAT3(shadow.trav_nodes,1,1,1);
          
          /*! single ray intersection with 4 boxes */
          const Node* node = cur.node();

#if OLD_TRAVERSAL == 1
          const size_t farX  = nearX ^ sizeof(float8), farY  = nearY ^ sizeof(float8), farZ  = nearZ ^ sizeof(float8);
          const float8 _tNearX = msub(load8f((const char*)node+nearX), rdir.x, org_rdir.x);
          const float8 _tNearY = msub(load8f((const char*)node+nearY), rdir.y, org_rdir.y);
          const float8 _tNearZ = msub(load8f((const char*)node+nearZ), rdir.z, org_rdir.z);
          const float8 _tFarX  = msub(load8f((const char*)node+farX ), rdir.x, org_rdir.x);
          const float8 _tFarY  = msub(load8f((const char*)node+farY ), rdir.y, org_rdir.y);
          const float8 _tFarZ  = msub(load8f((const char*)node+farZ ), rdir.z, org_rdir.z);

          const float8 tNearX  = _tNearX;
          const float8 tNearY  = _tNearY;
          const float8 tNearZ  = _tNearZ;

          const float8 tFarX   = _tFarX;
          const float8 tFarY   = _tFarY;
          const float8 tFarZ   = _tFarZ;

          const float8 tNear = maxi(maxi(tNearX,tNearY),maxi(tNearZ,rayNear));
          const float8 tFar  = mini(mini(tFarX ,tFarY ),mini(tFarZ ,rayFar ));
          const bool8 vmask = cast(tNear) > cast(tFar);
          size_t mask = movemask(vmask)^0xff;

#else

          const float8 _tNearX = msub(node->lower_x, rdir.x, org_rdir.x);
          const float8 _tNearY = msub(node->lower_y, rdir.y, org_rdir.y);
          const float8 _tNearZ = msub(node->lower_z, rdir.z, org_rdir.z);
          const float8 _tFarX  = msub(node->upper_x, rdir.x, org_rdir.x);
          const float8 _tFarY  = msub(node->upper_y, rdir.y, org_rdir.y);
          const float8 _tFarZ  = msub(node->upper_z, rdir.z, org_rdir.z);

          const bool8  nactive = float8(pos_inf) == node->lower_x;
          const float8 tNearX  = mini(_tNearX,_tFarX);
          const float8 tNearY  = mini(_tNearY,_tFarY);
          const float8 tNearZ  = mini(_tNearZ,_tFarZ);

          const float8 tFarX   = maxi(_tNearX,_tFarX);
          const float8 tFarY   = maxi(_tNearY,_tFarY);
          const float8 tFarZ   = maxi(_tNearZ,_tFarZ);

          const float8 tNear = maxi(maxi(tNearX,tNearY),maxi(tNearZ,rayNear));
          const float8 tFar  = mini(mini(tFarX ,tFarY ),mini(tFarZ ,rayFar ));
          const bool8 vmask  = nactive | cast(tNear) > cast(tFar);
          size_t mask = movemask(vmask)^0xff;

#endif
          
          /*! if no child is hit, pop next node */
          if (unlikely(mask == 0))
            goto pop;
          
          /*! one child is hit, continue with that child */
          size_t r = __bscf(mask);
          if (likely(mask == 0)) {
            cur = node->child(r); cur.prefetch(); 
            assert(cur != BVH8::emptyNode);
            continue;
          }
          
          /*! two children are hit, push far child, and continue with closer child */
          NodeRef c0 = node->child(r); c0.prefetch(); const unsigned int d0 = ((unsigned int*)&tNear)[r];
          r = __bscf(mask);
          NodeRef c1 = node->child(r); c1.prefetch(); const unsigned int d1 = ((unsigned int*)&tNear)[r];
          assert(c0 != BVH8::emptyNode);
          assert(c1 != BVH8::emptyNode);
          if (likely(mask == 0)) {
            assert(stackPtr < stackEnd);
            if (d0 < d1) { *stackPtr = c1; stackPtr++; cur = c0; continue; }
            else         { *stackPtr = c0; stackPtr++; cur = c1; continue; }
          }
          assert(stackPtr < stackEnd);
          *stackPtr = c0; stackPtr++;
          assert(stackPtr < stackEnd);
          *stackPtr = c1; stackPtr++;
          
	  /*! three children are hit */
          r = __bscf(mask);
          cur = node->child(r); cur.prefetch(); *stackPtr = cur; stackPtr++;
          if (likely(mask == 0)) {
            stackPtr--;
            continue;
          }

	  /*! process more than three children */
	  while(1)
	  {
	    r = __bscf(mask);
	    NodeRef c = node->child(r); c.prefetch(); *stackPtr = c; stackPtr++;
	    if (unlikely(mask == 0)) break;
	  }
	  cur = (NodeRef) stackPtr[-1]; stackPtr--;
        }
        
        /*! this is a leaf node */
	assert(cur != BVH8::emptyNode);
        STAT3(shadow.trav_leaves,1,1,1);
        size_t num; Triangle* prim = (Triangle*) cur.leaf(num);

        size_t lazy_node = 0;
        if (PrimitiveIntersector8::occluded(pre,ray,k,prim,num,bvh->scene,lazy_node)) {
          //ray.geomID = 0;
          //break;
	  return true;
        }

        if (unlikely(lazy_node)) {
          *stackPtr = lazy_node;
          stackPtr++;
        }
      }
#endif
      return false;
    }

     template<typename PrimitiveIntersector8>
    void BVH8Intersector8Test<PrimitiveIntersector8>::occluded(bool8* valid_i, BVH8* bvh, Ray8& ray)
    {
      /* load ray */
      bool8 valid = *valid_i;
#if defined(RTCORE_IGNORE_INVALID_RAYS)
      valid &= ray.valid();
#endif
      assert(all(valid,ray.tnear > -FLT_MIN));
      bool8 terminated = !valid;
      Vec3f8 ray_org = ray.org, ray_dir = ray.dir;
      float8 ray_tnear = ray.tnear, ray_tfar  = ray.tfar;
      const Vec3f8 rdir = rcp_safe(ray_dir);
      const Vec3f8 org(ray_org), org_rdir = org * rdir;
      ray_tnear = select(valid,ray_tnear,float8(pos_inf));
      ray_tfar  = select(valid,ray_tfar ,float8(neg_inf));
      const float8 inf = float8(pos_inf);
      Precalculations pre(valid,ray);

      /* compute near/far per ray */
      Vec3i8 nearXYZ;
      nearXYZ.x = select(rdir.x >= 0.0f,int8(0*(int)sizeof(float8)),int8(1*(int)sizeof(float8)));
      nearXYZ.y = select(rdir.y >= 0.0f,int8(2*(int)sizeof(float8)),int8(3*(int)sizeof(float8)));
      nearXYZ.z = select(rdir.z >= 0.0f,int8(4*(int)sizeof(float8)),int8(5*(int)sizeof(float8)));

      /* allocate stack and push root node */
      float8    stack_near[stackSizeChunk];
      NodeRef stack_node[stackSizeChunk];
      stack_node[0] = BVH8::invalidNode;
      stack_near[0] = inf;
      stack_node[1] = bvh->root;
      stack_near[1] = ray_tnear; 
      NodeRef* stackEnd = stack_node+stackSizeChunk;
      NodeRef* __restrict__ sptr_node = stack_node + 2;
      float8*    __restrict__ sptr_near = stack_near + 2;

#if !defined(__WIN32__) || defined(__X86_64__)
      const bool8 active = ray_tnear < ray_tfar;
      size_t bits = movemask(active);
      for (size_t i=__bsf(bits); bits!=0; bits=__btc(bits,i), i=__bsf(bits)) {
        if (occluded1(bvh,bvh->root,i,pre,ray,ray_org,ray_dir,rdir,ray_tnear,ray_tfar,nearXYZ))
          terminated[i] = -1;
      }
#endif
      store8i(valid & terminated,&ray.geomID,0);
      AVX_ZERO_UPPER();
    }

    DEFINE_INTERSECTOR8(BVH8Triangle4Intersector8TestMoeller,BVH8Intersector8Test<ArrayIntersector8_1<TriangleNIntersectorMMoellerTrumbore<Ray8 COMMA Triangle4 COMMA true> > >);
    DEFINE_INTERSECTOR8(BVH8Triangle4Intersector8TestMoellerNoFilter,BVH8Intersector8Test<ArrayIntersector8_1<TriangleNIntersectorMMoellerTrumbore<Ray8 COMMA Triangle4 COMMA false> > >);
    
    DEFINE_INTERSECTOR8(BVH8Triangle8Intersector8TestMoeller,BVH8Intersector8Test<ArrayIntersector8_1<TriangleNIntersectorMMoellerTrumbore<Ray8 COMMA Triangle8 COMMA true> > >);
    DEFINE_INTERSECTOR8(BVH8Triangle8Intersector8TestMoellerNoFilter,BVH8Intersector8Test<ArrayIntersector8_1<TriangleNIntersectorMMoellerTrumbore<Ray8 COMMA Triangle8 COMMA false> > >);

    DEFINE_INTERSECTOR8(BVH8Triangle8vIntersector8TestPluecker, BVH8Intersector8Test<ArrayIntersector8_1<TriangleNvIntersectorMPluecker2<Ray8 COMMA Triangle8v COMMA true> > >);
    DEFINE_INTERSECTOR8(BVH8Triangle8vIntersector8TestPlueckerNoFilter, BVH8Intersector8Test<ArrayIntersector8_1<TriangleNvIntersectorMPluecker2<Ray8 COMMA Triangle8v COMMA false> > >);

  }
}  
