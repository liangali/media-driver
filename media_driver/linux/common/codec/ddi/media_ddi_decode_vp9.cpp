/*
* Copyright (c) 2015-2017, Intel Corporation
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
//! \file     media_ddi_decode_vp9.cpp
//! \brief    The class implementation of DdiDecodeVP9  for VP9 decode
//!

#include "media_libva_decoder.h"
#include "media_libva_util.h"

#include "media_ddi_decode_vp9.h"
#include "mos_solo_generic.h"
#include "codechal_memdecomp.h"
#include "media_ddi_decode_const.h"
#include "media_ddi_factory.h"

VAStatus DdiDecodeVP9::ParseSliceParams(
    DDI_MEDIA_CONTEXT         *mediaCtx,
    VASliceParameterBufferVP9 *slcParam)
{
    PCODEC_VP9_PIC_PARAMS     picParam  = (PCODEC_VP9_PIC_PARAMS)(m_ddiDecodeCtx->DecodeParams.m_picParams);
    PCODEC_VP9_SEGMENT_PARAMS segParams = (PCODEC_VP9_SEGMENT_PARAMS)(m_ddiDecodeCtx->DecodeParams.m_iqMatrixBuffer);

    if ((slcParam == nullptr) ||
        (picParam == nullptr) ||
        (segParams == nullptr))
    {
        DDI_ASSERTMESSAGE("Invalid Parameter for Parsing VP9 Slice parameter\n");
        return VA_STATUS_ERROR_INVALID_PARAMETER;
    }

    picParam->BSBytesInBuffer = slcParam->slice_data_size;

    int32_t i, j, k;
    for (i = 0; i < 8; i++)
    {
        segParams->SegData[i].SegmentFlags.fields.SegmentReferenceEnabled = slcParam->seg_param[i].segment_flags.fields.segment_reference_enabled;
        segParams->SegData[i].SegmentFlags.fields.SegmentReference        = slcParam->seg_param[i].segment_flags.fields.segment_reference;
        segParams->SegData[i].SegmentFlags.fields.SegmentReferenceSkipped = slcParam->seg_param[i].segment_flags.fields.segment_reference_skipped;

        for (j = 0; j < 4; j++)
        {
            for (k = 0; k < 2; k++)
            {
                segParams->SegData[i].FilterLevel[j][k] = slcParam->seg_param[i].filter_level[j][k];
            }
        }

        segParams->SegData[i].LumaACQuantScale   = slcParam->seg_param[i].luma_ac_quant_scale;
        segParams->SegData[i].LumaDCQuantScale   = slcParam->seg_param[i].luma_dc_quant_scale;
        segParams->SegData[i].ChromaACQuantScale = slcParam->seg_param[i].chroma_ac_quant_scale;
        segParams->SegData[i].ChromaDCQuantScale = slcParam->seg_param[i].chroma_dc_quant_scale;
    }

    return VA_STATUS_SUCCESS;
}

VAStatus DdiDecodeVP9::ParsePicParams(
    DDI_MEDIA_CONTEXT              *mediaCtx,
    VADecPictureParameterBufferVP9 *picParam)
{
    PCODEC_VP9_PIC_PARAMS picVp9Params = (PCODEC_VP9_PIC_PARAMS)(m_ddiDecodeCtx->DecodeParams.m_picParams);

    if ((picParam == nullptr) || (picVp9Params == nullptr))
    {
        DDI_ASSERTMESSAGE("Invalid Parameter for Parsing VP9 Picture parameter\n");
        return VA_STATUS_ERROR_INVALID_PARAMETER;
    }

    picVp9Params->FrameHeightMinus1 = picParam->frame_height - 1;
    picVp9Params->FrameWidthMinus1  = picParam->frame_width - 1;

    picVp9Params->PicFlags.fields.frame_type                   = picParam->pic_fields.bits.frame_type;
    picVp9Params->PicFlags.fields.show_frame                   = picParam->pic_fields.bits.show_frame;
    picVp9Params->PicFlags.fields.error_resilient_mode         = picParam->pic_fields.bits.error_resilient_mode;
    picVp9Params->PicFlags.fields.intra_only                   = picParam->pic_fields.bits.intra_only;
    picVp9Params->PicFlags.fields.LastRefIdx                   = picParam->pic_fields.bits.last_ref_frame;
    picVp9Params->PicFlags.fields.LastRefSignBias              = picParam->pic_fields.bits.last_ref_frame_sign_bias;
    picVp9Params->PicFlags.fields.GoldenRefIdx                 = picParam->pic_fields.bits.golden_ref_frame;
    picVp9Params->PicFlags.fields.GoldenRefSignBias            = picParam->pic_fields.bits.golden_ref_frame_sign_bias;
    picVp9Params->PicFlags.fields.AltRefIdx                    = picParam->pic_fields.bits.alt_ref_frame;
    picVp9Params->PicFlags.fields.AltRefSignBias               = picParam->pic_fields.bits.alt_ref_frame_sign_bias;
    picVp9Params->PicFlags.fields.allow_high_precision_mv      = picParam->pic_fields.bits.allow_high_precision_mv;
    picVp9Params->PicFlags.fields.mcomp_filter_type            = picParam->pic_fields.bits.mcomp_filter_type;
    picVp9Params->PicFlags.fields.frame_parallel_decoding_mode = picParam->pic_fields.bits.frame_parallel_decoding_mode;
    picVp9Params->PicFlags.fields.segmentation_enabled         = picParam->pic_fields.bits.segmentation_enabled;
    picVp9Params->PicFlags.fields.segmentation_temporal_update = picParam->pic_fields.bits.segmentation_temporal_update;
    picVp9Params->PicFlags.fields.segmentation_update_map      = picParam->pic_fields.bits.segmentation_update_map;
    picVp9Params->PicFlags.fields.reset_frame_context          = picParam->pic_fields.bits.reset_frame_context;
    picVp9Params->PicFlags.fields.refresh_frame_context        = picParam->pic_fields.bits.refresh_frame_context;
    picVp9Params->PicFlags.fields.frame_context_idx            = picParam->pic_fields.bits.frame_context_idx;
    picVp9Params->PicFlags.fields.LosslessFlag                 = picParam->pic_fields.bits.lossless_flag;

    int32_t frameIdx;
    frameIdx = GetRenderTargetID(&m_ddiDecodeCtx->RTtbl, m_ddiDecodeCtx->RTtbl.pCurrentRT);
    if (frameIdx == DDI_CODEC_INVALID_FRAME_INDEX)
    {
        return VA_STATUS_ERROR_INVALID_PARAMETER;
    }
    picVp9Params->CurrPic.FrameIdx = frameIdx;

    int32_t i;
    for (i = 0; i < 8; i++)
    {
        if (picParam->reference_frames[i] < mediaCtx->uiNumSurfaces)
        {
            PDDI_MEDIA_SURFACE refSurface          = DdiMedia_GetSurfaceFromVASurfaceID(mediaCtx, picParam->reference_frames[i]);
            frameIdx                               = GetRenderTargetID(&m_ddiDecodeCtx->RTtbl, refSurface);
            picVp9Params->RefFrameList[i].FrameIdx = ((uint32_t)frameIdx >= CODECHAL_NUM_UNCOMPRESSED_SURFACE_VP9) ? (CODECHAL_NUM_UNCOMPRESSED_SURFACE_VP9 - 1) : frameIdx;
        }
        else
        {
            picVp9Params->RefFrameList[i].FrameIdx = CODECHAL_NUM_UNCOMPRESSED_SURFACE_VP9 - 1;
        }
    }

    picVp9Params->filter_level                    = picParam->filter_level;
    picVp9Params->sharpness_level                 = picParam->sharpness_level;
    picVp9Params->log2_tile_rows                  = picParam->log2_tile_rows;
    picVp9Params->log2_tile_columns               = picParam->log2_tile_columns;
    picVp9Params->UncompressedHeaderLengthInBytes = picParam->frame_header_length_in_bytes;
    picVp9Params->FirstPartitionSize              = picParam->first_partition_size;
    picVp9Params->profile                         = picParam->profile;

    /* Only 8bit depth is supported on VAProfileVP9Profile0.
     * If it is VAProfileVP9Profile2/3, it is possible to support the 10/12 bit-depth.
     * otherwise the bit_depth is 8.
     */
    if (((picParam->profile == VAProfileVP9Profile2) ||
            (picParam->profile == VAProfileVP9Profile3)) &&
        (picParam->bit_depth >= 8))
    {
        picVp9Params->BitDepthMinus8 = picParam->bit_depth - 8;
    }
    else
    {
        picVp9Params->BitDepthMinus8 = 0;
    }

    picVp9Params->subsampling_x = picParam->pic_fields.bits.subsampling_x;
    picVp9Params->subsampling_y = picParam->pic_fields.bits.subsampling_y;

    memcpy(picVp9Params->SegTreeProbs, picParam->mb_segment_tree_probs, 7);
    memcpy(picVp9Params->SegPredProbs, picParam->segment_pred_probs, 3);

    return VA_STATUS_SUCCESS;
}

