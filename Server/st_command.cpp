#include "server.hpp"
// Function to write a message to the socket
void sendMessageTCP(int client_sockfd, const char* message) {
    size_t total_sent = 0;                 // Tracks the total bytes sent
    size_t message_size = strlen(message); 

    while (total_sent < message_size) {
        ssize_t bytes_sent = write(client_sockfd, message + total_sent, message_size - total_sent);
        if (bytes_sent < 0) {
            perror("Error sending data to socket");
            close(client_sockfd);
            return;
        }
        total_sent += bytes_sent; // Update the total bytes sent
    }
}

void processSTRCommand(int client_sockfd, const char *buffer, struct sockaddr_in client_addr) {
    if (strlen(buffer) != 11 || buffer[strlen(buffer) - 1] != '\n'){
        sendMessageTCP(client_sockfd,"RST NOK\n");
        close(client_sockfd);
        return;
    }
    char plid[7];
    sscanf(buffer, "STR %s", plid);

    //verbose mode 
    auto [client_ip, client_port] = getClientIPAndPort(client_addr);                    
    string message = "[REQUEST]: STR " + string(plid)+ ", from: " + client_ip + ":" + to_string(client_port) + ", PLID: " + string(plid);
    verbose_print(message);

    if (has_ongoing_game(plid)){
        shared_lock<shared_mutex> lock(map_mutex); // Protect map for read access
        Game& existing_game = games[plid]; // Access the instance
        lock.unlock(); // Unlock the map
        if (secondsSinceStartDate(existing_game.start_time) > existing_game.max_playtime) {
            save_game(existing_game, 'T');  
        }
    }

    shared_lock<shared_mutex> lock(map_mutex); // Protect map for read access
    Game& existing_game = games[plid]; // Access the instance
    lock.unlock(); // Unlock the map
    int time_left = existing_game.max_playtime - secondsSinceStartDate(existing_game.start_time);
    send_trials(client_sockfd, existing_game.plid, existing_game, time_left);
}