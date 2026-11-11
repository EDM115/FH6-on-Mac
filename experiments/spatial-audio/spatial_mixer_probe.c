/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Exported from the October 2026 FH6 experiments, see THIRD_PARTY_NOTICES.md
 */
/* Silent offline capability check for the mixer configuration used by the audio patch */
#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHANNELS 12
#define FRAMES 512
#define MAX_FRAMES 4096

struct Fixture {
    UInt32 channels, active;
    double rate;
    unsigned long long sample;
    float planes[CHANNELS][MAX_FRAMES];
};

static OSStatus source(void *opaque, AudioUnitRenderActionFlags *flags, const AudioTimeStamp *time, UInt32 bus, UInt32 frames, AudioBufferList *data)
{
    struct Fixture *f = opaque;
    (void)flags; (void)time; (void)bus;
    if (frames > MAX_FRAMES || data->mNumberBuffers != f->channels) return kAudio_ParamError;
    for (UInt32 c = 0; c < f->channels; c++) {
        memset(f->planes[c], 0, frames * sizeof(float));
        data->mBuffers[c].mData = f->planes[c];
        data->mBuffers[c].mDataByteSize = frames * sizeof(float);
        data->mBuffers[c].mNumberChannels = 1;
    }
    for (UInt32 i = 0; i < frames; i++) {
        float v = 0.05f * sin(2.0 * M_PI * (f->active == 3 ? 80.0 : 440.0) * (f->sample + i) / f->rate);
        if (f->active == 3) {
            /* The patch folds LFE into the front pair at -6 dB for built-in speakers */
            f->planes[0][i] = f->planes[1][i] = 0.5f * v;
        } else f->planes[f->active][i] = v;
    }
    f->sample += frames;
    return noErr;
}

static OSStatus set(AudioUnit unit, AudioUnitPropertyID property, AudioUnitScope scope, const void *data, UInt32 size)
{
    OSStatus status = AudioUnitSetProperty(unit, property, scope, 0, data, size);
    if (status) fprintf(stderr, "property %u scope %u failed: %d\n", (unsigned)property, (unsigned)scope, (int)status);
    return status;
}

