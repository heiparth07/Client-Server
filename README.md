# File Server System

A comprehensive client-server file system implementation in C using socket programming, designed for Linux platforms. The system provides distributed file access capabilities with automatic load balancing between a main server and mirror server.

## Features

### Core Functionality
- **Multi-client Support**: Handles multiple concurrent client connections using fork-based process management
- **File Search**: Find files by name across the user's home directory
- **Size-based Retrieval**: Get files within specified size ranges
- **Date-based Retrieval**: Retrieve files modified within date ranges
- **Extension-based Retrieval**: Fetch files by specific extensions (up to 6 extensions)
- **Tar Archive Delivery**: All file collections are compressed and delivered as tar.gz archives
- **Load Balancing**: Automatic distribution of client connections between main and mirror servers

### Advanced Features
- **Connection Routing**: First 4 connections → main server, next 4 → mirror server, then alternating
- **Transparent Redirection**: Clients are seamlessly redirected to mirror server when needed
- **Input Validation**: Comprehensive client-side command validation before server communication
- **Error Handling**: Robust error checking and informative error messages
- **Progress Indicators**: Visual feedback during file downloads

## Architecture

```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   Client    │◄──►│Main Server  │◄──►│Mirror Server│
│   (8080)    │    │   (8080)    │    │   (8081)    │
└─────────────┘    └─────────────┘    └─────────────┘
```

## Installation

### Prerequisites
- Linux operating system
- GCC compiler
- Standard C libraries

### Build Instructions

1. **Clone the repository:**
   ```bash
   git clone https://github.com/heiparth07/Client-Server.git
   cd Client-Server
   ```

2. **Compile the project:**
   ```bash
   # Compile the main server
   gcc -o server src/server.c
   
   # Compile the mirror server
   gcc -o mirror src/mirror.c
   
   # Compile the client
   gcc -o client src/client.c
   ```

3. **Or compile all at once:**
   ```bash
   gcc -o server src/server.c && gcc -o mirror src/mirror.c && gcc -o client src/client.c
   ```

## Usage

### Starting the Servers

1. **Start the main server:**
   ```bash
   ./server
   ```

2. **Start the mirror server (in another terminal):**
   ```bash
   ./mirror
   ```

3. **Start the client:**
   ```bash
   ./client
   ```

### Available Commands

| Command | Syntax | Description | Example |
|---------|--------|-------------|---------|
| `findfile` | `findfile <filename>` | Find a file by name | `findfile document.txt` |
| `sgetfiles` | `sgetfiles <size1> <size2>` | Get files within size range (bytes) | `sgetfiles 1024 10485760` |
| `dgetfiles` | `dgetfiles <date1> <date2>` | Get files within date range | `dgetfiles 2023-01-01 2023-12-31` |
| `getfiles` | `getfiles <ext1> [ext2] ... [ext6]` | Get files by extensions | `getfiles txt pdf jpg` |
| `getftar` | `getftar <filename>` | Get specific file as tar | `getftar config.conf` |
| `quit` | `quit` | Exit client | `quit` |
| `help` | `help` | Show help message | `help` |

### Command Details

#### File Search (`findfile`)
- Searches recursively through the user's home directory
- Returns the full path of the first matching file
- Case-sensitive filename matching

#### Size-based Retrieval (`sgetfiles`)
- Specify size range in bytes
- `size1` must be ≤ `size2`
- Both sizes must be non-negative integers
- Returns all files within the specified size range as a tar.gz archive

#### Date-based Retrieval (`dgetfiles`)
- Date format: `YYYY-MM-DD`
- Retrieves files modified between the specified dates (inclusive)
- Returns matching files as a tar.gz archive

#### Extension-based Retrieval (`getfiles`)
- Accepts 1-6 file extensions
- Extensions should be alphanumeric only
- Returns all matching files as a tar.gz archive

#### Tar Retrieval (`getftar`)
- Retrieves a specific file and packages it as a tar.gz archive
- Useful for maintaining file permissions and metadata

## Technical Implementation

