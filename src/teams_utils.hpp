#ifndef TEAMS_UTILS_HPP
#define TEAMS_UTILS_HPP

#include <utility>
#include <deque>
#include <future>
#include <sys/wait.h>
#include <sys/mman.h>
#include <unistd.h>

#include "shared_collatz.hpp"

void future_collatz(ContestInput& inputs, std::shared_ptr<SharedResults>& shared_results,
                    std::promise<ContestResult>& result_promise) {
    ContestResult res;
    res.resize(inputs.size());
    uint64_t idx = 0;

    for (InfInt const & input : inputs) {
        if (!shared_results) {
            res[idx] = calcCollatz(input);
        } else {
            res[idx] = calc_shared_collatz_threads(input, shared_results);
        }
        ++idx;
    }
    result_promise.set_value(res);
}

std::vector<ContestInput> split_into_const_sized_blocks(ContestInput contestInput, uint32_t block_size) {
    std::vector<ContestInput> inputs;
    uint64_t input_size = contestInput.size();
    inputs.resize((input_size / block_size) + (input_size % block_size != 0));
    uint64_t block = 0;
    uint32_t idx = 0;
    for (InfInt const & singleInput : contestInput) {
        inputs[block].push_back(singleInput);
        idx++;
        if (idx == block_size) {
            idx = 0;
            block++;
        }
    }
    return inputs;
}

std::vector<ContestInput> split_into_n_blocks(ContestInput contestInput, uint32_t blocks) {
    std::vector<ContestInput> inputs;
    inputs.resize(blocks);
    for (uint64_t i = 0; i < contestInput.size(); i++) {
        inputs[i % blocks].push_back(contestInput[i]);
    }
    return inputs;
}

uint64_t* new_shared_array(uint64_t size) {
    uint64_t* res;
    int fd_memory = -1;
    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_SHARED | MAP_ANONYMOUS;

    res = (uint64_t*) mmap(NULL, size * sizeof(uint64_t), prot, flags,
                           fd_memory, 0);

    if (res == MAP_FAILED) {
        std::cerr << "Error in mmap\n";
        exit(1);
    }
    return res;
}

void wait_for_children(uint32_t children) {
    for (uint32_t i = 0; i < children; i++) {
        if (wait(0) == -1) {
            std::cerr << "Error in wait\n";
            exit(1);
        }
    }
}

#endif //TEAMS_UTILS_HPP
