#include "Data.hpp"
#include "Exceptions.hpp"

#include <unordered_map>

Data::Data(po::variables_map params) : params(params) {
    std::string subcall = params["subcall"].as<std::string>();
    fs::path outDir = fs::path(params["outdir"].as<std::string>());

    // create output directory (e.g., outdir)
    helper::createOutDir(outDir, std::cout);
    // create directory for subcall (e.g., outdir/preproc)
    fs::path outSubcallDir = outDir / fs::path(subcall);
    helper::createDir(outSubcallDir, std::cout);
    helper::createDir(outDir / fs::path("tmp"), std::cout);

    // stage source map: maps subcall to its input stage directory
    static const std::unordered_map<std::string, std::string> stageSource = {
        {"detect",     "align"},
        {"clustering", "detect"},
        {"analysis",   "detect"},
    };

    if(subcall == "preproc") {
        rawInputDataPrep();
    } else if(subcall == "align") {
        if(params["preproc"].as<std::bitset<1>>() == std::bitset<1>("0")) {
            std::cout << helper::getTime() << "Skipping preprocessing\n";
            rawInputDataPrep();
        } else {
            stageDataPrep("preproc");
        }
    } else if(auto it = stageSource.find(subcall); it != stageSource.end()) {
        if(subcall == "detect") {
            helper::createDir(outDir / fs::path("IBPTree"), std::cout);
        }
        stageDataPrep(it->second);
    }
}

Data::~Data() {
    fs::path outDir = fs::path(params["outdir"].as<std::string>());
    fs::path tmpDir = outDir / fs::path("tmp");
    helper::deleteDir(tmpDir);
}

void Data::rawInputDataPrep() {
    fs::path ctrlsPath = fs::path(this->params["ctrls"].as<std::string>());
    fs::path trtmsPath = fs::path(this->params["trtms"].as<std::string>());

    GroupsPath groups = getGroupsPath(ctrlsPath, trtmsPath);
    getCondition(groups);
}

void Data::stageDataPrep(const std::string& sourceStage) {
    fs::path ctrlsPath;
    if(!params["ctrls"].as<std::string>().empty()) {
        ctrlsPath = fs::path(params["outdir"].as<std::string>()) / sourceStage / "ctrls";
    }
    fs::path trtmsPath = fs::path(params["outdir"].as<std::string>()) / sourceStage / "trtms";

    GroupsPath groups = getGroupsPath(ctrlsPath, trtmsPath);
    getCondition(groups);
}

GroupsPath Data::getGroupsPath(fs::path& ctrls, fs::path& trtms) {
    GroupsPath groups;
    std::cout << helper::getTime() << "Retrieve the path that contain the reads\n";

    if(!trtms.empty()) { // '--trtms' has been set in the config file or cmdline
        if(!ctrls.empty()) { // '--ctrls' has been set in the config or cmdline
            groups.insert(std::make_pair("ctrls", ctrls));
        } else { // '--ctrls' has not been set in either config or cmdline - still continue
            std::cout << helper::getTime() << "### WARNING - '--ctrls' has not been set. ";
            std::cout << "This runs RNAnue without control data\n";
        }
        groups.insert(std::make_pair("trtms", trtms));
    } else { // abort - trtms have not been set in either config file or cmdline
        throw ConfigError("No treatment data specified");
    }
    return groups;
}

