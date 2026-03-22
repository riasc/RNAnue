#include <gtest/gtest.h>

#include <fstream>
#include <filesystem>

#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "Data.hpp"

namespace po = boost::program_options;
namespace fs = std::filesystem;
namespace pt = boost::property_tree;

// Recursively compare two ptrees, but for string values only compare
// the relative path suffix (everything after the last occurrence of
// "rawreads_" or "results_") to make tests path-independent.
std::string relativeSuffix(const std::string& path) {
    for(const auto& marker : {"rawreads_SE", "rawreads_PE", "results_SE", "results_PE"}) {
        auto pos = path.find(marker);
        if(pos != std::string::npos) {
            return path.substr(pos);
        }
    }
    return path;
}

void compareTrees(const pt::ptree& ref, const pt::ptree& actual, const std::string& context) {
    ASSERT_EQ(ref.size(), actual.size()) << "Size mismatch at: " << context;

    if(ref.empty() && actual.empty()) {
        // Leaf nodes — compare relative suffixes
        EXPECT_EQ(relativeSuffix(ref.data()), relativeSuffix(actual.data()))
            << "Value mismatch at: " << context;
        return;
    }

    auto refIt = ref.begin();
    auto actIt = actual.begin();
    for(; refIt != ref.end(); ++refIt, ++actIt) {
        ASSERT_EQ(refIt->first, actIt->first)
            << "Key mismatch at: " << context;
        compareTrees(refIt->second, actIt->second,
                     context + "." + refIt->first);
    }
}

void compareJsonFiles(const fs::path& refPath, const fs::path& actualPath) {
    pt::ptree refTree, actualTree;
    pt::read_json(refPath.string(), refTree);
    pt::read_json(actualPath.string(), actualTree);
    compareTrees(refTree, actualTree, "root");
}

TEST(DataHandling, PreprocSE) {
    fs::path testdir{fs::path(__FILE__).parent_path()};
    fs::path outdir{testdir / "data" / "datahandling" / "results_SE"};
    fs::path ctrls{testdir / "data" / "datahandling" / "rawreads_SE" / "ctrls"};
    fs::path trtms{testdir / "data" / "datahandling" / "rawreads_SE" / "trtms"};

    po::variables_map params;
    params.insert(std::make_pair("ctrls", po::variable_value(ctrls.string(), false)));
    params.insert(std::make_pair("trtms", po::variable_value(trtms.string(), false)));
    params.insert(std::make_pair("outdir", po::variable_value(outdir.string(), false)));
    params.insert(std::make_pair("readtype", po::variable_value(std::string("SE"), false)));
    params.insert(std::make_pair("subcall", po::variable_value(std::string("preproc"), false)));

    Data data(params);

    fs::path ref{testdir / "data" / "datahandling" / "dataPreprocSE.json"};
    fs::path res{outdir / "data.json"};
    compareJsonFiles(ref, res);
}

TEST(DataHandling, PreprocPE) {
    fs::path testdir{fs::path(__FILE__).parent_path()};
    fs::path outdir{testdir / "data" / "datahandling" / "results_PE"};
    fs::path ctrls{testdir / "data" / "datahandling" / "rawreads_PE" / "ctrls"};
    fs::path trtms{testdir / "data" / "datahandling" / "rawreads_PE" / "trtms"};

    po::variables_map params;
    params.insert(std::make_pair("ctrls", po::variable_value(ctrls.string(), false)));
    params.insert(std::make_pair("trtms", po::variable_value(trtms.string(), false)));
    params.insert(std::make_pair("outdir", po::variable_value(outdir.string(), false)));
    params.insert(std::make_pair("readtype", po::variable_value(std::string("PE"), false)));
    params.insert(std::make_pair("subcall", po::variable_value(std::string("preproc"), false)));

    Data data(params);

    fs::path ref{testdir / "data" / "datahandling" / "dataPreprocPE.json"};
    fs::path res{outdir / "data.json"};
    compareJsonFiles(ref, res);
}