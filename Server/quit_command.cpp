#include "server.hpp"
void processQUTCommand(int udp_sockfd, char* buffer, struct sockaddr_in client_addr, socklen_t addr_len) {
    char plid[7];
    sscanf(buffer, "QUT %s", plid);

    //verbose mode 
    auto [client_ip, client_port] = getClientIPAndPort(client_addr);                    
    string message = "[REQUEST]: QUT " + string(plid)+ ", from: " + client_ip + ":" + to_string(client_port) + ", PLID: " + string(plid);
    verbose_print(message);

    if (has_ongoing_game(plid)){
        shared_lock<shared_mutex> lock(map_mutex); // Protect map for read access
        Game& existing_game = games[plid]; // Access the instance
        lock.unlock(); // Unlock the map
        if (secondsSinceStartDate(existing_game.start_time) > existing_game.max_playtime) {
            save_game(existing_game, 'T');  
        }
    }
    if (!has_ongoing_game(plid)) {
        const char response[MAXBUF] = "RQT NOK\n";
        sendMessageUDP(udp_sockfd, response, (struct sockaddr*)&client_addr, addr_len);
        return;
    }

    shared_lock<shared_mutex> lock(map_mutex); // Protect map for read access
    Game& existing_game = games[plid]; // Access the instance
    lock.unlock(); // Unlock the map
    existing_game.is_active = false;
    save_game(existing_game, 'Q');

    char response[MAXBUF];
    //cout << "string: " << existing_game.secret_key.c_str() << endl;
    snprintf(response, sizeof(response), "RQT OK %s\n", existing_game.secret_key.c_str());
    sendMessageUDP(udp_sockfd, response, (struct sockaddr*)&client_addr, addr_len);
}