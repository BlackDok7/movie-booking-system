#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @file booking_service.hpp
 * @brief Public API for the in-memory movie booking service (atomic implementation).
 *
 * This header defines:
 * - Domain types: Movie, Theater, Show
 * - BookingService: the main API for listing and booking seats
 *
 * Concurrency model (atomic implementation):
 * - Each show has its own independent booking state (no cross-show contention).
 * - Seats are tracked via an atomic bitmask (20 LSB bits used).
 * - Booking is all-or-nothing using a CAS loop (compare-and-swap).
 *
 * Seat labels are "a1".."a20", mapped to indices [0..19].
 */

namespace booking {

/**
 * @brief Movie identifier type.
 *
 * Kept as an alias for readability in public APIs.
 */
using MovieId = int;

/**
 * @brief Theater identifier type.
 *
 * Kept as an alias for readability in public APIs.
 */
using TheaterId = int;

/**
 * @brief Show identifier type.
 *
 * Kept as an alias for readability in public APIs.
 */
using ShowId = int;

/**
 * @brief Represents a movie.
 */
struct Movie {
    MovieId id;           /**< Unique movie identifier. */
    std::string title;    /**< Human-readable movie title. */
};

/**
 * @brief Represents a theater.
 */
struct Theater {
    TheaterId id;         /**< Unique theater identifier. */
    std::string name;     /**< Human-readable theater name. */
};

/**
 * @brief Represents a show (a movie shown at a theater).
 */
struct Show {
    ShowId id;            /**< Unique show identifier. */
    MovieId movie_id;     /**< The movie being shown. */
    TheaterId theater_id; /**< The theater where the show runs. */
};

/**
 * @brief Result of a booking attempt.
 */
struct BookingResult {
    bool success;         /**< True if booking succeeded; false otherwise. */
    std::string message;  /**< Human-readable result description (useful for CLI & tests). */
};

/**
 * @brief In-memory booking service with concurrency-safe seat reservation.
 *
 * @details
 * This service provides a small API to:
 * - list movies
 * - list theaters showing a movie
 * - find a show by (movie, theater)
 * - list available seats for a show
 * - book seats for a show
 *
 * ### Thread-safety
 * - Multiple threads may call @ref book_seats concurrently for the same show.
 * - Overbooking is prevented via atomic updates.
 * - Booking multiple seats is **all-or-nothing**: if any requested seat is already booked,
 *   no seats are booked.
 *
 * ### Seat representation
 * Seats are stored as an atomic 32-bit mask; 20 least significant bits represent seats:
 * - bit i == 0 => seat available
 * - bit i == 1 => seat booked
 *
 * Seat labels are "a1".."a20".
 */
class BookingService {
public:
    /**
     * @brief Constructs the service and initializes in-memory data.
     *
     * @details
     * The constructor sets up a minimal sample dataset (movies, theaters, shows) and
     * creates per-show booking state entries in @ref show_state_.
     */
    BookingService();

    /**
     * @brief Returns all available movies.
     * @return Vector of movies stored by the service.
     *
     * @note Returns by value (copy). The dataset is small and keeps the API simple.
     */
    std::vector<Movie> list_movies() const;

    /**
     * @brief Lists theaters that have at least one show for the given movie.
     *
     * @param movie_id The movie identifier.
     * @return Vector of theaters sorted by theater id (stable, deterministic output).
     *
     * @note Deterministic ordering is useful for unit tests and predictable CLI output.
     */
    std::vector<Theater> list_theaters_for_movie(MovieId movie_id) const;

    /**
     * @brief Finds a show for a given (movie, theater) pair.
     *
     * @param movie_id The movie identifier.
     * @param theater_id The theater identifier.
     * @return The show id if it exists; otherwise returns -1.
     */
    ShowId find_show(MovieId movie_id, TheaterId theater_id) const;

