#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <sstream>
#include <zlib.h>
#include <openssl/sha.h>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <ctime>
#include <set>
#include <functional>
#include <unordered_set>
#include <queue>

struct TreeEntry {
    std::string mode;
    std::string hash;
    std::string filename;
};


std::string hashToHexString(const unsigned char* hash) {
    std::stringstream hashStream;
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        hashStream << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(hash[i]);
    }
    return hashStream.str();
}

std::string hexStringToBinary(const std::string& hexString) {
    std::string binary;
    for (int i = 0; i < 20; i++) {
        std::string byteStr = hexString.substr(i * 2, 2);
        char byte = static_cast<char>(std::stoi(byteStr, nullptr, 16));
        binary += byte;
    }
    return binary;
}

std::string binaryToHexString(const std::string& binary) {
    std::string hexString;
    for (unsigned char byte : binary) {
        std::stringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(byte);
        hexString += ss.str();
    }
    return hexString;
}

std::string getObjectPath(const std::string& hash) {
    return ".git/objects/" + hash.substr(0, 2) + "/" + hash.substr(2);
}

std::string writeObject(const std::string& type, const std::string& content) {
    // Format: <type> <size>\0<content>
    std::string formattedContent = type + " " + std::to_string(content.size()) + '\0' + content;

    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(formattedContent.data()), 
         formattedContent.size(), 
         hash);
    
    std::string hashString = hashToHexString(hash);
    
    uLong compressedSize = compressBound(formattedContent.size());
    std::string compressedContent(compressedSize, 0);
    
    int compressResult = compress(reinterpret_cast<Bytef*>(&compressedContent[0]), &compressedSize,
                                reinterpret_cast<const Bytef*>(formattedContent.data()), 
                                formattedContent.size());
    if (compressResult != Z_OK) {
        std::cerr << "Compression failed\n";
        return "";
    }
    compressedContent.resize(compressedSize);
    
    std::string saveDir = ".git/objects/" + hashString.substr(0, 2);
    std::filesystem::create_directories(saveDir);
    
    std::string saveFile = getObjectPath(hashString);
    std::ofstream saveFileStream(saveFile, std::ios::binary);
    if (!saveFileStream.is_open()) {
        std::cerr << "Failed to create save file.\n";
        return "";
    }

    saveFileStream.write(compressedContent.data(), compressedContent.size());
    saveFileStream.close();
    
    return hashString;
}

std::string writeBlob(const std::string& content) {
    return writeObject("blob", content);
}

std::string writeTree(const std::string& directoryPath) {
    std::vector<TreeEntry> entries;
    
    for (const auto& entry : std::filesystem::directory_iterator(directoryPath)) {
        std::string name = entry.path().filename().string();
        
        if (name == ".git") continue;
        
        TreeEntry treeEntry;
        treeEntry.filename = name;
        
        if (entry.is_regular_file()) {
            std::ifstream file(entry.path());
            std::stringstream contentStream;
            contentStream << file.rdbuf();
            std::string content = contentStream.str();
            
            treeEntry.hash = writeBlob(content);
            treeEntry.mode = "100644";
        } else if (entry.is_directory()) {
            treeEntry.hash = writeTree(entry.path().string());
            treeEntry.mode = "40000";
        }
        
        if (treeEntry.hash.empty()) return "";
        entries.push_back(treeEntry);
    }
    
    std::sort(entries.begin(), entries.end(), 
              [](const TreeEntry& a, const TreeEntry& b) {
                  return a.filename < b.filename;
              });
    
    // Build tree content
    std::string treeContent;
    for (const auto& entry : entries) {
        treeContent += entry.mode + " " + entry.filename + '\0';
        treeContent += hexStringToBinary(entry.hash);
    }
    
    return writeObject("tree", treeContent);
}

std::string decompressObject(const std::string& compressedContent) {
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = compressedContent.size();
    strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressedContent.data()));
    
    int ret = inflateInit(&strm);
    if (ret != Z_OK) {
        std::cerr << "inflateInit failed\n";
        return "";
    }
    
    std::string decompressedContent;
    char outBuffer[4096];
    
    do {
        strm.avail_out = sizeof(outBuffer);
        strm.next_out = reinterpret_cast<Bytef*>(outBuffer);
        ret = inflate(&strm, Z_NO_FLUSH);
        
        if (ret != Z_STREAM_END && ret != Z_OK) {
            std::cerr << "inflate failed: " << ret << "\n";
            inflateEnd(&strm);
            return "";
        }
        
        int have = sizeof(outBuffer) - strm.avail_out;
        decompressedContent.append(outBuffer, have);
    } while (strm.avail_out == 0);
    
    inflateEnd(&strm);
    return decompressedContent;
}

