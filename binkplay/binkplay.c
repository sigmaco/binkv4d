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

avxRaster dumpImg = NIL;

afxWindow wnd;
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

afxClock startClock, lastClock;
afxReal64 ft = 0;
afxUnit fpsi = 0;
afxUnit fps = 0;

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

    TEXTURE(0, 0, sampler2D, samp0);
    TEXTURE(0, 1, sampler2D, samp1);
    TEXTURE(0, 2, sampler2D, samp2);

    in block
    {
        vec2 uv0;
    } sgl_v;

    OUT(0, vec4, sgl_rgba);

    void main()
    {
        vec3 yuv;
        yuv.x = texture(samp0, sgl_v.uv0).r;
        yuv.y = texture(samp2, sgl_v.uv0).r - 0.5;
        yuv.z = texture(samp1, sgl_v.uv0).r - 0.5;

        vec3 rgb = mat3(1, 1, 1, 0, -0.34414, 1.772, 1.402, -0.71414, 0) * yuv;
        sgl_rgba = vec4(rgb, 1);
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
        AfxBinkUnpackFrame(dctx, &bnk.buffers, bnk.stageBuffers, bnk.rasters, bnk.unpakOff, bnk.hasAlphaPlane);

        avxCanvas canv;
        afxLayeredRect bounds;
        AvxGetSurfaceCanvas(dout, outBufIdx, &canv, &bounds);
        AFX_ASSERT_OBJECTS(afxFcc_CANV, 1, &canv);

        afxBool readjust = TRUE;
        afxBool upscale = FALSE;
        afxRect crop = AFX_RECT(bounds.area.x, bounds.area.y, bnk.whd.w, bnk.whd.h);

        if (readjust)
        {
            crop.w = bounds.area.w;
            crop.h = bounds.area.h;
        }

        if (!upscale)
        {
            crop.w = AFX_CLAMP(crop.w, 1, bnk.whd.w);
            crop.h = AFX_CLAMP(crop.h, 1, bnk.whd.h);
        }

        avxDrawScope dps = { 0 };
        dps.canv = canv;
        dps.bounds.area = crop;
        dps.bounds.layerCnt = 1;
        dps.targetCnt = 1;
        dps.targets[0].clearVal.rgba[0] = 0.3;
        dps.targets[0].clearVal.rgba[1] = 0.1;
        dps.targets[0].clearVal.rgba[2] = 0.3;
        dps.targets[0].clearVal.rgba[3] = 1;
        dps.targets[0].loadOp = avxLoadOp_CLEAR;
        dps.targets[0].storeOp = avxStoreOp_STORE;
        if (AfxSucceded(AvxCmdCommenceDrawScope(dctx, &dps)))
        {
            avxViewport vp = AVX_VIEWPORT(crop.x, crop.y, crop.w, crop.h, 0, 1);
            //AvxFlipViewport(&vp, &vp, FALSE);
            AvxCmdAdjustViewports(dctx, 0, 1, &vp);

            //AvxCmdChangeCullMode(dctx, avxCullMode_BACK);
            //AvxCmdChangeFillModeEXT(dctx, avxFillMode_SOLID);
            //AvxCmdSwitchFrontFace(dctx, FALSE);

            // turn off Z buffering, culling, and projection (since we are drawing orthographically)
            //AvxCmdSwitchDepthTesting(dctx, FALSE);

            //AfxBinkBlitFrame(&bnk, dctx);

            AvxCmdBindPipeline(dctx, yv12ToRgbaDtec, NIL, NIL);

            // Set the textures.
            AvxCmdBindRasters(dctx, avxBus_GFX, 0, 0, bnk.hasAlphaPlane ? 4 : 3, bnk.rasters);
            AvxCmdBindSamplers(dctx, avxBus_GFX, 0, 0, bnk.hasAlphaPlane ? 4 : 3, (avxSampler[]) { sampler, sampler, sampler, sampler });

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

    afxClock currClock;
    AfxGetClock(&currClock);
    afxReal64 ct = AfxGetSecondsElapsed(&startClock, &currClock);
    afxReal64 dt = AfxGetSecondsElapsed(&lastClock, &currClock);
    lastClock = currClock;
    if (ct - ft >= 1.0)
    {
        fps = fpsi;
        fpsi = 0;
        ft = ct;
    }
    ++fpsi;
    AfxFormatWindowTitle(wnd, "FPS %u %u", fps, 0);

    return err;
}

int main(int argc, char const* argv[])
{
    afxError err = AFX_ERR_NONE;

    afxSystemConfig sysCfg = { 0 };
    AfxConfigureSystem(&sysCfg, NIL);
    AfxBootstrapSystem(&sysCfg);

    afxUnit avxIcd = 0;
    avxSystemConfig dsyc = { 0 };
    dsyc.caps = avxAptitude_GFX;
    dsyc.accel = afxAcceleration_DPU;
    dsyc.exuCnt = 1;
    AvxConfigureDrawSystem(avxIcd, &dsyc);
    AvxEstablishDrawSystem(avxIcd, &dsyc, &dsys);
    AFX_ASSERT_OBJECTS(afxFcc_DSYS, 1, &dsys);

    afxUnit auxIcd = 0;
    afxEnvironment env;
    afxEnvironmentConfig ecfg = { 0 };
    ecfg.dsys = dsys;
    AfxConfigureEnvironment(auxIcd, &ecfg);
    AfxEstablishEnvironment(auxIcd, &ecfg, &env);
    AFX_ASSERT_OBJECTS(afxFcc_ENV, 1, &env);

    afxUri2048 uriB;
    AfxMakeUri2048(&uriB, NIL);
    
    //BinkSoundUseDirectSound(0);

    bnk.running |= TRUE;

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
    specs[1].prog = AFX_STRING("rgbOutYuvSigma");
    AvxCompileShader(codb, &specs[0].prog, &vsh);
    AvxCompileShader(codb, &specs[1].prog, &fsh);

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
    avxContextConfig dctxi = { 0 };
    dctxi.caps = avxAptitude_GFX;
    if (AvxAcquireDrawContexts(dsys, NIL, &dctxi, 3, contexts))
    {
        AfxThrowError();
    }

    AfxGetClock(&startClock);
    lastClock = startClock;

    while (1)
    {
        AfxBinkDoFrame(&bnk, TRUE, TRUE);

        afxUnit outBufIdx = 0;
        if (!AvxLockSurfaceBuffer(dout[0], AFX_TIMEOUT_IGNORED, NIL, NIL, &outBufIdx))
        {
            if (DoVideo(dout[0], outBufIdx, contexts[outBufIdx]))
            {
                AvxUnlockSurfaceBuffer(dout[0], outBufIdx);
            }
        }

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
