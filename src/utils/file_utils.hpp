#pragma once
#include <string>
#include <vector>

namespace vit::utils {

class FileUtils {
public:
    static std::string readFile(const std::string& filePath);
    
    static bool writeFile(const std::string& filePath, const std::string& content);
    
    static bool fileExists(const std::string& filePath);
    
    static std::string getFileExtension(const std::string& filePath);
    
    static std::string createBackup(const std::string& filePath);

    static std::vector<std::string> getFilesInDirectory(const std::string& directory = ".");
};

}