std::string readObject(const std::string& hash) {
    std::string objectPath = getObjectPath(hash);
    
    std::ifstream objectFile(objectPath, std::ios::binary);
    if (!objectFile.is_open()) {
        std::cerr << "Object not found: " << hash << '\n';
        return "";
    }

    std::stringstream objectStream;
    objectStream << objectFile.rdbuf();
    std::string compressedContent = objectStream.str();

    return decompressObject(compressedContent);
}

// without header
std::string readObjectContent(const std::string& hash) {
    std::string decompressedContent = readObject(hash);
    if (decompressedContent.empty()) {
        return "";
    }
    
    size_t nullPos = decompressedContent.find('\0');
    if (nullPos != std::string::npos) {
        return decompressedContent.substr(nullPos + 1);
    }
    return decompressedContent;
}

std::string writeCommit(const std::string& treeHash, const std::string& parentHash, 
                       const std::string& message, const std::string& author, 
                       const std::string& email) {
    // commit <size>\0tree <hash>\nparent <hash>\n...
    std::string content = "tree " + treeHash + "\n";
    if (!parentHash.empty()) {
        content += "parent " + parentHash + "\n";
    }
    content += "author " + author + " <" + email + "> " + std::to_string(std::time(nullptr)) + "\n";
    content += "committer " + author + " <" + email + "> " + std::to_string(std::time(nullptr)) + "\n\n";
    content += message + "\n";
    
    return writeObject("commit", content);
}

std::string readHead() {
    std::ifstream headFile(".git/HEAD");
    if (!headFile.is_open()) {
        return ""; // no head file exists (first commit)
    }
    
    std::string headContent;
    std::getline(headFile, headContent);
    headFile.close();
    
    // Handle both formats:
    // 1. "ref: refs/heads/main" (branch reference)
    // 2. "a1b2c3d4e5f6..." (direct commit hash)
    
    if (headContent.substr(0, 5) == "ref: ") {
        // branch reference, read the actual commit hash
        std::string refPath = ".git/" + headContent.substr(5);
        std::ifstream refFile(refPath);
        if (!refFile.is_open()) {
            return ""; // branch file doesn't exist yet (first commit)
        }
        
        std::string commitHash;
        std::getline(refFile, commitHash);
        refFile.close();
        return commitHash;
    } else {
        // direct commit hash
        return headContent;
    }
}

// Write the HEAD to point to a specific commit
bool writeHead(const std::string& commitHash) {
    // For now, we'll use the simple format: direct commit hash
    // Later we can add branch support
    std::ofstream headFile(".git/HEAD");
    if (!headFile.is_open()) {
        std::cerr << "Failed to write HEAD file\n";
        return false;
    }
    
    headFile << commitHash << '\n';
    headFile.close();
    return true;
}

// Alternative: Write HEAD as a branch reference (more Git-like)
bool writeHeadAsBranch(const std::string& commitHash, const std::string& branchName = "main") {
    // Write the commit hash to the branch file
    std::string branchPath = ".git/refs/heads/" + branchName;
    std::filesystem::create_directories(".git/refs/heads");
    
    std::ofstream branchFile(branchPath);
    if (!branchFile.is_open()) {
        std::cerr << "Failed to write branch file\n";
        return false;
    }
    
    branchFile << commitHash << '\n';
    branchFile.close();
    
    // Write HEAD to point to the branch
    std::ofstream headFile(".git/HEAD");
    if (!headFile.is_open()) {
        std::cerr << "Failed to write HEAD file\n";
        return false;
    }
    
    headFile << "ref: refs/heads/" << branchName << '\n';
    headFile.close();
    return true;
}

struct CommitInfo {
    std::string hash;
    std::string treeHash;
    std::string parentHash;
    std::string author;
    std::string message;
    std::string timestamp;
};

