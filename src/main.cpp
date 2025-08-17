#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <zlib.h>
#include <iomanip>
#include <algorithm>
#include <openssl/sha.h>

#include "commit.hpp"
#include "branch.hpp"
#include "features/comment_generator.hpp"
#include "ai/ai_client.hpp"
#include "utils/file_utils.hpp"
#include "features/interactive_review.hpp"
#include "features/review_generator.hpp"
#include "features/commit_splitter.hpp"

struct VitConfig {
    bool localAI = true;
    std::string userName;
    std::string userEmail;
};

VitConfig config;

void persistConfig() {
    std::ofstream configFile(".vitconfig");
    configFile << config.localAI << '\n';
    configFile << config.userName << '\n';
    configFile << config.userEmail << '\n';
    configFile.close();
}

void loadConfig() {
    std::ifstream configFile(".vitconfig");
    configFile >> config.localAI;
    configFile >> config.userName;
    configFile >> config.userEmail;
    configFile.close();
}

bool handleInit() {
    try {
        std::filesystem::create_directory(".git");
        std::filesystem::create_directory(".git/objects");
        std::filesystem::create_directory(".git/refs");
        std::filesystem::create_directory(".git/refs/heads");

        // Initialize HEAD to point to main branch (but no commits yet)
        std::ofstream headFile(".git/HEAD");
        if (headFile.is_open()) {
            headFile << "ref: refs/heads/main\n";
            headFile.close();
        } else {
            std::cerr << "Failed to create .git/HEAD file.\n";
            return false;
        }

        std::cout << "Initialized vit directory\n";
        return true;
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << e.what() << '\n';
        return false;
    }
}

bool handleCatFile(int argc, char *argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: cat-file -p <hash>\n";
        return false;
    }
    
    std::string flag = argv[2];
    if (flag != "-p") {
        std::cerr << "Unknown flag: " << flag << '\n';
        return false;
    }

    std::string objectHash = argv[3];
    std::string content = readObjectContent(objectHash);
    if (content.empty()) {
        return false;
    }
    
    std::cout << content;
    return true;
}

bool handleHashObject(int argc, char *argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: hash-object -w <file>\n";
        return false;
    }
    
    std::string flag = argv[2];
    if (flag != "-w") {
        std::cerr << "Unknown flag: " << flag << '\n';
        return false;
    }

    std::string file = argv[3];
    std::ifstream fileStream(file);
    if (!fileStream.is_open()) {
        std::cerr << "Failed to open file: " << file << '\n';
        return false;
    }
    
    std::stringstream contentStream;
    contentStream << fileStream.rdbuf();
    std::string fileContent = contentStream.str();

    std::string hashString = writeBlob(fileContent);
    if (hashString.empty()) {
        return false;
    }
    
    std::cout << hashString << '\n';
    return true;
}

bool handleLsTree(int argc, char *argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: ls-tree <hash> [--name-only|-l]\n";
        return false;
    }
    
    std::string flag = argv[3];
    bool nameOnly = (flag == "--name-only");
    if (flag != "--name-only" && flag != "-l") {
        std::cerr << "Unknown flag: " << flag << '\n';
        return false;
    }
    
    std::string treeHash = argv[2];
    std::vector<FileInfo> files = parseTree(treeHash);
    
    if (files.empty()) {
        std::cerr << "Tree not found or empty: " << treeHash << '\n';
        return false;
    }
    
    for (const auto& file : files) {
        if (nameOnly) {
            std::cout << file.name << '\n';
        } else {
            std::string type = file.isDirectory ? "tree" : "blob";
            std::cout << file.mode << " " << type << " " << file.hash << "\t" << file.name << '\n';
        }
    }
    
    return true;
}

bool handleWriteTree() {
    std::string hashString = writeTree(".");
    if (hashString.empty()) {
        std::cerr << "Failed to write tree\n";
        return false;
    }
    std::cout << hashString << '\n';
    return true;
}

bool handleCommitTree(int argc, char *argv[]) {
    if (argc < 7) {
        std::cerr << "Usage: commit-tree <tree_sha> -p <commit_sha> -m <message>\n";
        return false;
    }

    std::string treeHash = argv[2];
    std::string parentHash = argv[4];  // -p flag
    std::string message = argv[6];     // -m flag
    
    std::string author = config.userName;
    std::string email = config.userEmail;

    std::string commitHash = writeCommit(treeHash, parentHash, message, author, email);
    if (commitHash.empty()) {
        std::cerr << "Failed to create commit\n";
        return false;
    }
    
    std::cout << commitHash << '\n';
    return true;
}

