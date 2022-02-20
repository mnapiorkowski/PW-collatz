#include <utility>
#include <deque>
#include <future>
#include <unistd.h>

#include "teams.hpp"
#include "teams_utils.hpp"
#include "contest.hpp"
#include "collatz.hpp"
#include "shared_collatz.hpp"

ContestResult TeamNewThreads::runContestImpl(ContestInput const & contestInput) {
    ContestResult r;
    uint32_t THREADS = getSize();
    std::shared_ptr<SharedResults> shared_results = getSharedResults();

    // dzielę input na bloki o rozmiarze THREADS
    std::vector<ContestInput> inputs = split_into_const_sized_blocks(contestInput, THREADS);

    // tworzę THREADS promise'ów
    std::promise<ContestResult> result_promise[THREADS];
    std::future<ContestResult> result_future[THREADS];
    for (uint32_t i = 0; i < THREADS; i++) {
        result_future[i] = result_promise[i].get_future();
    }

    std::thread threads[THREADS];
    for(ContestInput input : inputs) {
        ContestInput single_input[input.size()];

        // tworzę jednocześnie co najwyżej THREADS wątków
        // i każdy oblicza tylko jeden wynik
        for (uint32_t i = 0; i < input.size(); i++) {
            single_input[i].push_back(input[i]);
            threads[i] = createThread([&, i] {
                future_collatz(single_input[i], shared_results, result_promise[i]);
            });
        }

        // wypełniam vector wynikowy, resetuję promise'y i joinuję wątki
        for (uint32_t i = 0; i < input.size(); i++) {
            ContestResult res = result_future[i].get();
            r.push_back(res.front());
            result_promise[i] = std::promise<ContestResult>();
            result_future[i] = result_promise[i].get_future();
            threads[i].join();
        }
    }
    return r;
}

ContestResult TeamConstThreads::runContestImpl(ContestInput const & contestInput) {
    ContestResult r;
    r.resize(contestInput.size());
    uint32_t THREADS = getSize();
    std::shared_ptr<SharedResults> shared_results = getSharedResults();

    // dzielę input na THREADS bloków o zrównoważonych rozmiarach
    std::vector<ContestInput> inputs = split_into_n_blocks(contestInput, THREADS);

    // tworzę THREADS promise'ów
    std::promise<ContestResult> result_promise[THREADS];
    std::future<ContestResult> result_future[THREADS];
    for (uint32_t i = 0; i < THREADS; i++) {
        result_future[i] = result_promise[i].get_future();
    }

    // tworzę THREADS wątków
    std::thread threads[THREADS];
    for (uint32_t i = 0; i < THREADS; i++) {
        threads[i] = createThread([&, i] {
            future_collatz(inputs[i], shared_results, result_promise[i]);
        });
    }

    // wypełniam vector wynikowy i joinuję wątki
    for (uint32_t i = 0; i < THREADS; i++) {
        ContestResult res = result_future[i].get();
        for (uint64_t j = 0; j < res.size(); j++) {
            uint64_t idx = (THREADS * j) + i;
            r[idx] = res[j];
        }
        threads[i].join();
    }
    return r;
}

ContestResult TeamPool::runContest(ContestInput const & contestInput) {
    ContestResult r;
    uint64_t input_size = contestInput.size();
    uint64_t idx = 0;
    std::shared_ptr<SharedResults> shared_results = getSharedResults();
    r.resize(input_size);

    std::future<uint64_t> result_future[input_size];

    // wrzucam do puli wątków obliczenie każdego wyniku z osobna
    for(InfInt const & singleInput : contestInput) {
        if (!shared_results) {
            result_future[idx] = pool.push(calcCollatz, singleInput);
        } else {
            result_future[idx] = pool.push(calc_shared_collatz_threads, singleInput, shared_results);
        }
        idx++;
    }

    // wypełniam vector wynikowy
    for(uint64_t i = 0; i < input_size; i++) {
        r[i] = result_future[i].get();
    }
    return r;
}

