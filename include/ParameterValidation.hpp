#ifndef RNANUE_PARAMETERVALIDATION_HPP
#define RNANUE_PARAMETERVALIDATION_HPP

// Standard
#include <iostream>

// Boost
#include <boost/program_options.hpp>
#include <filesystem>

// Class
#include "Utility.hpp"

namespace po = boost::program_options;
namespace fs = std::filesystem;

class ParameterValidation {
    public:
        ParameterValidation(po::variables_map& params);
        ~ParameterValidation();

        // validation routines
        void validatePreproc();
        void validateDirs(std::string param);

    private:
        po::variables_map& params;
};

#endif //RNANUE_PARAMETERVALIDATION_HPP
