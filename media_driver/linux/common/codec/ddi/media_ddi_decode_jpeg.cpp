/*
* Copyright (c) 2009-2017, Intel Corporation
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
//! \file     media_ddi_decode_jpeg.cpp
//! \brief    The class implementation of DdiDecodeJPEG  for JPEG decode
//!
//!

#include <va/va_dec_jpeg.h>
#include "media_libva_decoder.h"
#include "media_libva_util.h"
#include "media_ddi_decode_jpeg.h"
#include "mos_solo_generic.h"
#include "codechal_decode_jpeg.h"
#include "media_ddi_decode_const.h"
#include "media_ddi_factory.h"

typedef enum _DDI_DECODE_JPEG_BUFFER_STATE
{
    BUFFER_UNLOADED = 0,
    BUFFER_LOADED   = 1,
} DDI_DECODE_JPEG_BUFFER_STATE;

#define DDI_DECODE_JPEG_MAXIMUM_HUFFMAN_TABLE    2
#define DDI_DECODE_JPEG_MAXIMUM_QMATRIX_NUM      4
#define DDI_DECODE_JPEG_MAXIMUM_QMATRIX_ENTRIES  64

#define DDI_DECODE_JPEG_SLICE_PARAM_BUF_NUM      0x4     //According to JPEG SPEC, max slice per frame is 4

static const uint32_t zigzag_order[64] =
{
    0,   1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

VAStatus DdiDecodeJPEG::ParseSliceParams(
    DDI_MEDIA_CONTEXT                   *mediaCtx,
    VASliceParameterBufferJPEGBaseline  *slcParam,
    int32_t                             numSlices)
{
    CodecDecodeJpegScanParameter *jpegSliceParam =
        (CodecDecodeJpegScanParameter *)(m_ddiDecodeCtx->DecodeParams.m_sliceParams);

    CodecDecodeJpegPicParams *picParam = (CodecDecodeJpegPicParams *)(m_ddiDecodeCtx->DecodeParams.m_picParams);

    if ((jpegSliceParam == nullptr) ||
        (picParam == nullptr) ||
        (slcParam == nullptr))
    {
        DDI_ASSERTMESSAGE("Invalid Parameter for Parsing JPEG Slice parameter\n");
        return VA_STATUS_ERROR_INVALID_PARAMETER;
    }

    jpegSliceParam->NumScans += numSlices;
    picParam->m_totalScans += numSlices;
    if (picParam->m_totalScans == 1 && slcParam[0].num_components > 1)
    {
        picParam->m_interleavedData = 1;
    }

    int32_t j, i;
    int32_t startIdx = m_numScans;
    for (j = 0; j < numSlices; j++)
    {
        for (i = 0; i < slcParam[j].num_components; i++)
        {
            jpegSliceParam->ScanHeader[j + startIdx].ComponentSelector[i] = slcParam[j].components[i].component_selector;
            jpegSliceParam->ScanHeader[j + startIdx].DcHuffTblSelector[i] = slcParam[j].components[i].dc_table_selector;
            jpegSliceParam->ScanHeader[j + startIdx].AcHuffTblSelector[i] = slcParam[j].components[i].ac_table_selector;
        }
        jpegSliceParam->ScanHeader[j + startIdx].NumComponents    = slcParam[j].num_components;
        jpegSliceParam->ScanHeader[j + startIdx].RestartInterval  = slcParam[j].restart_interval;
        jpegSliceParam->ScanHeader[j + startIdx].MCUCount         = slcParam[j].num_mcus;
        jpegSliceParam->ScanHeader[j + startIdx].ScanHoriPosition = slcParam[j].slice_horizontal_position;
        jpegSliceParam->ScanHeader[j + startIdx].ScanVertPosition = slcParam[j].slice_vertical_position;
        jpegSliceParam->ScanHeader[j + startIdx].DataOffset       = slcParam[j].slice_data_offset;
        jpegSliceParam->ScanHeader[j + startIdx].DataLength       = slcParam[j].slice_data_size;
    }

    return VA_STATUS_SUCCESS;
}

VAStatus DdiDecodeJPEG::ParsePicParams(
    DDI_MEDIA_CONTEXT                    *mediaCtx,
    VAPictureParameterBufferJPEGBaseline *picParam)
{
    CodecDecodeJpegPicParams *jpegPicParam = (CodecDecodeJpegPicParams *)(m_ddiDecodeCtx->DecodeParams.m_picParams);

    if ((jpegPicParam == nullptr) ||
        (picParam == nullptr))
    {
        DDI_ASSERTMESSAGE("Null Parameter for Parsing JPEG Picture parameter\n");
        return VA_STATUS_ERROR_INVALID_PARAMETER;
    }

    jpegPicParam->m_frameWidth     = picParam->picture_width;
    jpegPicParam->m_frameHeight    = picParam->picture_height;
    jpegPicParam->m_numCompInFrame = picParam->num_components;

    switch (picParam->rotation)
    {
    case VA_ROTATION_NONE:
        jpegPicParam->m_rotation = jpegRotation0;
        break;
    case VA_ROTATION_90:
        jpegPicParam->m_rotation = jpegRotation90;
        break;
    case VA_ROTATION_180:
        jpegPicParam->m_rotation = jpegRotation180;
        break;
    case VA_ROTATION_270:
        jpegPicParam->m_rotation = jpegRotation270;
        break;
    default:
        /* For the other rotation type, the rotation is disabled. */
        jpegPicParam->m_rotation = jpegRotation0;
        break;
        ;
    }

    if (jpegPicParam->m_numCompInFrame == 1)
    {
        jpegPicParam->m_chromaType = jpegYUV400;
    }
    else if (jpegPicParam->m_numCompInFrame == 3)
    {
        int32_t h1 = picParam->components[0].h_sampling_factor;
        int32_t h2 = picParam->components[1].h_sampling_factor;
        int32_t h3 = picParam->components[2].h_sampling_factor;
        int32_t v1 = picParam->components[0].v_sampling_factor;
        int32_t v2 = picParam->components[1].v_sampling_factor;
        int32_t v3 = picParam->components[2].v_sampling_factor;

        if (h1 == 2 && h2 == 1 && h3 == 1 &&
            v1 == 2 && v2 == 1 && v3 == 1)
        {
            jpegPicParam->m_chromaType = jpegYUV420;
        }
        else if (h1 == 2 && h2 == 1 && h3 == 1 &&
                 v1 == 1 && v2 == 1 && v3 == 1)
        {
            jpegPicParam->m_chromaType = jpegYUV422H2Y;
        }
        else if (h1 == 1 && h2 == 1 && h3 == 1 &&
                 v1 == 1 && v2 == 1 && v3 == 1)
        {
            switch (picParam->color_space)
            {
            case 0:  //YUV
                jpegPicParam->m_chromaType = jpegYUV444;
                break;
            case 1:  //RGB
                jpegPicParam->m_chromaType = jpegRGB;
                break;
            case 2:  //BGR
                jpegPicParam->m_chromaType = jpegBGR;
                break;
            default:
                /* For the other type, the default YUV444 is used */
                jpegPicParam->m_chromaType = jpegYUV444;
                break;
                ;
            }
        }
        else if (h1 == 4 && h2 == 1 && h3 == 1 &&
                 v1 == 1 && v2 == 1 && v3 == 1)
        {
            jpegPicParam->m_chromaType = jpegYUV411;
        }
        else if (h1 == 1 && h2 == 1 && h3 == 1 &&
                 v1 == 2 && v2 == 1 && v3 == 1)
        {
            jpegPicParam->m_chromaType = jpegYUV422V2Y;
        }
        else if (h1 == 2 && h2 == 1 && h3 == 1 &&
                 v1 == 2 && v2 == 2 && v3 == 2)
        {
            jpegPicParam->m_chromaType = jpegYUV422H4Y;
        }
        else if (h1 == 2 && h2 == 2 && h3 == 2 &&
                 v1 == 2 && v2 == 1 && v3 == 1)
        {
            jpegPicParam->m_chromaType = jpegYUV422V4Y;
        }
        else
        {
            DDI_NORMALMESSAGE("Unsupported sampling factor in JPEG Picture  parameter\n");
            return VA_STATUS_ERROR_INVALID_PARAMETER;
        }
    }

    memset(jpegPicParam->m_componentIdentifier, 0, jpegNumComponent);
    memset(jpegPicParam->m_quantTableSelector, 0, jpegNumComponent);

    if (picParam->num_components > jpegNumComponent)
    {
        DDI_NORMALMESSAGE("Unsupported component num in JPEG Picture  parameter\n");
        return VA_STATUS_ERROR_INVALID_PARAMETER;
    }
    for (int32_t i = 0; i < picParam->num_components; i++)
    {
        jpegPicParam->m_componentIdentifier[i] = picParam->components[i].component_id;
        jpegPicParam->m_quantTableSelector[i]  = picParam->components[i].quantiser_table_selector;
    }

    return VA_STATUS_SUCCESS;
}