ContestResult TeamNewProcesses::runContest(ContestInput const & contestInput) {
    ContestResult r;
    uint64_t input_size = contestInput.size();
    r.resize(input_size);
    uint32_t PROC = getSize();
    std::shared_ptr<SharedResults> shared_results = getSharedResults();
    pid_t pid;

    // przyłączam anonimową pamięć współdzieloną, do której zapisywane będą wyniki
    uint64_t* res = new_shared_array(input_size);

    uint64_t part_res_capacity = 1000000;
    uint64_t* partial_results;
    if (shared_results) {
        // przyłączam anonimową pamięć współdzieloną na wyniki częściowe
        partial_results = new_shared_array(part_res_capacity);
    }

    // dzielę input na bloki o rozmiarze PROC
    std::vector<ContestInput> inputs = split_into_const_sized_blocks(contestInput, PROC);

    uint64_t idx = 0;
    for (ContestInput input : inputs) {

        // tworzę co najwyżej PROC procesów potomnych jednocześnie
        for (uint32_t i = 0; i < input.size(); i++) {
            switch (pid = fork()) {
                case -1:
                    std::cerr << "Error in fork\n";
                    exit(1);

                case 0:
                    // każde dziecko oblicza wynik dla swojego pojedynczego inputu
                    // i zapisuje go w pamięci współdzielonej
                    if (!shared_results) {
                        res[idx] = calcCollatz(input[i]);
                    } else {
                        res[idx] = calc_shared_collatz_proc(input[i], part_res_capacity, partial_results);
                    }
                    exit(0);
            }
            idx++;
        }

        // czekanie na zakończenie procesow potomnych
        wait_for_children(input.size());
    }

    // przypisuję vectorowi wynikowemu wyniki z pamięci współdzielonej
    r.assign(res, res + input_size);
    return r;
}

ContestResult TeamConstProcesses::runContest(ContestInput const & contestInput) {
    ContestResult r;
    uint64_t input_size = contestInput.size();
    r.resize(input_size);
    uint32_t PROC = getSize();
    std::shared_ptr<SharedResults> shared_results = getSharedResults();
    pid_t pid;

    // przyłączam anonimową pamięć współdzieloną, do której zapisywane będa wyniki
    uint64_t* res = new_shared_array(input_size);

    uint64_t part_res_capacity = 1000000;
    uint64_t* partial_results;
    if (shared_results) {
        // przyłączam anonimową pamięć współdzieloną na wyniki częściowe
        partial_results = new_shared_array(part_res_capacity);
    }

    // dzielę input na PROC bloków o zrównoważonych rozmiarach
    std::vector<ContestInput> inputs = split_into_n_blocks(contestInput, PROC);

    // tworzę PROC procesów potomnych
    for (uint32_t i = 0; i < PROC; i++) {
        switch (pid = fork()) {
            case -1:
                std::cerr << "Error in fork\n";
                exit(1);

            case 0:
                // każde dziecko oblicza wyniki dla swojego bloku danych
                // i zapisuje je w pamięci współdzielonej
                uint64_t j = 0;
                for(InfInt const & input : inputs[i]) {
                    uint64_t idx = (PROC * j) + i;
                    if (!shared_results) {
                        res[idx] = calcCollatz(input);
                    } else {
                        res[idx] = calc_shared_collatz_proc(input, part_res_capacity, partial_results);
                    }
                    j++;
                }
                exit(0);
        }
    }

    // czekanie na zakończenie procesow potomnych
    wait_for_children(PROC);

    // przypisuję vectorowi wynikowemu wyniki z pamięci współdzielonej
    r.assign(res, res + input_size);
    return r;
}

ContestResult TeamAsync::runContest(ContestInput const & contestInput) {
    ContestResult r;
    uint64_t input_size = contestInput.size();
    uint64_t idx = 0;
    std::shared_ptr<SharedResults> shared_results = getSharedResults();
    r.resize(input_size);

    std::future<uint64_t> result_future[input_size];

    // asynchronicznie obliczam każdy wynik z osobna
    for(InfInt const & singleInput : contestInput) {
        if (!shared_results) {
            result_future[idx] = std::async(std::launch::async | std::launch::deferred,
                                            calcCollatz, singleInput);
        } else {
            result_future[idx] = std::async(std::launch::async | std::launch::deferred,
                                            calc_shared_collatz_threads, singleInput, shared_results);
        }
        ++idx;
    }

    // wypełniam vector wynikowy
    for(uint64_t i = 0; i < input_size; i++) {
        r[i] = result_future[i].get();
    }
    return r;
}
