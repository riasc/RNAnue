#ifndef RNANUE_EXCEPTIONS_HPP
#define RNANUE_EXCEPTIONS_HPP

#include <stdexcept>

struct RNAnueError : std::runtime_error { using std::runtime_error::runtime_error; };
struct ConfigError : RNAnueError { using RNAnueError::RNAnueError; };
struct FileError : RNAnueError { using RNAnueError::RNAnueError; };
struct ValidationError : RNAnueError { using RNAnueError::RNAnueError; };

#endif //RNANUE_EXCEPTIONS_HPP