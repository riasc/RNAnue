#include "ScoringMatrix.hpp"

int complementarityScore(char a, char b, int matchScore, int mismatchScore) {
    // RNA complementarity: A-U(T), G-C, G-U(T) wobble pairs
    switch(a) {
        case 'A': return (b == 'T') ? matchScore : mismatchScore;
        case 'T': return (b == 'A' || b == 'G') ? matchScore : mismatchScore;
        case 'G': return (b == 'C' || b == 'T') ? matchScore : mismatchScore;
        case 'C': return (b == 'G') ? matchScore : mismatchScore;
        default:  return mismatchScore;
    }
}

ScoringMatrix createScoringMatrix(const char *a, const char *b, int matchScore, int mismatchScore, int gapCost) {
    size_t height = strlen(a) + 1;
    size_t width = strlen(b) + 1;

    ScoringMatrix matrix;
    matrix.width = width;
    matrix.height = height;
    matrix.scoring.assign(width * height, 0);
    matrix.traceback.assign(width * height, 0);

    for(size_t i = 1; i < height; ++i) {
        for(size_t j = 1; j < width; ++j) {
            double scores[3];
            scores[0] = matrix.at(i - 1, j - 1) + complementarityScore(a[i-1], b[j-1], matchScore, mismatchScore);
            scores[1] = matrix.at(i - 1, j) - gapCost;
            scores[2] = matrix.at(i, j - 1) - gapCost;

            // Find max (Smith-Waterman: clamp to 0)
            int maxScore = 0;
            uint8_t trace = 0;
            for(int k = 0; k < 3; ++k) {
                if(scores[k] > maxScore) {
                    maxScore = static_cast<int>(scores[k]);
                    trace = (1 << k); // reset to only this direction
                } else if(scores[k] == maxScore && scores[k] > 0) {
                    trace |= (1 << k); // add this direction
                }
            }

            matrix.at(i, j) = maxScore;
            matrix.traceAt(i, j) = trace;
        }
    }
    return matrix;
}

void printMatrix(const ScoringMatrix& matrix) {
    std::cout << "Scoring Matrix\n";
    for(size_t i = 0; i < matrix.height; ++i) {
        for(size_t j = 0; j < matrix.width; ++j) {
            std::cout << matrix.at(i, j) << " ";
        }
        std::cout << '\n';
    }
    std::cout << "Traceback Matrix\n";
    for(size_t i = 0; i < matrix.height; ++i) {
        for(size_t j = 0; j < matrix.width; ++j) {
            std::cout << static_cast<int>(matrix.traceAt(i, j)) << " ";
        }
        std::cout << '\n';
    }
}