/*
* Copyright (c) 2017, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/
//!
//! \file      cm_surface_sampler.h  
//! \brief     Contains Class CmSurfaceSampler8x8  definitions  
//!
#pragma once

#include "cm_surface.h"

namespace CMRT_UMD
{
class CmSurfaceSampler : public CmSurface
{
public:
    static int32_t Create(uint32_t index, uint32_t handleFor2D3D, uint32_t indexForCurrent, SAMPLER_SURFACE_TYPE surfaceType, CmSurfaceManager* pSurfaceManager, CmSurfaceSampler* &pSurfaceSampler, CM_FLAG* pFlag);
    int32_t GetSurfaceIndex(SurfaceIndex*& pIndex);
    int32_t GetHandle( uint32_t& handle ); 
    int32_t GetSurfaceType(SAMPLER_SURFACE_TYPE& type);
    int32_t GetCmIndexCurrent( uint16_t & index ) ;
    int32_t SetMemoryObjectControl(MEMORY_OBJECT_CONTROL mem_ctrl, MEMORY_TYPE mem_type, uint32_t age);

    //NOT depend on RTTI::dynamic_cast
    CM_ENUM_CLASS_TYPE Type() const {return CM_ENUM_CLASS_TYPE_CMSURFACESAMPLER;};  

protected:
    CmSurfaceSampler(uint32_t indexForCurrent, uint32_t handleFor2D3D, SAMPLER_SURFACE_TYPE surfaceType, CmSurfaceManager* pSurfaceManager, CM_FLAG* pFlag);
    ~CmSurfaceSampler( void );

    int32_t Initialize( uint32_t index );
    uint32_t m_handleFor2D3D; 
    uint32_t m_CmIndexForCurrent; // SurfaceIndex's ID for 2D/3D, also indexing surface array

    SAMPLER_SURFACE_TYPE m_surfaceType; //0-Surface 2D, 1-Surface 3D
    CM_FLAG m_Flag;
};
};//namespace


