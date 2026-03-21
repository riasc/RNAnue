// Standard
#include <iostream>
#include <bitset>

// Class
#include "Base.hpp"
#include "Exceptions.hpp"

Base::Base(po::variables_map params) : params(params), paramsVal(params), data(params) {
    std::string subcall = params["subcall"].as<std::string>();

    if(subcall == "preproc") {
        if(params["preproc"].as<std::bitset<1>>() != std::bitset<1>("1")) {
            throw ConfigError("'preproc' has not been set correctly - check parameters");
        }
        data.preproc();
    } else if(subcall == "align") {
        data.align();
    } else if(subcall == "detect") {
        data.detect();
    } else if(subcall == "clustering") {
        data.clustering();
    } else if(subcall == "analysis") {
        data.analysis();
    } else if(subcall == "complete") {
        data.preproc();
        data.align();
        data.detect();
        if(params["clust"].as<std::bitset<1>>() == std::bitset<1>("1")) {
            data.clustering();
        }
        data.analysis();
    } else {
        throw ConfigError("Invalid subcall: " + subcall);
    }
}

Base::~Base() {}