VAStatus DdiDecodeJPEG::ParseHuffmanTbl(
    DDI_MEDIA_CONTEXT *               mediaCtx,
    VAHuffmanTableBufferJPEGBaseline *huffmanTbl)
{
    PCODECHAL_DECODE_JPEG_HUFFMAN_TABLE jpegHuffTbl = (PCODECHAL_DECODE_JPEG_HUFFMAN_TABLE)(m_ddiDecodeCtx->DecodeParams.m_huffmanTable);

    if ((jpegHuffTbl == nullptr) ||
        (huffmanTbl == nullptr))
    {
        DDI_ASSERTMESSAGE("Null Parameter for Parsing JPEG Huffman Tableparameter\n");
        return VA_STATUS_ERROR_INVALID_PARAMETER;
    }

    memset(jpegHuffTbl, 0, sizeof(CODECHAL_DECODE_JPEG_HUFFMAN_TABLE));

    for (int32_t i = 0; i < DDI_DECODE_JPEG_MAXIMUM_HUFFMAN_TABLE; i++)
    {
        if (huffmanTbl->load_huffman_table[i] == BUFFER_LOADED)
        {
            //the size of jpegHuffTbl->HuffTable[i].DC_BITS is 12 (defined in driver)
            //the size of huffmanTbl->huffman_table[i].num_dc_codes is 16 (defined in libva)
            //it is using the size of "DC_BITS" for solve the overflow
            MOS_SecureMemcpy(jpegHuffTbl->HuffTable[i].DC_BITS,
                sizeof(jpegHuffTbl->HuffTable[i].DC_BITS),
                huffmanTbl->huffman_table[i].num_dc_codes,
                sizeof(jpegHuffTbl->HuffTable[i].DC_BITS));
            MOS_SecureMemcpy(jpegHuffTbl->HuffTable[i].DC_HUFFVAL,
                sizeof(jpegHuffTbl->HuffTable[i].DC_HUFFVAL),
                huffmanTbl->huffman_table[i].dc_values,
                sizeof(huffmanTbl->huffman_table[i].dc_values));
            MOS_SecureMemcpy(jpegHuffTbl->HuffTable[i].AC_BITS,
                sizeof(jpegHuffTbl->HuffTable[i].AC_BITS),
                huffmanTbl->huffman_table[i].num_ac_codes,
                sizeof(huffmanTbl->huffman_table[i].num_ac_codes));
            MOS_SecureMemcpy(jpegHuffTbl->HuffTable[i].AC_HUFFVAL,
                sizeof(jpegHuffTbl->HuffTable[i].AC_HUFFVAL),
                huffmanTbl->huffman_table[i].ac_values,
                sizeof(huffmanTbl->huffman_table[i].ac_values));
        }
    }

    return VA_STATUS_SUCCESS;
}

