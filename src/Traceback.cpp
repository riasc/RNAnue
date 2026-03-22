#include "Traceback.hpp"

#include <algorithm>
#include <cstring>

std::vector<TracebackResult> traceback(const ScoringMatrix& matrix, const char *a, const char *b) {
    // Find all cells with the maximum score
    std::vector<std::pair<int,int>> mids;
    mids.push_back({0, 0});

    for(size_t i = 0; i < matrix.height; ++i) {
        for(size_t j = 0; j < matrix.width; ++j) {
            int current = matrix.at(i, j);
            int best = matrix.at(mids[0].first, mids[0].second);
            if(current > best) {
                mids.clear();
                mids.push_back({static_cast<int>(i), static_cast<int>(j)});
            } else if(current == best && current > 0) {
                mids.push_back({static_cast<int>(i), static_cast<int>(j)});
            }
        }
    }

    int bestScore = matrix.at(mids[0].first, mids[0].second);

    std::vector<TracebackResult> res;
    for(auto& max : mids) {
        TracebackResult trc{"", "", 0, 0, 0, 0.0, 0.0};

        int x = max.first;
        int y = max.second;
        int matches = 0;

        std::string aAlign;
        std::string bAlign;

        while(matrix.at(x, y) > 0) {
            uint8_t direction = matrix.traceAt(x, y);
            if(direction == 0) break;

            if(direction & TRACE_DIAG) {
                aAlign += a[x-1];
                bAlign += b[y-1];
                x--;
                y--;
                if(x > 0 && y > 0) {
                    if(matrix.at(x, y) - 1 == matrix.at(x - 1, y - 1)) {
                        matches++;
                    }
                }
            } else if(direction & TRACE_UP) {
                aAlign += '-';
                bAlign += b[y-1];
                y--;
            } else if(direction & TRACE_LEFT) {
                aAlign += a[x-1];
                bAlign += '-';
                x--;
            }
        }

        std::reverse(aAlign.begin(), aAlign.end());
        std::reverse(bAlign.begin(), bAlign.end());

        trc.a = aAlign;
        trc.b = bAlign;
        trc.length = static_cast<int>(aAlign.size());
        trc.matches = matches;
        trc.score = bestScore;
        trc.cmpl = (trc.length > 0) ? static_cast<double>(trc.matches) / trc.length : 0.0;
        trc.ratio = static_cast<double>(trc.matches) / (strlen(a) + strlen(b));
        res.push_back(trc);
    }
    return res;
}