//
void Data::getCondition(GroupsPath& groups) {
    // ptree object containing
    pt::ptree ptData, ptSubcall, ptPath, ptGroup, ptCondition;

    for(auto& group : groups) { // iterate through the groups (e.g., ctrls, trtms)
        std::cout << helper::getTime() << group.first << ": " << group.second << " ";
        if(fs::exists(group.second)) { // check if the provided path exists...
            std::cout << "has been found in the filesystem!\n";
            if(fs::is_directory(group.second)) { // .. and check if its a directory (and not a file)
                // retrieve content (conditions) within group (e.g., rpl-exp,...) as absolute path
                PathVector conditions = helper::listDirFiles(group.second);

                for(auto& condition : conditions) {
                    if(fs::exists(condition) && !fs::is_directory(condition)) {
                        std::cout << helper::getTime() << "### WARNING - " << condition << " is not a directory! ";
                        std::cout << "This condition will be skipped!\n";
                        continue;
                    }

                    ptCondition = getData(group.first, condition);
                    ptGroup.push_back(std::make_pair("", ptCondition));

                }
                ptSubcall.add_child(group.first, ptGroup);
                ptGroup.erase("");

            } else {
                throw FileError(group.second.string() + " is not a directory");
            }
        } else {
            throw FileError(group.second.string() + " has not been found in the filesystem");
        }
    }

    // write data structure to file
    std::string subcall = params["subcall"].as<std::string>();
    dataStructure.add_child(subcall, ptSubcall);
    fs::path dataStructurePath = fs::path(params["outdir"].as<std::string>()) / fs::path("data.json");
    jp::write_json(dataStructurePath.string(), dataStructure);
    std::cout << helper::getTime() << "Data structure has been written to " << dataStructurePath << "\n";
}

//
pt::ptree Data::getData(std::string group, fs::path& condition) {
    std::string subcall = params["subcall"].as<std::string>();
    std::string outdir = params["outdir"].as<std::string>();

    pt::ptree ptCondition, ptFiles, ptSamples, ptSample, ptOutput;

    // scan the condition directory for files
    PathVector dataFiles = helper::listDirFiles(condition);
    dataFiles = helper::filterDirFiles(dataFiles, subcall);

    /*
    for(auto& file : dataFiles) {
        std::cout << file << std::endl;
    }*/

    // define helper variables
    int numberElements = getNumberElements(dataFiles);
    std::vector<std::string> sampleKeys = getSampleKeys();

    fs::path conditionOutDir = fs::path(outdir);
    conditionOutDir /= fs::path(subcall);
    conditionOutDir /= fs::path(group);
    conditionOutDir /= fs::path(condition.filename());

    int elementCounter = 0;
    for(unsigned i=0;i<dataFiles.size();++i) {
        ptFiles.put(sampleKeys[i % numberElements], dataFiles[i].string());
        if(elementCounter == numberElements-1) {
            ptSample.add_child("input", ptFiles);
            // determine output TODO
            ptOutput = getOutputData(ptSample, conditionOutDir);
            ptSample.add_child("output", ptOutput);

            ptSamples.push_back(std::make_pair("", ptSample));
            ptSample.erase("input");
            ptSample.erase("output");
            for(unsigned j=0;j<sampleKeys.size();++j) {
                ptFiles.erase(sampleKeys[j]);
            }
            elementCounter = 0;
        } else {
            ++elementCounter;
        }
    }
    ptCondition.put("condition", condition.stem().string());
    ptCondition.add_child("samples", ptSamples);

    return ptCondition;
}


int Data::getNumberElements(PathVector& vec) {
    std::string subcall = params["subcall"].as<std::string>();
    bool isPE = params["readtype"].as<std::string>() == "PE";

    if(subcall == "preproc") { return isPE ? 2 : 1; }
    else if(subcall == "align") { return isPE ? 5 : 1; }
    else if(subcall == "detect") { return 1; }
    else if(subcall == "clustering" || subcall == "analysis") { return 3; }
    return 1;
}

std::vector<std::string> Data::getSampleKeys() {
    std::string subcall = params["subcall"].as<std::string>();
    bool isPE = params["readtype"].as<std::string>() == "PE";

    if(subcall == "preproc") { return {"forward", "reverse"}; }
    else if(subcall == "align") {
        return isPE ? std::vector<std::string>{"forward", "R1only", "R1unmerged", "R2only", "R2unmerged"}
                    : std::vector<std::string>{"forward"};
    }
    else if(subcall == "detect") { return {"matched"}; }
    else if(subcall == "clustering" || subcall == "analysis") { return {"multsplits", "single", "splits"}; }
    return {};
}

