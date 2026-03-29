#pragma once

#include <chrono>
#include <cstring>
#include <iomanip>
#include <stdexcept>
#include <vector>
#include <string>
#include <memory>
#include <sstream>
#include <string>

#define ANSI_ESC        "\033["
#define ANSI_RESET      ANSI_ESC "0m"
#define ANSI_BOLD       ANSI_ESC "1m"
#define ANSI_DIM        ANSI_ESC "2m"
#define ANSI_RGB(r,g,b) ANSI_ESC "38;2;" #r ";" #g ";" #b "m"

/// @file jamanak.hpp
/// @brief Lightweight scope-based profiler with epoch averaging and ANSI terminal output.

namespace jamanak {

/// @brief Profiler state: actively measuring or idle.
enum State { jamming, idle };

/// @brief Time unit (currently unused externally, reserved for future API).
enum Unit { milli, nano, sec };

/// @brief A single named timing measurement.
struct Jam {
    std::string context;                                   ///< Label for this measurement.
    std::chrono::_V2::system_clock::time_point t0{};      ///< Start timestamp.
    std::chrono::_V2::system_clock::time_point t1{};      ///< End timestamp.
    double duration_ms;                                    ///< Elapsed time in milliseconds.
};

/// @brief Main profiler class. Collects named Jam measurements and supports epoch averaging.
class Jamanak {

private:
    std::string global_context{"default"};   ///< Label shown in the report header.
    std::vector<Jam> jams;                   ///< Jams recorded in the current epoch.
    std::vector<std::vector<Jam>> epochs;    ///< Completed epochs for averaging.
    std::shared_ptr<Jam> current_jam;        ///< The currently active Jam, if any.
    State jam_state{State::idle};            ///< Whether a measurement is in progress.
    size_t longest{0};                       ///< Longest context label (for alignment).
    size_t longest_dur{0};                   ///< Longest duration string (for alignment).
    size_t longest_bc{0};                    ///< Digits before decimal point (for alignment).

    /// @brief Returns @p n repetitions of the string @p f.
    std::string fence(const int n, const std::string f) {
        std::ostringstream out;
        for (size_t i = 0; i < n; i++) { out << f; }
        return out.str();
    }

    /// @brief Returns the number of characters before the decimal point in @p num.
    size_t get_shift(const std::string num) {
        auto idx = num.find(".");
        auto s = num.substr(0, idx);
        return s.size();
    }

public:
    /// @brief Constructs a profiler with the given report header label.
    /// @param context Global label shown at the top of every report.
    Jamanak(const std::string context) : global_context(context) {  }

    /// @brief Starts a new measurement. Throws if already jamming.
    /// @param context Label for this measurement.
    void start(const std::string& context) {
        if (jam_state == State::jamming) throw std::runtime_error("already jamming");

        current_jam = std::make_shared<Jam>();
        current_jam->context = context;
        current_jam->t0 = std::chrono::high_resolution_clock::now();
        jam_state = State::jamming;
    }

    /// @brief Stops the current measurement, records it, and returns it.
    /// @return Shared pointer to the completed Jam.
    /// @throws std::runtime_error if no measurement is active.
    std::shared_ptr<Jam> end() {
        if (jam_state == State::idle || !current_jam) throw std::runtime_error("jamming not started");

        current_jam->t1 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> ms_double = current_jam->t1 - current_jam->t0;
        current_jam->duration_ms = ms_double.count();

        jams.emplace_back(*current_jam);
        longest = std::max(longest, current_jam->context.size());

        std::ostringstream ss;
        ss << std::fixed << std::setprecision(5) << current_jam->duration_ms;
        std::string s = ss.str();
        longest_dur = std::max(longest_dur, s.size());
        longest_bc = std::max(longest_bc, get_shift(s));

        auto ret = current_jam;
        current_jam.reset();
        jam_state = State::idle;

        return ret;
    }

