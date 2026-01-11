#include "booking_service.hpp"

#include <iostream>
#include <sstream>

static void print_help() {
    std::cout
        << "Commands:\n"
        << "  movies\n"
        << "  theaters <movie_id>\n"
        << "  seats <movie_id> <theater_id>\n"
        << "  book <movie_id> <theater_id> a1 a2 ...\n"
        << "  exit \n";
}

int main() {
    booking::BookingService svc;

    std::cout << "Movie Booking CLI\n";
    print_help();

    std::string line;
    while (true) {
        std::cout << "\n> ";
        if (!std::getline(std::cin, line)) break;
        if (line == "exit") break;
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd == "help") {
            print_help();
        } else if (cmd == "movies") {
            std::vector<booking::Movie> ms = svc.list_movies();
            for (std::size_t i = 0; i < ms.size(); ++i) {
                std::cout << ms[i].id << ": " << ms[i].title << "\n";
            }
        } else if (cmd == "theaters") {
            int movie_id = -1;
            iss >> movie_id;
            std::vector<booking::Theater> ts = svc.list_theaters_for_movie(movie_id);
            if (ts.empty()) {
                std::cout << "No theaters found for movie_id=" << movie_id << "\n";
            } else {
                for (std::size_t i = 0; i < ts.size(); ++i) {
                    std::cout << ts[i].id << ": " << ts[i].name << "\n";
                }
            }
        } else if (cmd == "seats") {
            int movie_id = -1, theater_id = -1;
            iss >> movie_id >> theater_id;
            const booking::ShowId show_id = svc.find_show(movie_id, theater_id);
            if (show_id < 0) {
                std::cout << "No show for that movie+theater\n";
                continue;
            }
            std::vector<std::string> seats = svc.list_available_seats(show_id);
            std::cout << "Available seats (" << seats.size() << "): ";
            for (std::size_t i = 0; i < seats.size(); ++i) {
                std::cout << seats[i] << (i + 1 < seats.size() ? ", " : "\n");
            }
        } else if (cmd == "book") {
            int movie_id = -1, theater_id = -1;
            iss >> movie_id >> theater_id;
            const booking::ShowId show_id = svc.find_show(movie_id, theater_id);
            if (show_id < 0) {
                std::cout << "No show for that movie+theater\n";
                continue;
            }

            std::vector<std::string> seats;
            std::string s;
            while (iss >> s) seats.push_back(s);

            booking::BookingResult r = svc.book_seats(show_id, seats);
            std::cout << (r.success ? "OK: " : "FAIL: ") << r.message << "\n";
        } else {
            std::cout << "Unknown command. Type 'help'.\n";
        }
    }

    return 0;
}
