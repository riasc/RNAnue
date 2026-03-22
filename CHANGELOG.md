# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

# [Unreleased]

## Fixed

- **Pipeline order**: Fixed wrong execution order in `complete` subcall — `detect` was running before `align` (#1, PR #14)
- **Overlap check**: Fixed tautological condition `p1Start >= p1Start` in `SplitReadCalling::decide()` that caused incorrect overlap filtering (#1, PR #14)
- **Missing return**: Added missing `return false` in `SplitReadCalling::matchSpliceSites()` to fix undefined behavior (#1, PR #14)
- **Docker CI**: Fixed workflow to build-only on PRs instead of attempting to push, and fixed deprecated `::set-output` syntax (PR #15)
- **Dockerfile**: Updated base image from Ubuntu 23.04 (EOL) to 24.04 LTS (PR #15)
- **IBPTree silent failure**: Bare `EXIT_FAILURE;` statements were no-ops — replaced with `throw FileError` ([#13](https://github.com/riasc/RNAnue/issues/13), [#18](https://github.com/riasc/RNAnue/pull/18))
- **Version flag**: `--version` and `--help` now work without providing a subcall ([#13](https://github.com/riasc/RNAnue/issues/13), [#18](https://github.com/riasc/RNAnue/pull/18))
- **Clustering bugs**: Fixed wrong strand flag for second segment, duplicate refid comparison, operator precedence in overlap check, and removed shadowed variables ([#10](https://github.com/riasc/RNAnue/issues/10), [#19](https://github.com/riasc/RNAnue/pull/19))
- **ScoringMatrix memory**: Replaced C-style malloc/calloc/free with RAII vectors, fixed inner-loop heap allocation leak, replaced nested `std::map` scoring lookup with `switch` ([#5](https://github.com/riasc/RNAnue/issues/5), [#20](https://github.com/riasc/RNAnue/pull/20))
- **IBPTree leaks and crash**: Added destructor to free all nodes and intervals, fixed `search()` segfault on unknown chromosome, initialized uninitialized pointer ([#6](https://github.com/riasc/RNAnue/issues/6), [#21](https://github.com/riasc/RNAnue/pull/21))

## Refactored

- **DataPrep consolidation**: Collapsed 5 near-identical `*DataPrep()` methods into 2 parameterized methods (`rawInputDataPrep`, `stageDataPrep`) (#3, PR #16)
- **Dispatch chains**: Replaced deeply nested if/else subcall dispatch with flat if/else-if chains and map lookup (#3, PR #16)
- **Namespace pollution**: Moved `using namespace seqan3::literals` from headers to `.cpp` files (#3, PR #16)
- **Filesystem migration**: Replaced `boost::filesystem` with `std::filesystem` across all source files, removed `Boost::filesystem` CMake dependency (#3, PR #16)
- **Error handling**: Replaced all `exit(EXIT_FAILURE)` calls with custom exception hierarchy (`ConfigError`/`FileError`/`ValidationError`), added RAII wrappers for HTSlib resources (#3, PR #16)

## Fixed

- **Test UB**: Fixed missing return in `compareFiles()` test helper (declared `bool`, returned nothing) that caused SIGTRAP in debug builds (PR #16)

## Added

- **ScoringMatrix tests**: 8 new tests for complementarity scoring, traceback, edge cases ([#5](https://github.com/riasc/RNAnue/issues/5), [#20](https://github.com/riasc/RNAnue/pull/20))
- **Ubuntu CI**: Build+test workflow for GCC 12/13/14 × Debug/Release ([#20](https://github.com/riasc/RNAnue/pull/20))

## Changed

- **Test framework**: Switched from Boost.Test to GoogleTest via FetchContent, renamed `test/` to `tests/`, made DataHandling tests path-independent ([#20](https://github.com/riasc/RNAnue/pull/20))
- **Docker CI**: Replaced QEMU cross-compilation with native ARM runners (`ubuntu-24.04-arm`), amd64-only on PRs, parallel native builds + manifest push on tags (PR #17)
- **README**: Revised with updated URLs, corrected build instructions, complete dependency list, and missing subcall/tag documentation (PR #15)

# [0.2.4]

## Fix

- Added lock guard for when writing progress (processed reads) to std::cout to ensure threads do not write over each other
- Fixed bug/added check when no control data is provided

# [0.2.3]

## Fix

- Consumer/Producer pattern for 'detect' to integrate concurrency
- Write to .fq using SeqAn3 write operations
- Assertion error when writing to .fq (caused by variable initialization outside worker)


# [0.2.2]

## Features

- add test data

## Fix

- Code cleanup
- Fix in writing to stats.txt that cause overwriting in different subcalls


# [0.2.1]

## Features

- code cleanup 
- fix in segmentation of the `preproc` that caused boost::anycast error ()

# [0.2.0]

## Features

- update to C++20 and SeqAn 3.3.0
- native support for concurrency (ditches OpenMP)

# [0.1.1]

## Fix

- Replaced raw pointers with smart pointers (e.g., std::unique_ptr and std::shared_ptr) to ensure automatic memory management.
- Updated constructor and destructor to properly initialize and clean up resources.
- Used exceptions to handle critical errors instead of error codes for better clarity and control flow.
- Updated the CMakeLists.txt to include necessary libraries and set appropriate compile options.

# [0.1.0]

### Features
- Initial implementation of RNAnue 

