/*
 *          ::::::::  :::       :::     :::     :::::::::  :::::::::   ::::::::
 *         :+:    :+: :+:       :+:   :+: :+:   :+:    :+: :+:    :+: :+:    :+:
 *         +:+    +:+ +:+       +:+  +:+   +:+  +:+    +:+ +:+    +:+ +:+    +:+
 *         +#+    +:+ +#+  +:+  +#+ +#++:++#++: +#+    +:+ +#++:++#:  +#+    +:+
 *         +#+  # +#+ +#+ +#+#+ +#+ +#+     +#+ +#+    +#+ +#+    +#+ +#+    +#+
 *         #+#   +#+   #+#+# #+#+#  #+#     #+# #+#    #+# #+#    #+# #+#    #+#
 *          ###### ###  ###   ###   ###     ### #########  ###    ###  ########
 *
 *                  Q W A D R O   E X E C U T I O N   E C O S Y S T E M
 *
 *                                   Public Test Build
 *                               (c) 2017 SIGMA FEDERATION
 *                             <https://sigmaco.org/qwadro/>
 */

#define _CRT_SECURE_NO_WARNINGS 1
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
//#include "../dep/bink/bink.h"
#define BINKNOFRAMEBUFFERS    0x00000400L // Don't allocate internal frame buffers - application must call BinkRegisterFrameBuffers

#define _AFX_BINK_VIDEO_C
#include "afxBinkProxy.h"
#include "afxBinkVideo.h"

static void Start_us_count(afxUnit64* out_count)
{
    QueryPerformanceCounter((LARGE_INTEGER*)out_count);
}

static afxUnit32 Delta_us_count(afxUnit64* last_count)
{
    static afxUnit64 frequency = 1000;
    static afxInt32 got_frequency = 0;
    afxUnit64 start;

    if (!got_frequency)
    {
        got_frequency = 1;
        QueryPerformanceFrequency((LARGE_INTEGER*)&frequency);
    }

    start = *last_count;
    QueryPerformanceCounter((LARGE_INTEGER*)last_count);
    return((afxUnit32)(((*last_count - start) * (afxUnit64)1000000) / frequency));
}

#define Start_timer() { afxUnit64 __timer; Start_us_count( &__timer );
#define End_and_start_next_timer( count ) count += Delta_us_count( &__timer );
#define End_timer( count ) End_and_start_next_timer( count ) }

DLLEXPORT afxError AmxBindVideoDecodingBuffer(afxBinkVideo* bnk)
{
    afxError err = AFX_ERR_NONE;


    avxCanvasConfig ccfg;
    //ccfg.

    return err;
}

DLLEXPORT afxError CreateBinkTextureBuffers(afxDrawSystem dsys, avxBinkFramebuffers* buffers, afxBool hasAlpha, afxBool fetch, afxUnit offsets[4], afxUnit ranges[4], avxBuffer vbufs[BINKMAXFRAMEBUFFERS])
{
    afxError err = { 0 };

    afxUnit rasCnt = (hasAlpha || (hasAlpha = buffers->Frames[0][3].Allocate)) ? 4 : 3;

    offsets[0] = 0;
    ranges[0] = AFX_ALIGN_SIZE(buffers->YABufferWidth, AVX_RASTER_ALIGNMENT) * buffers->YABufferHeight;
    offsets[1] = offsets[0] + ranges[0];
    ranges[1] = AFX_ALIGN_SIZE(buffers->cRcBBufferWidth, AVX_RASTER_ALIGNMENT) * buffers->cRcBBufferHeight;
    offsets[2] = offsets[1] + ranges[1];
    ranges[2] = AFX_ALIGN_SIZE(buffers->cRcBBufferWidth, AVX_RASTER_ALIGNMENT) * buffers->cRcBBufferHeight;
    offsets[3] = offsets[2] + ranges[2];
    ranges[3] = (!hasAlpha) ? 0 : AFX_ALIGN_SIZE(buffers->YABufferWidth, AVX_RASTER_ALIGNMENT) * buffers->YABufferHeight;

    afxUnit bufSiz = ranges[3] + ranges[2] + ranges[1] + ranges[0];

    avxBufferInfo bufis[BINKMAXFRAMEBUFFERS] = { 0 };
    for (afxUnit i = 0; i < BINKMAXFRAMEBUFFERS; ++i)
    {
        bufis[i].size = bufSiz;
        bufis[i].flags = avxBufferFlag_WX | avxBufferFlag_C;
        if (!fetch)
        {
            bufis[i].usage = avxBufferUsage_UPLOAD/* | avxBufferUsage_FETCH*/;
        }
        else
        {
            bufis[i].usage = avxBufferUsage_FETCH;
            bufis[i].fmt = avxFormat_R8u;
        }
        bufis[i].mapped = TRUE;
    }

    if (AfxFailed(AvxAcquireBuffers(dsys, BINKMAXFRAMEBUFFERS, bufis, vbufs)))
    {
        AfxThrowError();
        return err;
    }

    AFX_ASSERT_OBJECTS(afxFcc_BUF, BINKMAXFRAMEBUFFERS, vbufs);

    for (afxUnit i = 0; i < BINKMAXFRAMEBUFFERS; ++i)
    {
        afxByte* start = AvxGetBufferMap(vbufs[i], 0, bufis[i].size);
        AFX_ASSERT(start);

        for (afxUnit j = 0; j < rasCnt; j++)
        {
            buffers->Frames[i][j].Buffer = &start[offsets[j]];
            buffers->Frames[i][j].BufferPitch = AFX_ALIGN_SIZE(((j == 1 || 2 == j) ? buffers->cRcBBufferWidth : buffers->YABufferWidth), 16);
        }
    }

    return err;
}

