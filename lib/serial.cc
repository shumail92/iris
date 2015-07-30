
#include <serial.h>
#include <stddef.h>

namespace device {

serial serial::open(const std::string &str) {
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
    tio.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ISIG | IEXTEN);
    tio.c_oflag &= ~OPOST;

    tio.c_iflag &= ~(INLCR | IGNCR | ICRNL | IGNBRK);

    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 10;

    res = tcsetattr(fd, TCSANOW, &tio);
    if (res < 0) {
        throw std::runtime_error("Could not set tc attrs");
    }

    return serial(fd);
}


void serial::send_data(const std::string &str) {
    const char *data = str.c_str();
    const size_t size = str.size();
    size_t pos = 0;

    //lets say we sleep 10ms per char
    sleeper timeout(static_cast<sleeper::rep>(size) * 10, 1);

    while (pos < size) {

        ssize_t n = write(fd, data + pos, 1);
        if (n < 0) {
            throw std::runtime_error("w failed: io error");
        }

        pos += static_cast<size_t>(n);
        if (!timeout.sleep(n == 0)) {
            throw std::runtime_error("w failed: timeout");
        }
    }

    char cr[1] = {'\r'};

    for (; ;) {
        ssize_t n = write(fd, cr, 1);
        if (n == 1) {
            break;
        }

        if (!timeout.sleep(n == 0)) {
            throw std::runtime_error("w failed: timeout");

        }
    }

    drain_retry:
    int fres =  tcdrain(fd);
    if (fres < 0) {
        if (errno == EINTR) {
            goto drain_retry;
        }
    }
}


std::vector<char> serial::recv_data(size_t to_read, sleeper::rep read_timeout) {
    std::vector<char> buf(to_read + 1, 0);

    size_t pos = 0;

    sleeper timeout(read_timeout, 10);

    while (pos < to_read) {
        ssize_t nread = read(fd, buf.data() + pos, to_read - pos);
        if (nread < 0) {
            if (errno == EAGAIN || errno == EINTR) {
                nread = 0;
            } else {
                throw std::runtime_error("r failed [recv_data]");
            }
        }

        pos += static_cast<size_t>(nread);

        if (!timeout.sleep(nread == 0)) {
            throw std::runtime_error("w failed: timeout");
        }
    }

    return buf;
}


std::string serial::recv_line(sleeper::rep read_timeout) {
    sleeper timeout(read_timeout, 1);

    std::vector<char> buf;

    char ch = '\0';

    do {
        ssize_t nread = read(fd, &ch, 1);
        if (nread < 0) {
            if (errno == EAGAIN || errno == EINTR) {
                nread = 0;
            } else {
                throw std::runtime_error("r failed [recv_line]");
            }
        }

        if (nread > 0 && ch != '\r' && ch != '\n') {
            buf.push_back(ch);
        }

        if (!timeout.maybe_sleep(nread == 0)) {
            throw std::runtime_error("r failed: timeout");
        }

    } while (ch != '\n');

    return std::string(buf.data(), buf.size());
}


void serial::eatup() {
    char buff[255];
    while ((read(fd, buff, sizeof(buff))) > 0) {
        wait(10);
    }
}

} //namespace device