#ifndef FASTMODE_SOUNDTOUCH_BRIDGE_H
#define FASTMODE_SOUNDTOUCH_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

void *st_create(unsigned int sampleRate, unsigned int channels);
void st_destroy(void *context);
void st_clear(void *context);
unsigned int st_process(
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
