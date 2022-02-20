#ifndef SHARED_COLLATZ_HPP
#define SHARED_COLLATZ_HPP

#include <assert.h>
#include <memory>
#include <future>

#include "sharedresults.hpp"
#include "contest.hpp"

inline uint64_t calc_shared_collatz_threads(InfInt n, std::shared_ptr<SharedResults> shared_result) {
    uint64_t count = 0;
    assert(n > 0);
    InfInt start = n;

    while (n != 1) {
        uint64_t res = shared_result->get_result(n);
        if (res) {
            shared_result->add_result(start, res + count);
            return res + count;
        } else {
            if (n % 2 == 1) {
                n *= 3;
                n += 1;
            } else {
                n /= 2;
            }
        }
        count++;
    }
    shared_result->add_result(start, count);
    return count;
}

/*
 * W przypadku zespołów Team*ProcessesX wyniki częściowe dla liczb mniejszych
 * od part_res_capacity są zapisywane w pamięci współdzielonej w tablicy partial_result.
 */
inline uint64_t calc_shared_collatz_proc(InfInt n, uint64_t part_res_capacity, uint64_t* partial_results) {
    uint64_t count = 0;
    assert(n > 0);
    InfInt start = n;

    while (n != 1) {
        if (n < part_res_capacity && start < part_res_capacity && *(partial_results + n.toInt()) != 0) {
            *(partial_results + start.toInt()) = *(partial_results + n.toInt()) + count;
            return *(partial_results + start.toInt());
        } else {
            if (n % 2 == 1) {
                n *= 3;
                n += 1;
            } else {
                n /= 2;
            }
        }
        count++;
    }
    if (start < part_res_capacity) {
        *(partial_results + start.toInt()) = count;
    }
    return count;
}

#endif