CommitInfo parseCommit(const std::string& commitHash) {
    CommitInfo info;
    info.hash = "";
    std::string content = readObjectContent(commitHash);
    if (content.empty() || content.substr(0,5) != "tree ")
        return info;               // early-return invalid commit

    info.hash = commitHash;
    
    std::istringstream stream(content);
    std::string line;
    
    while (std::getline(stream, line)) {
        if (line.substr(0, 5) == "tree ") {
            info.treeHash = line.substr(5);
        } else if (line.substr(0, 7) == "parent ") {
            info.parentHash = line.substr(7);
        } else if (line.substr(0, 7) == "author ") {
            // Parse author line: "author Name <email> timestamp"
            size_t emailStart = line.find('<');
            size_t emailEnd = line.find('>');
            if (emailStart != std::string::npos && emailEnd != std::string::npos) {
                info.author = line.substr(7, emailStart - 7 - 1); // Remove "author " and space before <
                size_t timestampStart = emailEnd + 2; // Skip "> "
                if (timestampStart < line.length()) {
                    info.timestamp = line.substr(timestampStart);
                }
            }
        } else if (line.substr(0, 10) == "committer ") {
            // Skip committer line for now
        } else if (line.empty()) {
            // Empty line separates headers from message
            // Read the rest as the commit message
            std::string message;
            while (std::getline(stream, line)) {
                if (!message.empty()) message += '\n';
                message += line;
            }
            info.message = message;
            break;
        }
    }
    
    return info;
}

// Convert Unix timestamp to readable format
std::string formatTimestamp(const std::string& timestamp) {
    if (timestamp.empty()) return "";
    
    try {
        time_t time = std::stol(timestamp);
        char buffer[100];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&time));
        return std::string(buffer);
    } catch (...) {
        return timestamp; // Return original if parsing fails
    }
}

// Command handler functions
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

struct FileInfo {
    std::string name;
    std::string hash;
    std::string mode;
    bool isDirectory;
};

// return list of files/directories
std::vector<FileInfo> parseTree(const std::string& treeHash) {
    std::vector<FileInfo> files;
    
    std::string treeContent = readObjectContent(treeHash);
    if (treeContent.empty()) {
        return files;
    }
    
    size_t pos = 0;
    while (pos < treeContent.size()) {
        // Find next null byte (end of current entry)
        size_t nextNull = treeContent.find('\0', pos);
        if (nextNull == std::string::npos) break;
        
        // Extract the entry (mode + name)
        std::string entry = treeContent.substr(pos, nextNull - pos);
        
        // Find the last space (separates mode from name)
        size_t lastSpace = entry.find_last_of(' ');
        if (lastSpace != std::string::npos) {
            FileInfo file;
            file.mode = entry.substr(0, lastSpace);
            file.name = entry.substr(lastSpace + 1);
            
            // Extract hash (20 bytes after null)
            std::string binaryHash = treeContent.substr(nextNull + 1, 20);
            file.hash = binaryToHexString(binaryHash);
            
            // Determine if it's a directory
            file.isDirectory = (file.mode.substr(0, 3) == "400");
            
            files.push_back(file);
        }
        
        // Skip the hash (20 bytes) and move to next entry
        pos = nextNull + 1 + 20;
    }
    
    return files;
}

bool restoreTree(const std::string& treeHash, const std::string& basePath = "") {
    std::vector<FileInfo> files = parseTree(treeHash);
    
    for (const auto& file : files) {
        std::string fullPath = basePath.empty() ? file.name : basePath + "/" + file.name;
        
        if (file.isDirectory) {
            // Create directory and recursively restore its contents
            if (!basePath.empty() || !file.name.empty()) {  // Prevent empty path
                std::filesystem::create_directories(fullPath);
                if (!restoreTree(file.hash, fullPath)) {
                    return false;
                }
            }
        } else {
            // Create file with content from blob
            std::string content = readObjectContent(file.hash);
            if (content.empty()) {
                std::cerr << "Failed to read blob: " << file.hash << '\n';
                return false;
            }
            
            // Ensure parent directory exists
            std::filesystem::path filePath(fullPath);
            if (filePath.has_parent_path()) {
                std::filesystem::create_directories(filePath.parent_path());
            }
            
            // Write file
            std::ofstream outFile(fullPath);
            if (!outFile.is_open()) {
                std::cerr << "Failed to create file: " << fullPath << '\n';
                return false;
            }
            outFile << content;
            outFile.close();
        }
    }
    
    return true;
}

