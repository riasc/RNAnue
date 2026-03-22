#ifndef RNANUE_TRACEBACK_HPP
#define RNANUE_TRACEBACK_HPP

#include <string>
#include <vector>

#include "ScoringMatrix.hpp"

struct TracebackResult {
    std::string a;
    std::string b;
    int length;
    int matches;
    int score;
    double cmpl;
    double ratio;
};

std::vector<TracebackResult> traceback(const ScoringMatrix& matrix, const char *a, const char *b);

#endif //RNANUE_TRACEBACK_HPP