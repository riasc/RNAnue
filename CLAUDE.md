# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

RNAnue is a C++20 bioinformatics tool for detecting RNA-RNA interactions from Direct-Duplex-Detection (DDD) data. It processes sequencing reads through preprocessing, alignment, split read detection, clustering, and interaction analysis.

## Build Commands

```bash
# Out-of-source build (from build/ directory)
cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && make

# Debug build
cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug && make

# Run tests (GoogleTest)
./build/RNAnue_tests

# Run tests matching a filter
./build/RNAnue_tests --gtest_filter="ScoringMatrix.*"
```

## Dependencies

- **CMake** >= 3.22.1
- **C++20** standard required
- **SeqAn3** (v3.3.0) - expected at `./seqan3/` in repo root
- **Boost** >= 1.56.0 (program_options)
- **GoogleTest** (v1.15.2) - fetched automatically via CMake FetchContent
- **HTSlib** - found via pkg-config
- **Segemehl** (v0.3.4) - must be in `$PATH` (external aligner invoked at runtime)
- **ViennaRNA** - must be in `$PATH` (hybridization energy calculations)

## Architecture

### Entry Point & Dispatch

`src/main.cpp` parses CLI args via Boost.program_options, supports config files (`--config`), and creates a `Base` object. `Base` dispatches to `Data` methods based on the subcall positional argument:

- **`preproc`** → `Data::preproc()` → `SeqRickshaw` (adapter trimming, quality filtering, PE merging)
- **`align`** → `Data::align()` → `Align` (wraps segemehl.x for read alignment)
- **`detect`** → `Data::detect()` → `SplitReadCalling` (split read detection from BAM files)
- **`clustering`** → `Data::clustering()` → `Clustering` (overlapping split reads into clusters)
- **`analysis`** → `Data::analysis()` → `Analysis` (annotation, statistical scoring, interaction tables)
- **`complete`** → runs the full pipeline in sequence

### Key Classes

- **`Data`** (`include/Data.hpp`) - orchestrates I/O, manages folder structure (trtms/ctrls/conditions), and delegates to processing classes. Uses `boost::property_tree` for JSON data structures.
- **`SeqRickshaw`** - preprocessing engine using SeqAn3 for FASTQ I/O. Uses `StateTransition` tables for adapter matching.
- **`SplitReadCalling`** - producer/consumer multithreaded architecture with `SafeQueue`. Reads BAM via SeqAn3, detects chimeric reads from CIGAR strings, computes complementarity (`ScoringMatrix`/`Traceback`) and hybridization energy (ViennaRNA).
- **`IBPTree`** - Interval B+ Tree for GFF3 annotation lookups. Used by both `SplitReadCalling` and `Analysis` to map genomic coordinates to gene features.
- **`Clustering`** - merges overlapping split read segments into clusters.
- **`Analysis`** - annotates interactions, computes global complementarity/hybridization scores, statistical significance (binomial test), and BH-adjusted p-values.

### Custom SAM Tags

Split reads carry custom tags: `XC` (complementarity), `XE` (hybridization energy), `XA` (alignment), `XM` (matches), `XL` (alignment length), `XR` (site length ratio), `XD` (MFE dot-bracket), `XX`/`XY` (split boundaries).

### Data Types

`include/DataTypes.hpp` defines shared type aliases in the `dtp` namespace (DNASpan, CigarVector, BAMOut, Feature, Interval, etc.).

## Input Data Structure

Reads must follow this folder hierarchy:
```
trtms/
  condition1/    # FASTQ files
  condition2/
ctrls/           # optional
  condition1/
  condition2/
```

## Tests

Tests use GoogleTest (fetched via CMake `FetchContent`). Test files live in `tests/`, test data in `tests/data/`. The test executable is `RNAnue_tests`, built alongside the main binary by CMake.

```bash
# Run all tests
./build/RNAnue_tests

# Run tests matching a filter
./build/RNAnue_tests --gtest_filter="ScoringMatrix.*"
```

## Docker

`Dockerfile` builds a complete environment with all dependencies. CI (`.github/workflows/docker.yml`) builds and pushes Docker images on tag pushes.

## Workflow

- **Always commit via PR** — never push directly to `master`. Create a feature branch, open a PR, and merge through GitHub.
- **After PR is merged** — use `/merge-pr` to: update `CHANGELOG.md` under `[Unreleased]`, merge the PR, clean up branches, and pull latest `master`.
