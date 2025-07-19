# Vit - A Simple Version Control System

Vit is a lightweight version control system implemented in C++. It provides basic Git-like functionality for tracking changes in your projects.

## Features

- **Repository initialization** - Create new version-controlled projects
- **Object storage** - Store files, directories, and commits using Git's object format
- **Commit history** - Track changes with commit messages and metadata
- **File inspection** - View contents of stored objects
- **Tree operations** - List and explore directory structures

## Commands

### `init`
Initialize a new vit repository in the current directory.

```bash
./your_program.sh init
```

### `hash-object -w <file>`
Create a blob object from a file and store it in the repository.

```bash
./your_program.sh hash-object -w filename.txt
```

### `cat-file -p <hash>`
Display the contents of a stored object.

```bash
./your_program.sh cat-file -p a1b2c3d4e5f6...
```

### `write-tree`
Create a tree object from the current directory structure.

```bash
./your_program.sh write-tree
```

### `ls-tree <hash>`
List the contents of a tree object.

```bash
./your_program.sh ls-tree a1b2c3d4e5f6...
# or with --name-only flag for just filenames
./your_program.sh ls-tree --name-only a1b2c3d4e5f6...
```

### `commit-tree <tree> -p <parent> -m <message>`
Create a commit object.

```bash
./your_program.sh commit-tree a1b2c3d4e5f6... -p b2c3d4e5f6a1... -m "Initial commit"
```

## Building

### Prerequisites
- CMake 3.13 or later
- C++23 compatible compiler
- OpenSSL development libraries
- Zlib development libraries
- vcpkg (for dependency management)

### Build Instructions
```bash
# Clone the repository
git clone <your-repo-url>
cd vit

# Build the project
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake
cmake --build ./build
```

## Usage Example

```bash
# Initialize a new repository
./your_program.sh init

# Add a file to the repository
./your_program.sh hash-object -w myfile.txt

# Create a tree from current directory
./your_program.sh write-tree

# Create your first commit
./your_program.sh commit-tree <tree-hash> -m "Initial commit"
```

## Project Structure

- `src/main.cpp` - Main program implementation
- `CMakeLists.txt` - Build configuration
- `your_program.sh` - Convenience script for running vit
- `vcpkg.json` - Dependency specifications

## License

[Add your chosen license here]

## Contributing

[Add contribution guidelines if desired]
