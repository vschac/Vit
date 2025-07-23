#pragma once
#include <string>
#include <vector>
#include <future>
#include <memory>

namespace vit::ai {

/**
 * Abstract interface for AI clients that can generate text responses.
 * This allows the main vit application to work with different AI providers
 * (OpenAI, Ollama, etc.) without knowing the implementation details.
 */
class AIClient {
public:
    /**
     * Represents a message in a conversation with the AI
     */
    struct Message {
        std::string role;    // "user", "system", "assistant"
        std::string content; // The message content
        
        Message(const std::string& r, const std::string& c) : role(r), content(c) {}
    };

    /**
     * Result of an AI generation request
     */
    struct GenerationResult {
        bool success;
        std::string content;
        std::string error;
        
        GenerationResult(bool s, const std::string& c, const std::string& e = "") 
            : success(s), content(c), error(e) {}
        
        static GenerationResult Success(const std::string& content) {
            return GenerationResult(true, content);
        }
        
        static GenerationResult Error(const std::string& error) {
            return GenerationResult(false, "", error);
        }
    };

    virtual ~AIClient() = default;

    /**
     * Generate a response from the AI based on the conversation messages
     * @param messages The conversation history
     * @return Future containing the generation result
     */
    virtual std::future<GenerationResult> generateResponse(
        const std::vector<Message>& messages) = 0;

    /**
     * Check if this AI client is available and ready to use
     * @return true if the client can make requests
     */
    virtual bool isAvailable() const = 0;

    /**
     * Get the name of this AI provider
     * @return Provider name (e.g., "OpenAI", "Ollama")
     */
    virtual std::string getProviderName() const = 0;

    /**
     * Get the model being used by this client
     * @return Model name (e.g., "gpt-3.5-turbo", "llama3.2")
     */
    virtual std::string getModelName() const = 0;

    /**
     * Set the model to use for this client
     * @param modelName The model to use
     * @return true if the model was set successfully
     */
    virtual bool setModel(const std::string& modelName) = 0;

protected:
    /**
     * Utility function to create a simple user message
     */
    static Message createUserMessage(const std::string& content) {
        return Message("user", content);
    }

    /**
     * Utility function to create a system message
     */
    static Message createSystemMessage(const std::string& content) {
        return Message("system", content);
    }
};

class AI {
public:
    static std::unique_ptr<AIClient> createOpenAI(const std::string& apiKey, 
                                                  const std::string& model = "gpt-3.5-turbo");

    static std::unique_ptr<AIClient> createOllama(const std::string& baseUrl = "http://localhost:11434",
                                                  const std::string& model = "llama3.2");

private:
    static std::string getEnvVar(const std::string& name);
};

} 