// NeuralModelSlot.h - one loaded neural model (NAM .nam, AIDA-X .aidax or RTNeural .json)
// running through the NeuralAudio engine, with sample-rate conversion when the host rate
// differs from the rate the model was trained at.
//
// Plain C++ (no JUCE). The NeuralAudio headers are only included in the .cpp.
#pragma once

#include "Resampler.h"
#include <memory>
#include <string>

namespace NeuralAudio
{
class NeuralModel;
}

namespace rack
{
class LoadedModel
{
public:
    ~LoadedModel();

    // Loads a model file. `modelRate` = the rate the model was trained at (read from the file's
    // metadata by the caller; 48000 when unknown). Returns nullptr and fills `error` on failure.
    // Not real-time safe: call it from a background thread.
    static std::unique_ptr<LoadedModel> load (const std::string& utf8Path, double modelRate, std::string& error);

    // Not real-time safe (allocates). Must be called before process().
    void prepare (double hostRate, int maxBlock);
    void reset() { resampler.reset(); }

    // Mono, in place. numSamples <= maxBlock given to prepare().
    void process (float* io, int numSamples);

    int    latencySamples() const { return resampler.latencySamples(); }
    bool   isResampling() const { return resampler.isResampling(); }
    double modelSampleRate() const { return modelRate; }

    // Output level change (dB) that brings the model to a standard loudness (0 if the file has no loudness data)
    float normalizeDb() const { return normDb; }

    // Largest block given to the model at once (NeuralAudio's default maximum buffer size)
    static constexpr int kModelChunk = 128;

private:
    LoadedModel() = default;

    NeuralAudio::NeuralModel* model = nullptr;
    ResampledProcessor        resampler;
    double                    modelRate = 48000.0;
    float                     normDb    = 0.0f;
};

} // namespace rack
