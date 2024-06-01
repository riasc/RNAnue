#include "SplitReadCalling.hpp"

ThreadPool::ThreadPool(size_t num_threads) : stop(false) {
    for(size_t i = 0; i < num_threads; ++i) {
        workers.emplace_back([this] {
            for(;;) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->queue_mutex);
                    this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                    if(this->stop && this->tasks.empty())
                        return;
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }
                task();
            }
        });
    }
}


ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for(std::thread &worker: workers)
        worker.join();
}

template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result_t<F, Args...>> {
    using return_type = typename std::invoke_result_t<F, Args...>;
    auto task = std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    std::future<return_type> res = task->get_future(); {
        std::unique_lock<std::mutex> lock(queue_mutex);
        if (stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");
        tasks.emplace([task]() { (*task)(); });
    }
    condition.notify_one();
    return res;
}

SplitReadCalling::SplitReadCalling(po::variables_map params) {
    this->params = params;
    // create IBPTree (if splicing need to be considered)
    if(params["splicing"].as<std::bitset<1>>() ==  std::bitset<1>("1")) {
        // order of the IBPTree
        int k = 7; // can be later extracted from config file
        // create IBPT
        this->features = IBPTree(params, k);
    }
    this->condition = ""; // stores the current condition

    // initialize stats
    this->stats = std::make_shared<Stats>();

    const char* seq = "GGGAAAUCC";
}

SplitReadCalling::~SplitReadCalling() {}

void SplitReadCalling::iterate(std::string& matched, std::string& single, std::string &splits, std::string &multsplits) {
    ThreadPool pool(params["threads"].as<int>()); // initialize thread pool
    seqan3::sam_file_input fin{matched}; // initialize input file
    // header
    std::vector<size_t> ref_lengths{};
    for(auto &info : fin.header().ref_id_info) {
        ref_lengths.push_back(std::get<0>(info));
    }
    std::deque<std::string> ref_ids = fin.header().ref_ids();
    seqan3::sam_file_output singleOut{single, ref_ids, ref_lengths}; // initialize output file
    seqan3::sam_file_output splitsOut{splits, ref_ids, ref_lengths}; // initialize output file
    seqan3::sam_file_output multsplitsOut{multsplits, ref_ids, ref_lengths}; // initialize output file

    // create mutex objects for output - had to be done here as class variables cause errors w/ std::bind
    std::mutex singleOutMutex;
    std::mutex splitsOutMutex;
    std::mutex multsplitsOutMutex;

    std::string currentQNAME = "";
    using record_type = typename decltype(fin)::record_type;
    std::vector<record_type> readrecords;
    for(auto && record: fin) {
        // ignore reads if unmapped and do not suffice length requirements
        if(static_cast<bool>(record.flag() & seqan3::sam_flag::unmapped)) { continue; }
        if(static_cast<int>(record.sequence().size()) <= params["minlen"].as<int>()) { continue; }
        if(static_cast<int>(record.mapping_quality()) <= params["mapq"].as<int>()) { continue; }

        std::string QNAME = record.id();
        if((currentQNAME != "") && (currentQNAME != QNAME)) {
            this->stats->setReadsCount(this->condition, 1);
            auto task = [this, records = std::move(readrecords), &singleOut, &splitsOut,
                         &multsplitsOut, &singleOutMutex, &splitsOutMutex, &multsplitsOutMutex]() mutable {
                this->process(records, singleOut, splitsOut, multsplitsOut,
                              singleOutMutex, splitsOutMutex, multsplitsOutMutex);
            };
            pool.enqueue(task);
            readrecords.clear();

            // add for next iteration
            readrecords.push_back(record);
            currentQNAME = QNAME;
        } else {
            readrecords.push_back(record);
            currentQNAME = QNAME;
        }
    }
    if(readrecords.size() > 0) {
        this->stats->setReadsCount(this->condition, 1);
        auto task = [this, records = std::move(readrecords), &singleOut, &splitsOut, &multsplitsOut, &singleOutMutex, &splitsOutMutex, &multsplitsOutMutex]() mutable {
            this->process(records, singleOut, splitsOut, multsplitsOut, singleOutMutex, splitsOutMutex, multsplitsOutMutex);
        };
        pool.enqueue(task);
        readrecords.clear();
    }
}

template <typename T>
void SplitReadCalling::process(std::vector<T>& readrecords, auto& singleOut, auto& splitsOut, auto& multsplitsOut,
                                auto& singleOutMutex, auto& splitsOutMutex, auto& multsplitsOutMutex) {

    // define variables
    std::string qname = "";
    seqan3::sam_flag flag = seqan3::sam_flag::none;
    std::optional<int32_t> ref_id, ref_offset = std::nullopt;
    std::optional<uint8_t> mapq = std::nullopt;
    dtp::CigarVector cigar = {};
    dtp::DNAVector seq = {};
    seqan3::sam_tag_dictionary tags = {};

    int segNum, splitId = 0; // number of segments and splitID in the read

    // cigar
    uint32_t cigarSize;
    seqan3::cigar::operation cigarOp;
    dtp::CigarVector cigarSplit{}; // cigar for each split
    uint32_t cigarMatch{}; // cigar for the matching part of the split

    uint32_t startPosRead, endPosRead; // absolute position in read/alignment (e.g., 1 to end)
    uint32_t startPosSplit, endPosSplit; // position in split read (e.g., XX:i to XY:i / 14 to 20)

    // stores the putative splits/segments
    std::vector<SAMrecord> curated;
    std::map<int, std::vector<SAMrecord>> segments;

    for(auto& rec : readrecords) {
        qname = rec.id();
        flag = rec.flag();
        ref_id = rec.reference_id();
        ref_offset = rec.reference_position();
        mapq = rec.mapping_quality();

        // CIGAR string
        cigar = rec.cigar_sequence();
        cigarSplit.clear();

        tags = rec.tags();
        auto xxtag = tags.get<"XX"_tag>();
        auto xytag = tags.get<"XY"_tag>();
        auto xjtag = tags.get<"XJ"_tag>();
        auto xhtag = tags.get<"XH"_tag>();

        seqan3::debug_stream << "QNAME: " << qname << "\n";
        seqan3::debug_stream << "FLAG: " << flag << "\n";

        if(xjtag < 2) { continue; } // ignore reads with less than 2 splits
        startPosRead = 1; // initialize start/end of read
        endPosRead = 0;
        startPosSplit = xxtag;
        endPosSplit = xytag;

        seqan3::debug_stream << cigar << std::endl;

        cigarMatch = 0; // reset matches count in cigar
        // check the cigar string for splits
        for(auto& cig: cigar) {
            // determine the size and operator of the cigar element
            cigarSize = get<uint32_t>(cig);
            cigarOp = get<seqan3::cigar::operation>(cig);

            if(cigarOp == 'N'_cigar_operation) {
                auto subSeq = seq | seqan3::views::slice(startPosRead - 1, endPosRead);
                // add properties to tags
                seqan3::sam_tag_dictionary newTags{};
                newTags.get<"XX"_tag>() = startPosSplit;
                newTags.get<"XY"_tag>() = endPosSplit;
                newTags.get<"XN"_tag>() = splitId;

                // filter segments
                if(filter(subSeq, cigarMatch)) {
                    storeSegments(rec, ref_offset, cigarSplit, seq, tags, curated);
                }





                // settings to prepare for the next split
                startPosSplit = endPosSplit+1;
                startPosRead = endPosRead+1;
                cigarSplit.clear(); // new split - new CIGAR
                ref_offset.value() += cigarSize + endPosRead + 1; // adjust left-most mapping position
                splitId++; // increase split ID
            } else {
                if(cigarOp == 'S'_cigar_operation &&
                    params["exclclipping"].as<std::bitset<1>>() == std::bitset<1>("1")) {
                    // exclude soft clipping from the alignments
                    if(cigarSplit.size() == 0) { // left soft clipping
                        startPosRead += cigarSize;
                        endPosRead += cigarSize;
                        startPosSplit += cigarSize;
                        endPosSplit += cigarSize;
                    }
                } else {
                    if(cigarOp != 'D'_cigar_operation) {
                        endPosRead += cigarSize;
                        endPosSplit += cigarSize;
                        if(cigarOp != '='_cigar_operation) { cigarMatch += cigarSize; }
                    }
                    cigarSplit.push_back(cig);
                }
            }
        }

        auto subSeq = seq | seqan3::views::slice(startPosRead - 1, endPosRead);
        seqan3::sam_tag_dictionary newTags{};
        newTags.get<"XX"_tag>() = startPosSplit;
        newTags.get<"XY"_tag>() = endPosSplit;
        newTags.get<"XN"_tag>() = splitId;

        // filter segements

        if(segNum) {

        }


        /*
        std::string recid = rec.id();
        dtp::DNAVector recseq = rec.sequence();
        dtp::CigarVector reccigar = rec.cigar_sequence();

        SAMRec outrec{recid, recseq, reccigar};*/





        // single output
        /*
        {
            std::lock_guard<std::mutex> lock(singleOutMutex);
            singleOut.push_back(outrec);
        }*/
        /*
        // splits output
        {
            std::lock_guard<std::mutex> lock(splitsOutMutex);
            splitsOut.push_back(outrec);
        }
        // multsplits output
        {
            std::lock_guard<std::mutex> lock(multsplitsOutMutex);
            multsplitsOut.push_back(outrec);
        }*/
    }
}

bool SplitReadCalling::filter(auto& sequence, uint32_t cigarmatch) {
    // filter segments
    if(sequence.size() < params["minlen"].as<int>()) { return false; }
    double matchrate = static_cast<double>(cigarmatch) / static_cast<double>(sequence.size());
    if(matchrate < params["minfragmatches"].as<double>()) { return false; }
    return true;
}

void SplitReadCalling::storeSegments(auto& splitrecord, std::optional<int32_t>& refOffset,
                                     dtp::CigarVector& cigar, dtp::DNAVector& seq,
                                     seqan3::sam_tag_dictionary& tags, std::vector<SAMrecord>& curated) {
    SAMrecord segment{}; // create new SAM Record

    using types = seqan3::type_list<std::vector<seqan3::dna5>, std::string, std::vector<seqan3::cigar>>;
    using fields = seqan3::fields<seqan3::field::seq, seqan3::field::id, seqan3::field::cigar>;
    using sam_record_type = seqan3::sam_record<types, fields>;


    // write the following to the file
    // r001 0   *   0   0   4M2I2M2D    *   0   0   ACGTACGT    *
    sam_record_type record{};
    record.id() = "r001";
    record.sequence() = "ACGTACGT"_dna5;

    seqan3::debug_stream << splitrecord.id() << std::endl;

}


void SplitReadCalling::sort(const std::string& inputFile, const std::string& outputFile) {
    const char *input = inputFile.c_str();
    const char *output = outputFile.c_str();

    samFile *in = sam_open(input, "rb"); // open input BAM file
    if (!in) {
        std::cerr << helper::getTime() << "Failed to open input BAM file " << inputFile << "\n";
        exit(EXIT_FAILURE);
    }
    samFile *out = sam_open(output, "wb");
    if (!out) {
        std::cerr << helper::getTime() << "Failed to open output BAM file: " << outputFile << "\n";
        sam_close((htsFile *) input);
        exit(EXIT_FAILURE);
    }

    bam_hdr_t *header = sam_hdr_read(in); // load the header
    if (!header) {
        std::cerr << helper::getTime() << "Failed to read header from " << inputFile << "\n";
        sam_close((htsFile *) input);
        exit(EXIT_FAILURE);
    }
    if (sam_hdr_write(out, header) < 0) { // write the header
        std::cerr << helper::getTime() << "Error: Failed to write header to output file " << outputFile << std::endl;
        bam_hdr_destroy(header);
        sam_close(in);
        sam_close(out);
        exit(EXIT_FAILURE);
    }

    // Read BAM records into a vector
    std::vector<bam1_t*> records;
    bam1_t* b = bam_init1();
    while (sam_read1(in, header, b) >= 0) {
        bam1_t* new_record = bam_dup1(b);
        records.push_back(new_record);
    }
    bam_destroy1(b);
    sam_close(in); // close the input file

    // sort the records by QNAME
    std::sort(records.begin(), records.end(), [](const bam1_t* a, const bam1_t* b) {
        return strcmp(bam_get_qname(a), bam_get_qname(b)) < 0;
    });

    // Write sorted BAM records to output file
    for (auto rec : records) {
        if (sam_write1(out, header, rec) < 0) {
            std::cerr << "Error writing BAM record to output file" << std::endl;
            break;
        }
        bam_destroy1(rec);
    }
    // Clean up
    bam_hdr_destroy(header);
    sam_close(out);
}


void SplitReadCalling::start(pt::ptree sample, pt::ptree condition) {
    // input
    pt::ptree input = sample.get_child("input");
    std::string matched = input.get<std::string>("matched");

    this->condition = condition.get<std::string>("condition"); // store current condition (e.g. rpl_37C)

    // output
    pt::ptree output = sample.get_child("output");
    std::string single = output.get<std::string>("single");
    std::string splits = output.get<std::string>("splits");
    std::string multsplits = output.get<std::string>("multsplits");

    srand(time(NULL)); // seed the random number generator
    int randNum = rand() % 1000;
    std::string sortedBam = this->condition + "_" + std::to_string(randNum) + ".bam";
    fs::path sortedBamFile = fs::path(params["outdir"].as<std::string>()) / fs::path("tmp") / fs::path(sortedBam);
    std::string sortedBamFileStr = sortedBamFile.string();

    sort(matched, sortedBamFileStr);
    iterate(sortedBamFileStr, single, splits, multsplits);
}


