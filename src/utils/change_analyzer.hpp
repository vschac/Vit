#pragma once
#include "../commit.hpp"
#include "../utils/file_utils.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

namespace vit::utils {

class ChangeAnalyzer {
public:
    enum class ChangeType {
        ADDED,
        MODIFIED,
        DELETED
    };

    struct FileChange {
        std::string filePath;
        ChangeType changeType;
        std::string oldContent;
        std::string newContent;
        size_t oldSize;
        size_t newSize;
        
        FileChange(const std::string& path, ChangeType type) 
            : filePath(path), changeType(type), oldSize(0), newSize(0) {}
        
        size_t totalSize() const {
            return oldSize + newSize;
        }
    };

    struct AnalysisResult {
        std::vector<FileChange> changes;
        size_t totalFilesAnalyzed;
        size_t sourceFilesChanged;
        size_t totalContentSize;
        bool withinAILimits;
        
        bool hasChanges() const { return !changes.empty(); }
    };


    AnalysisResult analyzeChanges(const std::string& commitHash = "", bool sourceOnly = true);

    bool hasAnyChanges(const std::string& commitHash = "");

private:
    static constexpr size_t MAX_FILE_SIZE = 50000;
    static constexpr size_t MAX_TOTAL_SIZE = 200000;
    static constexpr size_t MAX_FILES = 10;
    
    std::string getFileContentFromCommit(const std::string& filePath, const std::string& treeHash);
    std::unordered_map<std::string, std::string> getCommitFileMap(const std::string& treeHash);
    std::vector<std::string> getWorkingDirectoryFiles();
    
    std::string findFileInTree(const std::string& treeHash, const std::string& filePath);
    void collectTreeFileMap(const std::string& treeHash, const std::string& basePath, 
                           std::unordered_map<std::string, std::string>& fileMap);
    std::string normalizeFilePath(const std::string& path);
    bool shouldAnalyzeFile(const std::string& filePath, size_t contentSize);
};

} 