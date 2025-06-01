#ifndef PROFILER_HPP
#define PROFILER_HPP

#include <chrono>
#include <string>
#include <iostream>

class ScopeTimer {
public:
    ScopeTimer(const std::string& name, std::vector<long long>* output = nullptr)
        : label(name), start(std::chrono::steady_clock::now()), results(output) {}

    ~ScopeTimer() {
        auto end = std::chrono::steady_clock::now();
        #ifdef SECONDS
            auto duration = std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count();
            std::cout << "[Profiler] " << label << ": " << duration << " s" << std::endl;
            if (results)
                results->push_back(duration);
        #else
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
            std::cout << "[Profiler] " << label << ": " << duration << " µs" << std::endl;
            if (results)
                results->push_back(duration);
        #endif
        
    }

private:
    std::string label;
    std::chrono::steady_clock::time_point start;
    std::vector<long long>* results = nullptr;
};

#endif
