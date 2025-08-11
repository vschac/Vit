#pragma once
#include "../commit.hpp"
#include "../utils/file_utils.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include "../ai/openai_client.hpp"

namespace vit::utils {

// This class is used in place of creating a staging area
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
        
        FileChange(const std::string& path, ChangeType type) 
            : filePath(path), changeType(type) {}
    };

    // contains all the info for every changed file in the working directory
    struct AnalysisResult {
        std::vector<FileChange> changes;
        size_t totalFilesAnalyzed;
        size_t sourceFilesChanged;
        
        bool hasChanges() const { return !changes.empty(); }
    };

    explicit ChangeAnalyzer(std::shared_ptr<vit::ai::AIClient> aiClient);

    AnalysisResult analyzeChanges(const std::string& commitHash = "", bool sourceOnly = true);

private:
    std::shared_ptr<vit::ai::AIClient> aiClient_;

    std::string getFileContentFromCommit(const std::string& filePath, const std::string& treeHash);
    std::unordered_map<std::string, std::string> getCommitFileMap(const std::string& treeHash);
    std::vector<std::string> getWorkingDirectoryFiles();
    
    std::string findFileInTree(const std::string& treeHash, const std::string& filePath);
    void collectTreeFileMap(const std::string& treeHash, const std::string& basePath, 
                           std::unordered_map<std::string, std::string>& fileMap);
    std::string normalizeFilePath(const std::string& path);
};

} 