
#include "file_utils.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <set>

namespace vit::utils {

std::string FileUtils::readFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filePath);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool FileUtils::writeFile(const std::string& filePath, const std::string& content) {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return false;
    }
    file << content;
    return true;
}

bool FileUtils::fileExists(const std::string& filePath) {
    return std::filesystem::exists(filePath) && std::filesystem::is_regular_file(filePath);
}

std::string FileUtils::getFileExtension(const std::string& filePath) {
    std::filesystem::path path(filePath);
    return path.extension().string();
}

std::string FileUtils::createBackup(const std::string& filePath) {
    if (!fileExists(filePath)) {
        return "";
    }

    std::string backupPath = filePath + ".backup";
    try {
        std::filesystem::copy_file(filePath, backupPath, 
                                 std::filesystem::copy_options::overwrite_existing);
        return backupPath;
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Failed to create backup: " << e.what() << std::endl;
        return "";
    }
}

std::vector<std::string> FileUtils::getFilesInDirectory(const std::string& directory) {
    std::vector<std::string> files;
    std::set<std::string> uniqueFiles;
    
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string filePath = entry.path().string();
                // Skip .git directory and common build directories
                if (filePath.find("/.git/") == std::string::npos &&
                    filePath.find("/build/") == std::string::npos &&
                    filePath.find("/node_modules/") == std::string::npos) {
                    
                    // Only add if not already seen
                    if (uniqueFiles.insert(filePath).second) {
                        files.push_back(filePath);
                    }
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error scanning directory " << directory << ": " << e.what() << std::endl;
    }
    
    return files;
}

}