#define _CRT_SECURE_NO_WARNINGS 1
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>

#include "qwadro/afxQwadro.h"
#include "../binq/afxBinkVideo.h"

#define ENABLE_DIN1 // 
#define ENABLE_DOUT1

//#define ENABLE_DIN2 // yuv
//#define ENABLE_DOUT2

//#define ENABLE_DIN3 // rgb
//#define ENABLE_DOUT3

arxSimulation sim = NIL;
avxRaster dumpImg = NIL;

afxSurface dout[3] = { NIL, NIL, NIL };
afxDrawSystem dsys = NIL;
afxEnvironment env = NIL;

//avxCanvas canv[3][60] = { NIL };

afxUri2048 uri, uri2;

avxBuffer ubo = NIL;
avxPipeline dpip[2] = { NIL, NIL };
avxRaster tex[4] = { NIL, NIL, NIL, NIL };

afxBinkVideo bnk = { 0 };
avxBuffer viewConstantsBuffer = NIL;

avxSampler sampler;
avxPipeline yv12ToRgbaDtec;

afxString vsh = AFX_STATIC_STRING_R(
    // QWADRO (c) 2017 SIGMA TECHNOLOGY GROUP

    const vec4 gsTriPos[3] = vec4[](vec4(-1, -1, 0, 1), vec4(3, -1, 0, 1), vec4(-1, 3, 0, 1));
    const vec2 gsTriUv[3] = vec2[](vec2(0, 0), vec2(2, 0), vec2(0, 2));

    out block
    {
        vec2 uv0;
    } sgl_v;

    void main()
    {
        // draw a full coverage triangle (3 indices). AfxDraw(3, 1, 0, 0)
        gl_Position = gsTriPos[gl_VertexID];
        sgl_v.uv0 = 0.5 * vec2(gl_Position.x, gl_Position.y * -1.0) + vec2(0.5);
    }
);

afxString fsh = AFX_STATIC_STRING_R(
    // QWADRO (c) 2017 SIGMA TECHNOLOGY GROUP

    FETCH(0, 0, texY); 
    FETCH(0, 1, texU); 
    FETCH(0, 2, texV); 
    FETCH(0, 3, texA); 

    PUSH(data)
    {
        int imageWidth;
        int imageHeight;
        bool hasAlpha;
    };

    in block
    {
        vec2 uv0;
    } sgl_v;

    OUT(0, vec4, sgl_rgba);

    vec3 yuvToRgb(float y, float u, float v)
    {
        y = y * 1.1643;
        u = u - 0.5;
        v = v - 0.5;

        float r = y + 1.5958 * v;
        float g = y - 0.39173 * u - 0.81290 * v;
        float b = y + 2.017 * u;

        return vec3(r, g, b);
    }

    void main()
    {
        int x = int(gl_FragCoord.x);
        int yCoord = int(gl_FragCoord.y);
        int idx = yCoord * imageWidth + x;

        float yVal = texelFetch(texY, idx).r;
        float uVal = texelFetch(texU, idx).r - 0.5;
        float vVal = texelFetch(texV, idx).r - 0.5;
        float aVal = hasAlpha ? texelFetch(texA, idx).r : 1.0;

        vec3 rgb = yuvToRgb(yVal, uVal, vVal);
        sgl_rgba = vec4(rgb, aVal);
    }
);

