#define _CRT_SECURE_NO_WARNINGS 1
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SAMPLE_RATE 44100
#define BUFFER_SIZE 512
#define NUM_CHANNELS 4
#define NUM_ROWS 64
#define TICKS_PER_ROW 6
#define MAX_PATTERNS 64
#define MAX_ORDERS 128

#include "qwadro/afxQwadro.h"
#include "../binq/afxBinkVideo.h"

arxSimulation sim = NIL;
avxRaster dumpImg = NIL;

afxSurface dout = NIL;
afxDrawSystem dsys = NIL;
afxEnvironment env = NIL;

#pragma pack(push, 1)

typedef struct
{
    char name[22];
    uint16_t length;     // in words (2 bytes), big endian
    uint8_t finetune;
    uint8_t volume;
    uint16_t repeatPoint;  // big endian
    uint16_t repeatLength; // big endian
} ModSampleHeader;

typedef struct
{
    char songName[20];
    ModSampleHeader samples[31];
    uint8_t songLength;
    uint8_t unused;
    uint8_t patternTable[128];
    char signature[4];
} ModHeader;

typedef struct
{
    uint8_t data[4];  // 4 bytes per note
} ModNote;

typedef struct
{
    ModNote notes[64][4];  // 64 rows, 4 channels
} ModPattern;

#pragma pack(pop)

uint16_t be16(const uint8_t *p)
{
    return (p[0] << 8) | p[1];
}

AFX_STATIC_ASSERT(offsetof(ModHeader, signature) == 1080, "");

typedef struct {
    int note;        // e.g. 48 = C-4
    int instrument;  // sample index
    int effect_type;
    int effect_param;
} Note;

Note patterns[MAX_PATTERNS][64][NUM_CHANNELS]; // Output

// Standard ProTracker period table (for Amiga tuning)
int period_to_note(uint16_t period) {
    static const uint16_t period_table[] = {
        1712,1616,1524,1440,1356,1280,1208,1140,1076,1016, 960, 906,
         856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
         428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226
    };

    for (int i = 0; i < sizeof(period_table) / sizeof(period_table[0]); ++i) {
        if (period >= period_table[i]) return i;
    }
    return -1; // unknown period
}

Note decode_mod_note(uint8_t* data) {
    Note n = { 0 };

    uint16_t raw0 = data[0];
    uint16_t raw1 = data[1];
    uint16_t raw2 = data[2];
    uint16_t raw3 = data[3];

    uint16_t period = ((raw0 & 0x0F) << 8) | raw1;
    n.note = (period > 0) ? period_to_note(period) : -1;

    n.instrument = ((raw0 & 0xF0) >> 4) | (raw2 & 0xF0);
    n.effect_type = raw2 & 0x0F;
    n.effect_param = raw3;

    return n;
}

void parse_pattern(uint8_t* pattern_data, int pattern_index, Note dest_patterns[MAX_PATTERNS][NUM_ROWS][NUM_CHANNELS]) {
    Note(*out)[NUM_CHANNELS] = dest_patterns[pattern_index];

    for (int row = 0; row < 64; ++row) {
        for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
            int offset = (row * NUM_CHANNELS + ch) * 4;
            out[row][ch] = decode_mod_note(&pattern_data[offset]);
        }
    }
}

