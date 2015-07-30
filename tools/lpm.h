#include <serial.h>


namespace device {

    class arduino {

    public:

        static device::serial open(const std::string &path) {
            return serial::open(path);
        }

        void sendToArduino(const std::string &cmd) {
            io.send_data(cmd);
        }

        void recvFromArduino() {
            std::string line = io.recv_line();
            std::cout << line <<std::endl;
        }

    private:
        serial io;

    };
}