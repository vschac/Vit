#include <iostream>
#include <string>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Callback function to write HTTP response data
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* response) {
    size_t totalSize = size * nmemb;
    response->append((char*)contents, totalSize);
    return totalSize;
}

class OllamaClient {
private:
    std::string baseUrl;

public:
    explicit OllamaClient(const std::string& url = "http://localhost:11434") : baseUrl(url) {}

    bool isAvailable() {
        CURL* curl;
        CURLcode res;
        long responseCode;

        curl = curl_easy_init();
        if (!curl) return false;

        std::string checkUrl = baseUrl + "/api/tags";
        curl_easy_setopt(curl, CURLOPT_URL, checkUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); // HEAD request
        
        res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
        curl_easy_cleanup(curl);

        return (res == CURLE_OK && responseCode == 200);
    }

    std::string generateResponse(const std::string& prompt, const std::string& model = "llama3.2") {
        CURL* curl;
        CURLcode res;
        std::string response;

        curl = curl_easy_init();
        if (!curl) {
            std::cerr << "Failed to initialize CURL\n";
            return "";
        }

        // Prepare JSON payload for Ollama API
        json payload = {
            {"model", model},
            {"prompt", prompt},
            {"stream", false}  // Get complete response, not streaming
        };

        std::string jsonStr = payload.dump();
        std::string generateUrl = baseUrl + "/api/generate";

        // Set headers
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        // Configure CURL
        curl_easy_setopt(curl, CURLOPT_URL, generateUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonStr.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        // Perform request
        res = curl_easy_perform(curl);

        // Cleanup
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            std::cerr << "CURL error: " << curl_easy_strerror(res) << '\n';
            return "";
        }

        return response;
    }

    std::string extractMessage(const std::string& jsonResponse) {
        try {
            json parsed = json::parse(jsonResponse);
            if (parsed.contains("response")) {
                return parsed["response"].get<std::string>();
            }
        } catch (const std::exception& e) {
            std::cerr << "JSON parsing error: " << e.what() << '\n';
            std::cerr << "Response: " << jsonResponse << '\n';
        }
        return "";
    }

    std::vector<std::string> listModels() {
        CURL* curl;
        CURLcode res;
        std::string response;
        std::vector<std::string> models;

        curl = curl_easy_init();
        if (!curl) return models;

        std::string tagsUrl = baseUrl + "/api/tags";
        curl_easy_setopt(curl, CURLOPT_URL, tagsUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res == CURLE_OK) {
            try {
                json parsed = json::parse(response);
                if (parsed.contains("models")) {
                    for (const auto& model : parsed["models"]) {
                        if (model.contains("name")) {
                            models.push_back(model["name"].get<std::string>());
                        }
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Error parsing models: " << e.what() << '\n';
            }
        }

        return models;
    }
};

int main() {
    OllamaClient client;

    std::cout << "Ollama API Test\n";
    std::cout << "===============\n";

    // Check if Ollama is running
    if (!client.isAvailable()) {
        std::cout << "Ollama is not running or not accessible at http://localhost:11434\n";
        std::cout << "Please start Ollama with: ollama serve\n";
        std::cout << "And pull a model with: ollama pull llama3.2\n";
        return 1;
    }

    std::cout << "Ollama is running!\n\n";

    // List available models
    auto models = client.listModels();
    std::cout << "Available models:\n";
    for (const auto& model : models) {
        std::cout << "  - " << model << '\n';
    }
    std::cout << '\n';

    if (models.empty()) {
        std::cout << "No models available. Please pull a model first:\n";
        std::cout << "  ollama pull llama3.2\n";
        return 1;
    }

    // Use the first available model
    std::string selectedModel = models[0];
    std::cout << "Using model: " << selectedModel << '\n';

    std::string prompt = "Write a brief comment for a C++ function that calculates factorial";
    
    std::cout << "Prompt: " << prompt << '\n';
    std::cout << "Calling Ollama API...\n\n";

    std::string response = client.generateResponse(prompt, selectedModel);
    
    if (!response.empty()) {
        std::string message = client.extractMessage(response);
        if (!message.empty()) {
            std::cout << "AI Response:\n" << message << '\n';
        } else {
            std::cout << "Raw response:\n" << response << '\n';
        }
    } else {
        std::cout << "Failed to get response from Ollama\n";
    }

    return 0;
}