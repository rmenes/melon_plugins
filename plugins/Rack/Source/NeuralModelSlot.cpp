// NeuralModelSlot.cpp - the only file that talks to the NeuralAudio engine.
// If a future NeuralAudio version changes its API, this is the file to adapt.
#include "NeuralModelSlot.h"

#include <NeuralAudio/NeuralModel.h>
#include <filesystem>
#include <exception>
#include <utility>

namespace rack
{
namespace
{
// NeuralModel::CreateFromFile takes a std::filesystem::path in some versions and a std::string
// in others. A path built with u8path keeps non-ASCII folder names working on Windows.
template <class P>
auto createModel (const P& path, int) -> decltype (NeuralAudio::NeuralModel::CreateFromFile (path))
{
    return NeuralAudio::NeuralModel::CreateFromFile (path);
}

template <class P>
auto createModel (const P& path, long) -> decltype (NeuralAudio::NeuralModel::CreateFromFile (path.string()))
{
    return NeuralAudio::NeuralModel::CreateFromFile (path.string());
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
        m->model = createModel (path, 0);
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

    m->modelRate = rate > 1000.0 ? rate : 48000.0;
    m->normDb    = (float) m->model->GetRecommendedOutputDBAdjustment();
    return m;
}

void LoadedModel::prepare (double hostRate, int maxBlock)
{
    resampler.prepare (hostRate, modelRate, maxBlock, kModelChunk);

    // Warm the model up with silence so the first real block does not click
    float silence[kModelChunk] = {}, out[kModelChunk];
    for (int i = 0; i < 8; ++i)
        model->Process (silence, out, kModelChunk);
}

void LoadedModel::process (float* io, int numSamples)
{
    resampler.process (io, numSamples, [this] (const float* in, float* out, int n) {
        model->Process (const_cast<float*> (in), out, n);
    });
}

} // namespace rack