struct ParsedArgs {
    bool generateReview = false;
    bool addComments = false;
    std::vector<std::string> targetFiles;
};

ParsedArgs parseCommitArguments(int argc, char *argv[], int startIndex) {
    ParsedArgs args;
    
    for (int i = startIndex; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--review") {
            args.generateReview = true;
        } else if (arg == "--add-comments") {
            args.addComments = true;
        } else if (arg.substr(0, 2) != "--") {
            // Not a flag, must be a file
            args.targetFiles.push_back(arg);
        } else {
            std::cerr << "Warning: Unknown flag " << arg << std::endl;
        }
    }
    
    return args;
}

std::unique_ptr<vit::ai::AIClient> setupAI(bool localAI) {
    std::unique_ptr<vit::ai::AIClient> client;
    if (localAI) {
        client = vit::ai::AI::createOllama();

    } else {
        client = vit::ai::AI::createOpenAI(vit::ai::AI::getEnvVar("OPENAI_API_KEY"));
    }
    if (!client) {
        if (localAI) {
            std::cerr << "Failed to create local AI client.\n";
        } else {
            std::cerr << "Failed to create AI client. Please set OPENAI_API_KEY environment variable.\n";
        }
        return nullptr;
    }
    return client;
}

bool handleCommit(int argc, char *argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: commit -m <message> [--add-comments] [--review] [file1 file2 ...]\n";
        return false;
    }
    
    std::string flag = argv[2];
    if (flag != "-m") {
        std::cerr << "Usage: commit -m <message>\n";
        return false;
    }
    
    std::string message = argv[3];
    
    ParsedArgs args = parseCommitArguments(argc, argv, 4); // Start parsing after message

    // Create AI client if needed
    std::unique_ptr<vit::ai::AIClient> client;
    if (args.generateReview || args.addComments) {
        client = setupAI(config.localAI);
    }

    // Determine target files
    std::vector<std::string> files;
    if (!args.targetFiles.empty()) {
        // Use explicitly specified files
        files = args.targetFiles;
        std::cout << "Processing " << files.size() << " specified file(s)\n";
    } else if (args.generateReview || args.addComments) {
        // Use all source files in directory if AI features are enabled
        files = vit::utils::FileUtils::getSourceFilesInDirectory(".");
        std::cout << "Processing " << files.size() << " source file(s) from directory\n";
    }

    // Handle --add-comments
    if (args.addComments) {
        std::cout << "Generating AI comments...\n";
        
        // Create separate client for comments (since we may need two AI calls)
        auto commentClient = vit::ai::AI::createOpenAI(vit::ai::AI::getEnvVar("OPENAI_API_KEY"));
        if (!commentClient) {
            std::cerr << "Failed to create AI client for comments.\n";
            return false;
        }
        
        vit::features::CommentGenerator commentGenerator(std::move(commentClient));
        auto results = commentGenerator.generateCommentsForFiles(files);
        
        // Interactive review step
        vit::features::InteractiveReview review;
        auto reviewResult = review.reviewComments(results);
        
        if (!reviewResult.shouldProceed) {
            std::cout << "Commit cancelled by user.\n";
            return false;
        }
        
        // Apply only accepted comments
        for (const auto& acceptedResult : reviewResult.accepted) {
            bool writeSuccess = vit::utils::FileUtils::writeFile(acceptedResult.fileName, acceptedResult.modifiedContent);
            if (writeSuccess) {
                std::cout << "✓ Applied comments to " << acceptedResult.fileName << std::endl;
            } else {
                std::cout << "✗ Failed to write to " << acceptedResult.fileName << std::endl;
            }
        }
        
        if (!reviewResult.rejected.empty()) {
            std::cout << "Skipped " << reviewResult.rejected.size() << " file(s).\n";
        }
    }

    // Handle --review (BEFORE commit)
    if (args.generateReview) {        
        vit::features::ReviewGenerator reviewGenerator(std::move(client));
        auto reviewResult = reviewGenerator.generateReviewForFiles(files);
        
        if (!reviewResult.success) {
            std::cerr << "Warning: Failed to generate AI review: " << reviewResult.error << "\n";
            std::cout << "Proceeding with commit without review...\n";
        } else {
            // Write review to file
            if (!vit::utils::FileUtils::writeFile("review.md", reviewResult.reviewContent)) {
                std::cerr << "Warning: Failed to write review.md file\n";
            } else {
                std::cout << "✓ AI review generated as review.md\n";
            }
        }
    }

    // Commit everything (including review.md if generated)
    std::string treeHash = writeTree(".");
    if (treeHash.empty()) {
        std::cerr << "Failed to create tree\n";
        return false;
    }
    
    std::string parentHash = readHead();
    std::string author = config.userName;
    std::string email = config.userEmail;
    
    std::string commitHash = writeCommit(treeHash, parentHash, message, author, email);
    if (commitHash.empty()) {
        std::cerr << "Failed to create commit\n";
        return false;
    }
    
    std::string currentBranch = getCurrentBranch();
    if (!currentBranch.empty()) {
        if (!updateBranch(currentBranch, commitHash)) {
            return false;
        }
        std::cout << "Created commit " << commitHash << " on branch '" << currentBranch << "'";
    } else {
        writeHead(commitHash);
        std::cout << "Created commit " << commitHash << " (detached HEAD)";
    }
    
    if (args.generateReview) {
        std::cout << " with AI review";
    }
    std::cout << "\n";
    
    return true;
}

