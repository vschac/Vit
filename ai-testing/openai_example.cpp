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

class OpenAIClient {
private:
    std::string apiKey;
    std::string baseUrl = "https://api.openai.com/v1/chat/completions";

public:
    explicit OpenAIClient(const std::string& key) : apiKey(key) {}

    std::string generateResponse(const std::string& prompt, const std::string& model = "gpt-3.5-turbo") {
        CURL* curl;
        CURLcode res;
        std::string response;

        curl = curl_easy_init();
        if (!curl) {
            std::cerr << "Failed to initialize CURL\n";
            return "";
        }

        // Prepare JSON payload
        json payload = {
            {"model", model},
            {"messages", {
                {{"role", "user"}, {"content", prompt}}
            }},
            {"max_tokens", 150},
            {"temperature", 0.7}
        };

        std::string jsonStr = payload.dump();

        // Set headers
        struct curl_slist* headers = nullptr;
        std::string authHeader = "Authorization: Bearer " + apiKey;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, authHeader.c_str());

        // Configure CURL
        curl_easy_setopt(curl, CURLOPT_URL, baseUrl.c_str());
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
            if (parsed.contains("choices") && !parsed["choices"].empty()) {
                return parsed["choices"][0]["message"]["content"].get<std::string>();
            }
        } catch (const std::exception& e) {
            std::cerr << "JSON parsing error: " << e.what() << '\n';
            std::cerr << "Response: " << jsonResponse << '\n';
        }
        return "";
    }
};

int main() {
    // Get API key from environment variable
    const char* apiKeyEnv = std::getenv("OPENAI_API_KEY");
    if (!apiKeyEnv) {
        std::cerr << "Please set OPENAI_API_KEY environment variable\n";
        return 1;
    }

    OpenAIClient client(apiKeyEnv);

    std::cout << "OpenAI API Test\n";
    std::cout << "===============\n";

    std::string prompt = "Write a brief comment for a C++ function that calculates factorial";
    
    std::cout << "Prompt: " << prompt << '\n';
    std::cout << "Calling OpenAI API...\n\n";

    std::string response = client.generateResponse(prompt);
    
    if (!response.empty()) {
        std::string message = client.extractMessage(response);
        if (!message.empty()) {
            std::cout << "AI Response:\n" << message << '\n';
        } else {
            std::cout << "Raw response:\n" << response << '\n';
        }
    } else {
        std::cout << "Failed to get response from OpenAI\n";
    }

    return 0;
}