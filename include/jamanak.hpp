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

namespace jamanak {

enum State {
    jamming,
    idle,
};

enum Unit {
    milli,
    nano,
    sec,
};

struct Jam {
    std::string context;
    std::chrono::_V2::system_clock::time_point t0{};
    std::chrono::_V2::system_clock::time_point t1{};
    double duration_ms;
};

class Jamanak {

private:
    std::string global_context{"default"};
    std::vector<std::shared_ptr<Jam>> jams;
    std::shared_ptr<Jam> current_jam;
    State jam_state{State::idle};
    size_t longest{0};
    size_t longest_dur{0};
    size_t longest_bc{0};

    std::string fence(const int n, const std::string f) {
        std::ostringstream out;
        for (size_t i = 0; i < n; i++) { out << f; } 
        return out.str();
    }

    size_t get_shift(const std::string num) {
        auto idx = num.find(".");
        auto s = num.substr(0, idx);
        return s.size();
    }

public:
    Jamanak(const std::string context) : global_context(context) {  }

    void start (const std::string& context) {
        if (jam_state == State::jamming) throw std::runtime_error("already jamming");

        current_jam = std::make_shared<Jam>();
        current_jam->context = context;
        current_jam->t0 = std::chrono::high_resolution_clock::now();
        jam_state = State::jamming;
    } 

    std::shared_ptr<Jam>  end () {  
        if (jam_state == State::idle || !current_jam) throw std::runtime_error("jamming not started");

        current_jam->t1 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> ms_double = current_jam->t1-current_jam->t0;
        current_jam->duration_ms = ms_double.count();

        jams.emplace_back(current_jam);
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

    void clean_jams() {
        for (auto& j: jams) {
            j.reset();
        }
        jams.clear();
    };


    const std::vector<std::shared_ptr<Jam>>& get_jams() const {
        return jams;
    }

    bool is_jamming() const {
        return jam_state == State::jamming;
    }

    std::string to_string() {
        std::ostringstream out;
        std::ostringstream dur;
        size_t l_size = std::max(longest+longest_dur, global_context.size())+13;
        size_t sf_size = static_cast<size_t>(l_size/2) - static_cast<size_t>(global_context.size()/2);
        size_t j_context_size{0}, j_dur_size{0};

        auto old_flags = out.flags();
        auto old_prec  = out.precision();

        out << ANSI_BOLD << ANSI_RGB(227,225,127);
        out << fence(l_size, "–") << "\n"; 
        out << fence(sf_size," ");
        out << global_context.c_str();
        out << "\n"; 
        out << fence(l_size, "–") << "\n"; ; 

        for (const auto& j : jams) {
            
            j_context_size = j->context.size();
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(5) << j->duration_ms;
            std::string s = ss.str();
            j_dur_size = s.size();  

            out << ANSI_BOLD << ANSI_RGB(227,225,127) << "|| " << ANSI_RESET;
            out << ANSI_BOLD << ANSI_RGB(143,227,125) << j->context.c_str() << ANSI_RESET;
            out << fence(longest-j_context_size+2, "–") << ": ";
            out << ANSI_BOLD << ANSI_RGB(143,227,125);
            out << fence(longest_bc-get_shift(s), " ") << s << ANSI_RESET;
            out << " ms";
            out << ANSI_BOLD << ANSI_RGB(227,225,127) << " ||" << ANSI_RESET << "\n";

        }

        out << ANSI_BOLD << ANSI_RGB(227,225,127);
        out << fence(l_size, "–") << ANSI_RESET << "\n"; 


        out.flags(old_flags);
        out.precision(old_prec);

        return out.str();
    }
    
};
        
};