afxError DoVideo(afxSurface dout, afxUnit outBufIdx, afxDrawContext dctx)
{
    afxError err = AFX_ERR_NONE;

    afxUnit queIdx = 0;
    afxUnit portId = 0;
    if (AfxFailed(AvxPrepareDrawCommands(dctx, TRUE, avxCmdFlag_ONCE)))
    {
        AfxThrowError();
        return err;
    }
    else
    {
        //AfxBinkUnpackFrame(dctx, &bnk.buffers, bnk.stageBuffers, bnk.rasters, bnk.unpakOff, bnk.hasAlphaPlane);
#if 0
        AvxCohereMappedBuffers(dsys, FALSE, bnk.hasAlphaPlane ? 4 : 3, (avxBufferedMap[]) {
            AVX_BUFFERED_MAP(bnk.stageBuffers[bnk.buffers.FrameNum], bnk.unpakOff[0], bnk.unpakSiz[0], NIL),
            AVX_BUFFERED_MAP(bnk.stageBuffers[bnk.buffers.FrameNum], bnk.unpakOff[1], bnk.unpakSiz[1], NIL),
            AVX_BUFFERED_MAP(bnk.stageBuffers[bnk.buffers.FrameNum], bnk.unpakOff[2], bnk.unpakSiz[2], NIL),
            AVX_BUFFERED_MAP(bnk.stageBuffers[bnk.buffers.FrameNum], bnk.unpakOff[3], bnk.unpakSiz[3], NIL),
        });
#endif
        AvxCmdDeclareBarrier(dctx, avxBusStage_FRAGMENT, avxBusAccess_SHADER_R);

        avxCanvas canv;
        afxLayeredRect bounds;
        AvxGetSurfaceCanvas(dout, outBufIdx, &canv, &bounds);
        AFX_ASSERT_OBJECTS(afxFcc_CANV, 1, &canv);

        afxBool readjust = TRUE;
        afxBool upscale = FALSE;
        avxRange extent = bnk.whd;

        if (readjust)
        {
            extent.w = bounds.area.w;
            extent.h = bounds.area.h;
        }

        if (!upscale)
            extent = AvxClampRange(extent, AVX_RANGE(1, 1, 1), bnk.whd);

        avxDrawScope dps = { 0 };
        dps.canv = canv;
        dps.bounds = AFX_LAYERED_RECT(0, 0, extent.w, extent.h, 0, 1);
        dps.targetCnt = 1;
        dps.targets[0].clearVal.rgba[0] = 0.3;
        dps.targets[0].clearVal.rgba[1] = 0.1;
        dps.targets[0].clearVal.rgba[2] = 0.3;
        dps.targets[0].clearVal.rgba[3] = 1;
        dps.targets[0].loadOp = avxLoadOp_CLEAR;
        dps.targets[0].storeOp = avxStoreOp_STORE;

        if (AfxSucceded(AvxCmdCommenceDrawScope(dctx, &dps)))
        {
            avxViewport vp = AVX_VIEWPORT(0, 0, extent.w, extent.h, 0, 1);
            AvxCmdAdjustViewports(dctx, 0, 1, &vp);

            //AvxCmdChangeCullMode(dctx, avxCullMode_BACK);
            //AvxCmdChangeFillModeEXT(dctx, avxFillMode_SOLID);
            //AvxCmdSwitchFrontFace(dctx, FALSE);

            // turn off Z buffering, culling, and projection (since we are drawing orthographically)
            //AvxCmdSwitchDepthTesting(dctx, FALSE);

            //AfxBinkBlitFrame(&bnk, dctx);

            AvxCmdBindPipeline(dctx, yv12ToRgbaDtec, NIL, NIL);

            //BinkGetRealtime(bnk->bik, &bnk->rts, 0);

            // Set the textures.
            //AvxCmdBindLegos(cmdb, avxBus_DRAW, 0, 1, &(bnk->rsrc[bnk->buffers.FrameNum].lego));
            //AvxCmdBindSamplers(dctx, avxBus_GFX, 0, 0, 3, (avxSampler[]) { sampler, sampler, sampler });
            //AvxCmdBindRasters(dctx, avxBus_GFX, 0, 0, 3, bnk.rasters);
            AvxCmdBindBuffers(dctx, avxBus_GFX, 0, 0, bnk.hasAlphaPlane ? 4 : 3, (avxBufferedMap[]) {
                AVX_BUFFERED_MAP(bnk.stageBuffers[bnk.buffers.FrameNum], bnk.unpakOff[0], bnk.unpakSiz[0], NIL),
                AVX_BUFFERED_MAP(bnk.stageBuffers[bnk.buffers.FrameNum], bnk.unpakOff[1], bnk.unpakSiz[1], NIL),
                AVX_BUFFERED_MAP(bnk.stageBuffers[bnk.buffers.FrameNum], bnk.unpakOff[2], bnk.unpakSiz[2], NIL),
                AVX_BUFFERED_MAP(bnk.stageBuffers[bnk.buffers.FrameNum], bnk.unpakOff[3], bnk.unpakSiz[3], NIL),
            });

            struct
            {
                int imageWidth;
                int imageHeight;
                afxBool hasAlpha;
            } push =
            {
                bnk.whd.w,
                bnk.whd.h,
                bnk.hasAlphaPlane
            };
            AvxCmdPushConstants(dctx, 0, sizeof(push), &push);

            AvxCmdDraw(dctx, 3, 1, 0, 0); // tristripped quad in shader

            AvxCmdConcludeDrawScope(dctx);
        }

        if (AfxFailed(AvxCompileDrawCommands(dctx)))
        {
            AfxThrowError();
            return err;
        }

        avxSubmission subm = { 0 };
        avxFence dscrCompleteSem = NIL;
        subm.signal = dscrCompleteSem;
        subm.dctx = dctx;

        if (AfxFailed(AvxExecuteDrawCommands(dsys, 1, &subm, NIL)))
        {
            AfxThrowError();
            return err;
        }

        //AfxWaitForDrawQueue(dsys, subm.exuIdx, subm.baseQueIdx, 0);
        AvxWaitForDrawBridges(dsys, AFX_TIMEOUT_INFINITE, subm.exuMask);
    }

    avxPresentation pres = { 0 };
    pres.dout = dout;
    pres.bufIdx = outBufIdx;

    if (AfxFailed(AvxPresentSurfaces(dsys, 1, &pres, NIL)))
    {
        AfxThrowError();
        return err;
    }

    return err;
}