DLLEXPORT afxError CreateBinkTextureRasters(afxDrawSystem dsys, avxBinkFramebuffers* buffers, afxBool hasAlpha, avxRaster rasters[])
{
    afxError err = AFX_ERR_NONE;

    avxFormatDescription pfd;
    AvxDescribeFormats(1, (avxFormat[]) { avxFormat_R8un }, &pfd);
    
    avxRasterInfo texi[4] = { 0 };
    texi[0].whd = AVX_RANGE(buffers->YABufferWidth, buffers->YABufferHeight, 1);
    texi[1].whd = AVX_RANGE(buffers->cRcBBufferWidth, buffers->cRcBBufferHeight, 1);
    texi[2].whd = AVX_RANGE(buffers->cRcBBufferWidth, buffers->cRcBBufferHeight, 1);
    texi[3].whd = AVX_RANGE(buffers->YABufferWidth, buffers->YABufferHeight, 1);
    texi[0].fmt = avxFormat_R8un;
    texi[1].fmt = avxFormat_R8un;
    texi[2].fmt = avxFormat_R8un;
    texi[3].fmt = avxFormat_R8un;
    texi[0].usage = avxRasterUsage_TEXTURE;
    texi[1].usage = avxRasterUsage_TEXTURE;
    texi[2].usage = avxRasterUsage_TEXTURE;
    texi[3].usage = avxRasterUsage_TEXTURE;

    afxUnit rasCnt = (hasAlpha || (hasAlpha = buffers->Frames[0][3].Allocate)) ? 4 : 3;

    if (AfxFailed(AvxAcquireRasters(dsys, rasCnt, texi, rasters)))
    {
        AfxThrowError();
    }
    else
    {
        AFX_ASSERT_OBJECTS(afxFcc_RAS, rasCnt, rasters);
    }

    return err;
}

DLLEXPORT afxError AfxBinkUnpackFrame(afxDrawContext dctx, avxBinkFramebuffers const* info, avxBuffer buffers[], avxRaster rasters[], afxUnit offsets[], afxBool hasAlpha)
{
    afxError err = AFX_ERR_NONE;

    afxUnit frameIdx = info->FrameNum;

    avxRasterIo op = { 0 };
    op.rgn.whd = AvxGetRasterExtent(rasters[0], 0);
    op.offset = offsets[0];
    op.rowStride = info->Frames[frameIdx][0].BufferPitch;
    op.rowsPerImg = info->YABufferHeight;
    AvxCmdUnpackRaster(dctx, rasters[0], 1, &op, buffers[frameIdx]);
    
    op.rgn.whd = AvxGetRasterExtent(rasters[1], 0);
    op.offset = offsets[1];
    op.rowStride = info->Frames[frameIdx][1].BufferPitch;
    op.rowsPerImg = info->cRcBBufferHeight;
    AvxCmdUnpackRaster(dctx, rasters[1], 1, &op, buffers[frameIdx]);

    op.offset = offsets[2];
    op.rowStride = info->Frames[frameIdx][2].BufferPitch;
    op.rowsPerImg = info->cRcBBufferHeight;
    AvxCmdUnpackRaster(dctx, rasters[2], 1, &op, buffers[frameIdx]);

    if (hasAlpha)
    {
        op.offset = offsets[3];
        op.rowStride = info->Frames[frameIdx][3].BufferPitch;
        op.rowsPerImg = info->YABufferHeight;
        AvxCmdUnpackRaster(dctx, rasters[3], 1, &op, buffers[frameIdx]);
    }

    return err;
}