/////////////////////////////////////////////////////////////////////////////////////////
//
//   Function:    JpegQMatrixDecode
//   Description: Parse the QMatrix table from VAAPI, and load the valid Qmatrix to the Buffer used by 
//                    CodecHal
//
/////////////////////////////////////////////////////////////////////////////////////////
VAStatus DdiDecodeJPEG::ParseIQMatrix(
        DDI_MEDIA_CONTEXT            *mediaCtx,
        VAIQMatrixBufferJPEGBaseline *matrix)
{
    CodecJpegQuantMatrix *jpegQMatrix = (CodecJpegQuantMatrix *)(m_ddiDecodeCtx->DecodeParams.m_iqMatrixBuffer);

    if ((matrix == nullptr) || (jpegQMatrix == nullptr))
    {
        DDI_ASSERTMESSAGE("Null Parameter for Parsing JPEG IQMatrix \n");
        return VA_STATUS_ERROR_INVALID_PARAMETER;
    }

    memset(jpegQMatrix, 0, sizeof(CodecJpegQuantMatrix));

    int32_t idx, idx2;
    for (idx = 0; idx < DDI_DECODE_JPEG_MAXIMUM_QMATRIX_NUM; idx++)
    {
        if (matrix->load_quantiser_table[idx] == BUFFER_LOADED)
        {
            for (idx2 = 0; idx2 < DDI_DECODE_JPEG_MAXIMUM_QMATRIX_ENTRIES; idx2++)
            {
                jpegQMatrix->m_quantMatrix[idx][zigzag_order[idx2]] = matrix->quantiser_table[idx][idx2];
            }
        }
    }

    return VA_STATUS_SUCCESS;
}

VAStatus DdiDecodeJPEG::SetBufferRendered(VABufferID bufferID)
{
    DDI_CODEC_COM_BUFFER_MGR *bufMgr = &(m_ddiDecodeCtx->BufMgr);

    if (bufMgr == nullptr)
    {
        DDI_ASSERTMESSAGE("Null Parameter for Parsing Data buffer for JPEG\n");
        return VA_STATUS_ERROR_INVALID_PARAMETER;
    }

    bool renderFlag = false;
    for (int32_t i = 0; i < bufMgr->dwNumSliceData; i++)
    {
        // Depend on the ID we tracked, if application want to rendered one of them
        // we set some flags
        if (bufMgr->pSliceData[i].vaBufferId == bufferID)
        {
            if (bufMgr->pSliceData[i].bRendered == false)
            {
                // set the rendered flags. which will be used when EndPicture.
                bufMgr->pSliceData[i].bRendered = true;
                // calculate the size of a bunch of rendered buffers. for endpicture to allocate appropriate memory size of GPU.
                bufMgr->dwSizeOfRenderedSliceData += bufMgr->pSliceData[i].uiLength;
                // keep record the render order, so that we can calculate the offset correctly when endpicture.
                // in this array we save the buffer index in pSliceData which render in sequence. if application create buffers like: 4,3,5,1,2.
                // but only render 2,3,5. the content in this array is: [4,1,2], which is the index of created buffer.
                bufMgr->pRenderedOrder[bufMgr->dwNumOfRenderedSliceData] = i;
                bufMgr->dwNumOfRenderedSliceData++;
            }
            renderFlag = true;
            break;
        }
    }

    if (renderFlag)
    {
        return VA_STATUS_SUCCESS;
    }
    else
    {
        return VA_STATUS_ERROR_INVALID_BUFFER;
    }
}

