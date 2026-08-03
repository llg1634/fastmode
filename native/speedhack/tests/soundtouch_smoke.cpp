#include <cmath>
#include <cstdio>
#include <vector>

#include "../soundtouch_bridge.h"

int main() {
    constexpr unsigned int sampleRate = 48000;
    constexpr unsigned int channels = 2;
    constexpr unsigned int inputFrames = 960;
    constexpr unsigned int outputFrames = 480;
    constexpr float frequency = 440.0f;
    constexpr float tempo = 2.0f;
    constexpr float pi = 3.14159265358979323846f;
    void *context = st_create(sampleRate, channels);
    std::vector<float> input(inputFrames * channels);
    std::vector<float> output(outputFrames * channels);
    std::vector<float> collected;
    unsigned long long inputPosition = 0;

    if (!context) return 2;
    for (int block = 0; block < 400; block++) {
        for (unsigned int frame = 0; frame < inputFrames; frame++) {
            float sample = std::sin(2.0f * pi * frequency *
                                    static_cast<float>(inputPosition + frame) / sampleRate);
            input[frame * channels] = sample;
            input[frame * channels + 1] = sample;
        }
        inputPosition += inputFrames;
        unsigned int received = st_process(
            context, input.data(), inputFrames, tempo, output.data(), outputFrames);
        for (unsigned int frame = 0; frame < received; frame++)
            collected.push_back(output[frame * channels]);
    }
    st_destroy(context);

    if (collected.size() < sampleRate) return 3;
    size_t start = collected.size() / 2;
    size_t end = collected.size();
    unsigned int crossings = 0;
    for (size_t i = start + 1; i < end; i++) {
        if (collected[i - 1] <= 0.0f && collected[i] > 0.0f) crossings++;
    }
    float seconds = static_cast<float>(end - start) / sampleRate;
    float measuredFrequency = crossings / seconds;
    std::printf("tempo=%.2f measured_frequency=%.2f output_frames=%zu\n",
                tempo, measuredFrequency, collected.size());
    return measuredFrequency > 420.0f && measuredFrequency < 460.0f ? 0 : 4;
}
