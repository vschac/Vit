#include "change_analyzer.hpp"
#include <iostream>
#include <algorithm>
#include <functional>

namespace vit::utils {

ChangeAnalyzer::AnalysisResult ChangeAnalyzer::analyzeChanges(const std::string& commitHash, bool sourceOnly) {
    AnalysisResult result;
    
    // Get target commit (default to HEAD)
    std::string targetCommit = commitHash.empty() ? readHead() : commitHash;
    
    if (targetCommit.empty()) {
        // No commits yet - everything is new
        auto workingFiles = getWorkingDirectoryFiles();
        for (const std::string& filePath : workingFiles) {
            if (sourceOnly && !vit::utils::FileUtils::isSourceFile(filePath)) continue;
            
            try {
                FileChange change(filePath, ChangeType::ADDED);
                change.newContent = vit::utils::FileUtils::readFile(filePath);
                change.newSize = change.newContent.size();
                
                if (shouldAnalyzeFile(filePath, change.newSize)) {
                    result.changes.push_back(std::move(change));
                    result.totalContentSize += change.newSize;
                }
            } catch (const std::exception& e) {
                std::cerr << "Warning: Could not read file " << filePath << ": " << e.what() << std::endl;
            }
        }
        
        result.totalFilesAnalyzed = workingFiles.size();
        result.sourceFilesChanged = result.changes.size();
        result.withinAILimits = (result.totalContentSize <= MAX_TOTAL_SIZE && 
                                result.changes.size() <= MAX_FILES);
        return result;
    }
    
    CommitInfo commitInfo = parseCommit(targetCommit);
    if (commitInfo.hash.empty()) {
        throw std::runtime_error("Invalid commit: " + targetCommit);
    }
    
    // Get file mappings
    auto commitFiles = getCommitFileMap(commitInfo.treeHash);
    auto workingFiles = getWorkingDirectoryFiles();

    // Create unified set of all file paths
    std::unordered_set<std::string> allPaths;
    for (const auto& [path, hash] : commitFiles) allPaths.insert(path);
    for (const std::string& path : workingFiles) allPaths.insert(normalizeFilePath(path));
    

    result.totalFilesAnalyzed = allPaths.size();

    
    // Analyze each file
    for (const std::string& filePath : allPaths) {
        if (sourceOnly && !vit::utils::FileUtils::isSourceFile(filePath)) continue;
        
        bool inCommit = commitFiles.find(filePath) != commitFiles.end();
        bool inWorking = std::find(workingFiles.begin(), workingFiles.end(), filePath) != workingFiles.end();
        
        // std::cout << "filePath: " << filePath << std::endl;
        // std::cout << "inCommit: " << inCommit << ", inWorking: " << inWorking << std::endl;
        if (!inCommit && inWorking) {
            // ADDED file
            try {
                FileChange change(filePath, ChangeType::ADDED);
                change.newContent = vit::utils::FileUtils::readFile(filePath);
                change.newSize = change.newContent.size();
                
                if (shouldAnalyzeFile(filePath, change.newSize)) {
                    result.changes.push_back(std::move(change));
                    result.totalContentSize += change.newSize;
                }
            } catch (const std::exception& e) {
                std::cerr << "Warning: Could not read added file " << filePath << ": " << e.what() << std::endl;
            }
            
        } else if (inCommit && !inWorking) {
            // DELETED file
            FileChange change(filePath, ChangeType::DELETED);
            change.oldContent = getFileContentFromCommit(filePath, commitInfo.treeHash);
            change.oldSize = change.oldContent.size();
            
            if (shouldAnalyzeFile(filePath, change.oldSize)) {
                result.changes.push_back(std::move(change));
                result.totalContentSize += change.oldSize;
            }
            
        } else if (inCommit && inWorking) {
            // Potentially MODIFIED file
            std::cout << "Analyzing MODIFIED file: " << filePath << std::endl;
            try {
                std::string oldContent = getFileContentFromCommit(filePath, commitInfo.treeHash);
                std::string newContent = vit::utils::FileUtils::readFile(filePath);
                
                // Quick hash comparison
                if (oldContent != newContent) {
                    FileChange change(filePath, ChangeType::MODIFIED);
                    change.oldContent = oldContent;
                    change.newContent = newContent;
                    change.oldSize = oldContent.size();
                    change.newSize = newContent.size();
                    
                    if (shouldAnalyzeFile(filePath, change.totalSize())) {
                        result.changes.push_back(std::move(change));
                        result.totalContentSize += change.totalSize();
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Warning: Could not analyze file " << filePath << ": " << e.what() << std::endl;
            }
        }
    }
    
    result.sourceFilesChanged = result.changes.size();
    result.withinAILimits = (result.totalContentSize <= MAX_TOTAL_SIZE && 
                            result.changes.size() <= MAX_FILES);
    
    return result;
}

std::string ChangeAnalyzer::getFileContentFromCommit(const std::string& filePath, const std::string& treeHash) {
    return findFileInTree(treeHash, filePath);
}

std::string ChangeAnalyzer::findFileInTree(const std::string& treeHash, const std::string& filePath) {
    auto files = parseTree(treeHash);
    
    // Handle root level files
    for (const auto& file : files) {
        if (file.name == filePath && !file.isDirectory) {
            return readObjectContent(file.hash);
        }
    }
    
    // Handle nested files
    for (const auto& file : files) {
        if (file.isDirectory && filePath.starts_with(file.name + "/")) {
            std::string remainingPath = filePath.substr(file.name.length() + 1);
            return findFileInTree(file.hash, remainingPath);
        }
    }
    
    return "";  // File not found
}

std::unordered_map<std::string, std::string> ChangeAnalyzer::getCommitFileMap(const std::string& treeHash) {
    std::unordered_map<std::string, std::string> fileMap;
    collectTreeFileMap(treeHash, "", fileMap);
    return fileMap;
}

void ChangeAnalyzer::collectTreeFileMap(const std::string& treeHash, const std::string& basePath, 
                                       std::unordered_map<std::string, std::string>& fileMap) {
    auto files = parseTree(treeHash);
    
    for (const auto& file : files) {
        std::string fullPath = basePath.empty() ? file.name : basePath + "/" + file.name;
        
        if (file.isDirectory) {
            collectTreeFileMap(file.hash, fullPath, fileMap);
        } else {
            fileMap[fullPath] = file.hash;
        }
    }
}

std::vector<std::string> ChangeAnalyzer::getWorkingDirectoryFiles() {
    auto files =vit::utils::FileUtils::getFilesInDirectory(".");
    std::vector<std::string> normalizedFiles;
    for (const auto& file : files) {
        normalizedFiles.push_back(normalizeFilePath(file));
    }
    return normalizedFiles;
}

std::string ChangeAnalyzer::normalizeFilePath(const std::string& path) {
    std::string normalized = path;
    
    // Remove leading "./"
    if (normalized.rfind("./", 0) == 0) {
        normalized.erase(0, 2);
    }
    
    // Convert to forward slashes
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    
    return normalized;
}

bool ChangeAnalyzer::shouldAnalyzeFile(const std::string& filePath, size_t contentSize) {
    // Skip very large files
    if (contentSize > MAX_FILE_SIZE) {
        std::cout << "Skipping " << filePath << " (too large: " << contentSize << " bytes)\n";
        return false;
    }
    
    // Skip empty files
    if (contentSize == 0) {
        return false;
    }
    
    return true;
}

bool ChangeAnalyzer::hasAnyChanges(const std::string& commitHash) {
    auto result = analyzeChanges(commitHash, false);  // Check all files, not just source
    return result.hasChanges();
}

}