VAStatus DdiDecodeVP9::RenderPicture(
    VADriverContextP ctx,
    VAContextID      context,
    VABufferID       *buffers,
    int32_t          numBuffers)
{
    VAStatus           vaStatus = VA_STATUS_SUCCESS;
    PDDI_MEDIA_CONTEXT mediaCtx = DdiMedia_GetMediaContext(ctx);

    DDI_FUNCTION_ENTER();

    int32_t           i, j;
    DDI_MEDIA_BUFFER *buf  = nullptr;
    void             *data = nullptr;
    uint32_t          dataSize;
    for (i = 0; i < numBuffers; i++)
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
            if (slcFlag)
            {
                // VP9 only supports only one slice_data. If it is passed, another slice_data
                // buffer will be ignored.
                DDI_NORMALMESSAGE("Slice data is already rendered\n");
                break;
            }
            int32_t index = DdiDecode_GetBitstreamBufIndexFromBuffer(&m_ddiDecodeCtx->BufMgr, buf);
            if (index == DDI_CODEC_INVALID_BUFFER_INDEX)
            {
                return VA_STATUS_ERROR_INVALID_BUFFER;
            }

            DdiMedia_MediaBufferToMosResource(m_ddiDecodeCtx->BufMgr.pBitStreamBuffObject[index], &m_ddiDecodeCtx->BufMgr.resBitstreamBuffer);
            m_ddiDecodeCtx->DecodeParams.m_dataSize += dataSize;
            slcFlag = true;
            break;
        }
        case VASliceParameterBufferType:
        {
            if (m_ddiDecodeCtx->DecodeParams.m_numSlices)
            {
                // VP9 only supports only one slice. If it is passed, another slice_param
                // buffer will be ignored.
                DDI_NORMALMESSAGE("SliceParamBufferVP9 is already rendered\n");
                break;
            }
            if (buf->iNumElements == 0)
            {
                return VA_STATUS_ERROR_INVALID_BUFFER;
            }

            VASliceParameterBufferVP9 *slcInfoVP9 = (VASliceParameterBufferVP9 *)data;

            vaStatus = ParseSliceParams(mediaCtx, slcInfoVP9);
            if (vaStatus != VA_STATUS_SUCCESS)
            {
                return VA_STATUS_ERROR_INVALID_BUFFER;
            }
            m_ddiDecodeCtx->DecodeParams.m_numSlices++;
            m_ddiDecodeCtx->m_groupIndex++;
            break;
        }
        case VAPictureParameterBufferType:
        {
            VADecPictureParameterBufferVP9 *picParam;

            picParam = (VADecPictureParameterBufferVP9 *)data;

            if (ParsePicParams(mediaCtx, picParam) != VA_STATUS_SUCCESS)
            {
                return VA_STATUS_ERROR_INVALID_BUFFER;
            }
            break;
        }

        case VAProcPipelineParameterBufferType:
        {
            DDI_NORMALMESSAGE("ProcPipeline is not supported for VP9 decoding\n");
            break;
        }
        case VADecodeStreamoutBufferType:
        {
            DdiMedia_MediaBufferToMosResource(buf, &m_ddiDecodeCtx->BufMgr.resExternalStreamOutBuffer);
            m_ddiDecodeCtx->bStreamOutEnabled = true;
            break;
        }

        default:
            vaStatus = m_ddiDecodeCtx->pCpDdiInterface->RenderCencPicture(ctx, context, buf, data);
            break;
        }
        DdiMedia_UnmapBuffer(ctx, buffers[i]);
    }

    DDI_FUNCTION_EXIT(vaStatus);
    return vaStatus;
}

