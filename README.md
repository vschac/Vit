# Vit - AI-Enhanced Version Control System

Vit is an upgraded version control system implemented in C++ that combines Git-compatible functionality with powerful AI features. It provides traditional version control operations enhanced by intelligent code analysis, automated documentation generation, and smart commit management.

## 🚀 Features

### Core Version Control
- **Repository initialization** - Create new Git-compatible repositories
- **Object storage** - SHA-1 hashed object storage with zlib compression
- **Commit management** - Full commit history with branching support
- **File operations** - Complete blob, tree, and commit object handling
- **Branch operations** - Create, switch, and manage branches
- **Garbage collection** - Automatic cleanup of unreachable objects

### 🤖 AI-Powered Features
- **AI Comment Generation** - Automatically generate function and class documentation
- **Smart Commit Splitting** - AI-powered analysis to split large commits into logical units
- **Code Review Generation** - Automated code review reports before commits
- **Interactive Review** - Accept/reject AI suggestions with user control
- **Dual AI Support** - Choose between OpenAI API or local Ollama models

## 📋 Commands

### Basic Version Control

#### `init`
Initialize a new vit repository in the current directory.
```bash
./vit.sh init
```

#### `hash-object -w <file>`
Create a blob object from a file and store it in the repository.
```bash
./vit.sh hash-object -w filename.txt
```

#### `cat-file -p <hash>`
Display the contents of a stored object.
```bash
./vit.sh cat-file -p a1b2c3d4e5f6...
```

#### `write-tree`
Create a tree object from the current directory structure.
```bash
./vit.sh write-tree
```

#### `ls-tree <hash> [--name-only]`
List the contents of a tree object.
```bash
./vit.sh ls-tree a1b2c3d4e5f6...
./vit.sh ls-tree --name-only a1b2c3d4e5f6...
```

#### `commit-tree <tree> -p <parent> -m <message>`
Create a commit object manually.
```bash
./vit.sh commit-tree a1b2c3d4e5f6... -p b2c3d4e5f6a1... -m "Initial commit"
```

### Advanced Commit Operations

#### `commit -m <message> [options] [files...]`
Create commits with optional AI enhancements.
```bash
# Basic commit
./vit.sh commit -m "Add new feature"

# Commit with AI-generated comments
./vit.sh commit -m "Add new feature" --add-comments

# Commit with AI code review
./vit.sh commit -m "Add new feature" --review

# Commit specific files with AI features
./vit.sh commit -m "Update logic" --add-comments src/main.cpp src/utils.cpp
```

**Options:**
- `--add-comments` - Generate AI-powered function documentation
- `--review` - Generate code review as `review.md`
- Specify files to limit AI analysis to specific files

#### `split-commit -m <default-message>`
Use AI to analyze and split large commits into logical units. Note: the message provided will be used as a fallback message, otherwise each commit will have its own AI generated message.
```bash
./vit.sh split-commit -m "Multiple improvements"
```

### Branch Management

#### `branch [branch-name]`
List branches or create a new branch.
```bash
# List all branches
./vit.sh branch

# Create new branch
./vit.sh branch feature-x
```

#### `checkout <branch-or-commit>`
Switch to a branch or commit.
```bash
./vit.sh checkout main
./vit.sh checkout a1b2c3d4e5f6...
```

### History and Information

#### `log [--all]`
Show commit history.
```bash
# Show current branch history
./vit.sh log

# Show all commits
./vit.sh log --all
```

#### `show-head`
Display current HEAD commit.
```bash
./vit.sh show-head
```

#### `gc`
Run garbage collection to clean up unreachable objects.
```bash
./vit.sh gc
```

### Configuration

#### `config <command> [value]`
Manage vit configuration.
```bash
# Configure user information
./vit.sh config user-name "Your Name"
./vit.sh config user-email "you@example.com"

# AI provider settings
./vit.sh config local-ai    # Use local Ollama models
./vit.sh config api-ai      # Use OpenAI API

# Print current configuration
./vit.sh config print
```

## 🔧 Setup and Installation

### Prerequisites
- **CMake** 3.13 or later
- **C++23** compatible compiler (GCC 11+, Clang 14+, MSVC 2022+)
- **vcpkg** for dependency management
- **Git** (for cloning)

### Dependencies
- **OpenSSL** - Cryptographic operations
- **zlib** - Object compression
- **nlohmann-json** - JSON parsing for AI responses
- **curl** - HTTP requests for AI APIs

## Installation

### Quick Start
```bash
git clone https://github.com/vschac/Vit.git
cd vit
./build.sh          # Build once
./vit init           # Start using!
```

### System Installation
```bash
./build.sh
sudo cp build/vit /usr/local/bin/vit
vit init             # Use from anywhere
```


### **4. Make files executable:**
```bash
chmod +x build.sh vit.sh
```

## 🤖 AI Configuration

### OpenAI Setup
```bash
# Set API key
export OPENAI_API_KEY="your-api-key-here"

# Configure to use OpenAI
./vit.sh config api-ai
```

### Local Ollama Setup
```bash
# Install and start Ollama
curl -fsSL https://ollama.ai/install.sh | sh
ollama serve

# Pull a model
ollama pull llama3.2

# Configure vit to use local AI
./vit.sh config local-ai
```

### Supported Models
- **OpenAI**: GPT-4, GPT-3.5-turbo, and other chat models
- **Ollama**: Llama 3.2, CodeLlama, Mistral, and other local models

## 📖 Usage Examples

### Basic Workflow
```bash
# Initialize repository
./vit.sh init

# Configure user
./vit.sh config user-name "John Doe"
./vit.sh config user-email "john@example.com"

# Make changes to files, then commit
./vit.sh commit -m "Initial implementation"

# Create a new branch
./vit.sh branch feature-branch
./vit.sh checkout feature-branch

# Make more changes with AI assistance
./vit.sh commit -m "Add new features" --add-comments --review
```

### AI-Enhanced Development
```bash
# Generate documentation for all source files
./vit.sh commit -m "Add documentation" --add-comments

# Get AI review before committing
./vit.sh commit -m "Refactor code" --review

# Split a large commit intelligently
# (after making many changes)
./vit.sh split-commit -m "Multiple improvements"

# Work with specific files
./vit.sh commit -m "Update parser" --add-comments src/parser.cpp src/lexer.cpp
```