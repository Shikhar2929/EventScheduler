#ifndef PROFILER_HPP
#define PROFILER_HPP

#include <chrono>
#include <string>
#include <iostream>

class ScopeTimer {
public:
    ScopeTimer(const std::string& name)
        : label(name), start(std::chrono::steady_clock::now()) {}

    ~ScopeTimer() {
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        std::cout << "[Profiler] " << label << ": " << duration << " µs\n";
    }

private:
    std::string label;
    std::chrono::steady_clock::time_point start;
};

#endif