bool handleSplitCommit(int argc, char *argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: split-commit -m <default-message>\n";
        return false;
    }
    
    std::string message = argv[3];
    std::string commitHash = readHead();

    std::unique_ptr<vit::ai::AIClient> client = setupAI(config.localAI);

    vit::features::CommitSplitter commitSplitter(std::move(client), config.userName, config.userEmail);
    auto result = commitSplitter.analyzeAndSuggestSplits(commitHash, message);

    if (!result.success) {
        std::cerr << "Failed to split commit: " << result.error << "\n";
        return false;
    }

    // Dry run first
    std::cout << "Dry run:\n";
    commitSplitter.executeSplits(result, true);
    
    std::cout << "Proceed with splits? (y/n): ";
    std::string input;
    std::cin >> input;
    if (input != "y") {
        std::cout << "Commit split cancelled.\n";
        return false;
    }

    // Commit the splits
    commitSplitter.executeSplits(result, false);

    std::cout << "Commit split complete.\n";
    return true;
}


bool handleShowHead() {
    std::string headHash = readHead();
    if (headHash.empty()) {
        std::cout << "No commits yet\n";
    } else {
        std::cout << "HEAD: " << headHash << '\n';
    }
    return true;
}

bool handleLog(int argc, char *argv[]) {
    bool showAll = (argc >= 3 && std::string(argv[2]) == "--all");
    std::string currentHead = readHead();

    std::vector<std::string> hashesToShow;

    if (showAll) {
        hashesToShow = findAllCommitHashes();
    } else {
        std::vector<std::string> refs = {currentHead};
        hashesToShow = getReachableCommits(refs);
    }

    if (hashesToShow.empty()) {
        std::cout << "No commits found\n";
        return true;
    }

    for (const auto& hash : hashesToShow) {
        CommitInfo commit = parseCommit(hash);
        if (commit.hash.empty()) continue;

        std::string headMark = (hash == currentHead) ? "   <-- HEAD" : "";

        std::cout << "commit " << commit.hash << headMark << '\n';
        std::cout << "Author: " << commit.author << '\n';
        std::cout << "Date:   " << formatTimestamp(commit.timestamp) << '\n';
        std::cout << '\n';
        std::cout << "    " << commit.message << '\n';
        std::cout << '\n';
    }

    return true;
}

bool handleCheckout(int argc, char *argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: checkout <commit-hash-or-branch-name>\n";
        return false;
    }
    
    std::string target = argv[2];
    std::string commitHash;
    
    // Check if target is a branch name
    std::string branchPath = ".git/refs/heads/" + target;
    if (std::filesystem::exists(branchPath)) {
        // It's a branch name
        std::ifstream branchFile(branchPath);
        std::getline(branchFile, commitHash);
        branchFile.close();
        
        if (commitHash.empty()) {
            std::cerr << "Branch '" << target << "' has no commits\n";
            return false;
        }
        
        if (!safeCheckout(commitHash)) {
            return false;
        }
        
        if (!switchToBranch(target)) {
            return false;
        }
        
        std::cout << "Switched to branch '" << target << "'\n";
    } else {
        // Assume it's a commit hash
        commitHash = target;
        
        if (!safeCheckout(commitHash)) {
            return false;
        }
        
        std::cout << "HEAD is now at " << commitHash << " (detached HEAD)\n";
    }
    
    return true;
}

