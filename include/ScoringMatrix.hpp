#ifndef RNANUE_SCORINGMATRIX_HPP
#define RNANUE_SCORINGMATRIX_HPP

#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

struct ScoringMatrix {
    size_t width;
    size_t height;
    std::vector<int> scoring;
    std::vector<uint8_t> traceback;

    int& at(size_t row, size_t col) { return scoring[row * width + col]; }
    int at(size_t row, size_t col) const { return scoring[row * width + col]; }
    uint8_t& traceAt(size_t row, size_t col) { return traceback[row * width + col]; }
    uint8_t traceAt(size_t row, size_t col) const { return traceback[row * width + col]; }
};

// Traceback direction bits
constexpr uint8_t TRACE_DIAG = 0x01; // bit 0: diagonal
constexpr uint8_t TRACE_UP   = 0x02; // bit 1: up (gap in b)
constexpr uint8_t TRACE_LEFT = 0x04; // bit 2: left (gap in a)

// Complementarity scoring lookup (RNA base-pairing rules)
// Returns match/mismatch score for two nucleotide characters
int complementarityScore(char a, char b, int matchScore, int mismatchScore);

ScoringMatrix createScoringMatrix(const char *a,
                                  const char *b,
                                  int matchScore,
                                  int mismatchScore,
                                  int gapCost);

void printMatrix(const ScoringMatrix& mat);

#endif //RNANUE_SCORINGMATRIX_HPP