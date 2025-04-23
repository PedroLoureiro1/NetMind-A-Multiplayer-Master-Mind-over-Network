#include "server.hpp"

void handle_game_end(Game& game, int udp_sockfd, sockaddr_in& client_addr, socklen_t addr_len, char status, int black, int white) {
    game.is_active = false;
    save_trials(game, false); // Ensure trials are saved for all end scenarios

    char response[MAXBUF];
    string message;
    switch (status) {
        case 'T': // Time limit exceeded
            snprintf(response, sizeof(response), "RTR ETM %s\n", game.secret_key.c_str());
            break;
        case 'F': // Game lost (max trials reached)
            snprintf(response, sizeof(response), "RTR ENT %s\n", game.secret_key.c_str());
            break;
        case 'W': // Game won
            save_score(game);
            snprintf(response, sizeof(response), "RTR OK %d %d %d\n", game.trials, black, white);
            break;
        default:
            return;
    }
    save_game(game, status);
    sendMessageUDP(udp_sockfd, response, (struct sockaddr*)&client_addr, addr_len);
}

void processTRYCommand(const char* buffer, int udp_sockfd, sockaddr_in& client_addr, socklen_t addr_len, map<string, Game>& games) {
    if (strlen(buffer) != 21 || buffer[12] != ' ' || buffer[14] != ' ' || buffer[16] != ' ' ||  buffer[strlen(buffer) - 1] != '\n'){
        const char response[MAXBUF] = "RTR ERR\n";
        sendMessageUDP(udp_sockfd, response, (struct sockaddr*)&client_addr, addr_len);
        return;
    }

    char guess[8];
    char c1, c2, c3, c4;
    char plid[7];
    int trial_number;
    sscanf(buffer, "TRY %s %c %c %c %c %d", plid, &c1, &c2, &c3, &c4, &trial_number);

    // Function to check if a character belongs to COLORS
    auto is_valid_color = [](char ch) -> bool {
        return strchr(COLORS, ch) != nullptr;
    };
    // Validate that each color is in COLORS
    if (!is_valid_color(c1) || !is_valid_color(c2) || !is_valid_color(c3) || !is_valid_color(c4)) {
        const char response[MAXBUF] = "RTR ERR\n";
        sendMessageUDP(udp_sockfd, response, (struct sockaddr*)&client_addr, addr_len);
        return;
    }   
    snprintf(guess, sizeof(guess), "%c %c %c %c", c1, c2, c3, c4);
    char key[5]; // 4 caracteres + 1 espaço para o terminador nu
    // Atribuindo os caracteres ao buffer
    key[0] = c1;
    key[1] = c2;
    key[2] = c3;
    key[3] = c4;
    key[4] = '\0';

    //verbose mode 
    auto [client_ip, client_port] = getClientIPAndPort(client_addr);                    
    string message = "[REQUEST]: TRY " + string(guess)+ ", from: " + client_ip + ":" + to_string(client_port) + ", PLID: " + string(plid);
    verbose_print(message);

    if (has_ongoing_game(plid)){
        shared_lock<shared_mutex> lock(map_mutex); // Protect map for read access
        Game& existing_game = games[plid]; // Access the instance
        lock.unlock(); // Unlock the map
        if (secondsSinceStartDate(existing_game.start_time) > existing_game.max_playtime) {
            handle_game_end(existing_game, udp_sockfd, client_addr, addr_len, 'T', 0, 0);
            return;
        }
    }

    if (!has_ongoing_game(plid)) {
        const char response[MAXBUF] = "RTR NOK\n";
        sendMessageUDP(udp_sockfd, response, (struct sockaddr*)&client_addr, addr_len);
        return;
    }
    
    shared_lock<shared_mutex> lock(map_mutex); // Protect map for read access
    Game& existing_game = games[plid]; // Access the instance
    lock.unlock(); // Unlock the map

    if (is_duplicate_try(guess, existing_game.trial_history)) {
        const char response[MAXBUF] = "RTR DUP\n";
        sendMessageUDP(udp_sockfd, response, (struct sockaddr*)&client_addr, addr_len);
        return;
    }

    if ((trial_number - 1) != existing_game.trials) {
        if (existing_game.trial_history.empty()) {
            const char response[MAXBUF] = "RTR INV\n";
            sendMessageUDP(udp_sockfd, response, (struct sockaddr*)&client_addr, addr_len);
            return;
        }

        string last_trial = existing_game.trial_history.back();
        size_t pos = last_trial.find("T: ");
        if (pos != string::npos) {
            string last_guess = last_trial.substr(pos + 3, 4);
            if (last_guess != guess) {
                const char response[MAXBUF] = "RTR INV\n";
                sendMessageUDP(udp_sockfd, response, (struct sockaddr*)&client_addr, addr_len);
                return;
            }
        }
    }

    int black = 0, white = 0;
    calculate_feedback(guess, existing_game.secret_key, black, white);

    existing_game.trial_history.emplace_back("T: " + string(guess) + " nB=" + to_string(black) + ", nW=" + to_string(white) + " " + to_string(secondsSinceStartDate(existing_game.start_time)));
    existing_game.trials++;

    if (black == 4) {
        handle_game_end(existing_game, udp_sockfd, client_addr, addr_len, 'W', black, white);
    } else if (existing_game.trials >= MAX_TRIALS) {
        handle_game_end(existing_game, udp_sockfd, client_addr, addr_len, 'F', black, white);
    } else {
        save_trials(existing_game, false);
        char response[MAXBUF];
        snprintf(response, sizeof(response), "RTR OK %d %d %d\n", existing_game.trials, black, white);
        sendMessageUDP(udp_sockfd, response, (struct sockaddr*)&client_addr, addr_len);
    }
}