VAStatus DdiDecodeVP9::InitResourceBuffer()
{
    VAStatus                  vaStatus = VA_STATUS_SUCCESS;
    DDI_CODEC_COM_BUFFER_MGR *bufMgr   = &(m_ddiDecodeCtx->BufMgr);

    bufMgr->pSliceData = nullptr;

    bufMgr->ui64BitstreamOrder = 0;
    bufMgr->dwMaxBsSize        = m_ddiDecodeCtx->dwWidth *
                          m_ddiDecodeCtx->dwHeight * 3 / 2;
    // minimal 10k bytes for some special case. Will refractor this later
    if (bufMgr->dwMaxBsSize < DDI_CODEC_MIN_VALUE_OF_MAX_BS_SIZE)
    {
        bufMgr->dwMaxBsSize = DDI_CODEC_MIN_VALUE_OF_MAX_BS_SIZE;
    }

    int32_t i;
    // init decode bitstream buffer object
    for (i = 0; i < DDI_CODEC_MAX_BITSTREAM_BUFFER; i++)
    {
        bufMgr->pBitStreamBuffObject[i] = (DDI_MEDIA_BUFFER *)MOS_AllocAndZeroMemory(sizeof(DDI_MEDIA_BUFFER));
        if (bufMgr->pBitStreamBuffObject[i] == nullptr)
        {
            vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
            goto finish;
        }
        bufMgr->pBitStreamBuffObject[i]->iSize    = bufMgr->dwMaxBsSize;
        bufMgr->pBitStreamBuffObject[i]->uiType   = VASliceDataBufferType;
        bufMgr->pBitStreamBuffObject[i]->format   = Media_Format_Buffer;
        bufMgr->pBitStreamBuffObject[i]->uiOffset = 0;
        bufMgr->pBitStreamBuffObject[i]->bo       = nullptr;
        bufMgr->pBitStreamBase[i]                 = nullptr;
    }

    // VP9 can support only one SliceDataBuffer. In such case only one is enough.
    // 2 is allocated for the safety.
    bufMgr->m_maxNumSliceData = 2;
    bufMgr->pSliceData        = (DDI_CODEC_BITSTREAM_BUFFER_INFO *)MOS_AllocAndZeroMemory(sizeof(bufMgr->pSliceData[0]) * 2);

    if (bufMgr->pSliceData == nullptr)
    {
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        goto finish;
    }

    bufMgr->dwNumSliceData    = 0;
    bufMgr->dwNumSliceControl = 0;

    bufMgr->Codec_Param.Codec_Param_VP9.pVASliceParaBufVP9 = (VASliceParameterBufferVP9 *)MOS_AllocAndZeroMemory(sizeof(VASliceParameterBufferVP9));
    if (bufMgr->Codec_Param.Codec_Param_VP9.pVASliceParaBufVP9 == nullptr)
    {
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        goto finish;
    }

    return VA_STATUS_SUCCESS;

finish:
    FreeResourceBuffer();
    return vaStatus;
}