bool handleGC() {
    std::cout << "Running garbage collection...\n";
    
    std::vector<std::string> allCommits = findAllCommitHashes();
    std::vector<std::string> refs = collectReferenceCommits();
    auto reachableVec = getReachableCommits(refs);
    std::unordered_set<std::string> reachable(reachableVec.begin(),
                                          reachableVec.end());

    // delete unreachable commits
    int deleted = 0;
    for (const auto& hash : allCommits) {
        std::string path = getObjectPath(hash);
        if (reachable.count(hash) == 0) {
            if (!std::filesystem::exists(path)) {
                continue;
            }
            try {
                std::filesystem::remove(path);
                std::cout << "[GC] Deleted: " << hash << '\n';
                ++deleted;
            } catch (const std::filesystem::filesystem_error& e) {
                std::cerr << "[GC] Failed to delete " << hash << ": " << e.what() << '\n';
            }
        }
    }
    std::cout << "Garbage collection complete. " << deleted << " commits deleted.\n";
    return true;
}

bool handleBranch(int argc, char *argv[]) {
    if (argc == 2) {
        std::string currentBranch = getCurrentBranch();
        
        if (!std::filesystem::exists(".git/refs/heads")) {
            std::cout << "No branches yet\n";
            return true;
        }
        
        for (const auto& entry : std::filesystem::directory_iterator(".git/refs/heads")) {
            std::string branchName = entry.path().filename().string();
            if (branchName == currentBranch) {
                std::cout << "* " << branchName << '\n';  // Mark current branch
            } else {
                std::cout << "  " << branchName << '\n';
            }
        }
        return true;
    } else if (argc == 3) {
        // Create new branch
        std::string newBranchName = argv[2];
        std::string currentCommit = readHead();
        
        if (currentCommit.empty()) {
            std::cerr << "No commits yet - cannot create branch\n";
            return false;
        }
        
        if (!updateBranch(newBranchName, currentCommit)) {
            return false;
        }
        
        std::cout << "Created branch '" << newBranchName << "'\n";
        return true;
    } else {
        std::cerr << "Usage: branch [branch-name]\n";
        return false;
    }
}

bool handleConfig(int argc, char *argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: config <command>\n";
        return false;
    }

    std::string command = argv[2];
    if (command == "local-ai") {
        config.localAI = true;
    } else if (command == "api-ai") {
        config.localAI = false;
    }else if (command == "user-name") {
        config.userName = argv[3];
    } else if (command == "user-email") {
        config.userEmail = argv[3];
    } else if (command == "print") {
        std::cout << "localAI: " << config.localAI << '\n';
        std::cout << "userName: " << config.userName << '\n';
        std::cout << "userEmail: " << config.userEmail << '\n';
    } else {
        std::cerr << "Unknown config command: " << command << '\n';
        return false;
    }

    persistConfig();
    return true;
}

int main(int argc, char *argv[]) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    loadConfig();

    if (argc < 2) {
        std::cerr << "No command provided.\n";
        return EXIT_FAILURE;
    }
    
    std::string command = argv[1];
    bool success = false;
    
    if (command == "init") {
        success = handleInit();
    } else if (command == "cat-file") {
        success = handleCatFile(argc, argv);
    } else if (command == "hash-object") {
        success = handleHashObject(argc, argv);
    } else if (command == "ls-tree") {
        success = handleLsTree(argc, argv);
    } else if (command == "write-tree") {
        success = handleWriteTree();
    } else if (command == "commit-tree") {
        success = handleCommitTree(argc, argv);
    } else if (command == "commit") {
        success = handleCommit(argc, argv);
    } else if (command == "split-commit") {
        success = handleSplitCommit(argc, argv);
    } else if (command == "show-head") {
        success = handleShowHead();
    } else if (command == "log") {
        success = handleLog(argc, argv);
    } else if (command == "checkout") {
        success = handleCheckout(argc, argv);
    } else if (command == "gc") {
        success = handleGC();
    } else if (command == "branch") {
        success = handleBranch(argc, argv);
    } else if (command == "config") {
        success = handleConfig(argc, argv);
    }
    else {
        std::cerr << "Unknown command " << command << '\n';
        return EXIT_FAILURE;
    }
    
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