VAStatus DdiDecodeJPEG::RenderPicture(
    VADriverContextP ctx,
    VAContextID      context,
    VABufferID       *buffers,
    int32_t          numBuffers)
{
    DDI_FUNCTION_ENTER();

    VAStatus           vaStatus = VA_STATUS_SUCCESS;
    PDDI_MEDIA_CONTEXT mediaCtx = DdiMedia_GetMediaContext(ctx);

    DDI_MEDIA_BUFFER  *buf  = nullptr;
    void              *data = nullptr;
    uint32_t          dataSize;
    for (int32_t i = 0; i < numBuffers; i++)
    {
        if (!buffers || (buffers[i] == VA_INVALID_ID))
        {
            return VA_STATUS_ERROR_INVALID_BUFFER;
        }

        buf = DdiMedia_GetBufferFromVABufferID(mediaCtx, buffers[i]);
        if (nullptr == buf)
        {
            return VA_STATUS_ERROR_INVALID_BUFFER;
        }

        dataSize = buf->iSize;
        DdiMedia_MapBuffer(ctx, buffers[i], &data);

        if (data == nullptr)
        {
            return VA_STATUS_ERROR_INVALID_BUFFER;
        }

        switch ((int32_t)buf->uiType)
        {
        case VASliceDataBufferType:
        {
            vaStatus = SetBufferRendered(buffers[i]);
            if (vaStatus != VA_STATUS_SUCCESS)
            {
                return VA_STATUS_ERROR_INVALID_BUFFER;
            }
            m_ddiDecodeCtx->DecodeParams.m_dataSize += dataSize;
            break;
        }
        case VASliceParameterBufferType:
        {
            if (buf->iNumElements == 0)
            {
                return VA_STATUS_ERROR_INVALID_BUFFER;
            }

            int32_t numSlices = buf->iNumElements;

            if ((m_numScans + numSlices) > jpegNumComponent)
            {
                DDI_NORMALMESSAGE("the total number of JPEG scans are beyond the supported num(4)\n");
                return VA_STATUS_ERROR_INVALID_PARAMETER;
            }

            vaStatus = AllocSliceParamContext(numSlices);
            if (vaStatus != VA_STATUS_SUCCESS)
            {
                return vaStatus;
            }

            VASliceParameterBufferJPEGBaseline *slcInfo = (VASliceParameterBufferJPEGBaseline *)data;
            vaStatus                                    = ParseSliceParams(mediaCtx, slcInfo, numSlices);
            if (vaStatus != VA_STATUS_SUCCESS)
            {
                return VA_STATUS_ERROR_INVALID_BUFFER;
            }

            m_ddiDecodeCtx->BufMgr.pNumOfRenderedSliceParaForOneBuffer[m_ddiDecodeCtx->BufMgr.dwNumOfRenderedSlicePara] = numSlices;
            m_ddiDecodeCtx->BufMgr.dwNumOfRenderedSlicePara ++;

            m_ddiDecodeCtx->DecodeParams.m_numSlices += numSlices;
            m_numScans += numSlices;
            m_ddiDecodeCtx->m_groupIndex++;
            break;
        }
        case VAIQMatrixBufferType:
        {
            VAIQMatrixBufferJPEGBaseline *imxBuf = (VAIQMatrixBufferJPEGBaseline *)data;
            vaStatus                             = ParseIQMatrix(mediaCtx, imxBuf);
            if (vaStatus != VA_STATUS_SUCCESS)
            {
                return VA_STATUS_ERROR_INVALID_BUFFER;
            }
            break;
        }
        case VAPictureParameterBufferType:
        {
            VAPictureParameterBufferJPEGBaseline *picParam = (VAPictureParameterBufferJPEGBaseline *)data;

            if (ParsePicParams(mediaCtx, picParam) != VA_STATUS_SUCCESS)
            {
                return VA_STATUS_ERROR_INVALID_BUFFER;
            }
            break;
        }
        case VAHuffmanTableBufferType:
        {
            VAHuffmanTableBufferJPEGBaseline *huffTbl = (VAHuffmanTableBufferJPEGBaseline *)data;
            vaStatus                                  = ParseHuffmanTbl(mediaCtx, huffTbl);
            if (vaStatus != VA_STATUS_SUCCESS)
            {
                return VA_STATUS_ERROR_INVALID_BUFFER;
            }
            break;
        }
        case VAProcPipelineParameterBufferType:
        {
            DDI_NORMALMESSAGE("ProcPipeline is not supported for JPEGBaseline decoding\n");
            break;
        }
        case VADecodeStreamoutBufferType:
        {
            DdiMedia_MediaBufferToMosResource(buf, &m_ddiDecodeCtx->BufMgr.resExternalStreamOutBuffer);
            m_ddiDecodeCtx->bStreamOutEnabled = true;
            break;
        }
        default:
            vaStatus = VA_STATUS_ERROR_UNSUPPORTED_BUFFERTYPE;
            break;
        }
        DdiMedia_UnmapBuffer(ctx, buffers[i]);
    }

    DDI_FUNCTION_EXIT(vaStatus);
    return vaStatus;
}