int LoadModFile2(afxMixSystem msys, afxUri const* uri, ModHeader* fhdr, ModSampleHeader shdr[], Note patt[][NUM_ROWS][NUM_CHANNELS], afxByte data[])
{
    afxError err = NIL;

    AFX_ASSERT(shdr);
    AFX_ASSERT(fhdr);
    AFX_ASSERT(patt);

    afxStream file;
    if (AfxOpenFile(uri, afxFileFlag_R, &file))
    {
        AfxThrowError();
        return 1;
    }

    // Read header
    ModHeader header;
    if (AfxReadStream2(file, sizeof(ModHeader), 1, &header, 1))
    {
        // Failed to read header.
        AfxThrowError();
        AfxDisposeObjects(1, &file);
        return 1;
    }
    *fhdr = header;

    afxBool notOriginal = strncmp(header.signature, "M.K.", 4) == 0;
    printf("Song name: %.20s\n", header.songName);
    printf("Song length: %d\n", header.songLength);
    printf("Signature: %.4s\n", header.signature);

    printf("Signature: %.4s (raw hex: %02X %02X %02X %02X)\n",
        header.signature,
        header.signature[0], header.signature[1],
        header.signature[2], header.signature[3]);

#if 0
    amxBuffer sbuf;
    amxBufferInfo bufi = { 0 };
    bufi.usage = amxBufferUsage_MIX;
    bufi.flags = amxBufferFlag_RW;
    bufi.size = sizeof(ModSampleHeader) * 31;

    AmxAcquireBuffers(msys, 1, &bufi, &sbuf);
#endif

    afxSize offset = 0;

    ModSampleHeader s2[32];

    // Samples info
    for (int i = 0; i < 31; i++)
    {
        ModSampleHeader *s = &header.samples[i];
        uint16_t length = (s->length >> 8) | (s->length << 8);  // swap endian
        //length = ntohs(s->length);  // better to do ntohs here, but include <arpa/inet.h> on Unix.
        // Alternatively:
        //length = be16((uint8_t*)&s->length);

        uint16_t repeatPoint = be16((uint8_t*)&s->repeatPoint);
        uint16_t repeatLength = be16((uint8_t*)&s->repeatLength);

        printf("Sample %2d: %.22s, length: %d words (%.1f KB), volume: %d, repeat: %d, repeat length: %d\n",
            i + 1, s->name, length, length * 2 / 1024.0, s->volume, repeatPoint, repeatLength);

        s2[i] = *s;
        s2[i].length = length;
        s2[i].repeatLength = repeatLength;
        s2[i].repeatPoint = repeatPoint;
        shdr[i] = s2[i];
    }

#if 0
    amxBufferIo op = { 0 };
    op.rowCnt = sizeof(s2);
    op.srcStride = 1;
    op.dstStride = 1;
    op.dstOffset = offset;
    AmxUpdateBuffer(sbuf, 1, &op, &s2, NIL);
    AmxWaitForMixSystem(msys, AFX_TIMEOUT_INFINITE);
    offset += op.rowCnt;
#endif

    // Calculate number of patterns
    int maxPattern = 0;
    for (int i = 0; i < 128; i++)
    {
        if (header.patternTable[i] > maxPattern) maxPattern = header.patternTable[i];
    }
    int numPatterns = maxPattern + 1;

    printf("Number of patterns: %d\n", numPatterns);

#if 0
    amxBuffer pbuf;
    amxBufferInfo pbufi = { 0 };
    pbufi.usage = amxBufferUsage_MIX;
    pbufi.flags = amxBufferFlag_RW;
    pbufi.size = sizeof(ModPattern) * numPatterns;
    AmxAcquireBuffers(msys, 1, &pbufi, &pbuf);
#endif

    // Read patterns
    ModPattern *patterns = malloc(sizeof(ModPattern) * numPatterns);
    if (!patterns)
    {
        //Failed to allocate memory for patterns

        AfxThrowError();
        AfxDisposeObjects(1, &file);
        return 1;
    }

    size_t patternsSize = numPatterns * sizeof(ModPattern);
    // Pattern size is always 1024 bytes each; we can fread directly
    if (AfxReadStream(file, patternsSize, 0, patterns))
    {
        // Failed to read pattern data.
        free(patterns);

        AfxThrowError();
        AfxDisposeObjects(1, &file);
        return 1;
    }

    for (int i = 0; i < numPatterns; ++i) {
        parse_pattern(((afxByte*)patterns) + i * 1024, i, patt);
    }

#if 0
    amxBufferIo op2 = { 0 };
    op2.rowCnt = patternsSize;
    op2.srcStride = 1;
    op2.dstStride = 1;
    //op2.dstOffset = offset;
    //AmxUploadBuffer(pbuf, 1, &op2, patterns, NIL);
    AmxUpdateBuffer(pbuf, 1, &op2, patterns, NIL);
    AmxWaitForMixSystem(msys, AFX_TIMEOUT_INFINITE);
    //offset += op2.rowCnt;
#endif

    // Example: print first note of first pattern first channel
    ModNote *note = &patterns[0].notes[0][0];
    uint8_t b0 = note->data[0];
    uint8_t b1 = note->data[1];
    uint8_t b2 = note->data[2];
    uint8_t b3 = note->data[3];

    int sampleNum = ((b0 & 0xF0) >> 4) | (b2 & 0xF0);
    int period = ((b0 & 0x0F) << 8) | b1;
    int effect = ((b2 & 0x0F) << 8) | b3;

    printf("First note (pattern 0, row 0, channel 0): sample %d, period %d, effect 0x%03X\n", sampleNum, period, effect);

    // Read samples data
    // Sum all sample lengths to know how much to read
    int totalSampleBytes = 0;
    uint16_t sampleLengths[31];
    for (int i = 0; i < 31; i++)
    {
        uint16_t len = be16((uint8_t*)&header.samples[i].length) * 2;
        sampleLengths[i] = len;
        totalSampleBytes += len;
    }

    uint8_t *sampleData = malloc(totalSampleBytes);

    if (!sampleData)
    {
        //Failed to allocate memory for samples
        free(patterns);

        AfxThrowError();
        AfxDisposeObjects(1, &file);
        return 1;
    }

    if (AfxReadStream(file, totalSampleBytes, 0, sampleData))
    {
        // Failed to read sample data
        free(sampleData);
        free(patterns);

        AfxThrowError();
        AfxDisposeObjects(1, &file);
        return 1;
    }

    AfxCopy(data, sampleData, totalSampleBytes);

#if 0
    amxBuffer dbuf;
    amxBufferInfo dbufi = { 0 };
    dbufi.usage = amxBufferUsage_MIX;
    dbufi.flags = amxBufferFlag_RW;
    dbufi.size = totalSampleBytes;
    AmxAcquireBuffers(msys, 1, &dbufi, &dbuf);

    amxBufferIo op3 = { 0 };
    op3.rowCnt = totalSampleBytes;
    op3.srcStride = 1;
    op3.dstStride = 1;
    //op3.dstOffset = offset;
    //AmxUploadBuffer(dbuf, 1, &op3, sampleData, NIL);
    AmxUpdateBuffer(dbuf, 1, &op3, sampleData, NIL);
    AmxWaitForMixSystem(msys, AFX_TIMEOUT_INFINITE);
    //offset += op3.rowCnt;
#endif

    printf("Loaded %d bytes of sample data\n", totalSampleBytes);

    // At this point:
    // - header contains song info and samples info
    // - patterns contains all pattern notes
    // - sampleData contains raw PCM samples (signed 8-bit)

    // Remember samples are signed 8-bit, no headers in sample data block

#if 0
    *sampBuf = sbuf;
    *patBuf = pbuf;
    *dataBuf = dbuf;
#endif

    // Clean up
    free(sampleData);
    free(patterns);
    AfxDisposeObjects(1, &file);

    return 0;
}

