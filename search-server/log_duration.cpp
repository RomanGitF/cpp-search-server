#include "log_duration.h"

LogDuration::LogDuration(std::string_view id, std::ostream& dst_stream)
    : id_(id)
    , dst_stream_(dst_stream) {
}

LogDuration::~LogDuration() {
    using namespace std::chrono;
    using namespace std::literals;

    const auto end_time = Clock::now();
    const auto dur = end_time - start_time_;
    dst_stream_ << id_ << ": "sv << duration_cast<milliseconds>(dur).count() << " ms"sv << std::endl;
}