VAStatus DdiDecodeJPEG::BeginPicture(
    VADriverContextP ctx,
    VAContextID      context,
    VASurfaceID      renderTarget)
{
    VAStatus vaStatus = DdiMediaDecode::BeginPicture(ctx, context, renderTarget);

    if (vaStatus != VA_STATUS_SUCCESS)
    {
        return vaStatus;
    }

    if (m_jpegBitstreamBuf)
    {
        DdiMediaUtil_FreeBuffer(m_jpegBitstreamBuf);
        MOS_FreeMemory(m_jpegBitstreamBuf);
        m_jpegBitstreamBuf = nullptr;
    }

    CodecDecodeJpegScanParameter *jpegSliceParam =
        (CodecDecodeJpegScanParameter *)(m_ddiDecodeCtx->DecodeParams.m_sliceParams);
    jpegSliceParam->NumScans = 0;

    CodecDecodeJpegPicParams *picParam = (CodecDecodeJpegPicParams *)(m_ddiDecodeCtx->DecodeParams.m_picParams);
    picParam->m_totalScans             = 0;

    m_numScans = 0;
    return vaStatus;
}
/*
 * Make the end of render JPEG picture
 *
 *
 */
VAStatus DdiDecodeJPEG::EndPicture (
    VADriverContextP    ctx,
    VAContextID         context)
{
    DDI_FUNCTION_ENTER();

    VAStatus vaStatus = VA_STATUS_SUCCESS;
    /* the default CtxType is DECODER */
    m_ctxType = DDI_MEDIA_CONTEXT_TYPE_DECODER;

    DDI_CODEC_RENDER_TARGET_TABLE *rtTbl = &(m_ddiDecodeCtx->RTtbl);

    if ((rtTbl == nullptr) || (rtTbl->pCurrentRT == nullptr))
    {
        return VA_STATUS_ERROR_INVALID_PARAMETER;
    }

    /* skip the mediaCtx check as it is checked in caller */
    PDDI_MEDIA_CONTEXT mediaCtx = DdiMedia_GetMediaContext(ctx);

    DDI_CODEC_COM_BUFFER_MGR *bufMgr = &(m_ddiDecodeCtx->BufMgr);

    // we do not support mismatched usecase.
    if ((bufMgr->dwNumOfRenderedSlicePara != bufMgr->dwNumOfRenderedSliceData) ||
        (bufMgr->dwNumOfRenderedSlicePara == 0))
    {
        DDI_NORMALMESSAGE("DDI: Unsupported buffer mismatch usage!\n");
        return VA_STATUS_ERROR_INVALID_PARAMETER;
    }

    // Allocate GPU Buffer description keeper.
    m_jpegBitstreamBuf = (DDI_MEDIA_BUFFER *)MOS_AllocAndZeroMemory(sizeof(DDI_MEDIA_BUFFER));
    if (m_jpegBitstreamBuf == nullptr)
    {
        DDI_ASSERTMESSAGE("DDI: Allocate  Jpeg Media Buffer Failed!\n");
        return VA_STATUS_ERROR_UNKNOWN;
    }

    m_jpegBitstreamBuf->iSize        = bufMgr->dwSizeOfRenderedSliceData;
    m_jpegBitstreamBuf->iNumElements = bufMgr->dwNumOfRenderedSliceData;
    m_jpegBitstreamBuf->uiType       = VASliceDataBufferType;
    m_jpegBitstreamBuf->format       = Media_Format_Buffer;
    m_jpegBitstreamBuf->uiOffset     = 0;
    m_jpegBitstreamBuf->bCFlushReq   = false;
    m_jpegBitstreamBuf->pMediaCtx    = mediaCtx;

    // Create GPU buffer
    vaStatus = DdiMediaUtil_CreateBuffer(m_jpegBitstreamBuf, mediaCtx->pDrmBufMgr);
    if (vaStatus != VA_STATUS_SUCCESS)
    {
        DdiMediaUtil_FreeBuffer(m_jpegBitstreamBuf);
        MOS_FreeMemory(m_jpegBitstreamBuf);
        m_jpegBitstreamBuf = nullptr;
        return vaStatus;
    }

    // For the first time you call DdiMediaUtil_LockBuffer for a fresh GPU memory, it will map GPU address to a virtual address.
    // and then, DdiMediaUtil_LockBuffer is acutally to increase the reference count. so we used here for 2 resaons:
    //
    // 1. Map GPU address to virtual at the beginning when we combine the CPU -> GPU.
    uint8_t *mappedBuf = (uint8_t *)DdiMediaUtil_LockBuffer(m_jpegBitstreamBuf, MOS_LOCKFLAG_WRITEONLY);

    if (mappedBuf == nullptr)
    {
        DdiMediaUtil_FreeBuffer(m_jpegBitstreamBuf);
        MOS_FreeMemory(m_jpegBitstreamBuf);
        m_jpegBitstreamBuf = nullptr;
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }

    // get the JPEG Slice Header Params for offset recaculated.
    CodecDecodeJpegScanParameter *sliceParam =
        (CodecDecodeJpegScanParameter *)(m_ddiDecodeCtx->DecodeParams.m_sliceParams);
    int32_t  renderedBufIdx;
    int32_t  i, j;
    uint32_t bufOffset      = 0;
    int32_t  orderSlicePara = 0;
    int32_t  orderSliceData = 0;
    for (i = 0; i < bufMgr->dwNumOfRenderedSliceData; i++)
    {
        // get the rendered slice data index in rendered order.
        renderedBufIdx = bufMgr->pRenderedOrder[i];
        if (bufMgr->pSliceData[renderedBufIdx].bRendered)
        {
            MOS_SecureMemcpy((void *)(mappedBuf + bufOffset),
                bufMgr->pSliceData[renderedBufIdx].uiLength,
                bufMgr->pSliceData[renderedBufIdx].pBaseAddress,
                bufMgr->pSliceData[renderedBufIdx].uiLength);

            // since we assume application must make sure ONE slice parameter buffer ONE slice data buffer, so we recaculate header offset here.
            for (j = 0; j < bufMgr->pNumOfRenderedSliceParaForOneBuffer[orderSliceData]; j++)
            {
                sliceParam->ScanHeader[orderSlicePara].DataOffset += bufOffset;
                orderSlicePara++;
            }
            bufOffset += bufMgr->pSliceData[renderedBufIdx].uiLength;
            bufMgr->pNumOfRenderedSliceParaForOneBuffer[orderSliceData] = 0;
            orderSliceData++;
            bufMgr->pSliceData[renderedBufIdx].bRendered = false;
        }
    }
    DdiMediaUtil_UnlockBuffer(m_jpegBitstreamBuf);
    DdiMedia_MediaBufferToMosResource(m_jpegBitstreamBuf, &(bufMgr->resBitstreamBuffer));
    bufMgr->dwNumOfRenderedSliceData  = 0;
    bufMgr->dwNumOfRenderedSlicePara  = 0;
    bufMgr->dwSizeOfRenderedSliceData = 0;

    CodechalDecodeParams *decodeParams = &m_ddiDecodeCtx->DecodeParams;

    MOS_SURFACE destSurface;
    memset(&destSurface, 0, sizeof(MOS_SURFACE));
    destSurface.dwOffset = 0;
    destSurface.Format   = Format_NV12;

    DdiMedia_MediaSurfaceToMosResource(rtTbl->pCurrentRT, &(destSurface.OsResource));

    decodeParams->m_destSurface = &destSurface;

    decodeParams->m_deblockSurface = nullptr;

    decodeParams->m_dataBuffer       = &bufMgr->resBitstreamBuffer;
    decodeParams->m_bitStreamBufData = bufMgr->pBitstreamBuffer;
    Mos_Solo_OverrideBufferSize(decodeParams->m_dataSize, decodeParams->m_dataBuffer);

    decodeParams->m_bitplaneBuffer = nullptr;

    if (m_ddiDecodeCtx->bStreamOutEnabled)
    {
        decodeParams->m_streamOutEnabled        = true;
        decodeParams->m_externalStreamOutBuffer = &bufMgr->resExternalStreamOutBuffer;
    }
    else
    {
        decodeParams->m_streamOutEnabled        = false;
        decodeParams->m_externalStreamOutBuffer = nullptr;
    }

    DDI_CHK_RET(ClearRefList(rtTbl, true), "ClearRefList failed!");

    if (m_ddiDecodeCtx->pCodecHal == nullptr)
    {
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }

    MOS_STATUS eStatus = m_ddiDecodeCtx->pCodecHal->Execute((void*)(decodeParams));
    if (eStatus != MOS_STATUS_SUCCESS)
    {
        DDI_ASSERTMESSAGE("DDI:DdiDecode_DecodeInCodecHal return failure.");
        return VA_STATUS_ERROR_DECODING_ERROR;
    }

    rtTbl->pCurrentRT = nullptr;

    eStatus = m_ddiDecodeCtx->pCodecHal->EndFrame();

    if (eStatus != MOS_STATUS_SUCCESS)
    {
        return VA_STATUS_ERROR_DECODING_ERROR;
    }

    DDI_FUNCTION_EXIT(VA_STATUS_SUCCESS);
    return VA_STATUS_SUCCESS;
}