static void DecompressFrame(afxBinkVideo *bnk)
{
    Start_timer();

    //bnk->buffers.FrameNum = outBufIdx;

    // Lock the textures.
    //LockBinkTextures(bnk, bnk->buffers.FrameNum);

    End_and_start_next_timer(bnk->Render_microseconds);

    // Decompress a frame
    BinkDoFrame(bnk->bik);

    // if we are falling behind, decompress an extra frame to catch up
    while (BinkShouldSkip(bnk->bik))
    {
        BinkNextFrame(bnk->bik);
        BinkDoFrame(bnk->bik);
    }

    

    End_and_start_next_timer(bnk->Bink_microseconds);

    // Unlock the textures.
    //UnlockBinkTextures(bnk, bnk->buffers.FrameNum);

    End_and_start_next_timer(bnk->Render_microseconds);

    // Keep playing the movie.
    BinkNextFrame(bnk->bik);

    End_timer(bnk->Bink_microseconds);
}

DLLEXPORT afxError AfxBinkDoFrame(afxBinkVideo *bnk, afxBool copyAll, afxBool neverSkip)
{
    (void)copyAll;
    (void)neverSkip;
    afxError err = AFX_ERR_NONE;

    // Is it time for a new Bink frame (we play at the speed of the background vid in this example)

    if (!BinkWait(bnk->bik))
    {
        DecompressFrame(bnk);
    }
    return err;
}

DLLEXPORT afxError AfxBinkClose(afxBinkVideo *bnk)
{
    afxError err = AFX_ERR_NONE;
    AFX_ASSERT(BinkClose);

    if (bnk->bik)
        BinkClose(bnk->bik);

    bnk->bik = NIL;
    AfxDisposeObjects(BINKMAXFRAMEBUFFERS, bnk->stageBuffers);
    AfxDisposeObjects(4, bnk->rasters);

    return err;
}

DLLEXPORT afxError AfxOpenVideoBink(afxBinkVideo *bnk, afxUri const *uri)
{
    afxError err = AFX_ERR_NONE;

    AFX_ASSERT(!bnk->bik);

    //AfxResolveUri(afxFileFlag_R, uri, &uri2.uri);

    afxStream file;    
    AfxOpenFile(uri, afxFileFlag_R, &file);
    AFX_ASSERT_OBJECTS(afxFcc_IOB, 1, &file);

    afxUri2048 uri2;
    AfxMakeUri2048(&uri2, NIL);
    AfxGetResolvedFileUri(file, &uri2.uri);
    AfxDisposeObjects(1, &file);

    BinkSetSoundSystem(BinkOpenDirectSound, 0);

    void* bik;
    AFX_ASSERT(BinkOpen);
    bnk->bik = (bik = BinkOpen((void*)uri2.buf, BINKNOFRAMEBUFFERS));
    AFX_ASSERT(bik);


    BinkGetSummary(bik, &bnk->summary);

    bnk->whd.w = bnk->summary.Width;
    bnk->whd.h = bnk->summary.Height;
    bnk->whd.d = 1;

    AfxZero(&bnk->buffers, sizeof(bnk->buffers));
    BinkGetFrameBuffersInfo(bnk->bik, &bnk->buffers);
    CreateBinkTextureRasters(bnk->dsys, &bnk->buffers, FALSE, bnk->rasters);
    CreateBinkTextureBuffers(bnk->dsys, &bnk->buffers, bnk->hasAlphaPlane, bnk->useTbo, bnk->unpakOff, bnk->unpakSiz, bnk->stageBuffers);
    // Register our locked texture pointers with Bink
    BinkRegisterFrameBuffers(bnk->bik, &bnk->buffers);
    
    bnk->Last_timer = 0;

    return err;
}

DLLEXPORT afxError AfxDropVideoBink(afxBinkVideo *bnk)
{
    afxError err = AFX_ERR_NONE;
    AfxBinkClose(bnk);
    AfxDisposeObjects(1, &bnk->binkw32);

    //AfxReleaseObject(&bnk);
    return err;
}

DLLEXPORT afxError AfxSetUpBinkPlayer(afxBinkVideo *bnk, afxDrawSystem dsys)
{
    afxError err = AFX_ERR_NONE;
    bnk->running = FALSE;

    //BinkSetSoundSystem(BinkOpenMiles, 0);

    //AfxFindSymbolAddresses(bnk->binkw32, AFX_COUNTOF(test), test, bink19c.v);

    bnk->running = FALSE;
    bnk->dsys = dsys;
    bnk->bik = NIL;
    bnk->whd.w = 1;
    bnk->whd.h = 1;
    bnk->whd.d = 1;
    
    return err;
}

