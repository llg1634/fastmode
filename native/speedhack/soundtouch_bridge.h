#ifndef CE_SOUNDTOUCH_BRIDGE_H
#define CE_SOUNDTOUCH_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

void *ce_soundtouch_create(unsigned int sampleRate, unsigned int channels);
void ce_soundtouch_destroy(void *context);
void ce_soundtouch_clear(void *context);
unsigned int ce_soundtouch_process(
    void *context,
    const float *input,
    unsigned int inputFrames,
    float tempo,
    float *output,
    unsigned int outputFrames);

#ifdef __cplusplus
}
#endif

#endif