pt::ptree Data::getOutputData(pt::ptree& input, fs::path& conditionOutDir) {
    std::string subcall = params["subcall"].as<std::string>();

    if(subcall == "preproc") { return getPreprocOutputData(input, conditionOutDir); }
    else if(subcall == "align") { return getAlignOutputData(input, conditionOutDir); }
    else if(subcall == "detect") { return getDetectOutputData(input, conditionOutDir); }
    else if(subcall == "analysis") { return getAnalysisOutputData(input, conditionOutDir); }
    return {};
}

pt::ptree Data::getPreprocOutputData(pt::ptree& input, fs::path& conditionOutDir) {
    pt::ptree output;

    // replace input path with output path (results/...)
    fs::path inForward = fs::path(input.get<std::string>("input.forward"));
    std::string outForward = helper::replacePath(conditionOutDir, inForward).string();

    // create output files using the input files with an additonal suffix
    std::string outPreproc = helper::addSuffix(outForward,"_preproc", {"_R1","fwd"});
    output.put("forward", outPreproc); // write output back to ptree

    // using paired-end reads results in additional files for unassembled reads
    if(params["readtype"].as<std::string>() == "PE") {
        // replace
        fs::path inReverse = fs::path(input.get<std::string>("input.reverse"));
        std::string outReverse = helper::replacePath(conditionOutDir, inReverse).string();

        // create outfiles using the input files with an additional suffix
        std::string r1only = helper::addSuffix(outForward,"_R1only", {"_R1","fwd"});
        std::string r2only = helper::addSuffix(outReverse,"_R2only", {"_R2","rev"});

        std::string r1unmerged = helper::addSuffix(outForward, "_R1unmerged", {"_R1","fwd"});
        std::string r2unmerged = helper::addSuffix(outReverse, "_R2unmerged", {"_R2","rev"});

        output.put("R1only", r1only); // push both back to output ptree
        output.put("R2only", r2only);

        output.put("R1unmerged", r1unmerged);
        output.put("R2unmerged", r2unmerged);
    }

    return output;
}

pt::ptree Data::getAlignOutputData(pt::ptree& input, fs::path& conditionOutDir) {
    pt::ptree output;

    // replace input path with output path (results/...)
    fs::path inPreproc = fs::path(input.get<std::string>("input.forward"));
    std::string outPreproc = helper::replacePath(conditionOutDir,
                                              inPreproc.replace_extension(".bam")).string();

    // create output (before adding suffix for file)
    output.put("matched", helper::addSuffix(outPreproc, "_matched", {}));

    if(params["readtype"].as<std::string>() == "PE") {
        // create output file R1 only reads (
        fs::path inR1only = fs::path(input.get<std::string>("input.R1only"));
        std::string outR1only = helper::replacePath(conditionOutDir,
                                                    inR1only.replace_extension(".bam")).string();
        output.put("matched_R1only", helper::addSuffix(outR1only, "_matched", {}));

        // create output file R2 only reads
        fs::path inR2only = fs::path(input.get<std::string>("input.R2only"));
        std::string outR2only = helper::replacePath(conditionOutDir,
                                                    inR1only.replace_extension(".bam")).string();
        output.put("matched_R2only", helper::addSuffix(outR2only, "_matched", {}));

        // create out file for unmerged reads
        output.put("matched_unmerged", helper::addSuffix(outPreproc, "_unmerged_matched",
                                                 {"_preproc"}));
    }

    return output;
}