VAStatus DdiDecodeJPEG::AllocSliceParamContext(
    int32_t numSlices)
{
    uint32_t baseSize = sizeof(CodecDecodeJpegScanParameter);

    if (m_ddiDecodeCtx->dwSliceParamBufNum < (m_ddiDecodeCtx->DecodeParams.m_numSlices + numSlices))
    {
        // in order to avoid that the buffer is reallocated multi-times,
        // extra 10 slices are added.
        int32_t extraSlices = numSlices + 10;

        m_ddiDecodeCtx->DecodeParams.m_sliceParams = realloc(m_ddiDecodeCtx->DecodeParams.m_sliceParams,
            baseSize * (m_ddiDecodeCtx->dwSliceParamBufNum + extraSlices));

        if (m_ddiDecodeCtx->DecodeParams.m_sliceParams == nullptr)
        {
            return VA_STATUS_ERROR_ALLOCATION_FAILED;
        }

        memset((void *)((uint8_t *)m_ddiDecodeCtx->DecodeParams.m_sliceParams + baseSize * m_ddiDecodeCtx->dwSliceParamBufNum), 0, baseSize * extraSlices);
        m_ddiDecodeCtx->dwSliceParamBufNum += extraSlices;
    }

    return VA_STATUS_SUCCESS;
}

