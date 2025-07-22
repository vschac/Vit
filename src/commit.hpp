#pragma once
#include <string>
#include <vector>
#include <set>
#include <unordered_set>

/* ---------- Low-level object helpers ---------- */
std::string hashToHexString(const unsigned char* hash);
std::string hexStringToBinary(const std::string& hexString);
std::string binaryToHexString(const std::string& binary);
std::string getObjectPath(const std::string& hash);

std::string writeObject(const std::string& type, const std::string& content);
std::string writeBlob(const std::string& content);
std::string writeTree(const std::string& directoryPath);

std::string readObject(const std::string& hash);
std::string readObjectContent(const std::string& hash);

/* ---------- commit / branch primitives ---------- */
std::string writeCommit(const std::string& treeHash,
                        const std::string& parentHash,
                        const std::string& message,
                        const std::string& author,
                        const std::string& email);

std::string readHead();
bool        writeHead(const std::string& commitHash);

/* ---------- data structures ---------- */
struct TreeEntry {
    std::string mode;
    std::string hash;
    std::string filename;
};

struct CommitInfo {
    std::string hash;
    std::string treeHash;
    std::string parentHash;
    std::string author;
    std::string message;
    std::string timestamp;
};

struct FileInfo {
    std::string name;
    std::string hash;
    std::string mode;
    bool        isDirectory;
};

/* ---------- higher-level helpers ---------- */
CommitInfo              parseCommit(const std::string& commitHash);
std::string             formatTimestamp(const std::string& ts);

std::vector<FileInfo>   parseTree(const std::string& treeHash);
bool                    restoreTree(const std::string& treeHash,
                                    const std::string& basePath = "");
void                    collectTreeFiles(const std::string& treeHash,
                                         const std::string& basePath,
                                         std::set<std::string>& fileSet);
bool                    restoreTreeOverwrite(const std::string& treeHash,
                                             const std::string& basePath = "");

std::set<std::string>   getWorkingDirectoryFiles(const std::string& path = ".");
bool                    safeCheckout(const std::string& commitHash);

/* ---------- reachability / refs ---------- */
std::vector<std::string> findAllCommitHashes();
std::vector<std::string> getReachableCommits(const std::vector<std::string>& start);
std::vector<std::string> collectReferenceCommits();

std::string getCurrentBranch();
bool        updateBranch(const std::string& branchName,
                         const std::string& commitHash);
bool        switchToBranch(const std::string& branchName);