typedef struct {
    int8_t* data;
    int length;
    int loop_start;
    int loop_length;
    int is_looping;
} Sample;

typedef struct {
    Sample* sample;
    float sample_pos;
    float sample_inc;
    int volume;
    int is_playing;
} Channel;

typedef struct {
    int bpm;
    int speed;
    int tick;
    int row;
    int order;               // current order index
    float tick_timer;
    int samples_per_tick;

    Note*patterns[MAX_PATTERNS]; // pointers to pattern data
    int pattern_rows[MAX_PATTERNS]; // row counts per pattern
    int order_list[MAX_ORDERS];  // pattern indices
    int order_length;            // how many orders in list
    int num_rows;
} TrackerState;

Sample samples[31] = { 0 };
Channel channels[NUM_CHANNELS] = {0};
TrackerState tracker = {0};

float note_to_freq(int note) {
    return 440.0f * powf(2.0f, (note - 57) / 12.0f);
}

float freq_to_inc(float freq) {
    return freq / SAMPLE_RATE;
}

void trigger_note(Channel* ch, Note* note) {
    if (note->instrument < 0 || note->note < 0) return;
    
    Sample* s = &samples[note->instrument];
    ch->sample = s;
    ch->sample_pos = 0;
    ch->volume = 48;
    ch->is_playing = 1;

    float freq = note_to_freq(note->note);
    ch->sample_inc = freq_to_inc(freq);
}