void DdiDecodeJPEG::DestroyContext(
    VADriverContextP ctx)
{
    FreeResourceBuffer();
    // explicitly call the base function to do the further clean-up
    DdiMediaDecode::DestroyContext(ctx);
}

void DdiDecodeJPEG::ContextInit(
    int32_t picWidth,
    int32_t picHeight)
{
    // call the function in base class to initialize it.
    DdiMediaDecode::ContextInit(picWidth, picHeight);

    m_ddiDecodeCtx->wMode    = CODECHAL_DECODE_MODE_JPEG;
    m_ddiDecodeCtx->Standard = CODECHAL_JPEG;
}

VAStatus DdiDecodeJPEG::InitResourceBuffer()
{
    VAStatus                  vaStatus = VA_STATUS_SUCCESS;
    DDI_CODEC_COM_BUFFER_MGR *bufMgr   = &(m_ddiDecodeCtx->BufMgr);
    bufMgr->pSliceData                 = nullptr;

    bufMgr->ui64BitstreamOrder = 0;
    bufMgr->dwMaxBsSize        = m_ddiDecodeCtx->dwWidth *
                          m_ddiDecodeCtx->dwHeight * 3 / 2;

    bufMgr->dwNumSliceData    = 0;
    bufMgr->dwNumSliceControl = 0;

    m_ddiDecodeCtx->dwSliceCtrlBufNum = DDI_DECODE_JPEG_SLICE_PARAM_BUF_NUM;
    bufMgr->m_maxNumSliceData         = DDI_DECODE_JPEG_SLICE_PARAM_BUF_NUM;
    bufMgr->pSliceData                = (DDI_CODEC_BITSTREAM_BUFFER_INFO *)MOS_AllocAndZeroMemory(sizeof(bufMgr->pSliceData[0]) * DDI_DECODE_JPEG_SLICE_PARAM_BUF_NUM);
    if (bufMgr->pSliceData == nullptr)
    {
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        goto finish;
    }
    bufMgr->dwNumOfRenderedSlicePara                         = 0;
    bufMgr->dwNumOfRenderedSliceData                         = 0;
    bufMgr->pNumOfRenderedSliceParaForOneBuffer              = (int32_t *)MOS_AllocAndZeroMemory(sizeof(bufMgr->pNumOfRenderedSliceParaForOneBuffer[0]) * m_ddiDecodeCtx->dwSliceCtrlBufNum);
    bufMgr->pRenderedOrder                                   = (int32_t *)MOS_AllocAndZeroMemory(sizeof(bufMgr->pRenderedOrder[0]) * m_ddiDecodeCtx->dwSliceCtrlBufNum);
    bufMgr->Codec_Param.Codec_Param_JPEG.pVASliceParaBufJPEG = (VASliceParameterBufferJPEGBaseline *)MOS_AllocAndZeroMemory(sizeof(VASliceParameterBufferJPEGBaseline) * m_ddiDecodeCtx->dwSliceCtrlBufNum);
    if (bufMgr->Codec_Param.Codec_Param_JPEG.pVASliceParaBufJPEG == nullptr)
    {
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        goto finish;
    }

    return VA_STATUS_SUCCESS;

finish:
    FreeResourceBuffer();
    return vaStatus;
}

void DdiDecodeJPEG::FreeResourceBuffer()
{
    DDI_CODEC_COM_BUFFER_MGR *bufMgr = &(m_ddiDecodeCtx->BufMgr);

    if (bufMgr->Codec_Param.Codec_Param_JPEG.pVASliceParaBufJPEG)
    {
        MOS_FreeMemory(bufMgr->Codec_Param.Codec_Param_JPEG.pVASliceParaBufJPEG);
        bufMgr->Codec_Param.Codec_Param_JPEG.pVASliceParaBufJPEG = nullptr;
    }
    bufMgr->dwNumOfRenderedSlicePara = 0;
    bufMgr->dwNumOfRenderedSliceData = 0;
    MOS_FreeMemory(bufMgr->pNumOfRenderedSliceParaForOneBuffer);
    bufMgr->pNumOfRenderedSliceParaForOneBuffer = nullptr;
    MOS_FreeMemory(bufMgr->pRenderedOrder);
    bufMgr->pRenderedOrder = nullptr;

    for (int32_t i = 0; i < bufMgr->dwNumSliceData && (bufMgr->pSliceData != nullptr); i++)
    {
        if (bufMgr->pSliceData[i].pBaseAddress != nullptr)
        {
            MOS_FreeMemory(bufMgr->pSliceData[i].pBaseAddress);
            bufMgr->pSliceData[i].pBaseAddress = nullptr;
        }
    }

    bufMgr->dwNumSliceData    = 0;

    if (m_jpegBitstreamBuf)
    {
        DdiMediaUtil_FreeBuffer(m_jpegBitstreamBuf);
        MOS_FreeMemory(m_jpegBitstreamBuf);
        m_jpegBitstreamBuf = nullptr;
    }

    // free decode bitstream buffer object
    MOS_FreeMemory(bufMgr->pSliceData);
    bufMgr->pSliceData = nullptr;

    return;
}