    /**
     * @brief Lists available seats for a show.
     *
     * @param show_id The show identifier.
     * @return Vector of seat labels that are currently free (e.g. "a1", "a2", ...).
     *
     * @details
     * Reads the current atomic booking mask once and converts all 0-bits into labels.
     */
    std::vector<std::string> list_available_seats(ShowId show_id) const;

    /**
     * @brief Books one or more seats for a show atomically (all-or-nothing).
     *
     * @param show_id The show identifier.
     * @param seat_labels Seat labels to book (e.g. {"a1","a2"}).
     * @return BookingResult describing success or the reason for failure.
     *
     * @details
     * - Validates the show id and that seat_labels is non-empty.
     * - Parses and validates seat labels (range + duplicates).
     * - Converts requested seats to a bitmask.
     * - Uses a CAS loop on the atomic mask to ensure:
     *   - no overbooking under concurrency
     *   - all-or-nothing booking for multiple seats
     */
    BookingResult book_seats(ShowId show_id, const std::vector<std::string>& seat_labels);

    /**
     * @brief Parses a seat label (e.g. "a1") into a zero-based index [0..19].
     *
     * @param label Input seat label (expected "a1".."a20", case-insensitive for 'a').
     * @param out_index0 Output seat index in [0..19] on success.
     * @return True if label is valid; false otherwise.
     *
     * @details
     * Uses std::stoi on the numeric suffix and ensures the suffix is fully consumed.
     */
    static bool try_parse_seat_label(const std::string& label, int& out_index0);

    /**
     * @brief Converts a seat index [0..19] into a label ("a1".."a20").
     *
     * @param index0 Zero-based seat index [0..19].
     * @return Seat label string.
     */
    static std::string seat_label_from_index0(int index0);

private:
    /**
     * @brief Number of seats in each show.
     */
    static constexpr int kSeatCount = 20;

    /**
     * @brief Internal per-show seat booking state (atomic bitmask).
     *
     * @details
     * This type is stored behind a std::unique_ptr because std::atomic is non-copyable,
     * and we want stable storage for per-show state inside an unordered_map.
     */
    struct ShowState {
        std::atomic<std::uint32_t> booked_mask; /**< 20 LSB bits represent seats a1..a20. */

        /** @brief Initializes all seats as available (mask=0). */
        ShowState() : booked_mask(0u) {}

        ShowState(const ShowState&) = delete;
        ShowState& operator=(const ShowState&) = delete;
    };

    // In-memory data (small sample dataset)
    std::vector<Movie> movies_;     /**< Stored movies. */
    std::vector<Theater> theaters_; /**< Stored theaters. */
    std::vector<Show> shows_;       /**< Stored shows (movie x theater). */

    /**
     * @brief Per-show booking state map.
     *
     * @details
     * Key: show id
     * Value: pointer to per-show ShowState
     *
     * Using unordered_map enables O(1) average lookup for seat operations.
     */
    std::unordered_map<ShowId, std::unique_ptr<ShowState>> show_state_;

    /**
     * @brief Returns mutable ShowState for a show id (or nullptr if not found).
     *
     * @param show_id Show identifier.
     * @return Pointer to ShowState if present, otherwise nullptr.
     */
    ShowState* get_state_mut(ShowId show_id);

    /**
     * @brief Returns read-only ShowState for a show id (or nullptr if not found).
     *
     * @param show_id Show identifier.
     * @return Pointer to ShowState if present, otherwise nullptr.
     */
    const ShowState* get_state(ShowId show_id) const;

    /**
     * @brief Converts a list of seat labels into a bitmask.
     *
     * @param labels List of seat labels.
     * @param out_error Filled with error message on failure; cleared on entry.
     * @return Bitmask representing requested seats, or 0 on failure.
     *
     * @details
     * Performs:
     * - label validation (format + range)
     * - duplicate detection within the request
     *
     * This helper does not check current booking state; it only validates the request.
     */
    static std::uint32_t seats_to_mask_or_fail(const std::vector<std::string>& labels,
                                               std::string& out_error);
};

} // namespace booking