    /// @brief Clears current jams to start a fresh epoch without saving the previous one.
    /// @throws std::runtime_error if a measurement is in progress.
    void begin_epoch() {
        if (jam_state == State::jamming) throw std::runtime_error("cannot begin epoch while jamming");
        clean_jams();
    }

    /// @brief Saves the current jams as a completed epoch, then clears them.
    /// @throws std::runtime_error if a measurement is in progress.
    void end_epoch() {
        if (jam_state == State::jamming) throw std::runtime_error("cannot end epoch while jamming");
        if (!jams.empty()) {
            epochs.emplace_back(jams);
        }
        clean_jams();
    }

    /// @brief Discards the current jams without saving them as an epoch.
    /// @throws std::runtime_error if a measurement is in progress.
    void cancel_epoch() {
        if (jam_state == State::jamming) throw std::runtime_error("cannot cancel epoch while jamming");
        clean_jams();
    }

    /// @brief Clears all saved epochs and current jams.
    void clean_epochs() {
        epochs.clear();
        clean_jams();
    }

    /// @brief Returns the number of completed epochs.
    size_t epoch_count() const { return epochs.size(); }

    /// @brief Computes per-label average durations across all epochs.
    /// @return Vector of Jams with averaged `duration_ms`; empty if no epochs exist.
    /// @note Assumes every epoch contains the same number of jams in the same order.
    std::vector<Jam> epoch_averages() const {
        if (epochs.empty()) return {};

        const size_t jam_count = epochs[0].size();
        const double n = static_cast<double>(epochs.size());
        std::vector<Jam> avgs(jam_count);

        for (size_t i = 0; i < jam_count; ++i) {
            avgs[i].context = epochs[0][i].context;
            double sum = 0.0;
            for (const auto& ep : epochs) sum += ep[i].duration_ms;
            avgs[i].duration_ms = sum / n;
        }

        return avgs;
    }

    /// @brief Clears all jams in the current (unsaved) epoch.
    void clean_jams() { jams.clear(); }

    /// @brief Returns a copy of all jams in the current epoch.
    std::vector<Jam> get_jams() const { return jams; }

    /// @brief Returns true if a measurement is currently in progress.
    bool is_jamming() const { return jam_state == State::jamming; }

    /// @brief Renders a formatted ANSI report of all jams in the current epoch.
    /// @return Multi-line string with timing table and total.
    std::string to_string() {
        std::ostringstream out;
        size_t l_size = std::max(longest + longest_dur, global_context.size()) + 13;
        size_t sf_size = static_cast<size_t>(l_size / 2) - static_cast<size_t>(global_context.size() / 2);
        size_t j_context_size{0};

        auto old_flags = out.flags();
        auto old_prec  = out.precision();

        out << ANSI_BOLD << ANSI_RGB(227,225,127);
        out << fence(l_size, "–") << "\n";
        out << fence(sf_size, " ");
        out << global_context.c_str();
        out << "\n";
        out << fence(l_size, "–") << "\n";

        double total = 0.0;
        for (const auto& j : jams) {
            j_context_size = j.context.size();
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(5) << j.duration_ms;
            std::string s = ss.str();

            out << ANSI_BOLD << ANSI_RGB(227,225,127) << "|| " << ANSI_RESET;
            out << ANSI_BOLD << ANSI_RGB(143,227,125) << j.context.c_str() << ANSI_RESET;
            out << fence(longest - j_context_size + 2, "–") << ": ";
            out << ANSI_BOLD << ANSI_RGB(143,227,125);
            out << fence(longest_bc - get_shift(s), " ") << s << ANSI_RESET;
            out << " ms";
            out << ANSI_BOLD << ANSI_RGB(227,225,127) << " ||" << ANSI_RESET << "\n";

            total += j.duration_ms;
        }

        std::ostringstream tot_ss;
        tot_ss << std::fixed << std::setprecision(5) << total;
        std::string tot_str = tot_ss.str();
        size_t safe_bc = std::max(longest_bc, get_shift(tot_str));

        out << ANSI_BOLD << ANSI_RGB(227,225,127);
        out << fence(l_size, "–") << "\n";
        out << "|| total ──: ";
        out << ANSI_RGB(143,227,125);
        out << fence(safe_bc - get_shift(tot_str), " ") << tot_str << ANSI_RESET;
        out << " ms";
        out << ANSI_BOLD << ANSI_RGB(227,225,127) << "\n";
        out << fence(l_size, "–") << ANSI_RESET << "\n";

        out.flags(old_flags);
        out.precision(old_prec);

        return out.str();
    }