static int check(UInt32 channels, UInt32 active, double rate)
{
    static const AudioChannelLabel labels[CHANNELS] = {kAudioChannelLabel_Left, kAudioChannelLabel_Right, kAudioChannelLabel_Center, kAudioChannelLabel_LFEScreen, kAudioChannelLabel_RearSurroundLeft, kAudioChannelLabel_RearSurroundRight, kAudioChannelLabel_LeftSurround, kAudioChannelLabel_RightSurround, kAudioChannelLabel_LeftTopFront, kAudioChannelLabel_RightTopFront, kAudioChannelLabel_LeftTopRear, kAudioChannelLabel_RightTopRear};
    static const char *names[CHANNELS] = {"FL", "FR", "FC", "LFE-fold", "BL", "BR", "SL", "SR", "TFL", "TFR", "TBL", "TBR"};
    AudioComponentDescription description = {kAudioUnitType_Mixer, kAudioUnitSubType_SpatialMixer, kAudioUnitManufacturer_Apple, 0, 0};
    AudioComponent component = AudioComponentFindNext(NULL, &description);
    AudioUnit unit = NULL;
    struct Fixture *fixture = calloc(1, sizeof(*fixture));
    size_t layout_size = offsetof(AudioChannelLayout, mChannelDescriptions) + channels * sizeof(AudioChannelDescription);
    AudioChannelLayout *layout = calloc(1, layout_size);
    AudioBufferList *output = calloc(1, offsetof(AudioBufferList, mBuffers) + 2 * sizeof(AudioBuffer));
    float left[FRAMES], right[FRAMES];
    double sum[2] = {0, 0};
    unsigned samples = 0;
    int result = 1;
    OSStatus status;
    if (!component || !fixture || !layout || !output) goto done;
    if ((status = AudioComponentInstanceNew(component, &unit))) { fprintf(stderr, "new mixer: %d\n", (int)status); goto done; }
    fixture->channels = channels;
    fixture->active = active;
    fixture->rate = rate;
    AudioStreamBasicDescription in = {0};
    in.mSampleRate = rate;
    in.mFormatID = kAudioFormatLinearPCM;
    in.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved;
    in.mBytesPerPacket = in.mBytesPerFrame = sizeof(float);
    in.mFramesPerPacket = 1;
    in.mChannelsPerFrame = channels;
    in.mBitsPerChannel = 32;
    AudioStreamBasicDescription out = in;
    out.mChannelsPerFrame = 2;
    layout->mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelDescriptions;
    layout->mNumberChannelDescriptions = channels;
    for (UInt32 c = 0; c < channels; c++) layout->mChannelDescriptions[c].mChannelLabel = labels[c];
    UInt32 one = 1, max_frames = MAX_FRAMES, algorithm = kSpatializationAlgorithm_UseOutputType;
    UInt32 mode = kSpatialMixerSourceMode_AmbienceBed, type = kSpatialMixerOutputType_BuiltInSpeakers;
    AURenderCallbackStruct callback = {source, fixture};
    if (set(unit, kAudioUnitProperty_ElementCount, kAudioUnitScope_Input, &one, sizeof one) ||
        set(unit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, &max_frames, sizeof max_frames) ||
        set(unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, &in, sizeof in) ||
        set(unit, kAudioUnitProperty_AudioChannelLayout, kAudioUnitScope_Input, layout, (UInt32)layout_size) ||
        set(unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, &out, sizeof out) ||
        set(unit, kAudioUnitProperty_SpatializationAlgorithm, kAudioUnitScope_Input, &algorithm, sizeof algorithm) ||
        set(unit, kAudioUnitProperty_SpatialMixerSourceMode, kAudioUnitScope_Input, &mode, sizeof mode) ||
        set(unit, kAudioUnitProperty_SpatialMixerOutputType, kAudioUnitScope_Global, &type, sizeof type) ||
        set(unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, &callback, sizeof callback)) goto done;
    if ((status = AudioUnitInitialize(unit))) { fprintf(stderr, "initialize: %d\n", (int)status); goto done; }
    output->mNumberBuffers = 2;
    for (unsigned block = 0; block < 96; block++) {
        memset(left, 0, sizeof left);
        memset(right, 0, sizeof right);
        output->mBuffers[0] = (AudioBuffer){1, sizeof left, left};
        output->mBuffers[1] = (AudioBuffer){1, sizeof right, right};
        AudioTimeStamp stamp = {0};
        stamp.mFlags = kAudioTimeStampSampleTimeValid;
        stamp.mSampleTime = (double)block * FRAMES;
        AudioUnitRenderActionFlags flags = 0;
        if ((status = AudioUnitRender(unit, &flags, &stamp, 0, FRAMES, output))) { fprintf(stderr, "render: %d\n", (int)status); goto done; }
        if (block >= 8) {
            for (unsigned i = 0; i < FRAMES; i++) {
                if (!isfinite(left[i]) || !isfinite(right[i])) goto done;
                sum[0] += left[i] * left[i];
                sum[1] += right[i] * right[i];
            }
            samples += FRAMES;
        }
    }
    double rms_left = sqrt(sum[0] / samples), rms_right = sqrt(sum[1] / samples);
    result = rms_left + rms_right > 1e-5 ? 0 : 1;
    printf("rate=%.0f channels=%u source=%s rms_left=%.8f rms_right=%.8f %s\n", rate, (unsigned)channels, names[active], rms_left, rms_right, result ? "FAIL" : "PASS");
done:
    if (unit) AudioComponentInstanceDispose(unit);
    free(fixture);
    free(layout);
    free(output);
    return result;
}

int main(void)
{
    unsigned failed = 0, checked = 0;
    const double rates[] = {44100, 48000};
    const UInt32 channel_counts[] = {8, 12};
    for (unsigned r = 0; r < 2; r++) for (unsigned n = 0; n < 2; n++) for (UInt32 c = 0; c < channel_counts[n]; c++) {
        failed += check(channel_counts[n], c, rates[r]);
        checked++;
    }
    printf("RESULT: %s (%u/%u channel/rate checks passed, output rendered only to memory)\n", failed ? "FAIL" : "PASS", checked-failed, checked);
    return failed ? 1 : 0;
}