void DdiDecodeVP9::FreeResourceBuffer()
{
    DDI_CODEC_COM_BUFFER_MGR *bufMgr = &(m_ddiDecodeCtx->BufMgr);

    int32_t i;
    for (i = 0; i < DDI_CODEC_MAX_BITSTREAM_BUFFER; i++)
    {
        if (bufMgr->pBitStreamBase[i])
        {
            DdiMediaUtil_UnlockBuffer(bufMgr->pBitStreamBuffObject[i]);
            bufMgr->pBitStreamBase[i] = nullptr;
        }
        if (bufMgr->pBitStreamBuffObject[i])
        {
            DdiMediaUtil_FreeBuffer(bufMgr->pBitStreamBuffObject[i]);
            MOS_FreeMemory(bufMgr->pBitStreamBuffObject[i]);
            bufMgr->pBitStreamBuffObject[i] = nullptr;
        }
    }

    if (bufMgr->Codec_Param.Codec_Param_VP9.pVASliceParaBufVP9)
    {
        MOS_FreeMemory(bufMgr->Codec_Param.Codec_Param_VP9.pVASliceParaBufVP9);
        bufMgr->Codec_Param.Codec_Param_VP9.pVASliceParaBufVP9 = nullptr;
    }

    // free decode bitstream buffer object
    MOS_FreeMemory(bufMgr->pSliceData);
    bufMgr->pSliceData = nullptr;
}