// Get list of all files in current working directory (recursive)
std::vector<std::string> getAllFiles(const std::string& path = ".") {
    std::vector<std::string> files;
    
    for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
        std::string entryPath = entry.path().string();
        
        // Skip .git directory
        if (entryPath.find("/.git") != std::string::npos || 
            entryPath.find("\\.git") != std::string::npos) {
            continue;
        }
        
        if (entry.is_regular_file()) {
            files.push_back(entryPath);
        }
    }
    
    return files;
}

// Get all files that should exist in target commit (recursive)
std::set<std::string> getTargetFiles(const std::string& treeHash, const std::string& basePath = "") {
    std::set<std::string> targetFiles;
    
    std::vector<FileInfo> files = parseTree(treeHash);
    for (const auto& file : files) {
        std::string fullPath = basePath.empty() ? file.name : basePath + "/" + file.name;
        
        if (file.isDirectory) {
            // Recursively get files from subdirectory
            auto subFiles = getTargetFiles(file.hash, fullPath);
            targetFiles.insert(subFiles.begin(), subFiles.end());
        } else {
            targetFiles.insert(fullPath);
        }
    }
    
    return targetFiles;
}

// needed for removing files not in the commit
void collectTreeFiles(const std::string& treeHash, const std::string& basePath, std::set<std::string>& fileSet) {
    std::vector<FileInfo> files = parseTree(treeHash);
    for (const auto& file : files) {
        std::string fullPath = basePath.empty() ? file.name : basePath + "/" + file.name;
        if (file.isDirectory) {
            collectTreeFiles(file.hash, fullPath, fileSet);
        } else {
            fileSet.insert(fullPath);
        }
    }
}

// creates or overwrites files
bool restoreTreeOverwrite(const std::string& treeHash, const std::string& basePath = "") {
    std::vector<FileInfo> files = parseTree(treeHash);
    for (const auto& file : files) {
        std::string fullPath = basePath.empty() ? file.name : basePath + "/" + file.name;
        if (file.isDirectory) {
            std::filesystem::create_directories(fullPath);
            if (!restoreTreeOverwrite(file.hash, fullPath)) {
                return false;
            }
        } else {
            std::string content = readObjectContent(file.hash);
            if (content.empty()) {
                std::cerr << "Failed to read blob: " << file.hash << '\n';
                return false;
            }
            std::filesystem::path filePath(fullPath);
            if (filePath.has_parent_path()) {
                std::filesystem::create_directories(filePath.parent_path());
            }
            std::ofstream outFile(fullPath);
            if (!outFile.is_open()) {
                std::cerr << "Failed to create file: " << fullPath << '\n';
                return false;
            }
            outFile << content;
            outFile.close();
        }
    }
    return true;
}

std::set<std::string> getWorkingDirectoryFiles(const std::string& path = ".") {
    std::set<std::string> files;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
        std::string entryPath = entry.path().string();
        if (entryPath.find("/.git") != std::string::npos || entryPath.find("\\.git") != std::string::npos) {
            continue;
        }
        if (entry.is_regular_file()) {
            // Remove leading "./" if present
            if (entryPath.substr(0, 2) == "./") {
                entryPath = entryPath.substr(2);
            }
            files.insert(entryPath);
        }
    }
    return files;
}

