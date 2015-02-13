#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <stdexcept>

namespace device {


class serial {

    serial(int fd) : fd(fd) { }

public:

    serial() : fd(-1) { }

    serial(const serial &other) {
        fd = dup(other.fd);
    }

    static serial open(const std::string &str) {
        int fd = ::open(str.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
        if (fd < 0) {
            throw std::runtime_error("Could not open device file");
        }

        struct termios tio;

        int res = tcgetattr(fd, &tio);

        if (res < 0) {
            throw std::runtime_error("Could not get tc attrs");
        }

        cfsetispeed(&tio, B9600);
        cfsetospeed(&tio, B9600);

        // Set 8n1
        tio.c_cflag &= ~PARENB;
        tio.c_cflag &= ~CSTOPB;
        tio.c_cflag &= ~CSIZE;
        tio.c_cflag |= CS8;

        // no flow control (flag unavailable under POSIX, so just hope...)
        tio.c_cflag &= ~CRTSCTS;

        // turn on READ & ignore ctrl lines
        tio.c_cflag |= CREAD | CLOCAL;

        // turn off s/w flow ctrl
        tio.c_iflag &= ~(IXON | IXOFF | IXANY);

        // make raw
        tio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
        tio.c_oflag &= ~OPOST;

        tio.c_cc[VMIN] = 0;
        tio.c_cc[VTIME] = 10;

        res = tcsetattr(fd, TCSANOW, &tio);
        if (res < 0) {
            throw std::runtime_error("Could not set tc attrs");
        }

        return serial(fd);
    }

    void send_data(const std::string &str) {
        const char *data = str.c_str();
        const size_t size = str.size();
        size_t pos = 0;

        while (pos < size) {

            ssize_t n = write(fd, data + pos, 1);
            if (n < 0) {
                std::cerr << "w failed" << std::endl;
            }

            pos += static_cast<size_t>(n);
            std::chrono::milliseconds dura(1);
            std::this_thread::sleep_for(dura);
        }

        char cr[1] = {'\r'};

        for (; ;) {
            ssize_t n = write(fd, cr, 1);
            if (n == 1) {
                break;
            }
        }
    };

    std::string readline(size_t to_read) {
        char buf[to_read + 1];
        std::fill_n(buf, to_read + 1, 0);

        size_t pos = 0;

        auto t_start = std::chrono::system_clock::now();

        while (pos < to_read) {
            ssize_t nread = read(fd, buf + pos, to_read - pos);
            if (nread < 0) {
                throw std::runtime_error("r failed");
            }

            pos += static_cast<size_t>(nread);

            std::chrono::milliseconds dura(1);
            std::this_thread::sleep_for(dura);

            auto now = std::chrono::system_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - t_start);
            if (elapsed.count() > 10) {
                std::cerr << "TIMEOUT: " << buf << std::endl;
                return "";
            }

        }

        return std::string(buf);
    }

    ~serial() {
        close(fd);
    }

    explicit operator bool() const {
        return fd > 0;
    }

private:
    int fd;
};

class pr655 {

    typedef unsigned long status;

public:
    pr655(const serial &port) : io(port) {}

    static pr655 open(const std::string &path) {
        serial s = serial::open(path);
        pr655 meter(s);



        return meter;
    }

    bool start() {
        io.send_data("PHOTO");

        std::chrono::milliseconds dura(1000);
        std::this_thread::sleep_for(dura);

        std::string l = io.readline(12);

        return true;
    }

    void stop() {
        io.send_data("Q");
    }

    std::string serial_number() {
        io.send_data("D110");
        std::chrono::milliseconds dura(200);
        std::this_thread::sleep_for(dura);

        std::string l = io.readline(16);
        return l;
    }

    std::string model_number() {

        io.send_data("D111");
        std::chrono::milliseconds dura(200);
        std::this_thread::sleep_for(dura);
        std::string l = io.readline(14);
        return l;
    }

    std::string software_version() {
        io.send_data("D114");
        std::chrono::milliseconds dura(200);
        std::this_thread::sleep_for(dura);
        std::string l = io.readline(13);
        return l;
    }

    void units(bool metric) {
        if (metric) {
            io.send_data("SU1");
        } else {
            io.send_data("SU0");
        }
        std::string l = io.readline(7);
    }

    void measure() {
        io.send_data("M5");

        std::string header = io.readline(39);
        std::cout << header << std::endl;

        //qqqqq,UUUU,w.wwwe+eee,i.iiie-ee,p.pppe+ee CRLF [16]
        for(uint32_t i = 0; i < 101; i++) {
            std::string line = io.readline(16);

            char *s_end = nullptr;
            unsigned long lambda = std::strtoul(line.c_str(), &s_end, 10);
            double ri = std::strtod(line.c_str() + 5, &s_end);
            std::cout << lambda << " | " << ri << std::endl;
        }

    }

    status parse_status(const std::string &data) {
        char *s_end;

        if (data.size() < 4) {
            std::invalid_argument("Cannot parse status code (< 4)");
        }

        unsigned long code = strtoul(data.c_str(), &s_end, 10);
        return code;
    }

private:
    serial io;
};


}

int main(int argc, char **argv) {

    device::pr655 meter = device::pr655::open(argv[1]);

    meter.start();

    std::string res = meter.serial_number();
    std::cerr << res << std::endl;

    res = meter.model_number();
    std::cerr << res << std::endl;

    meter.units(true);

    meter.measure();

    meter.stop();

    return 0;
}