VAStatus DdiDecodeVP9::CodecHalInit(
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

    codecHalSettings.CodecFunction = codecFunction;
    codecHalSettings.dwWidth       = m_ddiDecodeCtx->dwWidth;
    codecHalSettings.dwHeight      = m_ddiDecodeCtx->dwHeight;
    codecHalSettings.bIntelProprietaryFormatInUse = false;

    codecHalSettings.ucLumaChromaDepth = CODECHAL_LUMA_CHROMA_DEPTH_8_BITS;
    if ((m_ddiDecodeAttr->profile == VAProfileVP9Profile2) ||
        (m_ddiDecodeAttr->profile == VAProfileVP9Profile3))
    {
        codecHalSettings.ucLumaChromaDepth |= CODECHAL_LUMA_CHROMA_DEPTH_10_BITS;
    }

    codecHalSettings.bShortFormatInUse = m_ddiDecodeCtx->bShortFormatInUse;

    codecHalSettings.Mode           = CODECHAL_DECODE_MODE_VP9VLD;
    codecHalSettings.Standard       = CODECHAL_VP9;
    codecHalSettings.ucChromaFormat = HCP_CHROMA_FORMAT_YUV420;

    m_ddiDecodeCtx->DecodeParams.m_iqMatrixBuffer = MOS_AllocAndZeroMemory(sizeof(CODEC_VP9_SEGMENT_PARAMS));
    if (m_ddiDecodeCtx->DecodeParams.m_iqMatrixBuffer == nullptr)
    {
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        goto CleanUpandReturn;
    }
    m_ddiDecodeCtx->DecodeParams.m_picParams = MOS_AllocAndZeroMemory(sizeof(CODEC_VP9_PIC_PARAMS));
    if (m_ddiDecodeCtx->DecodeParams.m_picParams == nullptr)
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
    MOS_FreeMemory(m_ddiDecodeCtx->DecodeParams.m_sliceParams);
    m_ddiDecodeCtx->DecodeParams.m_sliceParams = nullptr;

    return vaStatus;
}

