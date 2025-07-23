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
    
    std::string author = "Vincent Schacknies";
    std::string email = "vincent.schacknies@icloud.com";

    std::string commitHash = writeCommit(treeHash, parentHash, message, author, email);
    if (commitHash.empty()) {
        std::cerr << "Failed to create commit\n";
        return false;
    }
    
    std::cout << commitHash << '\n';
    return true;
}

bool handleCommit(int argc, char *argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: commit -m <message>\nOptional flags: --add-comments <file1> <file2> ...";
        return false;
    }
    
    std::string flag = argv[2];
    if (flag != "-m") {
        std::cerr << "Usage: commit -m <message>\n";
        return false;
    }
    
    std::string message = argv[3];

    if (argc > 4) {
        std::string flag = argv[4];
        if (flag == "--add-comments") {
            std::unique_ptr<vit::ai::AIClient> client = vit::ai::AI::createOpenAI(vit::ai::AI::getEnvVar("OPENAI_API_KEY"));
            if (!client) {
                std::cerr << "Failed to create AI client. Please set OPENAI_API_KEY environment variable.\n";
                return false;
            }
            
            vit::features::CommentGenerator commentGenerator(std::move(client));

            std::vector<std::string> files;
            if (argc > 5) {
                std::stringstream target_files = std::stringstream(argv[5]);
                std::string file;
                while (std::getline(target_files, file, ' ')) {
                    files.push_back(file);
                }
            } else {
                files = vit::utils::FileUtils::getFilesInDirectory(".");
            }
            
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
                std::cout << "Attempting to write to: '" << acceptedResult.fileName << "'" << std::endl;
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
    }

    
    std::string treeHash = writeTree(".");
    if (treeHash.empty()) {
        std::cerr << "Failed to create tree\n";
        return false;
    }
    
    std::string parentHash = readHead();
    
    std::string author = "Vincent Schacknies";
    std::string email = "vincent.schacknies@icloud.com";
    
    std::string commitHash = writeCommit(treeHash, parentHash, message, author, email);
    if (commitHash.empty()) {
        std::cerr << "Failed to create commit\n";
        return false;
    }
    
    std::string currentBranch = getCurrentBranch();
    if (!currentBranch.empty()) {
        // We're on a branch - update the branch
        if (!updateBranch(currentBranch, commitHash)) {
            return false;
        }
        std::cout << "Created commit " << commitHash << " on branch '" << currentBranch << "'\n";
    } else {
        // Detached HEAD - just update HEAD directly (existing behavior)
        writeHead(commitHash);
        std::cout << "Created commit " << commitHash << " (detached HEAD)\n";
    }
    
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

int main(int argc, char *argv[]) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

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
    } else {
        std::cerr << "Unknown command " << command << '\n';
        return EXIT_FAILURE;
    }
    
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
