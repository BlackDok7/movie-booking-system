#include <gtest/gtest.h>

#include "booking_service.hpp"

#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

using booking::BookingService;
using booking::MovieId;
using booking::TheaterId;
using booking::ShowId;

// ---------- Helpers ----------
static bool contains(const std::vector<std::string>& v, const std::string& s) {
    return std::find(v.begin(), v.end(), s) != v.end();
}

// ---------- Tests: parsing / formatting ----------
TEST(SeatLabelParsing, ValidLabels) {
    int idx = -1;
    EXPECT_TRUE(BookingService::try_parse_seat_label("a1", idx));
    EXPECT_EQ(idx, 0);

    EXPECT_TRUE(BookingService::try_parse_seat_label("a20", idx));
    EXPECT_EQ(idx, 19);

    EXPECT_TRUE(BookingService::try_parse_seat_label("A10", idx)); // uppercase row accepted
    EXPECT_EQ(idx, 9);
}

TEST(SeatLabelParsing, InvalidLabels) {
    int idx = -1;
    EXPECT_FALSE(BookingService::try_parse_seat_label("", idx));
    EXPECT_FALSE(BookingService::try_parse_seat_label("a", idx));
    EXPECT_FALSE(BookingService::try_parse_seat_label("b1", idx));
    EXPECT_FALSE(BookingService::try_parse_seat_label("a0", idx));
    EXPECT_FALSE(BookingService::try_parse_seat_label("a21", idx));
    EXPECT_FALSE(BookingService::try_parse_seat_label("a1x", idx));
    EXPECT_FALSE(BookingService::try_parse_seat_label("ax", idx));
    EXPECT_FALSE(BookingService::try_parse_seat_label("a-1", idx));
}

TEST(SeatLabelFormatting, IndexToLabel) {
    EXPECT_EQ(BookingService::seat_label_from_index0(0), "a1");
    EXPECT_EQ(BookingService::seat_label_from_index0(9), "a10");
    EXPECT_EQ(BookingService::seat_label_from_index0(19), "a20");
}

// ---------- Tests: base data ----------
TEST(BookingServiceData, ListMovies) {
    BookingService svc;
    auto movies = svc.list_movies();
    ASSERT_EQ(movies.size(), 3u);

    EXPECT_EQ(movies[0].id, 1);
    EXPECT_EQ(movies[0].title, "Inception");
}

TEST(BookingServiceData, ListTheatersForMovie) {
    BookingService svc;

    // Movie 1 (Inception) is in Theater 1 and 2
    auto theaters = svc.list_theaters_for_movie(1);
    ASSERT_EQ(theaters.size(), 2u);

    // Must be sorted by id (the implementation sorts)
    EXPECT_EQ(theaters[0].id, 1);
    EXPECT_EQ(theaters[1].id, 2);

    // Movie 2 (Interstellar) only in Theater 1
    theaters = svc.list_theaters_for_movie(2);
    ASSERT_EQ(theaters.size(), 1u);
    EXPECT_EQ(theaters[0].id, 1);

    // Non-existing movie -> empty
    theaters = svc.list_theaters_for_movie(999);
    EXPECT_TRUE(theaters.empty());
}

TEST(BookingServiceData, FindShow) {
    BookingService svc;

    // Inception @ Central => show id 1
    EXPECT_EQ(svc.find_show(1, 1), 1);

    // Inception @ Mall => show id 2
    EXPECT_EQ(svc.find_show(1, 2), 2);

    // Non-existing combination
    EXPECT_EQ(svc.find_show(2, 2), -1);
}

// ---------- Tests: availability ----------
TEST(Availability, InitiallyAllSeatsAvailable) {
    BookingService svc;

    ShowId show = svc.find_show(1, 1);
    ASSERT_NE(show, -1);

    auto seats = svc.list_available_seats(show);
    ASSERT_EQ(seats.size(), 20u);
    EXPECT_TRUE(contains(seats, "a1"));
    EXPECT_TRUE(contains(seats, "a20"));
}

