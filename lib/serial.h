#ifndef IRIS_SERIAL_H
#define IRIS_SERIAL_H

#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <stdexcept>

namespace device {

class sleeper {
public:
    typedef std::chrono::milliseconds dur_t;
    typedef dur_t::rep rep;

    sleeper(rep global, rep local) :
            night(global), nap(local), cur_nap(local) {
        start();
    }

    void start() {
        bedtime = std::chrono::system_clock::now();
    }

    bool sleep(bool sleep_longer) {

        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<dur_t>(now - bedtime); //in rep, i.e. ms

        const rep t_elapsed = elapsed.count();

        if (t_elapsed > night) {
            return false;
        }

        if (sleep_longer) {
            cur_nap *= 2;
        } else {
            reset_local();
        }

        auto the_nap = std::min(cur_nap, night - t_elapsed);

        dur_t timeout(the_nap);
        std::this_thread::sleep_for(timeout);

        return true;
    }

    bool maybe_sleep(bool should_sleep) {
        if (should_sleep) {
            return sleep(true);
        }

        reset_local();
        return true;
    }

    void reset_local() {
        cur_nap = nap;
    }

private:
    rep night;   // max sleeping until we can do, i.e. timeout
    rep nap;     // the starting consecutive amount of sleeping
    rep cur_nap; // the amount of consecutive sleeping we will do

    std::chrono::system_clock::time_point bedtime; // when the night started
};


class serial {

    serial(int fd) : fd(fd) { }

public:
    serial() : fd(-1) { }

    serial(const serial &other) {
        fd = dup(other.fd);
    }

    serial(serial &&other) {
        if (fd > -1) {
            close(fd);
        }
        fd = other.fd;
        other.fd = -1;
    }

    serial& operator=(const serial &other) {
        if (this == &other) {
            return *this;
        }

        close(fd);
        fd = dup(other.fd);
        return *this;
    }

    serial& operator=(serial &&other) {
        if (this == &other) {
            return *this;
        }

        if (fd > 0) {
            close(fd);
        }

        fd = other.fd;
        other.fd = -1;
        return *this;
    }

    static serial open(const std::string &str);

    void send_data(const std::string &str);

    std::vector<char> recv_data(size_t to_read, sleeper::rep read_timeout = 5000);

    std::string recv_line(sleeper::rep read_timeout = 5000);

    void eatup();

    ~serial() {
        close(fd);
    }

    explicit operator bool() const {
        return fd > 0;
    }

    inline void wait(sleeper::rep time_to_wait) {
        std::chrono::milliseconds dura(time_to_wait);
        std::this_thread::sleep_for(dura);
    }

    int printfd() {
        return fd;
    }

private:
    int fd;
};

} //namespace device

#endif //include guard