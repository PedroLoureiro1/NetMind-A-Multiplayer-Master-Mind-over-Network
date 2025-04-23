// client.cpp
#include "player.hpp"

void usage() {
    cout << "Usage: ./player [-n IP] [-p PORT]\n";
    cout << " -n IP: Server IP address (default: localhost)\n";
    cout << " -p PORT: Server port (default: " << DEFAULT_PORT << ")\n";
}

int open_udp_socket() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }
    return sockfd;
}

int create_tcp_socket() {
    int tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_sockfd < 0) {
        perror("Error creating TCP socket");
        return -1;  // Retorna -1 em caso de erro
    }
    return tcp_sockfd;
}

bool send_and_receive_with_timeout(int sockfd, const char* message, struct sockaddr_in& server_addr, socklen_t addr_len, char* response) {
    int total_timeout_count = 0;
    int timeout_since_last_send = 0; // Track consecutive timeouts since the last message was sent

    // Configure timeout on the UDP socket
    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_SEC; // Timeout duration
    timeout.tv_usec = 0;

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("Error setting socket timeout");
        return false;
    }

    while (total_timeout_count < MAX_TIMEOUT) {
        if (timeout_since_last_send == 0) { // Resend only when needed
            // Send the message to the server
            if (sendto(sockfd, message, strlen(message), 0, (struct sockaddr*)&server_addr, addr_len) < 0) {
                perror("Error sending message");
                return false;
            }
        }

        // Wait for a response
        int n = recvfrom(sockfd, response, MAXBUF - 1, 0, (struct sockaddr*)&server_addr, &addr_len);
        if (n > 0) {
            response[n] = '\0'; // Null-terminate the response
            return true; // Success, response received
        }

        // Timeout occurred
        timeout_since_last_send++;
        total_timeout_count++;

        // Resend after 3 consecutive timeouts
        if (timeout_since_last_send == RESEND_TIMEOUT) {
            cout << "Resending message...\n";
            timeout_since_last_send = 0; // Reset consecutive timeout counter
        }
    }
    cerr << "Maximum timeouts reached. Exiting.\n";
    return false; // Failed after 5 total timeouts
}