// ---------- Tests: booking success/failure ----------
TEST(Booking, RejectInvalidShowId) {
    BookingService svc;
    auto res = svc.book_seats(999, {"a1"});
    EXPECT_FALSE(res.success);
}

TEST(Booking, RejectEmptyRequest) {
    BookingService svc;
    ShowId show = svc.find_show(1, 1);
    ASSERT_NE(show, -1);

    auto res = svc.book_seats(show, {});
    EXPECT_FALSE(res.success);
}

TEST(Booking, RejectInvalidSeatLabel) {
    BookingService svc;
    ShowId show = svc.find_show(1, 1);
    ASSERT_NE(show, -1);

    auto res = svc.book_seats(show, {"a0"});
    EXPECT_FALSE(res.success);

    res = svc.book_seats(show, {"b1"});
    EXPECT_FALSE(res.success);

    res = svc.book_seats(show, {"a1x"});
    EXPECT_FALSE(res.success);
}

TEST(Booking, RejectDuplicateLabelsInSameRequest) {
    BookingService svc;
    ShowId show = svc.find_show(1, 1);
    ASSERT_NE(show, -1);

    auto res = svc.book_seats(show, {"a1", "a1"});
    EXPECT_FALSE(res.success);
    EXPECT_TRUE(res.message.find("Duplicate") != std::string::npos
                || res.message.find("duplicate") != std::string::npos);
}

TEST(Booking, SuccessfulBookingMarksSeatsUnavailable) {
    BookingService svc;
    ShowId show = svc.find_show(1, 1);
    ASSERT_NE(show, -1);

    auto res = svc.book_seats(show, {"a1", "a2", "a3"});
    ASSERT_TRUE(res.success);

    auto seats = svc.list_available_seats(show);
    EXPECT_FALSE(contains(seats, "a1"));
    EXPECT_FALSE(contains(seats, "a2"));
    EXPECT_FALSE(contains(seats, "a3"));
    EXPECT_TRUE(contains(seats, "a4"));
}

TEST(Booking, AllOrNothingBooking) {
    BookingService svc;
    ShowId show = svc.find_show(1, 1);
    ASSERT_NE(show, -1);

    // First book a1
    auto res1 = svc.book_seats(show, {"a1"});
    ASSERT_TRUE(res1.success);

    // Now request includes a1 (already booked) AND a2 (free)
    auto res2 = svc.book_seats(show, {"a1", "a2"});
    ASSERT_FALSE(res2.success);

    // Verify a2 was NOT booked as a side effect
    auto seats = svc.list_available_seats(show);
    EXPECT_TRUE(contains(seats, "a2"));
    EXPECT_FALSE(contains(seats, "a1"));
}

TEST(Booking, CannotOverbookSameSeat) {
    BookingService svc;
    ShowId show = svc.find_show(1, 1);
    ASSERT_NE(show, -1);

    auto res1 = svc.book_seats(show, {"a10"});
    ASSERT_TRUE(res1.success);

    auto res2 = svc.book_seats(show, {"a10"});
    ASSERT_FALSE(res2.success);
}

// ---------- Concurrency test ----------
TEST(Concurrency, OnlyOneThreadCanBookSameSeat) {
    BookingService svc;
    ShowId show = svc.find_show(1, 1);
    ASSERT_NE(show, -1);

    constexpr int kThreads = 16;
    std::atomic<bool> start{false};
    std::atomic<int> successes{0};

    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&] {
            while (!start.load()) {
                // spin until start
            }
            auto res = svc.book_seats(show, {"a1"});
            if (res.success) {
                successes.fetch_add(1);
            }
        });
    }

    start.store(true);

    for (auto& t : threads) {
        t.join();
    }

    // Exactly one booking must succeed
    EXPECT_EQ(successes.load(), 1);

    // Seat must be unavailable
    auto seats = svc.list_available_seats(show);
    EXPECT_FALSE(contains(seats, "a1"));
}