void mix_audio(float* output, int frames) {
    memset(output, 0, sizeof(float) * frames * 2);

    for (int i = 0; i < frames; ++i) {
        float left = 0.0f, right = 0.0f;

        for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
            Channel* c = &channels[ch];
            if (!c->is_playing || !c->sample)
                continue;

            int pos = (int)c->sample_pos;
            if (pos >= c->sample->length) {
                c->is_playing = 0;
                continue;
            }

            float val = c->sample->data[pos] / 128.0f;
            val *= c->volume / 64.0f;

            if (ch == 0 || ch == 2) left += val;
            else right += val;

            c->sample_pos += c->sample_inc;
        }

        if (left > 1.0f) left = 1.0f;
        if (left < -1.0f) left = -1.0f;
        if (right > 1.0f) right = 1.0f;
        if (right < -1.0f) right = -1.0f;

        output[i * 2] = left;
        output[i * 2 + 1] = right;
    }
}

void tracker_step(int frames) {
    tracker.tick_timer += frames;

    while (tracker.tick_timer >= tracker.samples_per_tick) {
        tracker.tick_timer -= tracker.samples_per_tick;

        int current_pattern_idx = tracker.order_list[tracker.order];
        int current_pattern_rows = 64;// tracker.pattern_rows[current_pattern_idx];

        if (tracker.tick == 0) {
            for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                Note* note = &patterns[current_pattern_idx][tracker.row][ch];

                trigger_note(&channels[ch], note);
            }
        }

        tracker.tick++;
        if (tracker.tick >= tracker.speed) {
            tracker.tick = 0;
            tracker.row++;

            if (tracker.row >= current_pattern_rows) {
                tracker.row = 0;
                tracker.order++;
                if (tracker.order >= tracker.order_length) {
                    tracker.order = 0; // loop song
                }
            }
        }
    }
}

void audio_callback(void* userdata, afxByte* stream, int len) {
    int frames = len / sizeof(float) / 2; // stereo float
    float* out = (float*)stream;

    tracker_step(frames);
    mix_audio(out, frames);
}

