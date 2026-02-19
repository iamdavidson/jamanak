#pragma once

#include <algorithm>  
#include <chrono>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

/**
 * @file jamanak.hpp
 * @brief Small timing utility for measuring durations and rendering a fancy ANSI-colored report.
 *
 * The library provides a simple start/stop API that collects timing samples ("jams")
 * and produces a formatted table-like output string.
 */

#define ANSI_ESC        "\033["
#define ANSI_RESET      ANSI_ESC "0m"
#define ANSI_BOLD       ANSI_ESC "1m"
#define ANSI_DIM        ANSI_ESC "2m"
#define ANSI_RGB(r,g,b) ANSI_ESC "38;2;" #r ";" #g ";" #b "m"

namespace jamanak {

/**
 * @brief Internal state of the timer.
 */
enum class State {
    jamming,  ///< A measurement is currently running.
    idle      ///< No measurement is currently running.
};

/**
 * @brief Time unit selection (reserved for future use).
 *
 * @note This is currently unused; durations are printed in milliseconds.
 */
enum class Unit {
    milli,
    nano,
    sec,
};

/**
 * @brief Represents a single timing measurement.
 */
struct Jam {
    std::string context;
    std::chrono::high_resolution_clock::time_point t0{};
    std::chrono::high_resolution_clock::time_point t1{};

    /// Duration in milliseconds.
    double duration_ms{0.0};
};

/**
 * @brief Simple benchmarking helper that records durations for labeled code sections.
 */
class Jamanak {
private:
    std::string global_context{"default"};
    std::vector<std::shared_ptr<Jam>> jams;
    std::shared_ptr<Jam> current_jam;
    State jam_state{State::idle};

    /// Longest context label length (for alignment).
    std::size_t longest{0};

    /// Longest formatted duration string length (for alignment).
    std::size_t longest_dur{0};

    /// Longest number of characters before decimal point across durations (for alignment).
    std::size_t longest_bc{0};

    /**
     * @brief Repeat a string @p f exactly @p n times.
     * @param n Number of repetitions.
     * @param f Token/string to repeat.
     * @return Concatenated string of length n * f.size().
     */
    std::string fence(int n, const std::string& f) const {
        std::ostringstream out;
        for (int i = 0; i < n; ++i) {
            out << f;
        }
        return out.str();
    }

    /**
     * @brief Returns the count of characters before the decimal separator.
     *
     * Example: "123.45600" -> 3
     *
     * @param num String representation of a floating point number using '.' as decimal separator.
     * @return Number of characters before '.' (or full length if '.' is not found).
     */
    std::size_t digits_before_decimal(const std::string& num) const {
        const auto idx = num.find('.');
        if (idx == std::string::npos) {
            return num.size();
        }
        return idx;
    }

public:
    /**
     * @brief Construct a new Jamanak instance.
     * @param context Global headline/title shown in the report.
     */
    explicit Jamanak(std::string context) : global_context(std::move(context)) {}

    /**
     * @brief Start a new measurement.
     *
     * @param context Label for this measurement (e.g. "load", "solve", "render").
     * @throws std::runtime_error If a measurement is already running.
     */
    void start(const std::string& context) {
        if (jam_state == State::jamming) {
            throw std::runtime_error("already jamming");
        }

        current_jam = std::make_shared<Jam>();
        current_jam->context = context;
        current_jam->t0 = std::chrono::high_resolution_clock::now();
        jam_state = State::jamming;
    }

    /**
     * @brief Stop the current measurement and store it.
     *
     * The measured duration is stored as milliseconds and appended to the internal list.
     *
     * @return Shared pointer to the finished Jam entry.
     * @throws std::runtime_error If no measurement is running.
     */
    std::shared_ptr<Jam> end() {
        if (jam_state == State::idle || !current_jam) {
            throw std::runtime_error("jamming not started");
        }

        current_jam->t1 = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<double, std::milli> ms_double = current_jam->t1 - current_jam->t0;
        current_jam->duration_ms = ms_double.count();

        jams.emplace_back(current_jam);
        longest = std::max(longest, current_jam->context.size());

        // Track formatted width for alignment.
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(5) << current_jam->duration_ms;
        const std::string s = ss.str();

        longest_dur = std::max(longest_dur, s.size());
        longest_bc  = std::max(longest_bc, digits_before_decimal(s));

        auto ret = current_jam;
        current_jam.reset();
        jam_state = State::idle;

        return ret;
    }

    /**
     * @brief Remove all stored measurements.
     *
     * @note This resets stored Jam pointers and clears the container.
     */
    void clean_jams() noexcept {
        for (auto& j : jams) {
            j.reset();
        }
        jams.clear();
        // Optional: also reset alignment stats if you want a fully clean state:
        // longest = longest_dur = longest_bc = 0;
    }

    /**
     * @brief Access all collected measurements.
     * @return Const reference to internal list of Jam entries.
     */
    const std::vector<std::shared_ptr<Jam>>& get_jams() const noexcept {
        return jams;
    }

    /**
     * @brief Check whether a measurement is currently running.
     * @return True if start() was called without a matching end().
     */
    bool is_jamming() const noexcept {
        return jam_state == State::jamming;
    }

    /**
     * @brief Render a formatted report of all measurements.
     *
     * The output contains ANSI escape sequences for styling.
     *
     * @return Report as a string.
     */
    std::string to_string() const {
        std::ostringstream out;

        // Compute total width.
        const std::size_t l_size =
            std::max(longest + longest_dur, global_context.size()) + 13;

        const std::size_t sf_size =
            static_cast<std::size_t>(l_size / 2) - static_cast<std::size_t>(global_context.size() / 2);

        out << ANSI_BOLD << ANSI_RGB(227,225,127);
        out << fence(static_cast<int>(l_size), "–") << "\n";
        out << fence(static_cast<int>(sf_size), " ") << global_context << "\n";
        out << fence(static_cast<int>(l_size), "–") << "\n";

        for (const auto& j : jams) {
            // Format duration consistently (fixed decimals).
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(5) << j->duration_ms;
            const std::string s = ss.str();

            const std::size_t j_context_size = j->context.size();

            out << ANSI_BOLD << ANSI_RGB(227,225,127) << "|| " << ANSI_RESET;
            out << ANSI_BOLD << ANSI_RGB(143,227,125) << j->context << ANSI_RESET;
            out << fence(static_cast<int>(longest - j_context_size + 2), "–") << ": ";
            out << ANSI_BOLD << ANSI_RGB(143,227,125);
            out << fence(static_cast<int>(longest_bc - digits_before_decimal(s)), " ")
                << s << ANSI_RESET;
            out << " ms";
            out << ANSI_BOLD << ANSI_RGB(227,225,127) << " ||" << ANSI_RESET << "\n";
        }

        out << ANSI_BOLD << ANSI_RGB(227,225,127);
        out << fence(static_cast<int>(l_size), "–") << ANSI_RESET << "\n";

        return out.str();
    }
};

} // namespace jamanak