VAStatus DdiDecodeJPEG::CodecHalInit(
    DDI_MEDIA_CONTEXT *mediaCtx,
    void              *ptr)
{
    VAStatus     vaStatus = VA_STATUS_SUCCESS;
    MOS_CONTEXT *mosCtx   = (MOS_CONTEXT *)ptr;
    
    CODECHAL_FUNCTION codecFunction = CODECHAL_FUNCTION_DECODE;
    m_ddiDecodeCtx->pCpDdiInterface->SetEncryptionType(m_ddiDecodeAttr->uiEncryptionType, &codecFunction);

    CODECHAL_SETTINGS      codecHalSettings;
    CODECHAL_STANDARD_INFO standardInfo;
    memset(&standardInfo, 0, sizeof(standardInfo));
    memset(&codecHalSettings, 0, sizeof(codecHalSettings));

    standardInfo.CodecFunction = codecFunction;
    standardInfo.Mode          = (CODECHAL_MODE)m_ddiDecodeCtx->wMode;

    codecHalSettings.CodecFunction                = codecFunction;
    codecHalSettings.dwWidth                      = m_ddiDecodeCtx->dwWidth;
    codecHalSettings.dwHeight                     = m_ddiDecodeCtx->dwHeight;
    codecHalSettings.bIntelProprietaryFormatInUse = false;

    codecHalSettings.ucLumaChromaDepth = CODECHAL_LUMA_CHROMA_DEPTH_8_BITS;

    codecHalSettings.bShortFormatInUse = m_ddiDecodeCtx->bShortFormatInUse;

    codecHalSettings.Mode     = CODECHAL_DECODE_MODE_JPEG;
    codecHalSettings.Standard = CODECHAL_JPEG;

    m_ddiDecodeCtx->DecodeParams.m_iqMatrixBuffer = MOS_AllocAndZeroMemory(sizeof(CodecJpegQuantMatrix));
    if (m_ddiDecodeCtx->DecodeParams.m_iqMatrixBuffer == nullptr)
    {
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        goto CleanUpandReturn;
    }
    m_ddiDecodeCtx->DecodeParams.m_picParams = MOS_AllocAndZeroMemory(sizeof(CodecDecodeJpegPicParams));
    if (m_ddiDecodeCtx->DecodeParams.m_picParams == nullptr)
    {
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        goto CleanUpandReturn;
    }

    m_ddiDecodeCtx->DecodeParams.m_huffmanTable = (void*)MOS_AllocAndZeroMemory(sizeof(CODECHAL_DECODE_JPEG_HUFFMAN_TABLE));
    if (!m_ddiDecodeCtx->DecodeParams.m_huffmanTable)
    {
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        goto CleanUpandReturn;
    }

    m_ddiDecodeCtx->dwSliceParamBufNum         = DDI_DECODE_JPEG_SLICE_PARAM_BUF_NUM;
    m_ddiDecodeCtx->DecodeParams.m_sliceParams = (void *)MOS_AllocAndZeroMemory(m_ddiDecodeCtx->dwSliceParamBufNum * sizeof(CodecDecodeJpegScanParameter));

    if (m_ddiDecodeCtx->DecodeParams.m_sliceParams == nullptr)
    {
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        goto CleanUpandReturn;
    }

    vaStatus = CreateCodecHal(mediaCtx,
        ptr,
        &codecHalSettings,
        &standardInfo);

    if (vaStatus != VA_STATUS_SUCCESS)
    {
        goto CleanUpandReturn;
    }

    if (InitResourceBuffer() != VA_STATUS_SUCCESS)
    {
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        goto CleanUpandReturn;
    }

    return vaStatus;

CleanUpandReturn:
    FreeResourceBuffer();

    if (m_ddiDecodeCtx->pDecodeStatusReport)
    {
        MOS_DeleteArray(m_ddiDecodeCtx->pDecodeStatusReport);
        m_ddiDecodeCtx->pDecodeStatusReport = nullptr;
    }

    if (m_ddiDecodeCtx->pCodecHal)
    {
        m_ddiDecodeCtx->pCodecHal->Destroy();
        MOS_Delete(m_ddiDecodeCtx->pCodecHal);
        m_ddiDecodeCtx->pCodecHal = nullptr;
    }

    MOS_FreeMemory(m_ddiDecodeCtx->DecodeParams.m_iqMatrixBuffer);
    m_ddiDecodeCtx->DecodeParams.m_iqMatrixBuffer = nullptr;
    MOS_FreeMemory(m_ddiDecodeCtx->DecodeParams.m_picParams);
    m_ddiDecodeCtx->DecodeParams.m_picParams = nullptr;
    MOS_FreeMemory(m_ddiDecodeCtx->DecodeParams.m_huffmanTable);
    m_ddiDecodeCtx->DecodeParams.m_huffmanTable = nullptr;
    MOS_FreeMemory(m_ddiDecodeCtx->DecodeParams.m_sliceParams);
    m_ddiDecodeCtx->DecodeParams.m_sliceParams = nullptr;

    return vaStatus;
}

extern template class MediaDdiFactory<DdiMediaDecode, DDI_DECODE_CONFIG_ATTR>;

static bool jpegRegistered =
    MediaDdiFactory<DdiMediaDecode, DDI_DECODE_CONFIG_ATTR>::RegisterCodec<DdiDecodeJPEG>(DECODE_ID_JPEG);
