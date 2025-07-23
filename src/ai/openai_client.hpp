#pragma once
#include "ai_client.hpp"
#include <string>
#include <memory>

namespace vit::ai {

class OpenAIClient : public AIClient {
public:

    struct Config {
        std::string apiKey;
        std::string baseUrl = "https://api.openai.com/v1";
        std::string model = "gpt-3.5-turbo";
        int maxTokens = 2000;
        double temperature = 0.7;
        int timeoutSeconds = 30;
    };

    explicit OpenAIClient(const std::string& apiKey);

    ~OpenAIClient() override;

    // AIClient interface implementation
    std::future<GenerationResult> generateResponse(
        const std::vector<Message>& messages) override;
    
    bool isAvailable() const override;
    std::string getProviderName() const override;
    std::string getModelName() const override;

private:
    struct Impl; // forward declaration
    std::unique_ptr<Impl> pImpl_; // pointer to the implementation to hide API key

    GenerationResult makeRequest(const std::vector<Message>& messages);
    std::string createJsonPayload(const std::vector<Message>& messages);
    GenerationResult parseResponse(const std::string& jsonResponse);
    bool validateConfig() const;
};

}