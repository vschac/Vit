#pragma once
#include "ai_client.hpp"
#include <string>
#include <memory>

namespace vit::ai {

/**
 * OpenAI API client implementation
 * Communicates with OpenAI's chat completions API
 */
class OpenAIClient : public AIClient {
public:
    /**
     * Configuration for OpenAI client
     */
    struct Config {
        std::string apiKey;
        std::string baseUrl = "https://api.openai.com/v1";
        std::string model = "gpt-3.5-turbo";
        int maxTokens = 500;
        double temperature = 0.7;
        int timeoutSeconds = 30;
    };

    /**
     * Create OpenAI client with configuration
     * @param config The configuration to use
     */
    explicit OpenAIClient(const Config& config);

    /**
     * Create OpenAI client with API key (uses default config)
     * @param apiKey The OpenAI API key
     */
    explicit OpenAIClient(const std::string& apiKey);

    ~OpenAIClient() override;

    // AIClient interface implementation
    std::future<GenerationResult> generateResponse(
        const std::vector<Message>& messages) override;
    
    bool isAvailable() const override;
    std::string getProviderName() const override;
    std::string getModelName() const override;
    bool setModel(const std::string& modelName) override;

    /**
     * Set the maximum tokens for responses
     * @param maxTokens Maximum tokens to generate
     */
    void setMaxTokens(int maxTokens);

    /**
     * Set the temperature for response generation
     * @param temperature Temperature value (0.0 to 2.0)
     */
    void setTemperature(double temperature);

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;

    // Private helper methods
    GenerationResult makeRequest(const std::vector<Message>& messages);
    std::string createJsonPayload(const std::vector<Message>& messages);
    GenerationResult parseResponse(const std::string& jsonResponse);
    bool validateConfig() const;
};

} // namespace vit::ai