bool safeCheckout(const std::string& commitHash) {
    CommitInfo commit = parseCommit(commitHash);
    if (commit.hash.empty()) {
        std::cerr << "Invalid commit hash: " << commitHash << '\n';
        return false;
    }
    std::cout << "Checking out commit " << commitHash << '\n';
    std::cout << "Message: " << commit.message << '\n';

    // collect all files that should exist after checkout
    std::set<std::string> targetFiles;
    collectTreeFiles(commit.treeHash, "", targetFiles);


    if (!restoreTreeOverwrite(commit.treeHash)) {
        std::cerr << "Failed to restore files - checkout aborted\n";
        return false;
    }

    // delete files not in the commit
    std::cout << "Cleaning up files not in commit...\n";
    std::set<std::string> currentFiles = getWorkingDirectoryFiles();
    for (const auto& file : currentFiles) {
        if (targetFiles.find(file) == targetFiles.end()) {
            try {
                std::filesystem::remove(file);
                std::cout << "Removed: " << file << '\n';
            } catch (const std::filesystem::filesystem_error& e) {
                std::cerr << "Warning: Failed to remove file " << file << ": " << e.what() << '\n';
            }
        }
    }


    if (!writeHead(commitHash)) {
        std::cerr << "Failed to update HEAD\n";
        return false;
    }
    std::cout << "Checkout complete!\n";
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

// Add these helper functions after your existing utility functions and before the command handlers

// Get the current branch name (returns empty if detached HEAD)
std::string getCurrentBranch() {
    std::ifstream headFile(".git/HEAD");
    if (!headFile.is_open()) {
        return "";
    }
    
    std::string headContent;
    std::getline(headFile, headContent);
    headFile.close();
    
    // Check if HEAD points to a branch reference
    if (headContent.substr(0, 5) == "ref: ") {
        std::string refPath = headContent.substr(5); // Remove "ref: "
        // Extract branch name from "refs/heads/main" -> "main"
        if (refPath.substr(0, 11) == "refs/heads/") {
            return refPath.substr(11); // Remove "refs/heads/"
        }
    }
    
    return ""; // Detached HEAD
}

// Update a branch to point to a specific commit
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

// Switch HEAD to point to a branch
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

bool handleCommit(int argc, char *argv[]) {
    if (argc < 4) {  // Changed from 3 to 4 to require message
        std::cerr << "Usage: commit -m <message>\n";
        return false;
    }
    
    std::string flag = argv[2];
    if (flag != "-m") {
        std::cerr << "Usage: commit -m <message>\n";
        return false;
    }
    
    std::string message = argv[3];
    
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
    
    // ENHANCED: Update current branch instead of just HEAD
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

std::vector<std::string> findAllCommitHashes() {
    std::vector<std::string> commitHashes;
    std::unordered_set<std::string> seen; // Avoid duplicates

    std::string objectsDir = ".git/objects";
    for (const auto& dirEntry : std::filesystem::directory_iterator(objectsDir)) {
        if (!dirEntry.is_directory()) continue;
        std::string dirName = dirEntry.path().filename().string();
        if (dirName.size() != 2) continue; // Only object directories

        for (const auto& fileEntry : std::filesystem::directory_iterator(dirEntry.path())) {
            std::string fileName = fileEntry.path().filename().string();
            if (fileName.size() != 38) continue; // Only object files

            std::string hash = dirName + fileName;
            std::string content = readObject(hash);
            if (!content.empty() && content.substr(0, 6) == "commit") {
                if (seen.find(hash) == seen.end()) {
                    commitHashes.push_back(hash);
                    seen.insert(hash);
                }
            }
        }
    }
    return commitHashes;
}

// returns in reverse-chronological order
std::vector<std::string> getReachableCommits(const std::vector<std::string>& startHashes) {
    std::vector<std::string> orderedCommits;
    std::unordered_set<std::string> visited;
    std::queue<std::string> toVisit;

    for (const auto& hash : startHashes) {
        if (!hash.empty() && visited.count(hash) == 0) {
            toVisit.push(hash);
            visited.insert(hash);
        }
    }

    int maxIterations = 1000;
    int iterations = 0;
    while (!toVisit.empty() && iterations++ < maxIterations) {
        std::string current = toVisit.front();
        toVisit.pop();
        if (current.empty()) continue;

        CommitInfo commit = parseCommit(current);
        if (commit.hash.empty()) continue;

        orderedCommits.push_back(current);

        if (!commit.parentHash.empty() && visited.count(commit.parentHash) == 0) {
            toVisit.push(commit.parentHash);
            visited.insert(commit.parentHash);
        }
    }
    if (iterations >= maxIterations) {
        std::cerr << "Aborted: possible infinite loop in commit graph traversal.\n";
    }
    return orderedCommits;
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
        
        // Checkout the commit and switch to the branch
        if (!safeCheckout(commitHash)) {
            return false;
        }
        
        // Update HEAD to point to the branch
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


std::vector<std::string> collectReferenceCommits() {
    std::vector<std::string> refs;
    std::string headHash = readHead();
    if (!headHash.empty()) refs.push_back(headHash);

    std::string refsDir = ".git/refs/heads";
    if (std::filesystem::exists(refsDir)) {
        for (const auto& entry : std::filesystem::directory_iterator(refsDir)) {
            std::ifstream refFile(entry.path());
            std::string hash;
            std::getline(refFile, hash);
            if (!hash.empty()) refs.push_back(hash);
        }
    }
    return refs;
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
        // List all branches
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