afxError DoVideo(afxSurface dout, afxUnit outBufIdx, afxDrawContext dctx)
{
    afxError err = AFX_ERR_NONE;

    afxUnit queIdx = 0;
    afxUnit portId = 0;
    afxUnit fdbId;
    if (AvxPrepareDrawCommands(dctx, FALSE, avxCmdFlag_ONCE))
    {
        AfxThrowError();
        return err;
    }

    afxRect crc;
    avxCanvas canv;
    AvxGetSurfaceCanvas(dout, outBufIdx, &canv, &crc);
    AFX_ASSERT_OBJECTS(afxFcc_CANV, 1, &canv);

    avxDrawScope dps = { 0 };
    dps.canv = canv;
    dps.bounds = AFX_LAYERED_RECT(0, 0, crc.w, crc.h, 0, 1);
    dps.targetCnt = 1;
    dps.targets[0].clearVal.rgba[0] = 0.3;
    dps.targets[0].clearVal.rgba[1] = 0.1;
    dps.targets[0].clearVal.rgba[2] = 0.3;
    dps.targets[0].clearVal.rgba[3] = 1;
    dps.targets[0].loadOp = avxLoadOp_CLEAR;
    dps.targets[0].storeOp = avxStoreOp_STORE;

    AvxCmdCommenceDrawScope(dctx, &dps);
    {
        avxViewport vp = AVX_VIEWPORT(0, 0, crc.w, crc.h, 0, 1);
        AvxCmdAdjustViewports(dctx, 0, 1, &vp);

    }
    AvxCmdConcludeDrawScope(dctx);

    if (AvxCompileDrawCommands(dctx))
    {
        AfxThrowError();
        return err;
    }

    avxSubmission subm = { 0 };
    avxFence dscrCompleteSem = NIL;
    subm.signal = dscrCompleteSem;
    subm.dctx = dctx;

    if (AvxExecuteDrawCommands(dsys, 1, &subm, NIL))
    {
        AfxThrowError();
        return err;
    }

    //AfxWaitForDrawQueue(dsys, subm.exuIdx, subm.baseQueIdx, 0);
    AvxWaitForDrawBridges(dsys, AFX_TIMEOUT_INFINITE, subm.exuMask);

    avxPresentation pres = { 0 };
    pres.dout = dout;
    pres.bufIdx = outBufIdx;

    if (AvxPresentSurfaces(dsys, 1, &pres, NIL))
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

    afxUnit dIcd = 0;
    avxSystemConfig dsc = { 0 };
    dsc.caps = avxAptitude_GFX;
    dsc.accel = afxAcceleration_DPU;
    dsc.exuCnt = 1;
    AvxConfigureDrawSystem(dIcd, &dsc);
    AvxEstablishDrawSystem(dIcd, &dsc, &dsys);
    AFX_ASSERT_OBJECTS(afxFcc_DSYS, 1, &dsys);

    afxUnit mIcd = 0;
    afxMixSystem msys;
    amxSystemConfig msc = { 0 };
    msc.accel = afxAcceleration_MPU;
    msc.caps = amxAptitude_SFX | amxAptitude_SINK;
    msc.exuCnt = 1;
    AmxConfigureMixSystem(mIcd, &msc);
    AmxEstablishMixSystem(mIcd, &msc, &msys);
    AFX_ASSERT_OBJECTS(afxFcc_MSYS, 1, &msys);

    afxUnit shIcd = 0;
    afxEnvironment env;
    afxEnvironmentConfig ecfg = { 0 };
    ecfg.dsys = dsys;
    ecfg.msys = msys;
    AfxConfigureEnvironment(shIcd, &ecfg);
    AfxEstablishEnvironment(shIcd, &ecfg, &env);

    afxWindow wnd;
    afxWindowConfig wcfg = { 0 };
    wcfg.dout.dsys = dsys;
    wcfg.dout.ccfg.whd.w = 1280;
    wcfg.dout.ccfg.whd.h = 720;
    wcfg.dout.ccfg.bins[0].fmt = avxFormat_BGRA8v;
    wcfg.dout.latency = 3;
    AfxConfigureWindow(env, &wcfg, NIL, NIL);
    AfxAcquireWindow(env, &wcfg, &wnd);
    AFX_ASSERT_OBJECTS(afxFcc_WND, 1, &wnd);
    AfxAdjustWindow(wnd, NIL, 0, NIL, &AFX_RECT(0, 0, wcfg.dout.ccfg.whd.w, wcfg.dout.ccfg.whd.h));

    AfxGetWindowSurface(wnd, &dout);
    AFX_ASSERT_OBJECTS(afxFcc_DOUT, 1, &dout);


    afxDrawContext contexts[3];
    avxContextConfig dcti = { 0 };
    dcti.caps = avxAptitude_GFX;
    if (AvxAcquireDrawContexts(dsys, NIL, &dcti, 3, contexts))
    {
        AfxThrowError();
    }

    afxSink sink;
    AfxGetEnvironmentAudio(NIL, &sink);

    afxMixContext mix;
    amxContextConfig mcfg = { 0 };
    AmxAcquireMixContexts(msys, NIL, &mcfg, 1, &mix);
    AFX_ASSERT_OBJECTS(afxFcc_MIX, 1, &mix);


    amxTracker trax;
    amxTrackerConfig tcfg = { 0 };
    AmxAcquireTracker(msys, &tcfg, &trax);
    AFX_ASSERT_OBJECTS(afxFcc_TRAX, 1, &trax);
    AmxBindSink(trax, sink);

    amxAudio aud;
    AmxGetSinkTrack(sink, &aud);
    amxBuffer sinkBuf = AmxGetAudioBuffer(aud);

    AmxBindAudioTrack(trax, 0, aud, 0, 48000);

    amxMixTarget target = { 0 };
    amxMixScope mscop = { 0 };
    mscop.sink = aud;
    mscop.chanCnt = 1;
    mscop.chans[0].clearAmpl = 1.f;
    AmxCmdCommenceMixScope(mix, &mscop);
    //AmxCmdLoadVoice(mix, 0, /*stream*/0);
    //AmxCmdSinkVoice(mix, 0, /*track*/0);
    AmxCmdConcludeMixScope(mix);

    ModHeader mhdr;
    ModSampleHeader mshdr[31];
    ModPattern mpat[128];
    afxByte mdata[4096*16];
    LoadModFile2(msys, AfxUri("//./z/../hymn.mod"), &mhdr, &mshdr[0], patterns, &mdata);

    afxUnit offset = 0;
    for (afxUnit i = 0; i < 31; i++)
    {
        Sample* s = &samples[i];
        s->length = mhdr.samples[i].length;
        s->loop_length = mhdr.samples[i].repeatLength;
        s->loop_start = mhdr.samples[i].repeatPoint;
        s->data = &mdata[offset];
        offset += s->length * 2;
    }

#if 0
    amxBuffer sampBuf;
    amxBuffer patBuf;
    amxBuffer dataBuf;
    AFX_ASSERT_OBJECTS(afxFcc_MBUF, 1, &sampBuf);
    AFX_ASSERT_OBJECTS(afxFcc_MBUF, 1, &patBuf);
    AFX_ASSERT_OBJECTS(afxFcc_MBUF, 1, &dataBuf);

    AmxBindBuffer(mix, 0, sampBuf, 0, 0, 0);
#endif
    //AmxCmdTrack(mix, );
    //AmxCmdTrackIndirect(mix, patBuf);

    tracker.bpm = 125;
    tracker.speed = TICKS_PER_ROW;
    tracker.tick = 0;
    tracker.row = 0;
    tracker.tick_timer = 0;
    //tracker.patterns = &patterns[0][0][0];
    tracker.num_rows = 64;
    tracker.samples_per_tick = (SAMPLE_RATE * 5) / (2 * tracker.bpm);

    AfxStream2(60, mhdr.patternTable, 1, tracker.order_list, 4);
    tracker.order_length = 60; // hardcoded from hymn.mod

    AmxStartTracker(trax, FALSE);

    amxBuffer mbuf;
    amxBufferInfo mbufi = { 0 };
    mbufi.usage = amxBufferUsage_MIX;
    mbufi.size = 1095920;
    mbufi.fmt = amxFormat_S16i;
    AmxAcquireBuffers(msys, 1, &mbufi, &mbuf);

    void* sinkBufPtr;
    AmxMapBuffer(mbuf, 0, 0, 0, &sinkBufPtr);

    amxVoicingInfo vi = { 0 };
    vi.srcBuf = mbuf;
    vi.srcRange = mbufi.size;
    vi.srcStride = sizeof(afxInt16) * 2;
    vi.iterCnt = AFX_U32_MAX;    
    vi.sampleRate = 48000;
    AmxFeedVoice(trax, 0, &vi);
    //AmxSetVoiceFrequencyRatio(trax, 0, 0.1);
    AmxSwitchVoice(trax, 0, 1);

    while (1)
    {
        afxUnit outBufIdx = 0;
        if (!AvxLockSurfaceBuffer(dout, AFX_TIMEOUT_IGNORED, NIL, NIL, &outBufIdx))
        {
            afxDrawContext dctx = contexts[outBufIdx];
            if (DoVideo(dout, outBufIdx, dctx))
            {
                // In case of error, unlock the buffer.
                AvxUnlockSurfaceBuffer(dout, outBufIdx);
            }
        }
#if 0
        amxBufferedTrack room;
        if (!AmxLockSinkBuffer(sink, 0, 512, &room))
        {
            AmxExecuteMixCommands(mix, room.frameCnt);
            AmxWaitForMixSystem(msys, AFX_TIMEOUT_INFINITE);
            audio_callback(NIL, sinkBufPtr, 512);
            AmxUnlockSinkBuffer(sink, NIL);
        }
#else
        audio_callback(NIL, sinkBufPtr, 512);

#endif
        AfxDoUx(NIL, AFX_TIMEOUT_IGNORED);

        if (!AfxSystemIsExecuting())
            break;
    }

    AfxDisposeObjects(1, &wnd);
    AfxDisposeObjects(1, &env);
    AfxDisposeObjects(1, &dsys);

    AfxDoSystemShutdown(0);
    Sleep(3000);
    return 0;
}