    /// @brief Renders a formatted ANSI report of epoch averages, including percentage breakdown.
    /// @return Multi-line string with averaged timing table and total; empty string if no epochs.
    std::string to_string_epochs() {
        if (epochs.empty()) return "";

        auto avgs = epoch_averages();
        double total = 0.0;
        for (const auto& a : avgs) total += a.duration_ms;

        size_t l_ctx{0}, l_bc{0}, l_dur{0};
        std::vector<std::string> dur_strs;

        for (const auto& a : avgs) {
            l_ctx = std::max(l_ctx, a.context.size());
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(5) << a.duration_ms;
            std::string s = ss.str();
            dur_strs.push_back(s);
            l_dur = std::max(l_dur, s.size());
            l_bc  = std::max(l_bc,  get_shift(s));
        }

        std::ostringstream tot_ss;
        tot_ss << std::fixed << std::setprecision(5) << total;
        std::string tot_str = tot_ss.str();
        l_bc = std::max(l_bc, get_shift(tot_str));

        std::string hdr = global_context + "  [" + std::to_string(epochs.size()) + " epochs]";
        size_t l_size = std::max(l_ctx + l_dur, hdr.size()) + 25;
        size_t sf_size = l_size / 2;
        if (hdr.size() / 2 < sf_size) sf_size -= hdr.size() / 2;
        else sf_size = 0;

        std::ostringstream out;
        auto old_flags = out.flags();
        auto old_prec  = out.precision();

        out << ANSI_BOLD << ANSI_RGB(227,225,127);
        out << fence(l_size, "–") << "\n";
        out << fence(sf_size, " ") << hdr << "\n";
        out << fence(l_size, "–") << "\n";

        for (size_t i = 0; i < avgs.size(); ++i) {
            const auto& a   = avgs[i];
            const auto& s   = dur_strs[i];
            double pct      = total > 0.0 ? (a.duration_ms / total * 100.0) : 0.0;

            std::ostringstream pct_ss;
            pct_ss << std::fixed << std::setprecision(1) << pct;

            out << ANSI_BOLD << ANSI_RGB(227,225,127) << "|| " << ANSI_RESET;
            out << ANSI_BOLD << ANSI_RGB(143,227,125) << a.context.c_str() << ANSI_RESET;
            out << fence(l_ctx - a.context.size() + 2, "–") << ": ";
            out << ANSI_BOLD << ANSI_RGB(143,227,125);
            out << fence(l_bc - get_shift(s), " ") << s << ANSI_RESET;
            out << " ms  ";
            out << ANSI_DIM << "(" << pct_ss.str() << "%)" << ANSI_RESET;
            out << ANSI_BOLD << ANSI_RGB(227,225,127) << " ||" << ANSI_RESET << "\n";
        }

        out << ANSI_BOLD << ANSI_RGB(227,225,127);
        out << fence(l_size, "–") << "\n";
        out << "|| total ──: ";
        out << ANSI_RGB(143,227,125);
        out << fence(l_bc - get_shift(tot_str), " ") << tot_str << ANSI_RESET;
        out << " ms";
        out << ANSI_BOLD << ANSI_RGB(227,225,127) << "\n";
        out << fence(l_size, "–") << ANSI_RESET << "\n";

        out.flags(old_flags);
        out.precision(old_prec);

        return out.str();
    }

};

} // namespace jamanak