int main(int argc, char const* argv[])
{
    afxError err = AFX_ERR_NONE;

    afxSystemConfig sysCfg = { 0 };
    AfxConfigureSystem(&sysCfg, NIL);
    AfxBootstrapSystem(&sysCfg);

    afxUnit avxIcd = 0;
    avxSystemConfig dsc = { 0 };
    dsc.caps = avxAptitude_GFX;
    dsc.accel = afxAcceleration_DPU;
    dsc.exuCnt = 1;
    AvxConfigureDrawSystem(avxIcd, &dsc);
    AvxEstablishDrawSystem(avxIcd, &dsc, &dsys);
    AFX_ASSERT_OBJECTS(afxFcc_DSYS, 1, &dsys);

    afxUnit amxIcd = 0;
    afxMixSystem msys;
    amxSystemConfig msc = { 0 };
    msc.caps = amxAptitude_SFX;
    msc.accel = afxAcceleration_MPU;
    msc.exuCnt = 1;
    AmxConfigureMixSystem(amxIcd, &msc);
    AmxEstablishMixSystem(amxIcd, &msc, &msys);
    AFX_ASSERT_OBJECTS(afxFcc_MSYS, 1, &msys);

    afxUnit auxIcd = 0;
    afxEnvironment env;
    afxEnvironmentConfig ecfg = { 0 };
    ecfg.dsys = dsys;
    ecfg.msys = msys;
    AfxConfigureEnvironment(auxIcd, &ecfg);
    AfxEstablishEnvironment(auxIcd, &ecfg, &env);

    afxUri2048 uriB;
    AfxMakeUri2048(&uriB, NIL);
    
    //BinkSoundUseDirectSound(0);

    bnk.running |= TRUE;

    bnk.useTbo = TRUE;

    err = AfxSetUpBinkPlayer(&bnk, dsys);
    AFX_ASSERT(!err);
#if 0
    AfxFormatUri(&uriB.uri, "//./a/fmv/t2.bik");
#else
    AfxFormatUri(&uriB.uri, "../fmv/ubi.bik");
    //AfxFormatUri(&uri.uri, "//./a/fmv/disco.bik");
#endif

    err = AfxOpenVideoBink(&bnk, &uriB.uri);
    AFX_ASSERT(!err);

    afxWindow wnd;
    afxWindowConfig wcfg = { 0 };
    wcfg.dout.dsys = dsys;
    wcfg.dout.ccfg.whd.w = bnk.whd.w;
    wcfg.dout.ccfg.whd.h = bnk.whd.h;
    wcfg.dout.ccfg.bins[0].fmt = avxFormat_BGRA8v;
    wcfg.dout.latency = 3;
    AfxConfigureWindow(env, &wcfg, NIL, NIL);
    AfxAcquireWindow(env, &wcfg, &wnd);
    AFX_ASSERT_OBJECTS(afxFcc_WND, 1, &wnd);
    AfxAdjustWindow(wnd, NIL, 0, NIL, &AFX_RECT(0, 0, wcfg.dout.ccfg.whd.w, wcfg.dout.ccfg.whd.h));
    AfxGetWindowSurface(wnd, &dout[0]);
    AFX_ASSERT_OBJECTS(afxFcc_DOUT, 1, &dout[0]);

    AfxMakeUri(&uri.uri, 0, "../qwa-512.tga", 0);
    AfxLoadWindowIcon(wnd, &uri.uri);

    //AfxAdjustDrawOutput(dout[0], bnk.whd);


    avxSamplerConfig smpSpec = { 0 };
    smpSpec.magnify = (smpSpec.minify = avxTexelFilter_LINEAR);
    smpSpec.mipFlt = avxTexelFilter_LINEAR;
    smpSpec.uvw[0] = (smpSpec.uvw[1] = (smpSpec.uvw[2] = avxTexelWrap_EDGE));
    //smpSpec.uvw[1] = avxTexelAddress_REPEAT;
    //smpSpec.uvw[2] = avxTexelAddress_REPEAT;
    AvxConfigureSampler(dsys, &smpSpec);
    AvxAcquireSamplers(dsys, 1, &smpSpec, &sampler);
    AFX_ASSERT_OBJECTS(afxFcc_SAMP, 1, &sampler);

    avxShader codb;
    AvxAcquireShaders(dsys, 1, NIL, &codb);
    AFX_ASSERT_OBJECTS(afxFcc_SHD, 1, &codb);

    avxShaderSpecialization specs[2] = { 0 };
    specs[0].stage = avxShaderType_VERTEX;
    specs[0].prog = AFX_STRING("frameblitVsh");
    specs[1].stage = avxShaderType_FRAGMENT;
    specs[1].prog = AFX_STRING("blitFetchYuv");
    AvxCompileShader(codb, &specs[0].prog, &vsh);
    AvxCompileShader(codb, &specs[1].prog, &fsh);

    avxPipeline pip;
    avxPipelineConfig pipb = { 0 };
    pipb.codb = codb;
    pipb.progCnt = 2;
    pipb.progSpecs = specs;
    pipb.cullMode = avxCullMode_BACK;
    pipb.primTop = avxTopology_TRI_LIST;
    pipb.fillMode = avxFillMode_FACE;
    AvxAssembleGfxPipelines(dsys, 1, &pipb, &yv12ToRgbaDtec);
    AFX_ASSERT_OBJECTS(afxFcc_PIP, 1, &yv12ToRgbaDtec);

    AfxDisposeObjects(1, &codb);

    afxDrawContext contexts[3];
    avxContextConfig dccfg = { 0 };
    dccfg.caps = avxAptitude_GFX;
    if (AvxAcquireDrawContexts(dsys, NIL, &dccfg, 3, contexts))
    {
        AfxThrowError();
    }

#if 0
    afxSink sink;
    AfxGetEnvironmentAmx(env, NIL, &sink);

    afxMixContext mix;
    amxContextConfig mcfg = { 0 };
    AmxAcquireMixContexts(msys, NIL, &mcfg, 1, &mix);
    AFX_ASSERT_OBJECTS(afxFcc_MIX, 1, &mix);

    amxAudio aud;
    amxAudioInfo audi = { 0 };
    audi.sampCnt = 1095920 / 2 / 2;
    audi.chanCnt = 2;
    audi.segCnt = 1;
    audi.fmt = amxFormat_S16i;
    audi.freq = 44100;
    AmxAcquireAudios(msys, 1, &audi, &aud);

    amxBuffer mbuf;
    amxBufferInfo mbufi = { 0 };
    mbufi.usage = amxBufferUsage_MIX;
    mbufi.size = 1095920;
    AmxAcquireBuffers(msys, 1, &mbufi, &mbuf);

    amxMixTarget target = { 0 };
    amxMixScope mscop = { 0 };
    mscop.sink = aud;
    mscop.chanCnt = 1;
    mscop.chans[0].clearAmpl = 1.f;
    AmxCmdCommenceMixScope(mix, &mscop);
    //AmxCmdLoadVoice(mix, 0, /*stream*/0);
    //AmxCmdSinkVoice(mix, 0, /*track*/0);
    AmxCmdConcludeMixScope(mix);

    amxBuffer sampBuf;
    amxBuffer patBuf;
    amxBuffer dataBuf;
    LoadModFile(msys, AfxUri("//./z/../hymn.mod"), &sampBuf, &patBuf, &dataBuf);
    AFX_ASSERT_OBJECTS(afxFcc_MBUF, 1, &sampBuf);
    AFX_ASSERT_OBJECTS(afxFcc_MBUF, 1, &patBuf);
    AFX_ASSERT_OBJECTS(afxFcc_MBUF, 1, &dataBuf);

    amxTracker trax;
    amxTrackerConfig tcfg = { 0 };
    AmxAcquireTracker(msys, &tcfg, &trax);
    AFX_ASSERT_OBJECTS(afxFcc_TRAX, 1, &trax);

    AmxBindBufferedStream(trax, 0, mbuf, 0, mbufi.size, 2, amxFormat_S16i);
    AmxBindBuffer(trax, 0, sampBuf, 0, 0, 0);
    AmxGetSinkTrack(sink, &aud);
    AmxBindAudioTrack(trax, 0, aud, 0, 48000);
    AmxBindSink(trax, sink);
    AmxStartTracker(trax, FALSE);

    //AmxCmdTrack(mix, );
    //AmxCmdTrackIndirect(mix, patBuf);
#endif

    while (1)
    {
        AfxBinkDoFrame(&bnk, TRUE, TRUE);

        afxUnit outBufIdx = 0;
        if (!AvxLockSurfaceBuffer(dout[0], AFX_TIMEOUT_IGNORED, NIL, NIL, &outBufIdx))
        {
            afxDrawContext dctx = contexts[outBufIdx];
            if (DoVideo(dout[0], outBufIdx, dctx))
            {
                // In case of error, unlock the buffer.
                AvxUnlockSurfaceBuffer(dout[0], outBufIdx);
            }
        }

#if 0
#if 0
        amxBufferedTrack room;
        if (!AmxLockSinkBuffer(sink, 0, 512, &room))
        {
            AmxExecuteMixCommands(mix, room.frameCnt);
            AmxUnlockSinkBuffer(sink, NIL);
        }
#else
        amxSubmission subm = { 0 };
        subm.mctx = mix;
        AmxExecuteMixCommands(msys, 1, &subm);
#endif
#endif
        AfxDoUx(NIL, AFX_TIMEOUT_INFINITE);

        if (!AfxSystemIsExecuting())
            break;

        AfxSleep(1);
    }

    AfxDropVideoBink(&bnk);

    AfxDisposeObjects(1, &wnd);
    AfxDisposeObjects(1, &env);
    AfxDisposeObjects(1, &dsys);

    AfxAbolishSystem(0);
    Sleep(3000);
    return 0;
}
