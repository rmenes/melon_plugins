// NeuralModelSlot.cpp - the only file that talks to the NeuralAudio engine.
// Written for NeuralAudio commit 29b90ac (Nov 2026). If its API changes, this is the file to adapt.
#include "NeuralModelSlot.h"

#include <NeuralAudio/NeuralModel.h>
#include <filesystem>
#include <exception>
#include <utility>

namespace mnam
{
namespace
{
// One loader for the whole plugin: it only holds settings (load modes, buffer size).
NeuralAudio::NeuralModelLoader& modelLoader()
{
    static NeuralAudio::NeuralModelLoader loader = [] {
        NeuralAudio::NeuralModelLoader l;
        l.SetDefaultMaxAudioBufferSize (LoadedModel::kModelChunk);
        return l;
    }();
    return loader;
}
} // namespace

LoadedModel::~LoadedModel()
{
    delete model;
}

std::unique_ptr<LoadedModel> LoadedModel::load (const std::string& utf8Path, double rate, std::string& error)
{
    std::unique_ptr<LoadedModel> m (new LoadedModel());
    try
    {
#if defined(__cpp_char8_t) // C++20: u8path is deprecated, build the path from UTF-8 text instead
        const std::filesystem::path path (std::u8string (utf8Path.begin(), utf8Path.end()));
#else
        const auto path = std::filesystem::u8path (utf8Path);
#endif
        m->model = modelLoader().CreateFromFile (path);
    }
    catch (const std::exception& e)
    {
        error = e.what();
        return nullptr;
    }
    catch (...)
    {
        error = "unknown error";
        return nullptr;
    }

    if (m->model == nullptr)
    {
        error = "unsupported or damaged model file";
        return nullptr;
    }

    // The engine reads the rate from the file (48 kHz when the file has none)
    const double reported = (double) m->model->GetSampleRate();
    m->modelRate = reported > 1000.0 ? reported : (rate > 1000.0 ? rate : 48000.0);
    m->normDb    = (float) m->model->GetRecommendedOutputDBAdjustment();
    return m;
}

void LoadedModel::prepare (double hostRate, int maxBlock)
{
    resampler.prepare (hostRate, modelRate, maxBlock, kModelChunk);

    // Warm the model up with silence so the first real block does not click
    float silence[kModelChunk] = {}, out[kModelChunk];
    for (int i = 0; i < 8; ++i)
        model->Process (silence, out, (size_t) kModelChunk);
}

void LoadedModel::process (float* io, int numSamples)
{
    resampler.process (io, numSamples, [this] (const float* in, float* out, int n) {
        model->Process (in, out, (size_t) n);
    });
}

} // namespace mnam
