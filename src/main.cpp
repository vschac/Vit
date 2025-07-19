#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <sstream>
#include <zlib.h>
#include <openssl/sha.h>
#include <iomanip>
#include <vector>

struct TreeEntry {
    std::string mode;
    std::string hash;
    std::string filename;
};

struct Commit {
    std::string treeHash;
    std::string parentHash;
    std::string message;
    std::string commiterName;
    std::string commiterEmail;
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

std::string writeBlob(const std::string& content) {
    // blob <size>\0<content>
    std::string formattedContent = "blob " + std::to_string(content.size()) + '\0' + content;

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
    
    std::string saveFile = saveDir + "/" + hashString.substr(2);
    std::ofstream saveFileStream(saveFile, std::ios::binary);
    if (!saveFileStream.is_open()) {
        std::cerr << "Failed to create save file.\n";
        return "";
    }

    saveFileStream.write(compressedContent.data(), compressedContent.size());
    saveFileStream.close();
    
    return hashString;
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
    
    // tree <size>\0<content>
    std::string formattedContent = "tree " + std::to_string(treeContent.size()) + '\0' + treeContent;
    
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
    if (compressResult != Z_OK) return "";
    
    compressedContent.resize(compressedSize);
    
    std::string saveDir = ".git/objects/" + hashString.substr(0, 2);
    std::filesystem::create_directories(saveDir);
    
    std::string saveFile = saveDir + "/" + hashString.substr(2);
    std::ofstream saveFileStream(saveFile, std::ios::binary);
    if (!saveFileStream.is_open()) return "";
    
    saveFileStream.write(compressedContent.data(), compressedContent.size());
    saveFileStream.close();
    
    return hashString;
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
    
    // Use same pattern as writeBlob/writeTree
    std::string formattedContent = "commit " + std::to_string(content.size()) + '\0' + content;
    
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
    
    std::string saveFile = saveDir + "/" + hashString.substr(2);
    std::ofstream saveFileStream(saveFile, std::ios::binary);
    if (!saveFileStream.is_open()) {
        std::cerr << "Failed to create save file.\n";
        return "";
    }

    saveFileStream.write(compressedContent.data(), compressedContent.size());
    saveFileStream.close();
    
    return hashString;
}


int main(int argc, char *argv[])
{
    // Flush after every std::cout / std::cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    // You can use print statements as follows for debugging, they'll be visible when running tests.
    std::cerr << "Logs from your program will appear here!\n";

    if (argc < 2) {
        std::cerr << "No command provided.\n";
        return EXIT_FAILURE;
    }
    
    std::string command = argv[1];
    
    if (command == "init") {
        try {
            std::filesystem::create_directory(".git");
            std::filesystem::create_directory(".git/objects");
            std::filesystem::create_directory(".git/refs");
    
            std::ofstream headFile(".git/HEAD");
            if (headFile.is_open()) {
                headFile << "ref: refs/heads/main\n";
                headFile.close();
            } else {
                std::cerr << "Failed to create .git/HEAD file.\n";
                return EXIT_FAILURE;
            }
    
            std::cout << "Initialized vit directory\n";
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << e.what() << '\n';
            return EXIT_FAILURE;
        }
    } else if (command == "cat-file") {
        if (argc < 3) {
            std::cerr << "No object hash provided.\n";
            return EXIT_FAILURE;
        }
        std::string flag = argv[2];
        if (flag != "-p") {
            std::cerr << "Unknown flag: " << flag << '\n';
            return EXIT_FAILURE;
        } // TODO: add pretty print functionality and other flags

        std::string objectHash = argv[3];
        std::string objectPath = ".git/objects/" + objectHash.substr(0, 2) + "/" + objectHash.substr(2);

        std::ifstream objectFile(objectPath, std::ios::binary);
        if (!objectFile.is_open()) {
            std::cerr << "Object file not found.\n";
            return EXIT_FAILURE;
        }

        std::stringstream objectStream;
        objectStream << objectFile.rdbuf();
        std::string objectContent = objectStream.str();

        std::string decompressedContent = decompressObject(objectContent);
        
        // only print content not header
        size_t nullPos = decompressedContent.find('\0');
        if (nullPos != std::string::npos) {
            std::string content = decompressedContent.substr(nullPos + 1);
            std::cout << content;
        } else {
            std::cout << decompressedContent;
        }
    } else if (command == "hash-object") {
        if (argc < 3) {
            std::cerr << "No object content provided.\n";
            return EXIT_FAILURE;
        }
        std::string flag = argv[2];
        if (flag != "-w") {
            std::cerr << "Unknown flag: " << flag << '\n';
            return EXIT_FAILURE;
        } // TODO: add flag functionality

        std::string file = argv[3];
        std::ifstream fileStream(file);
        if (!fileStream.is_open()) {
            std::cerr << "Failed to open file: " << file << '\n';
            return EXIT_FAILURE;
        }
        
        std::stringstream contentStream;
        contentStream << fileStream.rdbuf();
        std::string fileContent = contentStream.str();

        std::string hashString = writeBlob(fileContent);
        if (hashString.empty()) {
            return EXIT_FAILURE;
        }
        
        std::cout << hashString << '\n';
    } else if (command == "ls-tree") {
        if (argc < 3) {
            std::cerr << "No tree hash provided.\n";
            return EXIT_FAILURE;
        }
        std::string flag = argv[2];
        bool nameOnly = (flag == "--name-only");
        if (flag != "--name-only" && flag != "-l") {
            std::cerr << "Unknown flag: " << flag << '\n';
            return EXIT_FAILURE;
        }
        std::string treeHash = argv[3];
        std::string treePath = ".git/objects/" + treeHash.substr(0, 2) + "/" + treeHash.substr(2);

        std::ifstream treeFile(treePath, std::ios::binary);
        if (!treeFile.is_open()) {
            std::cerr << "Tree file not found.\n";
            return EXIT_FAILURE;
        }
        
        std::stringstream treeStream;
        treeStream << treeFile.rdbuf();
        std::string treeContent = treeStream.str(); // still compressed

        std::string decompressedContent = decompressObject(treeContent);

        // parse tree object format
        size_t nullPos = decompressedContent.find('\0');
        if (nullPos == std::string::npos) {
            std::cerr << "Invalid tree format\n";
            return EXIT_FAILURE;
        }
        
        // Skip header
        size_t pos = nullPos + 1;
        
        while (pos < decompressedContent.size()) {
            // Find next null byte (end of current entry)
            size_t nextNull = decompressedContent.find('\0', pos);
            if (nextNull == std::string::npos) break;
            
            // Extract the entry (mode + name)
            std::string entry = decompressedContent.substr(pos, nextNull - pos);
            
            // Find the last space (separates mode from name)
            size_t lastSpace = entry.find_last_of(' ');
            if (lastSpace != std::string::npos) {
                std::string mode = entry.substr(0, lastSpace);
                std::string filename = entry.substr(lastSpace + 1);
                
                if (nameOnly) {
                    std::cout << filename << '\n';
                } else {
                    // Extract hash (20 bytes after null)
                    std::string binaryHash = decompressedContent.substr(nextNull + 1, 20);
                    std::string hashHex = binaryToHexString(binaryHash);
                    
                    std::string type = (mode.substr(0, 3) == "400") ? "tree" : "blob";
                    
                    std::cout << mode << " " << type << " " << hashHex << "\t" << filename << '\n';
                }
            }
            
            // Skip the hash (20 bytes) and move to next entry
            pos = nextNull + 1 + 20;
        }
    } else if (command == "write-tree") {
        std::string hashString = writeTree("."); // helper function may have to be called recursively
        if (hashString.empty()) {
            std::cerr << "Failed to write tree\n";
            return EXIT_FAILURE;
        }
        std::cout << hashString << '\n';
    } else if (command == "commit-tree") {
        if (argc < 7) {
            std::cerr << "Usage: commit-tree <tree_sha> -p <commit_sha> -m <message>\n";
            return EXIT_FAILURE;
        }

        std::string treeHash = argv[2];
        std::string parentHash = argv[4];  // -p flag
        std::string message = argv[6];     // -m flag
        
        std::string author = "Vincent Schacknies";
        std::string email = "vincent.schacknies@icloud.com";

        std::string commitHash = writeCommit(treeHash, parentHash, message, author, email);
        if (commitHash.empty()) {
            std::cerr << "Failed to create commit\n";
            return EXIT_FAILURE;
        }
        
        std::cout << commitHash << '\n';
    }
    else {
        std::cerr << "Unknown command " << command << '\n';
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
