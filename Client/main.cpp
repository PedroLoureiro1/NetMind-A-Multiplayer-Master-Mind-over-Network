// main.cpp
#include "player.hpp"

int main(int argc, char* argv[]) {
    int port = DEFAULT_PORT;
    char server_ip[INET_ADDRSTRLEN] = "127.0.0.1";
    char buffer[MAXBUF];
    sockaddr_in server_addr{};
    socklen_t addr_len = sizeof(server_addr);

    // Process command-line arguments
    int opt;
    while ((opt = getopt(argc, argv, "n:p:")) != -1) {
        switch (opt) {
            case 'n':
                strncpy(server_ip, optarg, INET_ADDRSTRLEN - 1);
                server_ip[INET_ADDRSTRLEN - 1] = '\0';
                break;
            case 'p':
                port = atoi(optarg);
                break;
            default:
                usage();
                exit(EXIT_FAILURE);
        }
    }

    // Configure server address for UDP
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid IP address");
    }

    cout << "Connected to server " << server_ip << ":" << port << endl;

    ClientState state;

    // Main client loop
    while (true) {
        if (!fgets(buffer, sizeof(buffer), stdin)) {
            perror("Error reading command");
            continue;
        }

        buffer[strcspn(buffer, "\n")] = 0; 

        string command(buffer); 

        size_t space_pos = command.find(' ');

        string primary_command = (space_pos != string::npos) ? command.substr(0, space_pos) : command;

        // Extract the arguments part of the command
        string arguments = (space_pos != string::npos) ? command.substr(space_pos + 1) : "";

        // Copy the arguments back into the buffer
        strncpy(buffer, arguments.c_str(), sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0'; // Ensure null termination

        if (primary_command == "start") {
            handle_start(buffer, server_addr, addr_len, state);
        } 
        
        else if (primary_command == "try") {
            handle_try(buffer, server_addr, addr_len, state);
        } 
        
        else if (primary_command == "show_trials" || primary_command == "st") {
            handle_show_trials(server_ip, port, state);
        } 
        
        else if (primary_command == "scoreboard" || primary_command == "sb") {
            handle_scoreboard(server_ip, port, state);
        } 
        
        else if (primary_command == "quit") {
            handle_quit(server_addr, addr_len, state);
        } 
        
        else if (primary_command == "exit") {
            handle_exit(server_addr, addr_len, state);
        } 
        
        else if(primary_command == "debug"){
            handle_debug(buffer, server_addr, addr_len, state);
        }

        else {
            cout << "Invalid command.\n";
            cout << "\nAvailable commands:\n";
            cout << "  start <PLID> <max_playtime>\n";
            cout << "  try <C1> <C2> <C3> <C4>\n";
            cout << "  show_trials\n";
            cout << "  scoreboard\n";
            cout << "  quit\n";
            cout << "  exit\n";
        }
    }

    return 0;
}
