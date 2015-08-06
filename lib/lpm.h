#include <serial.h>


namespace device {


    class lpm {

    public:

        device::serial io;

        lpm(const device::serial &io) : io(io) { }

        static lpm open(const std::string &path) {
            lpm lpm(serial::open(path));
            return lpm;
        }

        void setPWM(std::string cmd) {
            io.send_data(cmd);
        }

        void getInfo() {
            io.send_data("info");
        }

        void reset() {
            io.send_data("reset");
        }

        void shoot() {
            io.send_data("shoot");
        }

        int isCommandPWM(std::string cmd) {
            std::string pwmCmd = "pwm";
            return cmd.compare(0, pwmCmd.length(), pwmCmd);
        }

        int isCommandInfo(std::string cmd) {
            std::string infoCmd = "info";
            return cmd.compare(0, infoCmd.length(), infoCmd);
        }

        int isCommandReset(std::string cmd) {
            std::string resetCmd = "reset";
            return cmd.compare(0, resetCmd.length(), resetCmd);
        }

        int isCommandShoot(std::string cmd) {
            std::string shootCmd = "shoot";
            return cmd.compare(0, shootCmd.length(), shootCmd);
        }

        void receiveArduinoOutput() {

            while(1) {
                std::string data = io.recv_line(1000);
                std::cout << data << std::endl;

                if(data == std::string("*")) {
                    break;
                }
            }
        }

    };


}