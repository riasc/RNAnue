#include <gtest/gtest.h>

#include "ScoringMatrix.hpp"
#include "Traceback.hpp"

TEST(ScoringMatrix, ComplementarityScore) {
    // Watson-Crick pairs
    EXPECT_EQ(complementarityScore('A', 'T', 1, -1), 1);
    EXPECT_EQ(complementarityScore('T', 'A', 1, -1), 1);
    EXPECT_EQ(complementarityScore('G', 'C', 1, -1), 1);
    EXPECT_EQ(complementarityScore('C', 'G', 1, -1), 1);

    // Wobble pairs (G-U)
    EXPECT_EQ(complementarityScore('G', 'T', 1, -1), 1);
    EXPECT_EQ(complementarityScore('T', 'G', 1, -1), 1);

    // Mismatches
    EXPECT_EQ(complementarityScore('A', 'A', 1, -1), -1);
    EXPECT_EQ(complementarityScore('G', 'G', 1, -1), -1);
    EXPECT_EQ(complementarityScore('A', 'C', 1, -1), -1);
    EXPECT_EQ(complementarityScore('C', 'A', 1, -1), -1);
}

TEST(ScoringMatrix, EmptySequences) {
    ScoringMatrix m = createScoringMatrix("", "", 1, -1, 2);
    EXPECT_EQ(m.width, 1u);
    EXPECT_EQ(m.height, 1u);
    EXPECT_EQ(m.at(0, 0), 0);
}

TEST(ScoringMatrix, PerfectComplement) {
    // ATGC vs TACG — perfect Watson-Crick complement
    ScoringMatrix m = createScoringMatrix("ATGC", "TACG", 1, -1, 2);
    EXPECT_EQ(m.width, 5u);
    EXPECT_EQ(m.height, 5u);

    int maxScore = 0;
    for(size_t i = 0; i < m.height; ++i) {
        for(size_t j = 0; j < m.width; ++j) {
            maxScore = std::max(maxScore, m.at(i, j));
        }
    }
    EXPECT_EQ(maxScore, 4); // 4 consecutive matches
}

TEST(ScoringMatrix, NoMatch) {
    // Same sequence — no complementarity (A-A, T-T, etc. are mismatches)
    ScoringMatrix m = createScoringMatrix("AAAA", "AAAA", 1, -1, 2);

    int maxScore = 0;
    for(size_t i = 0; i < m.height; ++i) {
        for(size_t j = 0; j < m.width; ++j) {
            maxScore = std::max(maxScore, m.at(i, j));
        }
    }
    EXPECT_EQ(maxScore, 0); // all mismatches, Smith-Waterman clamps to 0
}

TEST(Traceback, PerfectComplement) {
    const char* seq1 = "ATGC";
    const char* seq2 = "TACG";
    ScoringMatrix m = createScoringMatrix(seq1, seq2, 1, -1, 2);
    std::vector<TracebackResult> results = traceback(m, seq1, seq2);

    ASSERT_FALSE(results.empty());
    EXPECT_EQ(results[0].length, 4);
    EXPECT_EQ(results[0].score, 4);
    EXPECT_GT(results[0].cmpl, 0.0);
}

TEST(Traceback, EmptyInput) {
    const char* seq1 = "";
    const char* seq2 = "";
    ScoringMatrix m = createScoringMatrix(seq1, seq2, 1, -1, 2);
    std::vector<TracebackResult> results = traceback(m, seq1, seq2);

    ASSERT_FALSE(results.empty());
    EXPECT_EQ(results[0].score, 0);
    EXPECT_EQ(results[0].length, 0);
}

TEST(Traceback, WobblePairs) {
    // G-U wobble pairs should count as matches
    const char* seq1 = "GGGG";
    const char* seq2 = "TTTT"; // T = U in DNA representation
    ScoringMatrix m = createScoringMatrix(seq1, seq2, 1, -1, 2);
    std::vector<TracebackResult> results = traceback(m, seq1, seq2);

    ASSERT_FALSE(results.empty());
    EXPECT_EQ(results[0].score, 4);
}

TEST(Traceback, PartialMatch) {
    const char* seq1 = "CATATG";
    const char* seq2 = "GTATAC";
    ScoringMatrix m = createScoringMatrix(seq1, seq2, 1, -1, 2);
    std::vector<TracebackResult> results = traceback(m, seq1, seq2);

    ASSERT_FALSE(results.empty());
    EXPECT_GT(results[0].score, 0);
    EXPECT_GT(results[0].length, 0);
}