### Server Architecture
- **Multi-process**: Each client connection is handled by a separate forked process
- **Connection Management**: Global connection counter tracks client distribution
- **Load Balancing Algorithm**:
  - Connections 1-4: Main server
  - Connections 5-8: Mirror server
  - Connections 9+: Alternating (odd→main, even→mirror)

### Client Features
- **Command Validation**: Syntax checking before server communication
- **Automatic Redirection**: Transparent handling of server redirection
- **Progress Tracking**: Visual feedback during file transfers
- **Error Recovery**: Graceful handling of connection failures

### File Operations
- **Recursive Search**: Deep directory traversal for comprehensive file discovery
- **Efficient Packaging**: Dynamic tar.gz creation for file collections
- **Memory Management**: Bounded file collection (MAX_FILES = 1000)
- **Temporary File Cleanup**: Automatic cleanup of temporary tar archives

## Configuration

### Default Ports
- Main Server: `8080`
- Mirror Server: `8081`

### Modifiable Constants
```c
#define PORT 8080           // Main server port
#define MIRROR_PORT 8081    // Mirror server port
#define MAX_BUFFER 4096     // Buffer size for data transfer
#define MAX_FILES 1000      // Maximum files per operation
#define MAX_PATH 1024       // Maximum path length
```

## File Structure

```
Client-Server/
├── src/
│   ├── server.c          # Main server implementation
│   ├── mirror.c          # Mirror server implementation
│   └── client.c          # Client implementation
├── README.md             # This file
└── .gitignore            # Git ignore file
```

## Testing

Since this is a basic implementation, testing should be done manually:

1. **Start the servers in separate terminals:**
   ```bash
   # Terminal 1
   ./server
   
   # Terminal 2  
   ./mirror
   ```

2. **Test with multiple clients:**
   ```bash
   # Terminal 3, 4, 5, etc.
   ./client
   ```

3. **Test each command:**
   - Try `findfile` with existing files
   - Test `sgetfiles` with different size ranges
   - Test `dgetfiles` with date ranges
   - Test `getfiles` with various extensions
   - Test `getftar` with specific files

## Performance Considerations

- **Concurrent Connections**: Supports multiple simultaneous clients
- **Memory Usage**: Bounded by MAX_FILES constant
- **File Size Limits**: Limited by available disk space for temporary files
- **Network Efficiency**: Binary file transfer with progress tracking

## Error Handling

### Client-side Validation
- Command syntax verification
- Parameter range checking
- Extension format validation
- Date format validation

### Server-side Error Handling
- Invalid command responses
- File system access errors
- Tar creation failures
- Network communication errors

## Security Considerations

- **Scope Limitation**: Searches restricted to user's home directory
- **Path Traversal Protection**: No absolute path access
- **Input Sanitization**: Command parameter validation
- **Resource Limits**: Bounded file collection and buffer sizes

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/new-feature`)
3. Commit your changes (`git commit -am 'Add new feature'`)
4. Push to the branch (`git push origin feature/new-feature`)
5. Create a Pull Request


## Troubleshooting

### Common Issues

1. **Connection Refused**
   - Ensure servers are running before starting client
   - Check if ports 8080 and 8081 are available

2. **Permission Denied**
   - Verify executable permissions: `chmod +x server mirror client`
   - Check home directory access permissions

3. **File Not Found**
   - Ensure search is within user's home directory
   - Verify file names are case-sensitive

4. **Mirror Server Connection**
   - Confirm mirror server is running on port 8081
   - Check network connectivity between servers

### Debug Mode
```bash
# Compile with debug symbols
gcc -g -o server src/server.c
gcc -g -o mirror src/mirror.c  
gcc -g -o client src/client.c

# Debug with GDB
gdb ./server
```

## Future Enhancements

- [ ] SSL/TLS encryption for secure communication
- [ ] Configuration file support
- [ ] Logging system implementation
- [ ] Web-based client interface
- [ ] Database integration for file indexing
- [ ] Compression algorithm options
- [ ] User authentication system

## Authors

- **Parth Dangaria** 
- **Riddhi Jobanputra** 

## Acknowledgments

- Socket programming concepts from Stevens' "Unix Network Programming"
- File system operations based on POSIX standards
- Inspired by traditional FTP and file server architectures
