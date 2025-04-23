// main.c
#include "server.hpp"

map<string, Game> games; // Map to hold games by PLID
shared_mutex map_mutex;
shared_mutex scoreboard_mutex;
shared_mutex games_mutex;

bool verbose = false;

int main(int argc, char* argv[]) {
    int port = PORT;

    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) { // Skip argv[0] (program name)
        if (strcmp(argv[i], "-v") == 0) {
            verbose = true; // Enable verbose mode
        } else if (strcmp(argv[i], "-p") == 0) {
            // Check if the next argument exists
            if (i + 1 < argc) {
                port = atoi(argv[++i]); // Convert the next argument to an integer
            } else {
                cerr << "Error: Missing value for -p flag.\n";
                return EXIT_FAILURE;
            }
        } else {
            cerr << "Error: Unknown argument '" << argv[i] << "'.\n";
            return EXIT_FAILURE;
        }
    }

    // Display settings if verbose mode is enabled
    if (verbose) 
        cout << "Verbose mode enabled.\n";

    srand(time(nullptr));
    signal(SIGCHLD, SIG_IGN);

    int udp_sockfd, tcp_sockfd, max_fd;
    char buffer[MAXBUF];
    sockaddr_in udp_server_addr{}, tcp_server_addr{}, client_addr{};
    socklen_t addr_len = sizeof(client_addr);

    // Create UDP socket
    if ((udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Error creating socket");
        return EXIT_FAILURE;
    }

    // Create TCP socket
    if ((tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error creating TCP socket");
        return EXIT_FAILURE;
    }

    // Set socket options to allow address reuse for UDP
    int udp_opt = 1;
    if (setsockopt(udp_sockfd, SOL_SOCKET, SO_REUSEADDR, &udp_opt, sizeof(udp_opt)) < 0) {
        perror("Error setting socket options (SO_REUSEADDR) for UDP");
        close(udp_sockfd);
        return EXIT_FAILURE;
    }
    
    // Configure UDP server address
    memset(&udp_server_addr, 0, sizeof(udp_server_addr));
    udp_server_addr.sin_family = AF_INET;
    udp_server_addr.sin_addr.s_addr = INADDR_ANY;
    udp_server_addr.sin_port = htons(port);

    // Set socket options to allow address reuse for TCP
    int tcp_opt = 1;
    if (setsockopt(tcp_sockfd, SOL_SOCKET, SO_REUSEADDR, &tcp_opt, sizeof(tcp_opt)) < 0) {
        perror("Error setting socket options (SO_REUSEADDR) for TCP");
        close(tcp_sockfd);
        return EXIT_FAILURE;
    }

    // Configure TCP server address
    memset(&tcp_server_addr, 0, sizeof(tcp_server_addr));
    tcp_server_addr.sin_family = AF_INET;
    tcp_server_addr.sin_addr.s_addr = INADDR_ANY;
    tcp_server_addr.sin_port = htons(port);

    // Bind UDP socket
    if (bind(udp_sockfd, (struct sockaddr*)&udp_server_addr, sizeof(udp_server_addr)) < 0) {
        perror("Error binding UDP socket");
        close(udp_sockfd);
        return EXIT_FAILURE;
    }
    // Bind TCP socket
    if (bind(tcp_sockfd, (struct sockaddr*)&tcp_server_addr, sizeof(tcp_server_addr)) < 0) {
        perror("Error binding TCP socket");
        close(tcp_sockfd);
        return EXIT_FAILURE;
    }
    // Listen on TCP socket
    if (listen(tcp_sockfd, 5) < 0) {
        perror("Error in listen");
        close(tcp_sockfd);
        return EXIT_FAILURE;
    }
    fd_set read_fds;

    while (true) {

        FD_ZERO(&read_fds);
        FD_SET(udp_sockfd, &read_fds);
        FD_SET(tcp_sockfd, &read_fds);

        max_fd = max(udp_sockfd, tcp_sockfd);

        int activity = select(max_fd + 1, &read_fds, nullptr, nullptr, nullptr);

        if (activity < 0) {
            perror("select error");
            break;
        }

        if (FD_ISSET(udp_sockfd, &read_fds)) {

            // Handle UDP communication
            int n = recvfrom(udp_sockfd, buffer, MAXBUF, 0, (struct sockaddr*)&client_addr, &addr_len);
            buffer[n] = '\0';          

            if (strncmp(buffer, "SNG", 3) == 0) {
                processSNGCommand(udp_sockfd, buffer, client_addr, addr_len, games, map_mutex);
            } else if (strncmp(buffer, "TRY", 3) == 0) {
                processTRYCommand(buffer, udp_sockfd, client_addr, addr_len, games);
            } else if (strncmp(buffer, "QUT", 3) == 0) {
                processQUTCommand(udp_sockfd, buffer, client_addr, addr_len);
            } else if (strncmp(buffer, "DBG", 3) == 0) {
                processDBGCommand(udp_sockfd, buffer, client_addr, addr_len, games, map_mutex);
            } else {
                sendto(udp_sockfd, "ERR\n", strlen("ERR\n"), 0,
                       (struct sockaddr*)&client_addr, addr_len);
            }
        }
        
        if (FD_ISSET(tcp_sockfd, &read_fds)) {
            // Handle TCP communication
            int client_sockfd = accept(tcp_sockfd, (struct sockaddr*)&client_addr, &addr_len);
            if (client_sockfd < 0) {
                perror("Error accepting TCP connection");
                continue;
            }

            // Fork to handle the client
            pid_t pid = fork();
            if (pid < 0) {
                // Fork failed
                perror("Error forking process");
                close(client_sockfd); // Close the client socket since the parent won't handle it
                continue;
            }

            if (pid == 0) {
                // Child process
                close(tcp_sockfd); // Child does not need the listening socket

                size_t total_bytes_read = 0;
                ssize_t n;

                while (true) {
                    // Read data from the socket
                    n = read(client_sockfd, buffer, MAXBUF - 1);

                    if (n < 0) {
                        perror("Error reading data");
                        close(client_sockfd); // Always close the socket on error
                        exit(EXIT_FAILURE);   // Exit child process
                    }
                    if (n == 0) {
                        // No more data from client, connection closed
                        break;
                    }
                    // Copy the read data into the response buffer
                    memcpy(buffer + total_bytes_read, buffer, n);
                    total_bytes_read += n;

                    // Check if a newline character is present in the data read
                    if (memchr(buffer, '\n', n) != nullptr) {
                        break; // Stop reading if a newline is found
                    }
                }
                buffer[total_bytes_read] = '\0';

                if (strncmp(buffer, "SSB", 3) == 0) {
                    processSSBCommand(client_sockfd, client_addr);
                } else if (strncmp(buffer, "STR", 3) == 0) {
                    processSTRCommand(client_sockfd, buffer, client_addr);
                } else {
                    const char message[MAXBUF] = "ERR\n";
                    sendMessageTCP(client_sockfd, message);
                }
                close(client_sockfd); // Close the client socket after processing
                exit(EXIT_SUCCESS);   // Exit the child process when done
            } else {
                // Parent process
                close(client_sockfd); // Parent doesn't need this client socket
            }
        }
    }
    close(udp_sockfd);
    close(tcp_sockfd);
    return 0;
}
