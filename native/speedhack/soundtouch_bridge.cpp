#include "soundtouch_bridge.h"

#include <new>

#include "SoundTouch.h"

struct SoundTouchContext {
    soundtouch::SoundTouch processor;
    unsigned int channels;
    float tempo;
};

extern "C" void *st_create(unsigned int sampleRate, unsigned int channels) {
    if (!sampleRate || !channels || channels > SOUNDTOUCH_MAX_CHANNELS) return nullptr;
    try {
        SoundTouchContext *context = new SoundTouchContext();
        context->channels = channels;
        context->tempo = 1.0f;
        context->processor.setSampleRate(sampleRate);
        context->processor.setChannels(channels);
        context->processor.setRate(1.0);
        context->processor.setPitch(1.0);
        context->processor.setTempo(1.0);
        context->processor.setSetting(SETTING_USE_QUICKSEEK, 0);
        context->processor.setSetting(SETTING_SEQUENCE_MS, 20);
        context->processor.setSetting(SETTING_SEEKWINDOW_MS, 10);
        context->processor.setSetting(SETTING_OVERLAP_MS, 8);
        return context;
    } catch (...) {
        return nullptr;
    }
}

extern "C" void st_destroy(void *opaque) {
    delete static_cast<SoundTouchContext *>(opaque);
}

extern "C" void st_clear(void *opaque) {
    SoundTouchContext *context = static_cast<SoundTouchContext *>(opaque);
    if (!context) return;
    try {
        context->processor.clear();
    } catch (...) {
    }
}

extern "C" unsigned int st_process(
    void *opaque,
    const float *input,
    unsigned int inputFrames,
    float tempo,
    float *output,
    unsigned int outputFrames) {
    SoundTouchContext *context = static_cast<SoundTouchContext *>(opaque);
    if (!context || !input || !inputFrames || !output || !outputFrames || tempo <= 0.0f)
        return 0;
    try {
        if (tempo != context->tempo) {
            context->processor.clear();
            context->processor.setTempo(tempo);
            context->tempo = tempo;
        }
        context->processor.putSamples(input, inputFrames);
        return context->processor.receiveSamples(output, outputFrames);
    } catch (...) {
        context->processor.clear();
        return 0;
    }
}
