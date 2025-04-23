 Introduction
The goal of this project is to implement a simplified version of the classic Master Mind game, playable over the network using a client-server architecture.

Players interact through a Player Application, while game state and logic are managed by a Game Server (GS). The two components communicate via UDP and TCP protocols, enabling real-time multiplayer gameplay across different machines connected to the Internet.

Each game involves guessing a secret 4-color code (C1 C2 C3 C4), using a predefined set of colors: R (Red), G (Green), B (Blue), Y (Yellow), O (Orange), and P (Purple). Colors may repeat.

 Features
Players can:

Start a new game with a personal identifier (PLID) and a maximum playtime (in seconds).

Submit guesses and receive feedback in the form of black and white pegs (correct position and color, or correct color only).

View previous trials.

Check a global scoreboard (top 10).

Quit or exit the application at any time.

Use a debug mode to specify a known secret key for testing.

The Game Server:

Manages active games and enforces rules.

Handles both UDP (gameplay) and TCP (data transfer) communications.

Optionally runs in verbose mode to output request logs.

The project requires implementing a custom application-layer protocol over TCP and UDP sockets, demonstrating proficiency in low-level network programming and real-time client-server communication.