// Função para tratar o comando "start"
void handle_start(const string& command, sockaddr_in server_addr, socklen_t addr_len, ClientState& state) {
    char buffer[MAXBUF];
    char response[MAXBUF];
    char maxplaytime[4], plidChar[7];

    if (strlen(command.c_str()) != 10 || command[6] != ' ' ) {
        cerr << "Invalid command format. Expected 'start <PLID> <max_playtime>'\n";
        return;
    }
    sscanf(command.c_str(), "%6s %3s", plidChar, maxplaytime);
    
    // Validação: PLID deve ter exatamente 6 dígitos e ser numérico
    if (strlen(plidChar) != 6) {
        cerr << "Invalid PLID: It must be a 6-digit number.\n";
        return;
    }
    for (int i = 0; i < 6; i++) {
        if (!isdigit(plidChar[i])) {
            cerr << "Invalid PLID: Must contain only digits.\n";
            return;
        }
    }

    // Validação: maxplaytime deve ter exatamente 3 dígitos e ser numérico
    if (strlen(maxplaytime) != 3) {
        cerr << "Invalid max_playtime: Must be exactly 3 digits.\n";
        return;
    }
    for (int i = 0; i < 3; i++) {
        if (!isdigit(maxplaytime[i])) {
            cerr << "Invalid max_playtime: Must contain only digits.\n";
            return;
        }
    }

    int plid = stoi(plidChar);

    // Converte maxplaytime para inteiro e valida o intervalo
    int max_playtime = stoi(maxplaytime);
    if (max_playtime <= 0 || max_playtime > 600) {
        cerr << "Invalid max_playtime: Must be between 001 and 600 seconds.\n";
        return;
    }

    // Update the PLID and max_playtime in the client state
    state.plid = plid;
    state.max_playtime = max_playtime;

    // Build the message in the format "SNG <PLID> <time>"
    string message = "SNG " + command + "\n";

    // Copy the concatenated message to the buffer
    strncpy(buffer, message.c_str(), sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0'; // Ensure null termination
    
    int sockfd = open_udp_socket();
    
    // Use timeout-aware send and receive function
    bool success = send_and_receive_with_timeout(sockfd, buffer, server_addr, addr_len, response);

    if (!success) {
        cerr << "Failed to receive a response after multiple timeouts. Aborting start command.\n";
        close(sockfd);
        exit(0);
    }

    string server_response(response);
    // Process the server's response
    if (server_response.find("RSG OK") == 0) {
        state.num_tries = 1;        // Reset the number of tries
        state.game_active = true;
        cout << "New game started (max " << state.max_playtime << " sec)" << endl;
    } 
    else if (server_response.find("RSG NOK") == 0) {
        cerr << "Failed to start a new game: a game is already active.\n";
    } 
    else if (server_response.find("RSG ERR") == 0) {
        cerr << "Failed to start a new game: invalid input or conditions.\n";
    } 
    else {
        ;
    }
    // Fechar a conexão UDP
    close(sockfd); 
}

void handle_try(const string& command, sockaddr_in server_addr, socklen_t addr_len, ClientState& state) {

    int sockfd = open_udp_socket();
    if (!state.game_active) {
        cout << "No active game..." << endl;
        close(sockfd);
    }
    else{
        char buffer[MAXBUF];
        char response[MAXBUF];

        if (strlen(command.c_str()) != 7 || command[1] != ' ' || command[3] != ' ' || command[5] != ' '){
            cerr << "Invalid command format. Expected 'TRY <c1> <c2> <c3> <c4>'\n";
            close(sockfd);
            return;
        }

        // Function to check if a character belongs to COLORS
        auto is_valid_color = [](char ch) -> bool {
            return strchr(COLORS, ch) != nullptr;
        };
        // Validate that each color is in COLORS
        if (!is_valid_color(command[0]) || !is_valid_color(command[2]) || !is_valid_color(command[4]) || !is_valid_color(command[6])) {
            cerr << "Invalid color in guess. Allowed colors are: R, G, B, Y, O, P." << endl;
            close(sockfd);
            return;
        }   

        string plid_str = to_string(state.plid);
        string num_tries_str = to_string(state.num_tries);

        string message = "TRY " + plid_str + " " + command + " " + num_tries_str + "\n";

        strncpy(buffer, message.c_str(), sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0'; // Ensure null termination

        // Use timeout-aware send and receive function
        bool success = send_and_receive_with_timeout(sockfd, buffer, server_addr, addr_len, response);

        if (!success) {
            cerr << "Failed to receive a response after multiple timeouts. Aborting start command.\n";
            close(sockfd);
            exit(0);
        }

        string server_response(response);

        // Process the server's response
        if (server_response.find("RTR OK") == 0) {
            int nT, nB, nW;
            sscanf(server_response.c_str(), "RTR OK %d %d %d", &nT, &nB, &nW);
            if (nB == 4) {
                cout << "WELL DONE! You guessed the key in " << state.num_tries << " trials" << endl;
                close(sockfd); 
                return;
            }
            state.num_tries++; // Update the number of tries
            cout << "nB = " << nB << ", nW = " << nW << endl;

        } 
        else if (server_response.find("RTR DUP") == 0) {
            cerr << "Duplicate guess. Number of trials not increased." << endl;
        } 
        else if (server_response.find("RTR INV") == 0) {
            //corrigir esta dm
            cerr << "Invalid trial: The trial number is incorrect./ The guess does not match the previous attempt." << endl;
        } 
        else if (server_response.find("RTR NOK") == 0) {
            cerr << "No ongoing game for this player." << endl;
        } 
        else if (server_response.find("RTR ENT") == 0) {
            char c1, c2 , c3 , c4;
            char key[8];
            sscanf(server_response.c_str(), "RTR ENT %c %c %c %c", &c1, &c2, &c3, &c4);
            // Combine the characters into `guess` with spaces
            snprintf(key, sizeof(key), "%c %c %c %c", c1, c2, c3, c4);  
            state.game_active = false;
            cerr << "No more attempts available. Game over. Secret key: " << key << endl;
        }
        else if (server_response.find("RTR ETM") == 0) {
            char c1, c2 , c3 , c4;
            char key[8];
            sscanf(server_response.c_str(), "RTR ETM %c %c %c %c", &c1, &c2, &c3, &c4);
            // Combine the characters into `guess` with spaces
            snprintf(key, sizeof(key), "%c %c %c %c", c1, c2, c3, c4);              
            state.game_active = false;
            cerr << "Maximum playtime has been reached. Game over. Secret key: " << key << endl;
        }
        else if (server_response.find("RTR ERR") == 0) {
            cerr << "Invalid trial. Check your input or trial sequence." << endl;
        }         
        else {
            ;
        }
        // Fechar a conexão UDP
        close(sockfd); 
    }
}

void handle_show_trials(const char* server_ip, int port, ClientState& state) {

    char buffer[MAXBUF];
    struct sockaddr_in tcp_server_addr{};

    int tcp_sockfd = create_tcp_socket();
    if (tcp_sockfd < 0) {
        return;
    }

    // Initialize TCP server address
    memset(&tcp_server_addr, 0, sizeof(tcp_server_addr));
    tcp_server_addr.sin_family = AF_INET;
    tcp_server_addr.sin_port = htons(port);  // Set the port

    if (inet_pton(AF_INET, server_ip, &tcp_server_addr.sin_addr) <= 0) {
        perror("Invalid IP address for TCP connection");
        close(tcp_sockfd);
        return;
    }

    // Connect to the server
    if (connect(tcp_sockfd, (struct sockaddr*)&tcp_server_addr, sizeof(tcp_server_addr)) < 0) {
        perror("TCP connection failed");
        close(tcp_sockfd);
        return;
    }
    snprintf(buffer, sizeof(buffer), "STR %d\n", state.plid);

    size_t total_bytes = strlen(buffer);
    size_t bytes_sent = 0;
    while (bytes_sent < total_bytes) {
        ssize_t result = write(tcp_sockfd, buffer + bytes_sent, total_bytes - bytes_sent);
        if (result < 0) {
            perror("Error writing to socket");
            close(tcp_sockfd);
            return;
        }
        // Increment the number of bytes sent so far
        bytes_sent += result;
    }

    string response;  // String to store the complete response
    int n;
    memset(buffer, '\0', MAXBUF);
    while ((n = read(tcp_sockfd, buffer, MAXBUF - 1)) > 0) {
        response.append(buffer);  // Append the content to the response buffer
    }
    // Flush any remaining data
    while (read(tcp_sockfd, buffer, MAXBUF - 1) > 0);

    if (n < 0) {
        perror("Error receiving response");
    } else {
        // Process the response
        string header, file_info, trials_data;
        size_t pos = 0;
        int space_count = 0;

        // Find the position of the 4th blank space
        while (space_count < 4 && pos != string::npos) {
            pos = response.find(' ', pos + 1); // Start searching after the last found position
            space_count++;
        }

        if (pos != string::npos) {
            header = response.substr(0, pos); // First line (e.g., "RST ACT 101101_game.txt 62")
            trials_data = response.substr(pos + 1); // Rest of the response (trials data)
        } 
        else {
            cerr << "Invalid response format." << endl;
            close(tcp_sockfd);
            return;
        }

        // Parse the header
        string status, filename;
        int filesize;
        char temp[5];
        char temp2[20];
        sscanf(header.c_str(), "RST %s %s %d", temp, temp2, &filesize);
        status = string(temp);
        filename = string(temp2);
        if (status == "ACT") {
            // Save trials_data to a local file
            string client_filename = "Client/" + filename; 
            ofstream outfile(client_filename);
            if (outfile.is_open()) {
                outfile << trials_data;
                outfile.close();
            } else {
                cerr << "Error saving trials data to file: " << client_filename << endl;
                close(tcp_sockfd);
                return;
            }
            cout << "received trials file: \"" << filename << "\" (" << filesize << " bytes)" << endl;
            cout << "Trials so far:" << trials_data;
        }    
        else if (status == "FIN") {
            // Save trials_data to a local file
            string client_filename = "Client/" + filename; 
            ofstream outfile(client_filename);
            if (outfile.is_open()) {
                outfile << trials_data;
                outfile.close();
            } else {
                cerr << "Error saving trials data to file: " << client_filename << endl;
                close(tcp_sockfd);
                return;
            }

            cout << "received finished game summary file: \"" << filename << "\" (" << filesize << " bytes)" << endl;
            cout << "Summary of the most recent game:" << trials_data;
            state.game_active = false; // Mark the game as finished
        } 
        else if (status == "NOK") {
            cerr << "No active or finished games found for this player." << endl;
            state.game_active = false;

        }    
        else {
            ;
        }
    }
    // Close the TCP connection
    close(tcp_sockfd);
}

void handle_scoreboard(const char* server_ip, int port, ClientState& state) {

    char buffer[MAXBUF];
    struct sockaddr_in tcp_server_addr{};

    int tcp_sockfd = create_tcp_socket();
    if (tcp_sockfd < 0) {
        return;
    }

    // Initialize TCP server address
    memset(&tcp_server_addr, 0, sizeof(tcp_server_addr));
    tcp_server_addr.sin_family = AF_INET;
    tcp_server_addr.sin_port = htons(port);  // Set the port

    if (inet_pton(AF_INET, server_ip, &tcp_server_addr.sin_addr) <= 0) {
        perror("Invalid IP address for TCP connection");
        close(tcp_sockfd);
        return;
    }

    // Connect to the server
    if (connect(tcp_sockfd, (struct sockaddr*)&tcp_server_addr, sizeof(tcp_server_addr)) < 0) {
        perror("TCP connection failed");
        close(tcp_sockfd);
        return;
    }
    snprintf(buffer, sizeof(buffer), "SSB\n");

    size_t total_bytes = strlen(buffer);
    size_t bytes_sent = 0;
    while (bytes_sent < total_bytes) {
        ssize_t result = write(tcp_sockfd, buffer + bytes_sent, total_bytes - bytes_sent);
        if (result < 0) {
            perror("Error writing to socket");
            close(tcp_sockfd);
            return;
        }
        // Increment the number of bytes sent so far
        bytes_sent += result;
    }

    string response;  // String to store the complete response
    int n;
    memset(buffer, '\0', MAXBUF);
    while ((n = read(tcp_sockfd, buffer, MAXBUF - 1)) > 0) {
        buffer[n] = '\0';  // Ensure the response is a valid string
        response.append(buffer);  // Append the content to the response buffer
    }

    if (n < 0) {
        perror("Error receiving response");
        close(tcp_sockfd);
        return;
    }
    else{
        // Process the response
        string header, scores_data;
        size_t pos = 0;
        int space_count = 0;

        // Find the position of the 4th blank space
        while (space_count < 4 && pos != string::npos) {
            pos = response.find(' ', pos + 1); // Start searching after the last found position
            space_count++;
        }

        if (pos != string::npos) {
            header = response.substr(0, pos); // First line (e.g., "RSS OK scores.txt 43")
            scores_data = response.substr(pos + 1); // Rest of the response (score data)
        } else {
            cerr << "Invalid response format." << endl;
            close(tcp_sockfd);
            return;
        }
        // Parse the header
        string status, filename;
        int filesize;
        char temp[5];
        char temp2[20];
        sscanf(header.c_str(), "RSS %s %s %d", temp, temp2, &filesize);
        status = string(temp);
        filename = string(temp2);

        if (status == "OK") {
            // Save trials_data to a local file
            string client_filename = "Client/" + filename; 
            ofstream outfile(client_filename);
            if (outfile.is_open()) {
                outfile << scores_data;
                outfile.close();
            } else {
                cerr << "Error saving trials data to file: " << client_filename << endl;
                close(tcp_sockfd);
                return;
            }
            cout << "response saved as \"" << filename << "\" (" << filesize << " bytes):" << endl;
            cout << "Score:\n" << scores_data;
        } 
        else if (status == "EMPTY") {
            cout << "No games have been won yet. The scoreboard is empty." << endl;
        } 
        else {
            cout << "Error ocurred while doing the scoreboard command." << endl;
        }
    }
    // Close the TCP connection
    close(tcp_sockfd);
}

void handle_quit( sockaddr_in server_addr, socklen_t addr_len, ClientState& state) {
    

    if (!state.game_active) {
        cout << "No active game..." << endl;
    }
    else{
        int sockfd = open_udp_socket();
        char buffer[MAXBUF];
        char response[MAXBUF];
        memset(buffer, 0, sizeof(buffer)); // Ensure the buffer is cleared.

        // Construct the "QUT <PLID>" message
        snprintf(buffer, sizeof(buffer), "QUT %d\n", state.plid);

        // Use timeout-aware send and receive function
        bool success = send_and_receive_with_timeout(sockfd, buffer, server_addr, addr_len, response);

        if (!success) {
            cerr << "Failed to receive a response after multiple timeouts. Aborting start command.\n";
            close(sockfd);
            exit(0);
        }
        string server_response(response);

        // Process the response
        if (strncmp(server_response.c_str(), "RQT OK", 6) == 0) {
            char c1, c2, c3, c4;
            sscanf(server_response.c_str(), "RQT OK %c %c %c %c", &c1, &c2, &c3, &c4);
            cout << "Only losers quit. Secret key: " << c1 << " " << c2 << " " << c3 << " " << c4 << endl;
            state.game_active = false;            
        } 

        else if (strncmp(server_response.c_str(), "RQT NOK", 7) == 0) {
            cerr << "No active game..." << endl;
            state.game_active = false;
        } 

        else if (strncmp(server_response.c_str(), "RQT ERR", 7) == 0) {
            cerr << "There was a mistake when trying to finish the game." << endl;
        }
        close(sockfd);
    }
}

void handle_exit( sockaddr_in server_addr, socklen_t addr_len, ClientState& state) {
    
    if (!state.game_active) {
        cout << "Exiting application..." << endl;
        exit(0);
    }
    
    else {
        int sockfd = open_udp_socket();

        char buffer[MAXBUF];
        char response[MAXBUF];

        // Construct the "QUT <PLID>" message
        snprintf(buffer, sizeof(buffer), "QUT %d\n", state.plid);

        // Use timeout-aware send and receive function
        bool success = send_and_receive_with_timeout(sockfd, buffer, server_addr, addr_len, response);

        if (!success) {
            cerr << "Failed to receive a response after multiple timeouts. Aborting start command.\n";
            close(sockfd);
            exit(0);
        }
        string server_response(response);
        
        if (server_response.find("RQT OK") == 0) {
            cout << "Exiting application with an active game. Informing server..." << endl;
            state.game_active = false;
            close(sockfd);
            exit(0);
        } else if (server_response.find("RQT NOK") == 0) {
            cout << "Exiting application..." << endl;
            state.game_active = false;
            close(sockfd);
            exit(0);
        }else {
            cerr << "An error occurred while trying to quit the game." << endl;
        }
    close(sockfd);
    }
}

void handle_debug(const string& command, sockaddr_in server_addr, socklen_t addr_len, ClientState& state) {

    int sockfd = open_udp_socket();  // Abre o socket UDP
    if (sockfd < 0) {
        perror("Error creating UDP socket");
        return;
    }

    char buffer[MAXBUF];
    char response[MAXBUF];
    char maxplaytime[4], plidChar[7];

    // Parse the "debug <PLID> <time> <C1> <C2> <C3> <C4>" command
    sscanf(command.c_str(), "%6s %3s", plidChar, maxplaytime);
    

    if (strlen(command.c_str()) != 18 || command[6] != ' ' || command[10] != ' ' || command[12] != ' ' || command[14] != ' ' || command[16] != ' ') {
        cerr << "Invalid command format. Expected 'debug <PLID> <max_playtime> <c1> <c2> <c3> <c4>'\n";
        return;
    }
    
    if (strlen(plidChar) != 6) {
        cerr << "Invalid PLID: It must be a 6-digit number.\n";
        return;
    }

    for (int i = 0; i < 6; i++) {
        if (!isdigit(plidChar[i])) {
            cerr << "Invalid PLID: Must contain only digits.\n";
            return;
        }
    }

    // Valida que maxplaytime possui exatamente 3 dígitos numéricos
    if (strlen(maxplaytime) != 3) {
        cerr << "Invalid max_playtime: Must be exactly 3 digits.\n";
        return;
    }
    for (int i = 0; i < 3; i++) {
        if (!isdigit(maxplaytime[i])) {
            cerr << "Invalid max_playtime: Must contain only digits.\n";
            return;
        }
    }

    int plid = stoi(plidChar); 

    // Converte maxplaytime para inteiro e valida o intervalo
    int max_playtime = stoi(maxplaytime);
    if (max_playtime <= 0 || max_playtime > 600) {
        cerr << "Invalid max_playtime: Must be between 001 and 600 seconds.\n";
        return;
    }

    // Function to check if a character belongs to COLORS
    auto is_valid_color = [](char ch) -> bool {
        return strchr(COLORS, ch) != nullptr;
    };
    // Validate that each color is in COLORS
    if (!is_valid_color(command[11]) || !is_valid_color(command[13]) || !is_valid_color(command[15]) || !is_valid_color(command[17])) {
        cerr << "Invalid color in guess. Allowed colors are: R, G, B, Y, O, P." << endl;
        close(sockfd);
        return;
    } 

    state.plid = plid;
    state.max_playtime = max_playtime;
    
    // Build the message in the format "SNG <PLID> <time>"
    string message = "DBG " + command + "\n";

    // Copy the concatenated message to the buffer
    strncpy(buffer, message.c_str(), sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0'; // Ensure null termination

    // Use timeout-aware send and receive function
    bool success = send_and_receive_with_timeout(sockfd, buffer, server_addr, addr_len, response);

    if (!success) {
        cerr << "Failed to receive a response after multiple timeouts. Aborting start command.\n";
        close(sockfd);
        exit(0);
    }

    string server_response(response);

    // Process the server's response
    if (server_response.find("RDB OK") == 0) {
        cout << "Debug game started successfully\n"; 
        state.plid = plid;              // Update the player's PLID
        state.max_playtime = max_playtime; // Update the max play time
        state.num_tries = 1;            // Reset the number of tries
        state.game_active = true;       // Mark the game as active
    } 
    else if (server_response.find("RDB NOK") == 0) {
        cerr << "Failed to start a debug game: a game is already active." << endl;
    } 
    else if (server_response.find("RDB ERR") == 0) {
        cerr << "Failed to start a debug game: invalid input or conditions." << endl;
    } 
    else {
        ;
    }

    // Fechar a conexão UDP
    close(sockfd);
}