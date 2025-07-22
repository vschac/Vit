#include "branch.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>


// returns empty if detached HEAD
std::string getCurrentBranch() {
    std::ifstream headFile(".git/HEAD");
    if (!headFile.is_open()) {
        return "";
    }
    
    std::string headContent;
    std::getline(headFile, headContent);
    headFile.close();
    
    if (headContent.substr(0, 5) == "ref: ") {
        std::string refPath = headContent.substr(5); // remove "ref: "
        // removes "refs/heads/main" -> "main"
        if (refPath.substr(0, 11) == "refs/heads/") {
            return refPath.substr(11);
        }
    }
    
    return "";
}

bool updateBranch(const std::string& branchName, const std::string& commitHash) {
    std::string branchPath = ".git/refs/heads/" + branchName;
    std::filesystem::create_directories(".git/refs/heads");
    
    std::ofstream branchFile(branchPath);
    if (!branchFile.is_open()) {
        std::cerr << "Failed to update branch: " << branchName << '\n';
        return false;
    }
    
    branchFile << commitHash << '\n';
    branchFile.close();
    return true;
}

bool switchToBranch(const std::string& branchName) {
    std::ofstream headFile(".git/HEAD");
    if (!headFile.is_open()) {
        std::cerr << "Failed to update HEAD\n";
        return false;
    }
    
    headFile << "ref: refs/heads/" << branchName << '\n';
    headFile.close();
    return true;
}

bool writeHeadAsBranch(const std::string& hash, const std::string& branch)
{
    const std::string refPath = ".git/refs/heads/" + branch;
    std::filesystem::create_directories(std::filesystem::path(refPath).parent_path());
    std::ofstream(refPath)    << hash << '\n';
    std::ofstream(".git/HEAD") << "ref: refs/heads/" << branch << '\n';
    return true;
}