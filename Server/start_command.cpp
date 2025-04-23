#include "server.hpp"

// Função para validar o PLID
bool isValidPLID(const char* plid) {
    for (int i = 0; i < 6; i++) {
        if (!isdigit(plid[i])) {
            cerr << "Invalid PLID: Must contain only digits.\n";
            return false;
        }
    }
    if (strlen(plid) != 6) {
        cerr << "Invalid PLID: It must be a 6-digit number.\n";
        return false;
    }
    return true;
}

// Função para validar o tempo disponível
bool isValidPlaytime(int time_available) {
    return time_available > 0 && time_available <= 600;
}

// Função para enviar uma mensagem ao cliente
void sendMessageUDP(int sockfd, const char* message, struct sockaddr* client_addr, socklen_t addr_len) {
    // Attempt to send the message
    ssize_t bytes_sent = sendto(sockfd, message, strlen(message), 0, client_addr, addr_len);

    // Check if the message was sent successfully
    if (bytes_sent < 0) {
        perror("Error sending message");
        return;
    }
}

// Função para inicializar um jogo novo ou reiniciar um jogo existente
void initializeGame(Game& game, const string& plid, int time_available) {
    game.plid = plid;
    game.is_active = true;
    game.mode = 'P';
    game.start_date = getFormattedTime();
    time(&game.start_time);
    game.trials = 0;
    generate_secret_key(game.secret_key);
    game.trial_history.clear();
    game.max_playtime = time_available;
    save_trials(game, true);
}

// Função para processar comando SNG
void processSNGCommand(int udp_sockfd, const char* buffer, struct sockaddr_in client_addr, socklen_t addr_len, 
                        map<string, Game>& games, shared_mutex& map_mutex) {

    if (strlen(buffer) != 15 || buffer[10] != ' ' || buffer[strlen(buffer) - 1] != '\n') {
        const char response[MAXBUF] = "RSG ERR\n";
        sendMessageUDP(udp_sockfd, response, (struct sockaddr*)&client_addr, addr_len);
        return;
    }

    char plid[7];
    int time_available = 0;
    sscanf(buffer, "SNG %s %d", plid, &time_available);

    if (!isValidPLID(plid)) {
        const char response[MAXBUF] = "RSG ERR\n";
        sendMessageUDP(udp_sockfd, response, (struct sockaddr*)&client_addr, addr_len);
        return;
    }

    if (!isValidPlaytime(time_available)) {
        const char response[MAXBUF] = "RSG ERR\n";
        sendMessageUDP(udp_sockfd, response, (struct sockaddr*)&client_addr, addr_len);
        return;
    }

    //verbose mode 
    auto [client_ip, client_port] = getClientIPAndPort(client_addr);                    
    string message = "[REQUEST]: SNG " + string(plid)+ " " + to_string(time_available) + ", from: " + client_ip + ":" + to_string(client_port) + ", PLID: " + string(plid);
    verbose_print(message);
    
    bool game_already_existed = false;
    bool start_0_tries = false;
    {
        shared_lock<shared_mutex> lock(map_mutex);
        auto it = games.find(plid);

        if (it != games.end()) {
            // Existing game logic
            Game& existing_game = it->second;
            lock.unlock(); // Unlock shared_lock early since we're modifying

            if (has_ongoing_game(plid) && secondsSinceStartDate(existing_game.start_time) <= existing_game.max_playtime) {
                if (existing_game.trials == 0){//game is active with 0 tries and player does start
                    start_0_tries = true;
                }
                else{
                    const char response[MAXBUF] = "RSG NOK\n";
                    sendMessageUDP(udp_sockfd, response, (struct sockaddr*)&client_addr, addr_len);
                    return;
                }
            }
            if (has_ongoing_game(plid) && (!start_0_tries)) {
                save_game(existing_game, 'T');
            }
            initializeGame(existing_game, plid, time_available);
            game_already_existed = true; // Mark that we handled an existing game
        }
    }
    if (!game_already_existed) {
        // New game logic
        Game new_game;
        initializeGame(new_game, plid, time_available);
        unique_lock<shared_mutex> lock(map_mutex); 
        games[plid] = new_game;
    }
    // Common response and logging
    const char response[MAXBUF] = "RSG OK\n";
    sendMessageUDP(udp_sockfd, response, (struct sockaddr*)&client_addr, addr_len);
}