pt::ptree Data::getDetectOutputData(pt::ptree& input, fs::path& conditionOutDir) {
    pt::ptree output;

    // replace input path with output path (results/...)
    fs::path inMatched = fs::path(input.get<std::string>("input.matched"));
    std::string outMatched = helper::replacePath(conditionOutDir, inMatched).string();
    output.put("single", helper::addSuffix(outMatched, "_single", {"_matched"}));
    output.put("splits", helper::addSuffix(outMatched, "_splits", {"_matched"}));
    output.put("multsplits", helper::addSuffix(outMatched, "_multsplits", {"_matched"}));
    return output;
}

pt::ptree Data::getAnalysisOutputData(pt::ptree& input, fs::path& conditionOutDir) {
    pt::ptree output;

    // replace input path with output path (results/...)
    fs::path inSplits = fs::path(input.get<std::string>("input.splits"));
    std::string outSplits = helper::replacePath(conditionOutDir, inSplits.replace_extension(".txt")).string();
    output.put("interactions", helper::addSuffix(outSplits, "_interactions", {"_splits"}));
    return output;
}

template <typename Callable>
void Data::callInAndOut(Callable f) {

    // retrieve paths for parameters
    fs::path outDir = fs::path(params["outdir"].as<std::string>());
    std::string subcallStr = params["subcall"].as<std::string>();
    fs::path outSubcallDir = outDir / fs::path(subcallStr);

    pt::ptree subcall = dataStructure.get_child(subcallStr);
    std::deque<std::string> groups = {"trtms"};
    if(subcall.size() > 1) {
        groups.push_front("ctrls");
    }

    pt::ptree conditions, samples, path;
    fs::path outGroupDir, outConditionDir;

    for(unsigned i=0;i<groups.size();++i) {
        // create directory for groups (e.g., ctrls, trtms)
        outGroupDir = outSubcallDir / fs::path(groups[i]);
        if(params["subcall"].as<std::string>() != "clustering") {
            // create directory for groups (e.g., ctrls, trtms)
            helper::createDir(outGroupDir, std::cout); // not needed for clustering
        }

        conditions = subcall.get_child(groups[i]);
        BOOST_FOREACH(pt::ptree::value_type const &v, conditions.get_child("")) {
            pt::ptree condition = v.second;

            // create directory for condition (e.g., rpl_exp)
            if(params["subcall"].as<std::string>() != "clustering") {
                outConditionDir = outGroupDir / fs::path(condition.get<std::string>("condition"));
                helper::createDir(outConditionDir, std::cout);
            }

            samples = condition.get_child("samples");
            // iterate over samples
            BOOST_FOREACH(pt::ptree::value_type const &w, samples.get_child("")) {
                pt::ptree sample = w.second;
                f(sample, condition);
            }
        }
    }
}

//
void Data::preproc() {
    std::cout << helper::getTime() << "Start the Preprocessing\n";
    SeqRickshaw srs(params);
    callInAndOut(std::bind(&SeqRickshaw::start, srs, std::placeholders::_1));
}

void Data::align() {
    std::cout << helper::getTime() << "Start the Alignment\n";
    Align aln(params);
    callInAndOut(std::bind(&Align::start, aln, std::placeholders::_1));
}

void Data::detect() {
    std::cout << helper::getTime() << "Start the Split Read Calling\n";
    SplitReadCalling src(params);
    callInAndOut(std::bind(&SplitReadCalling::start, &src, std::placeholders::_1, std::placeholders::_2));
    src.writeStats();
}

void Data::clustering() {
    std::cout << helper::getTime() << "Start the Clustering of the Split Reads\n";
    Clustering clu(params);
    callInAndOut(std::bind(&Clustering::start, &clu, std::placeholders::_1));
    clu.sumup();
}

void Data::analysis() {
    std::cout << helper::getTime() << "Start the Analysis of the Split Reads\n";
    Analysis ana(params);
    callInAndOut(std::bind(&Analysis::start, &ana, std::placeholders::_1, std::placeholders::_2));
    // write file output files
    ana.writeAllInts();
    ana.writeAllIntsCounts();
    ana.writeAllIntsJGF();
    ana.writeStats();
}

