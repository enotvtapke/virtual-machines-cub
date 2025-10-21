#include <iostream>
#include <random>
#include <vector>
#include <chrono>
#include <numeric>
#include <algorithm>
#include <sys/mman.h>
#include <bits/stdc++.h>

#include "header.h"

constexpr unsigned int REPEATS = 200'000;

inline std::mt19937& get_random_engine() {
    static std::mt19937 engine(std::random_device{}());
    return engine;
}

char* memory;

double time(const int stride, const int spots_num) {
    std::vector<int> indices(spots_num);
    std::iota(indices.begin(), indices.end(), 0);
    std::ranges::shuffle(indices, get_random_engine());

    for (int i = 0; i < spots_num - 1; ++i) {
        * (void **) (memory + indices[i] * stride) = memory + indices[i + 1] * stride;
    }
    * (void **) (memory + indices[spots_num - 1] * stride) = memory + indices[0] * stride;

    // Warmup
    auto current = (void *) memory;
    for (unsigned int i = 0; i < REPEATS; ++i) {
        current = * (void **) current;
    }
    if (current == (void *) 256) {
        std::cout << "Impossible!" << std::endl;
    }

    current = (void *) memory;
    const auto start_time = std::chrono::high_resolution_clock::now();
    for (unsigned int i = 0; i < REPEATS; ++i) {
        current = * (void **) current;
    }
    const auto end_time = std::chrono::high_resolution_clock::now();

    if (current == (void *) 256) {
        std::cout << "Impossible!" << std::endl;
    }
    return static_cast<double>((end_time - start_time).count()) / REPEATS;
}

Table<int, double> cache_assoc_experiment(const int max_memory, const int max_assoc, const int max_stride) {
    std::vector<std::vector<double>> times{};
    std::vector<int> strides{};
    int stride = 16;
    while (stride * max_assoc <= max_memory) {
        int spots_num = 1;
        std::vector<double> times_for_stride{};
        while (spots_num <= max_assoc) {
            const auto current_time = time(stride, spots_num);
            times_for_stride.push_back(current_time);
            ++spots_num;
        }
        times.push_back(times_for_stride);
        strides.push_back(stride);
        stride *= 2;
        if (stride > max_stride) {
            break;
        }
    }
    return {strides, times};
}

std::vector<double> time_for_stride(const int stride, const int spots_num) {
    std::vector<double> times(spots_num);
    for (int spots = 0; spots < spots_num; ++spots) {
        times[spots] = time(stride, spots + 1);
    }
    return times;
}

Table<int, double> cache_line_size_experiment(const int max_memory, const int max_spots, const int max_stride) {
    Table<int, double> table{{}, {}};

    int higher_stride = 16;
    int lower_stride = higher_stride / 2;
    while ((higher_stride + lower_stride) * max_spots <= max_memory) {
        const auto times_higher_stride = time_for_stride(higher_stride, max_spots);
        const auto times_higher_lower_stride = time_for_stride(higher_stride + lower_stride, max_spots);

        table.index_column.push_back(higher_stride);
        table.data.push_back(times_higher_stride);
        table.index_column.push_back(higher_stride + lower_stride);
        table.data.push_back(times_higher_lower_stride);

        higher_stride *= 2;
        lower_stride = higher_stride / 2;
        if (higher_stride + lower_stride > max_stride) {
            break;
        }
    }
    return table;
}

void setup_affinity(const int cpu_id) {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu_id, &mask);
    if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) != 0) {
        std::cerr << "ERROR: Could not set CPU affinity" << std::endl;
    }
}

template<typename T>
void print_vector(const std::vector<T>& data) {
    for (const auto& d : data) {
        std::cout << d << " ";
    }
    std::cout << "\n";
}

template<typename T>
void print_vector(const std::vector<std::vector<T>>& data) {
    for (const auto& row : data) {
        for (const auto& element : row) {
            std::cout << element << " ";
        }
        std::cout << "\n";
    }
}

void analyze_jumps_for_assoc(const Table<int, size_t>& data) {
    const std::vector<size_t> assocs = data.data.back();
    for (const auto assoc: assocs) {
        printf("Entity have assoc %lu\n", assoc);
        for (int i = 0; i < data.index_column.size() - 1; ++i) {
            if (std::ranges::all_of(data.data[i], [&](const size_t jump) { return !similar(jump, assoc, 0.26); }) &&
                std::ranges::any_of(data.data[i], [&](const size_t jump) { return similar(jump, assoc * 2, 0.26); }) &&
                std::ranges::any_of(data.data[i + 1], [&](const size_t jump) { return similar(jump, assoc, 0.26); })) {
                printf("Entity with assoc %lu has entity stride %d Bytes and size %lu Bytes\n",
                       assoc, data.index_column[i + 1], assoc * data.index_column[i + 1]);
                break;
            }
        }
    }
}

void analyze_jumps_for_line_size(const Table<int, size_t>& jump_table) {
    for (int i = 0; i < jump_table.index_column.size() - 3; i += 2) {
        if (!jump_table.data[i].empty() && !jump_table.data[i + 1].empty() && similar(jump_table.data[i][0], jump_table.data[i + 1][0], 0.4) &&
            !jump_table.data[i + 2].empty() && !jump_table.data[i + 3].empty() && static_cast<double>(jump_table.data[i + 3][0]) / jump_table.data[i + 2][0] > 1.7) {
            printf("Entity has line size %d\n", jump_table.index_column[i]);
            break;
        }
    }
}

int main() {
    setup_affinity(0);
    constexpr int max_memory = 512 * 1024 * 1024;
    memory = (char *) mmap(nullptr, max_memory, PROT_READ | PROT_WRITE,MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    for (int i = 0; i < 1; ++i) {
        {
            constexpr int max_spots = 2000;

            const auto times = cache_line_size_experiment(max_memory, max_spots, 2 * 1024);
            // const auto times = Table<int, double>::from_csv("./cache_line_size_table.csv");
            times.print("./cache_line_size_table.csv", true, true);

            std::vector<std::vector<size_t>> jumps;
            for (int i = 0; i < times.index_column.size(); ++i) {
                jumps.push_back(jumpIndices(times.data[i], 80, 1.3, 0.2, 80));
            }
            const Table jump_table(times.index_column, jumps);
            jump_table.print("./cache_line_size_jump_table.csv", false, false);
            analyze_jumps_for_line_size(jump_table);
        }
        {
            const auto times = cache_assoc_experiment(max_memory, 48, 1 * 1024 * 1024);
            // const auto times = Table<int, double>::from_csv("./cache_assoc_table.csv");
            times.print("./cache_assoc_table.csv", true, true);

            std::vector<std::vector<size_t>> jumps;
            for (int i = 0; i < times.index_column.size(); ++i) {
                jumps.push_back(jumpIndices(times.data[i], 4, 1.3, 0.2, 48));
            }
            const Table jump_table(times.index_column, jumps);
            jump_table.print("./cache_assoc_jump_table.csv", false, false);
            analyze_jumps_for_assoc(jump_table);
        }
    }
    return 0;
}