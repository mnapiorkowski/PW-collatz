#ifndef SHAREDRESULTS_HPP
#define SHAREDRESULTS_HPP

#include <map>
#include <mutex>
#include <shared_mutex>

#include "lib/infint/InfInt.h"

class SharedResults {
public:
    SharedResults() = default;

    uint64_t get_result(InfInt n) {
        std::shared_lock<std::shared_timed_mutex> reader(mutex);
        auto res = results.find(n);
        if (res != results.end()) {
            return res->second;
        } else {
            return 0;
        }
    }

    void add_result(InfInt n, uint64_t count) {
        std::unique_lock<std::shared_timed_mutex> writer(mutex);
        results.insert({n, count});
    }

private:
    std::map<InfInt, uint64_t> results;
    std::shared_timed_mutex mutex;
};

#endif