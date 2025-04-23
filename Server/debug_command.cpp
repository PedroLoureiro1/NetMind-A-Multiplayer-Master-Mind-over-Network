#include "server.hpp"

// Função para inicializar um jogo novo ou reiniciar um jogo existente
void initializeGameDebug(Game& game, const string& plid, int time_available, const string& guess) {
    game.plid = plid;
    game.is_active = true;
    game.mode = 'D';
    game.start_date = getFormattedTime();
    time(&game.start_time);
    game.trials = 0;
    game.secret_key = guess;
    game.trial_history.clear();
    game.max_playtime = time_available;
    save_trials(game, true);
}

// Função para processar comando SNG
void processDBGCommand(int udp_sockfd, const char* buffer, struct sockaddr_in client_addr, socklen_t addr_len, 
                        map<string, Game>& games, shared_mutex& game_mutex) {
    if (strlen(buffer) != 23 || buffer[10] != ' ' || buffer[14] != ' ' || buffer[16] != ' ' ||
        buffer[18] != ' ' || buffer[20] != ' ' || buffer[strlen(buffer) - 1] != '\n'){
        const char response[MAXBUF] = "RDB ERR\n";
        sendMessageUDP(udp_sockfd, response, (struct sockaddr*)&client_addr, addr_len);
        return;
    }

    char plid[7];
    int time_available = 0;
    char guess[8];
    char c1, c2, c3, c4;    
    sscanf(buffer, "DBG %s %d %c %c %c %c", plid, &time_available, &c1, &c2, &c3, &c4);

    if (!isValidPLID(plid)) {
        const char response[MAXBUF] = "RDB ERR\n";
        sendMessageUDP(udp_sockfd, response, (struct sockaddr*)&client_addr, addr_len);
        return;
    }

    if (!isValidPlaytime(time_available)) {
        const char response[MAXBUF] = "RDB ERR\n";
        sendMessageUDP(udp_sockfd, response, (struct sockaddr*)&client_addr, addr_len);
        return;
    }

    // Function to check if a character belongs to COLORS
    auto is_valid_color = [](char ch) -> bool {
        return strchr(COLORS, ch) != nullptr;
    };
    // Validate that each color is in COLORS
    if (!is_valid_color(c1) || !is_valid_color(c2) || !is_valid_color(c3) || !is_valid_color(c4)) {
        const char response[MAXBUF] = "RDB ERR\n";
        sendMessageUDP(udp_sockfd, response, (struct sockaddr*)&client_addr, addr_len);
        return;
    }
    // Combine the characters into `guess` with spaces
    snprintf(guess, sizeof(guess), "%c %c %c %c", c1, c2, c3, c4);

    //verbose mode 
    auto [client_ip, client_port] = getClientIPAndPort(client_addr);                    
    string message = "[REQUEST]: DBG " + string(plid) + " " + to_string(time_available) + " " + string(guess) + ", from: " + client_ip + ":" + to_string(client_port) + ", PLID: " + string(plid);
    verbose_print(message);    
        
    bool game_already_existed = false;
    bool start_0_tries = false;
    {
        shared_lock<shared_mutex> lock(map_mutex);
        auto it = games.find(plid);        
        if (it != games.end()) {

            Game& existing_game = it->second;
            lock.unlock(); // Unlock shared_lock early since we're modifying

            if (has_ongoing_game(plid) && secondsSinceStartDate(existing_game.start_time) <= existing_game.max_playtime) {
                if (existing_game.trials == 0){//game is active with 0 tries and player does start
                    start_0_tries = true;
                }
                else{
                    const char response[MAXBUF] = "RDB NOK\n";
                    sendMessageUDP(udp_sockfd, response, (struct sockaddr*)&client_addr, addr_len);
                    return;
                }
            }
            if (has_ongoing_game(plid)) {
                save_game(existing_game, 'T');
            }
            initializeGameDebug(existing_game, plid, time_available, guess);
            game_already_existed = true; // Mark that we handled an existing game
        } 
    }
    if (!game_already_existed) {
        Game new_game;
        initializeGameDebug(new_game, plid, time_available, guess);
        unique_lock<shared_mutex> lock(map_mutex);
        games[plid] = new_game;                                    
    }  
    const char response[MAXBUF] = "RDB OK\n";
    sendMessageUDP(udp_sockfd, response, (struct sockaddr*)&client_addr, addr_len);

}
