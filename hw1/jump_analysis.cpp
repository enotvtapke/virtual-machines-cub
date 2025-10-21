#include <iostream>
#include <vector>
#include <numeric>
#include <cmath>
#include <stdexcept>
#include <algorithm> // For std::sort
#include "header.h"

// A struct to hold information about a potential jump
struct JumpCandidate {
    size_t index;
    double magnitude; // The strength of the jump (e.g., difference in means)

    // For sorting purposes
    bool operator<(const JumpCandidate& other) const {
        return index < other.index;
    }
};

bool similar(double a, double b, double offset) {
    return std::max(a, b) / std::min(a, b) < 1 + offset;
}

// Helper functions (calculateMean, calculateStdDev) remain the same...
double calculateMean(const std::vector<double>& data, size_t start, size_t end) {
    if (start >= end || end > data.size()) {
        return 0.0;
    }
    double sum = std::accumulate(data.begin() + start, data.begin() + end, 0.0);
    return sum / (end - start);
}

double calculateStdDev(const std::vector<double>& data, size_t start, size_t end, double mean) {
    if (start >= end || end > data.size()) {
        return 0.0;
    }
    double sq_sum = 0.0;
    for (size_t i = start; i < end; ++i) {
        sq_sum += (data[i] - mean) * (data[i] - mean);
    }
    return std::sqrt(sq_sum / (end - start));
}

/**
 * @brief Step 1: Detects all potential jumps (candidates) that exceed the threshold.
 *
 * @param data The input data series.
 * @param windowSize The size of the 'before' and 'after' windows.
 * @param jumpScale The number of standard deviations for the threshold.
 * @return A vector of JumpCandidate structs for all detected potential jumps.
 */
std::vector<JumpCandidate> detectJumpCandidates(const std::vector<double>& data, const size_t windowSize, const double jumpScale, const double stdDivScale) {
    if (2 * windowSize > data.size()) {
        throw std::runtime_error("Data size must be at least twice the window size.");
    }

    std::vector<JumpCandidate> candidates;

    for (size_t i = windowSize; i < data.size() - windowSize; ++i) {
        const size_t before_start = i - windowSize;
        const size_t after_start = i;

        const double mean_before = calculateMean(data, before_start, i);
        const double std_dev_before = calculateStdDev(data, before_start, i, mean_before);

        const double mean_after = calculateMean(data, after_start, i + windowSize);
        const double magnitude = std::abs(mean_after - mean_before);

        if (std_dev_before < data[0] * stdDivScale && std::max(mean_after, mean_before) / std::min(mean_after, mean_before) > jumpScale) {
            candidates.push_back({i, magnitude});
        }
    }
    return candidates;
}

/**
 * @brief Step 2: Filters jump candidates to find the single best jump in each cluster.
 *
 * @param candidates A vector of jump candidates, sorted by index.
 * @param minSeparation The minimum distance between two distinct jumps. Jumps closer than this are considered a single event.
 * @return A vector containing only the most significant jump from each cluster.
 */
std::vector<JumpCandidate> filterAndSelectBestJumps(const std::vector<JumpCandidate>& candidates, size_t minSeparation) {
    if (candidates.empty()) {
        return {};
    }

    std::vector<JumpCandidate> finalJumps;
    
    // Start with the first candidate as the best in the first group
    JumpCandidate bestInGroup = candidates[0];

    for (size_t i = 1; i < candidates.size(); ++i) {
        // If the current candidate is close to the previous one, it's in the same group
        if (candidates[i].index - bestInGroup.index <= minSeparation) {
            // Check if this candidate is stronger than the current best in the group
            if (candidates[i].magnitude > bestInGroup.magnitude) {
                bestInGroup = candidates[i];
            }
        } else {
            // The candidate is far away, so the previous group has ended.
            // Add the best from the last group to our final list.
            finalJumps.push_back(bestInGroup);
            // Start a new group with the current candidate.
            bestInGroup = candidates[i];
        }
    }

    // Don't forget to add the last group's best jump
    finalJumps.push_back(bestInGroup);

    return finalJumps;
}

std::vector<size_t> jumpIndices(const std::vector<double> &data, const int windowSize, const double jumpScale, const double stdDivScale, const int minSeparation) {
    const auto allCandidates = detectJumpCandidates(data, windowSize, jumpScale, stdDivScale);
    // for (const auto& d : allCandidates) {
    //     std::cout << d.index << " " << d.magnitude << " ";
    // }
    // std::cout << "\n";

    const auto finalJumps = filterAndSelectBestJumps(allCandidates, minSeparation);
    std::vector<size_t> indices;
    for (auto&[index, _]: finalJumps) {
        indices.push_back(index);
    }
    return indices;
}


// int main() {
//     // Sample data with a clear jump at index 10 and a smaller one at 24
//     std::vector<double> myData = {
//         11 ,11 ,11 ,11 ,11 ,12 ,27 ,27 ,27 ,27 ,27 ,27 ,51 ,51 ,51 ,51 ,52 ,51 ,52 ,51 ,51 ,51 ,51 ,51 ,51 ,51 ,51 ,51 ,52 ,51 ,52 ,52 ,51 ,51 ,51 ,52 ,51 ,52 ,51 ,51 ,52 ,51 ,51 ,52 ,51 ,52 ,51 ,51 ,51 ,51 ,51 ,51 ,51 ,51 ,51 ,51 ,51 ,51 ,51 ,51 ,51 ,51 ,51 ,52 ,51 ,51 ,51 ,51 ,51 ,51 ,51 ,51 ,51 ,51 ,52 ,51 ,51 ,51 ,51 ,54 ,58 ,53 ,53 ,51 ,52 ,51 ,51 ,51 ,51 ,51 ,51 ,51 ,51 ,51 ,51 ,51 ,51 ,52 ,51 ,51
//
//     };
//
//     try {
//         size_t windowSize = 2;
//         double sensitivity = 1.3;
//
//         // STEP 1: Detect all potential jump candidates
//         auto allCandidates = detectJumpCandidates(myData, windowSize, sensitivity);
//
//         std::cout << "Found " << allCandidates.size() << " raw jump candidates:" << std::endl;
//         for (const auto& cand : allCandidates) {
//             std::cout << "  - Index: " << cand.index << ", Magnitude: " << cand.magnitude << std::endl;
//         }
//         std::cout << "\n------------------------------------------\n" << std::endl;
//
//
//         // STEP 2: Filter the candidates to get only the most significant jump per event
//         // A good choice for minSeparation is the windowSize, as detections within one
//         // window length of each other are likely the same event.
//         size_t minSeparation = windowSize;
//         auto finalJumps = filterAndSelectBestJumps(allCandidates, minSeparation);
//
//         if (finalJumps.empty()) {
//             std::cout << "No significant jumps found after filtering." << std::endl;
//         } else {
//             std::cout << "Final filtered jumps:" << std::endl;
//             for (const auto& jump : finalJumps) {
//                 std::cout << "  -> Best jump detected at index: " << jump.index
//                           << " (Value changes from " << myData[jump.index-1] << " to " << myData[jump.index] << ")"
//                           << " with magnitude " << jump.magnitude << std::endl;
//             }
//         }
//
//     } catch (const std::runtime_error& e) {
//         std::cerr << "Error: " << e.what() << std::endl;
//     }
//
//     return 0;
// }