#pragma once
#include <vector>
#include <bits/stdc++.h>

#include "header.h"

std::vector<size_t> jumpIndices(const std::vector<double> &data, int windowSize, double jumpScale, double stdDivScale,
                                int minSeparation);

bool similar(double a, double b, double offset = 0.2);

template<typename INDEX_T, typename DATA_T>
struct Table {
    std::vector<INDEX_T> index_column;
    std::vector<std::vector<DATA_T> > data;

    [[nodiscard]] std::vector<DATA_T> row_by_index(const INDEX_T index) const {
        for (int i = 0; i < index_column.size(); ++i) {
            if (index_column[i] == index) {
                return data[i];
            }
        }
        throw std::runtime_error("Index not found");
    }

    void print(const std::string &filename, const bool with_header = true, const bool is_time = true) const {
        const auto file = fopen(filename.c_str(), "w");
        if (with_header) {
            print_header(file);
        }
        for (int i = 0; i < index_column.size(); ++i) {
            fprintf(file, "%-13d", index_column[i]);
            for (const double time: data[i]) {
                if (is_time) {
                    fprintf(file, ",%-2.0f ", time * 100);
                } else {
                    fprintf(file, ",%-2.0f ", time);
                }
            }
            fprintf(file, "\n");
        }
        fclose(file);
    }

    static Table from_csv(const std::string &filename, double scale = 0.01, bool skip_header = true) {
        Table table;
        std::ifstream file(filename);

        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file: " + filename);
        }

        std::string line;

        if (skip_header && std::getline(file, line)) {
        }

        while (std::getline(file, line)) {
            if (line.empty()) {
                continue;
            }

            std::stringstream ss(line);
            std::string token;
            std::vector<DATA_T> row_data;
            bool first_column = true;

            while (std::getline(ss, token, ',')) {
                token.erase(0, token.find_first_not_of(" \t\r\n"));
                token.erase(token.find_last_not_of(" \t\r\n") + 1);

                if (token.empty()) {
                    continue;
                }

                if (first_column) {
                    if constexpr (std::is_integral_v<INDEX_T>) {
                        table.index_column.push_back(static_cast<INDEX_T>(std::stoll(token)));
                    } else {
                        table.index_column.push_back(static_cast<INDEX_T>(std::stod(token)));
                    }
                    first_column = false;
                } else {
                    if constexpr (std::is_integral_v<DATA_T>) {
                        row_data.push_back(static_cast<DATA_T>(std::stoll(token)) * scale);
                    } else {
                        row_data.push_back(static_cast<DATA_T>(std::stod(token)));
                    }
                }
            }

            table.data.push_back(row_data);
        }

        file.close();
        return table;
    }

private:
    void print_header(FILE *file) const {
        fprintf(file, "%-13s", "stride\\spots");
        if (!data.empty()) {
            for (int i = 1; i <= data[0].size(); ++i) {
                fprintf(file, ",%-3d", i);
            }
        }
        fprintf(file, "\n");
    }
};
