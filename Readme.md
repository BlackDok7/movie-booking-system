# Movie Booking System (C++)

## Overview
This project implements a **simple in-memory movie booking system** written in modern C++ (C++17).
It allows users to list movies, see theaters where a movie is shown, inspect available seats, and
**book one or more seats concurrently without overbooking**.

The focus of the project is on:
- Clean API design
- Correct concurrency handling
- Atomic vs mutex synchronization strategies
- Unit testing and coverage
- Reproducible builds using Docker
- Clear API documentation using Doxygen

## Key Features
- In-memory data model (movies, theaters, shows)
- Seat labels `a1` .. `a20`
- **Thread-safe booking**
- **All-or-nothing seat booking** (atomicity guarantee)
- CLI interface for interaction
- Unit tests with concurrency scenarios
- Dockerized build environment
- Doxygen-generated API documentation

### Core Components
- **BookingService**  
  Main service class exposing the public API.

- **Movie / Theater / Show**  
  Lightweight data structures describing the domain.

- **ShowState (internal)**  
  Per-show booking state used for concurrency control.

## Seat Model
- Each show has **20 seats** labeled `a1` to `a20`
- Internally mapped to **indices 0..19**
- Booking state stored as a **bitmask**
  - Bit = 0 → seat available
  - Bit = 1 → seat booked

This representation allows fast, atomic updates.

## Concurrency Design
- Each show has its own `std::atomic<uint32_t>` booking mask
- Booking uses a **compare-and-swap (CAS) loop**
- Guarantees:
  - No seat can be overbooked
  - Booking multiple seats is **all-or-nothing**
  - No global locks
  - No contention between different shows

## Thread-Safety Guarantees
- Multiple threads may book seats for the same show
- Different shows never contend with each other
- Atomic implementation is lock-free at the seat level
- Unit tests include multi-threaded booking validation

## Command Line Interface (CLI)
The CLI allows interaction with the booking service.

### Example Commands
- movies
- theaters <movie_id>
- seats <movie_id> <theater_id>
- book <movie_id> <theater_id> a1 a2 a3

## Build Requirements
- C++17 compatible compiler (GCC / Clang)
- CMake ≥ 3.16
- Docker
- Ninja(optional, used in container for faster builds)

## Run Docker container
**./run_docker.sh**
The project directory is mounted into the container at /workspace.

## Build & Run
**./build_and_run.sh**
Run this script inside the container to get build done and run the CLI Application.

## Run tests
**./run_tests.sh**
Run this script inside container to build and run the Gtests.

What is tested:
- Listing movies and theaters
- Finding shows
- Seat parsing and formatting
- Successful bookings
- Duplicate and invalid seat handling
- Concurrent booking (multi-threaded test)

## Code Coverage
Coverage is generated using gcov + lcov.
**./run_coverage.sh**
Run this script inside container to build and get the code coverage report.

Output
HTML report generated under:
coverage/index.html

## API Documentation (Doxygen)
The public API is documented using Doxygen.

Generate documentation
**./gen_docs.sh**

Output
docs/html/index.html

## Design Decisions Summary
- Atomic bitmask chosen for simplicity and performance
- Per-show isolation avoids unnecessary contention
- STL-only implementation for clarity and portability
- Docker-based build ensures reproducibility
- Extensive tests to validate correctness under concurrency

## Possible Extensions
- Persistent storage (database)
- Multiple seat rows
- Reservation expiration
- Administrator view/APIs

## Author Notes
- This project is intentionally kept minimal while demonstrating:
- Modern C++ practices
- Correct concurrency handling
- Test-driven validation
- Clear documentation