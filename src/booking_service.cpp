#include "booking_service.hpp"

#include <algorithm>
#include <unordered_set>

namespace booking {

BookingService::BookingService() {
    // Minimal sample data (you can expand later)
    movies_.emplace_back(Movie{1, "Inception"});
    movies_.emplace_back(Movie{2, "Interstellar"});
    movies_.emplace_back(Movie{3, "The Matrix"});

    theaters_.emplace_back(Theater{1, "Central Cinema"});
    theaters_.emplace_back(Theater{2, "Mall Theater"});

    // Shows (movie x theater)
    shows_.emplace_back(Show{1, 1, 1}); // Inception @ Central
    shows_.emplace_back(Show{2, 1, 2}); // Inception @ Mall
    shows_.emplace_back(Show{3, 2, 1}); // Interstellar @ Central
    shows_.emplace_back(Show{4, 3, 2}); // Matrix @ Mall

    // Initialize per-show state
    for (const auto& show : shows_) { //using a range loop with const reference to avoid copying and showing the intend that objects won't be modified
        show_state_.emplace(show.id, std::make_unique<ShowState>());
    }
}

std::vector<Movie> BookingService::list_movies() const {
    return movies_;
}

std::vector<Theater> BookingService::list_theaters_for_movie(MovieId movie_id) const {
    std::vector<Theater> result;

    // Collect unique theater IDs that have a show for this movie
    std::unordered_set<TheaterId> theater_ids;
    for (const auto& show : shows_) {
        if (show.movie_id == movie_id) {
            theater_ids.insert(show.theater_id); //insert used instead of emplace as the argument is a primitive(int). So insert and emplace give the same performance here
        }
    }

    // Return theaters in the same order as theaters_ (stable, predictable)
    for (const auto& theater : theaters_) {
        if (theater_ids.find(theater.id) != theater_ids.end()) {
            result.push_back(theater);  //push_back used instead of emplace_back because the theather object is already created, so the same performance. Only if the objects needs to be created from arguments then emplace_back would be better.
        }
    }
    // sort the theathers by ID(optional)
    std::sort(result.begin(), result.end(),
          [](const Theater& a, const Theater& b) {
              return a.id < b.id;
          });

    //returning a std::vector because it keeps stable ordering all the time(needed for unit tests). Also vector is faster, and has lower memory overhead
    return result;
}

ShowId BookingService::find_show(MovieId movie_id, TheaterId theater_id) const {
    for (const auto& show : shows_) {
        if (show.movie_id == movie_id && show.theater_id == theater_id) {
            return show.id;
        }
    }
    return -1;
}

std::vector<std::string> BookingService::list_available_seats(ShowId show_id) const {
    std::vector<std::string> out;
    const ShowState* st = get_state(show_id);
    if (!st) return out;

    const std::uint32_t mask = st->booked_mask.load(); //load is an atomic read operation
    for (int i = 0; i < kSeatCount; ++i) {
        const std::uint32_t bit = (1u << i);
        if ((mask & bit) == 0u) {
            out.push_back(seat_label_from_index0(i));
        }
    }
    return out;
}

BookingResult BookingService::book_seats(ShowId show_id, const std::vector<std::string>& seat_labels) {
    ShowState* st = get_state_mut(show_id);
    if (!st) {
        return BookingResult{false, "Invalid show id"};
    }
    if (seat_labels.empty()) {
        return BookingResult{false, "No seats provided"};
    }

    std::string err;
    const std::uint32_t req = seats_to_mask_or_fail(seat_labels, err);
    if (!err.empty()) {
        return BookingResult{false, err};
    }

    // CAS loop: atomic all-or-nothing booking
    std::uint32_t current = st->booked_mask.load();
    while (true) {
        if ((current & req) != 0u) {
            return BookingResult{false, "One or more seats already booked"};
        }
        const std::uint32_t desired = (current | req);
        if (st->booked_mask.compare_exchange_weak(current, desired)) { // On failure: compare_exchange_weak updates current to the latest value in the atomic then you loop and retry with the new current
            return BookingResult{true, "Booked successfully"};
        }
        // compare_exchange updated 'current' to latest value; retry
    }
}

bool BookingService::try_parse_seat_label(const std::string& label, int& out_index0) {
    // Expected format: a1..a20 (case-insensitive 'a')
    if (label.size() < 2) return false; //check that a label has at least 2 chars in the string as a1 or a2, not just a

    char row = label[0];
    if (row == 'A') row = 'a'; // check that the first character in the string is A or a, and if it's A convert it to lowercase
    if (row != 'a') return false;

    try {
        std::size_t pos = 0;
        int num = std::stoi(label.substr(1), &pos); // use stoi to convert string to integer, taking a substring  starting from possition 1 in the label string, so after a, and returning at which possition it stoped

        // Ensure the entire numeric part was consumed (no "a12x")
        if (pos != label.size() - 1) return false;

        if (num < 1 || num > kSeatCount) return false; // check that the seat number is in range 1-20

        out_index0 = num - 1;
        return true;
    } catch (...) {
        return false; // if any exceptions thrown then return false
    }
}

// Method used for converting a seat index counting from 0 to a human readable seats naming in range a1...a20
std::string BookingService::seat_label_from_index0(int index0) {
    return std::string("a") + std::to_string(index0 + 1);
}

//Below we have 2 similar methods but one is const and second no because: One provides mutable access for write operations, the other enforces read-only access for const methods. This preserves const-correctness and prevents accidental mutation of shared state.
BookingService::ShowState* BookingService::get_state_mut(ShowId show_id) {
    auto it = show_state_.find(show_id);
    if (it == show_state_.end()) return nullptr;
    return it->second.get();
}

const BookingService::ShowState* BookingService::get_state(ShowId show_id) const {
    auto it = show_state_.find(show_id);
    if (it == show_state_.end()) return nullptr;
    return it->second.get();
}

std::uint32_t BookingService::seats_to_mask_or_fail(const std::vector<std::string>& labels,
                                                    std::string& out_error) {
    out_error.clear();
    std::uint32_t mask = 0u;

    for (const auto& lbl : labels) {
        int idx0 = -1;
        if (!try_parse_seat_label(lbl, idx0)) {
            out_error = "Invalid seat label: " + lbl;
            return 0u;
        }

        const std::uint32_t bit = (1u << idx0);
        if (mask & bit) {
            out_error = "Duplicate seat label: " + lbl;
            return 0u;
        }

        mask |= bit;
    }
    return mask;
}
} // namespace booking