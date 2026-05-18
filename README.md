[![CI (Ubuntu)](https://github.com/riasc/RNAnue/actions/workflows/ci-ubuntu.yml/badge.svg)](https://github.com/riasc/RNAnue/actions/workflows/ci-ubuntu.yml)
[![docker-release](https://github.com/riasc/RNAnue/actions/workflows/docker.yml/badge.svg)](https://github.com/riasc/RNAnue/actions/workflows/docker.yml)

# RNAnue

RNAnue is a comprehensive analysis tool to detect RNA-RNA interactions from Direct-Duplex-Detection (DDD) data.

> **Note**: This is an independently maintained fork. The original project was developed at [Ibvt/RNAnue](https://github.com/Ibvt/RNAnue) and is now continued by the original group at [RNABioInfo/RNAnue](https://github.com/RNABioInfo/RNAnue).

## Install

### Dependencies

RNAnue has the following dependencies. Make sure the requirements are satisfied by your system.

* **C++20** compiler — GCC >= 12 (SeqAn3 does not support Clang)
* **[CMake](https://cmake.org/)** (>= 3.22.1)
* **[Boost C++ Libraries](https://www.boost.org/)** (>= 1.56.0) — `program_options` component only
* **[SeqAn3](https://github.com/seqan/seqan3)** (v3.3.0) — expected in `./seqan3/` in the repo root
* **[HTSlib](https://github.com/samtools/htslib)** — linked at build time, found via `pkg-config` (`htslib.pc`)
* **[ViennaRNA Package](https://www.tbi.univie.ac.at/RNA/)** (v2.6.4 recommended) — linked at build time, found via `pkg-config` (`RNAlib2.pc`). Not packaged on Ubuntu/Debian; build from source with `./configure --without-swig --without-doc --without-tutorial && make && sudo make install`.
* **[Segemehl](http://www.bioinf.uni-leipzig.de/Software/segemehl/)** (v0.3.4) — runtime dependency for the `align` subcall, must be in `$PATH`

### Building from source

Clone the repository and retrieve SeqAn3:
```bash
git clone https://github.com/riasc/RNAnue.git
cd RNAnue

# download SeqAn3 into the repo root
curl -L https://github.com/seqan/seqan3/releases/download/3.3.0/seqan3-3.3.0-Source.tar.xz -o seqan3.tar.xz
tar -xf seqan3.tar.xz && mv seqan3-3.3.0-Source seqan3 && rm seqan3.tar.xz
```

Build using CMake (out-of-source build):
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
```

The `RNAnue` binary will be in the `build/` directory.

### Docker

A ready-to-use Docker container is available:
```bash
docker pull riasc/rnanue:latest
docker run -v /path/to/data:/data riasc/rnanue RNAnue <subcall> --config /data/params.cfg
```

The `-v` flag mounts your local data directory into the container at `/data`. Paths in your config file should refer to `/data/...` (the in-container path), not the host path.

### Singularity

The Docker container can also be used with Singularity:
```bash
singularity pull docker://riasc/rnanue:latest
singularity exec --bind /path/to/data:/data rnanue_latest.sif RNAnue <subcall> --config /data/params.cfg
```

## Overview

![Principle](principle.png)

## Usage

### Subcalls

RNAnue provides different subcalls for individual procedures:

| Subcall | Description |
| ------- | ----------- |
| `preproc` | Adapter trimming, quality filtering, PE merging |
| `align` | Read alignment via segemehl |
| `detect` | Split read detection from aligned BAM files |
| `clustering` | Merge overlapping split reads into clusters |
| `analysis` | Annotation, statistical scoring, interaction tables |
| `complete` | Run the full pipeline in sequence |

The pipeline order (also what `complete` runs) is: `preproc` → `align` → `detect` → `clustering` → `analysis`.

```bash
RNAnue <subcall> --config /path/to/params.cfg
```

Run `RNAnue --help` for the full list of options.

### Quickstart

End-to-end example using a config file:

```bash
RNAnue complete --config params.cfg
```

Or with the most common parameters on the command line:

```bash
RNAnue complete \
    --trtms ./trtms \
    --ctrls ./ctrls \
    --outdir ./results \
    --dbs reference.fa \
    --features annotation.gff \
    --threads 8
```

### Input

RNAnue requires sequencing files in a specific folder structure. The root folders for treatments (`--trtms`) and controls (`--ctrls`) contain subfolders with arbitrary condition names, each holding the read files:

```
trtms/
    condition1/    # FASTQ files
    condition2/
ctrls/             # optional
    condition1/
    condition2/
```

The `--trtms` parameter is required. `--ctrls` is optional.

### Parameters

RNAnue accepts parameters from the command line and through a configuration file:
```bash
RNAnue <subcall> --config /path/to/params.cfg
```
Command-line parameters take precedence over the config file.

[`tests/humanSE.cfg`](./tests/humanSE.cfg) is a complete annotated config file you can copy as a starting template.

## Results

Results are stored in the specified output folder and its subfolders (`./preproc`, `./align`, `./detect`, `./clustering`, `./analysis`).

### Split Reads (.BAM)

RNAnue reports detected splits in BAM format (`detect` subcall). Pairs of rows represent the split reads consisting of individual segments:

```
A00551:...:10645  16  gi|...|NC_010473.1|  3520484  22  1X51=  *  0  0  AGGG...TCAA  *  XA:Z:TTTCTGG  XC:f:0.714  XE:f:-15.6  ...
A00551:...:10645  16  gi|...|NC_010473.1|  3520662  22  11=5S  *  0  0  TTCG...GAAC  *  XA:Z:GAAGAAC  XC:f:0.714  XE:f:-15.6  ...
```

Custom SAM tags reported in split reads:

| Tag | Description |
| --- | ----------- |
| XC:f | Complementarity score |
| XE:f | Hybridization energy (MFE in kcal/mol) |
| XA:Z | Alignment of sequence |
| XM:i | Matches in alignment |
| XL:i | Length of alignment |
| XR:f | Site length ratio |
| XS:i | Alignment score |
| XD:Z | MFE structure in dot-bracket notation |
| XH:i | Number of splits (SAM record) |
| XJ:i | Number of splits (whole read) |
| XX:i | Split boundary start |
| XY:i | Split boundary end |

### Clustering Results

The `clustering` subcall produces a `clusters.tab` file — a tab-delimited file where each line represents a cluster of overlapping split reads:

| Field | Description |
| ----- | ----------- |
| clustID | Unique identifier of the cluster |
| fst_seg_chr | Chromosome (accession) of the first segment |
| fst_seg_strd | Strand of the first segment |
| fst_seg_strt | Start position of the first segment |
| fst_seg_end | End position of the first segment |
| sec_seg_chr | Chromosome (accession) of the second segment |
| sec_seg_strd | Strand of the second segment |
| sec_seg_strt | Start position of the second segment |
| sec_seg_end | End position of the second segment |
| no_splits | Number of split reads in the cluster |
| fst_seg_len | Length of the first segment |
| sec_seg_len | Length of the second segment |

### Interaction Table

The `analysis` subcall generates `_interactions` files for each library. Each line represents an annotated split read mapped to a transcript interaction:

| Field | Description |
| ----- | ----------- |
| qname | Read/template identifier |
| fst_seg_strd | Strand of the first segment |
| fst_seg_strt | Start position of the first segment |
| fst_seg_end | End position of the first segment |
| fst_seg_ref | Reference name of the first segment |
| fst_seg_name | Gene name/symbol of the first segment |
| first_seg_bt | Biotype of the transcript |
| fst_seg_anno_strd | Strand of the overlapping annotation |
| fst_seg_prod | Description of the transcript |
| fst_seg_ori | Orientation (sense/antisense) |
| sec_seg_strd | Strand of the second segment |
| sec_seg_strt | Start position of the second segment |
| sec_seg_end | End position of the second segment |
| sec_seg_ref | Reference name of the second segment |
| sec_seg_name | Gene name/symbol of the second segment |
| sec_seg_bt | Biotype of the transcript |
| sec_seg_anno_strd | Strand of the overlapping annotation |
| sec_seg_prod | Description of the transcript |
| sec_seg_ori | Orientation (sense/antisense) |
| cmpl | Complementarity score |
| fst_seg_compl_aln | Complementarity alignment of the first segment |
| sec_seg_cmpl_aln | Complementarity alignment of the second segment |
| mfe | Hybridization energy |
| mfe_struc | MFE structure in dot-bracket notation |

The main results are transcript interactions stored in `allints.txt`:

| Field | Description |
| ----- | ----------- |
| fst_rna | Gene/transcript name of the first partner |
| sec_rna | Gene/transcript name of the second partner |
| fst_rna_ori | Orientation of the first partner |
| sec_rna_ori | Orientation of the second partner |
| \<sample\>_supp_reads | Number of supporting split reads |
| \<sample\>_ges | Global energy score |
| \<sample\>_ghs | Global hybridization score |
| \<sample\>_pval | P-value (binomial test) |
| \<sample\>_padj | Benjamini-Hochberg adjusted p-value |

Additional output options:
- `--outcnt 1` — generates `counts.txt` (count table for differential expression analysis)
- `--outjgf 1` — generates `graph.json` (interactions in JSON graph format)
- `--stats 1` — generates `stats.txt` (basic statistics for each step)

## Testing

Run the test suite:
```bash
./build/RNAnue_tests
```

Test data is available in [tests/data/](./tests/data/).

## Troubleshooting

Please [create an issue](https://github.com/riasc/RNAnue/issues) if you encounter any problems.

## Citation

If you use RNAnue, please cite the original paper:

> Schäfer, R. A., & Voß, B. (2021). RNAnue: efficient data analysis for RNA-RNA interactomics. *Nucleic Acids Research*, 49(10), 1–10. https://doi.org/10.1093/nar/gkab340

See [`CITATION.cff`](./CITATION.cff) for a machine-readable citation entry.

## License

RNAnue is licensed under the GNU General Public License v3.0. See [`LICENSE`](./LICENSE) for the full text.
