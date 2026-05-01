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

#ifndef AFX_BINK_VIDEO_H
#define AFX_BINK_VIDEO_H

#include "qwadro/afxQwadro.h"

AFX_DEFINE_STRUCT(avxBinkSummary)
{
    afxUnit Width;                  // Width of frames
    afxUnit Height;                 // Height of frames
    afxUnit TotalTime;              // total time (ms)
    afxUnit FileFrameRate;          // frame rate
    afxUnit FileFrameRateDiv;       // frame rate divisor
    afxUnit FrameRate;              // frame rate
    afxUnit FrameRateDiv;           // frame rate divisor
    afxUnit TotalOpenTime;          // Time to open and prepare for decompression
    afxUnit TotalFrames;            // Total Frames
    afxUnit TotalPlayedFrames;      // Total Frames played
    afxUnit SkippedFrames;          // Total number of skipped frames
    afxUnit SkippedBlits;           // Total number of skipped blits
    afxUnit SoundSkips;             // Total number of sound skips
    afxUnit TotalBlitTime;          // Total time spent blitting
    afxUnit TotalReadTime;          // Total time spent reading
    afxUnit TotalVideoDecompTime;   // Total time spent decompressing video
    afxUnit TotalAudioDecompTime;   // Total time spent decompressing audio
    afxUnit TotalIdleReadTime;      // Total time spent reading while idle
    afxUnit TotalBackReadTime;      // Total time spent reading in background
    afxUnit TotalReadSpeed;         // Total io speed (bytes/second)
    afxUnit SlowestFrameTime;       // Slowest single frame time (ms)
    afxUnit Slowest2FrameTime;      // Second slowest single frame time (ms)
    afxUnit SlowestFrameNum;        // Slowest single frame number
    afxUnit Slowest2FrameNum;       // Second slowest single frame number
    afxUnit AverageDataRate;        // Average data rate of the movie
    afxUnit AverageFrameSize;       // Average size of the frame
    afxUnit HighestMemAmount;       // Highest amount of memory allocated
    afxUnit TotalIOMemory;          // Total extra memory allocated
    afxUnit HighestIOUsed;          // Highest extra memory actually used
    afxUnit Highest1SecRate;        // Highest 1 second rate
    afxUnit Highest1SecFrame;       // Highest 1 second start frame
};

AFX_DEFINE_STRUCT(avxBinkPlane)
{
    afxInt Allocate;
    void* Buffer;
    afxUnit BufferPitch;
};

AFX_DEFINE_STRUCT(avxBinkFramePlaneSet)
{
    avxBinkPlane YPlane;
    avxBinkPlane cRPlane;
    avxBinkPlane cBPlane;
    avxBinkPlane APlane;
};

#define BINKMAXFRAMEBUFFERS 2

AFX_DEFINE_STRUCT(avxBinkFramebuffers)
{
    afxInt TotalFrames;
    afxUnit YABufferWidth;
    afxUnit YABufferHeight;
    afxUnit cRcBBufferWidth;
    afxUnit cRcBBufferHeight;

    afxUnit FrameNum;
    avxBinkPlane Frames[BINKMAXFRAMEBUFFERS][4]; // Y, cR, cB, A
};

AFX_DEFINE_STRUCT(afxBinkRealtime)
{
    afxUnit FrameNum;
    afxUnit FrameRate;
    afxUnit FrameRateDiv;
    afxUnit Frames;
    afxUnit FramesTime;
    afxUnit FrameVideoDecompTime;
    afxUnit FrameAudioDecompTime;
    afxUnit FrameReadTime;
    afxUnit FrameIdleReadTime;
    afxUnit FrameThreadReadTime;
    afxUnit FramesBlitTime;
    afxUnit ReadBufferSize;
    afxUnit ReadBufferUsed;
    afxUnit FramesDataRate;
};

AFX_DEFINE_STRUCT(afxBinkVideo)
{
    afxBool             running;
    afxDrawSystem       dsys;
    afxModule           binkw32;
    void                *bik;
    avxRange            whd;

    avxBinkSummary      summary;
    
    afxBinkRealtime     rts;
    // this is the Bink info on the textures
    avxBinkFramebuffers buffers;
    //avxCanvas               canv[BINKMAXFRAMEBUFFERS];
    // this is the GPU info for the textures
    afxUnit              unpakOff[4];
    afxUnit              unpakSiz[4];
    afxUnit              stageBufSiz;
    avxBuffer           stageBuffers[BINKMAXFRAMEBUFFERS];
    avxRaster           rasters[4]; // Y, cR, cB, A.
    afxBool             hasAlphaPlane;
    afxBool             useTbo;

    afxUnit64            Last_timer;
    afxUnit32            Frame_count;
    afxUnit32            Bink_microseconds;
    afxUnit32            Render_microseconds;
};

#ifdef _AFX_BINK_VIDEO_C
#define AFXBINK DLLEXPORT
#else
#define AFXBINK DLLIMPORT
#endif

AFXBINK afxError AfxSetUpBinkPlayer(afxBinkVideo *bnk, afxDrawSystem dsys);
AFXBINK afxError AfxDropVideoBink(afxBinkVideo *bnk);

AFXBINK afxError AfxOpenVideoBink(afxBinkVideo *bnk, afxUri const *uri);
AFXBINK afxError AfxBinkClose(afxBinkVideo *bnk);

AFXBINK afxError AfxBinkDoFrame(afxBinkVideo *bnk, afxBool copyAll, afxBool neverSkip);
AFXBINK afxError AfxBinkUnpackFrame(afxDrawContext dctx, avxBinkFramebuffers const* info, avxBuffer buffers[], avxRaster rasters[], afxUnit offsets[], afxBool hasAlpha);
AFXBINK afxError AfxBinkBlitFrame(afxBinkVideo *bnk, afxDrawContext dctx);

#endif//AFX_BINK_VIDEO_H