VAStatus DdiDecodeVP9::EndPicture(
    VADriverContextP ctx,
    VAContextID      context)
{
    DDI_FUNCTION_ENTER();

    /* the default CtxType is DECODER */
    m_ctxType = DDI_MEDIA_CONTEXT_TYPE_DECODER;

    slcFlag = false;
    if ((context & DDI_MEDIA_MASK_VACONTEXT_TYPE) == DDI_MEDIA_VACONTEXTID_OFFSET_CENC)
    {
        m_ctxType = DDI_MEDIA_CONTEXT_TYPE_CENC_DECODER;
    }
    /* skip the mediaCtx check as it is checked in caller */
    PDDI_MEDIA_CONTEXT mediaCtx = DdiMedia_GetMediaContext(ctx);    

    VAStatus vaStatus = DecodeCombineBitstream(mediaCtx);

    if (vaStatus != VA_STATUS_SUCCESS)
        return vaStatus;

    DDI_CODEC_COM_BUFFER_MGR *bufMgr = &(m_ddiDecodeCtx->BufMgr);
    bufMgr->dwNumSliceData    = 0;
    bufMgr->dwNumSliceControl = 0;
    
    if (m_ctxType == DDI_MEDIA_CONTEXT_TYPE_CENC_DECODER)
    {
        vaStatus = m_ddiDecodeCtx->pCpDdiInterface->EndPictureCenc(ctx, context);
    
        return vaStatus;
    }

    DDI_CODEC_RENDER_TARGET_TABLE *rtTbl = &(m_ddiDecodeCtx->RTtbl);
    
    if ((rtTbl == nullptr) || (rtTbl->pCurrentRT == nullptr))
    {
        return VA_STATUS_ERROR_INVALID_PARAMETER;
    }

    CodechalDecodeParams *decodeParams = &m_ddiDecodeCtx->DecodeParams;

    if (decodeParams->m_numSlices == 0)
    {
        return VA_STATUS_ERROR_INVALID_PARAMETER;
    }

    MOS_SURFACE destSurface;
    memset(&destSurface, 0, sizeof(MOS_SURFACE));
    destSurface.dwOffset       = 0;
    MOS_FORMAT expectedFormat = Format_NV12;

    CODEC_VP9_PIC_PARAMS *picParams = (CODEC_VP9_PIC_PARAMS *)decodeParams->m_picParams;
    if (((picParams->profile == VAProfileVP9Profile2) ||
            (picParams->profile == VAProfileVP9Profile3)) &&
        (picParams->BitDepthMinus8 > 0))
    {
        expectedFormat = Format_P010;
        if ((picParams->subsampling_x == 0) ||
            (picParams->subsampling_y == 0))
        {
            expectedFormat = Format_Y210;
        }
    }

    destSurface.Format = expectedFormat;

    DdiMedia_MediaSurfaceToMosResource(rtTbl->pCurrentRT, &(destSurface.OsResource));

    if (destSurface.OsResource.Format != expectedFormat)
    {
        DDI_NORMALMESSAGE("Surface fomrat of decoded surface is inconsistent with VP9 bitstream\n");
        vaStatus = VA_STATUS_ERROR_INVALID_PARAMETER;
        return vaStatus;
    }

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

void DdiDecodeVP9::DestroyContext(
    VADriverContextP ctx)
{
    FreeResourceBuffer();
    // explicitly call the base function to do the further clean-up
    DdiMediaDecode::DestroyContext(ctx);
}

void DdiDecodeVP9::ContextInit(
    int32_t picWidth,
    int32_t picHeight)
{
    // call the function in base class to initialize it.
    DdiMediaDecode::ContextInit(picWidth, picHeight);

    m_ddiDecodeCtx->wMode    = CODECHAL_DECODE_MODE_VP9VLD;
    m_ddiDecodeCtx->Standard = CODECHAL_VP9;
}

extern template class MediaDdiFactory<DdiMediaDecode, DDI_DECODE_CONFIG_ATTR>;

static bool vp9Registered =
    MediaDdiFactory<DdiMediaDecode, DDI_DECODE_CONFIG_ATTR>::RegisterCodec<DdiDecodeVP9>(DECODE_ID_VP9);
