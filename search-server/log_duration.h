#pragma once

#include <chrono>
#include <iostream>

#define PROFILE_CONCAT_INTERNAL(X, Y) X##Y
#define PROFILE_CONCAT(X, Y) PROFILE_CONCAT_INTERNAL(X, Y)
#define UNIQUE_VAR_NAME_PROFILE PROFILE_CONCAT(profileGuard, __LINE__)

#define LOG_DURATION_STREAM(...) LogDuration UNIQUE_VAR_NAME_PROFILE(__VA_ARGS__)


class LogDuration {
public:

    using Clock = std::chrono::steady_clock;



    LogDuration(const std::string& id, std::ostream& out = std::cerr)
        : id_(id)
        , out_(out)
    {
    }


    ~LogDuration() {
        using namespace std::chrono;
        using namespace std::literals;

        const auto end_time = Clock::now();
        const auto dur = end_time - start_time_;
        out_ << id_ << ": "s << duration_cast<milliseconds>(dur).count() << " ms"s << std::endl;
    }

private:
    const std::string id_;
    const Clock::time_point start_time_ = Clock::now();
    std::ostream& out_ = std::cerr;
    
};