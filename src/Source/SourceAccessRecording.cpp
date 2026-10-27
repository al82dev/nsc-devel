#include "nativesharp/Source/SourceAccessRecording.h"

#include <algorithm>

namespace nativesharp::source
{
namespace
{
// The innermost recording of this thread; each recording links to the one it replaced.
// La grabacion mas interna de este hilo; cada grabacion enlaza con la que reemplazo.
thread_local SourceAccessRecording* activeRecording = nullptr;
}

SourceAccessRecording::SourceAccessRecording() noexcept
    : previous_(activeRecording)
{
    activeRecording = this;
}

SourceAccessRecording::~SourceAccessRecording()
{
    activeRecording = previous_;
}

void SourceAccessRecording::noteAccess(FileId file)
{
    for (SourceAccessRecording* recording = activeRecording; recording != nullptr; recording = recording->previous_)
        recording->fileIds_.insert(file.value());
}

std::vector<FileId> SourceAccessRecording::files() const
{
    std::vector<std::uint32_t> values(fileIds_.begin(), fileIds_.end());
    std::ranges::sort(values);
    std::vector<FileId> result;
    result.reserve(values.size());
    for (const std::uint32_t value : values) result.emplace_back(value);
    return result;
}
}
