#include <serial.h>


namespace device {


    device::serial io;

    class arduino {

    public:

        static device::serial open(const std::string &path) {
            return serial::open(path);
        }

    };


    class lpm {

    public:

        static void setPWM(std::string cmd) {
            device::io.send_data(cmd);
        }

        static void getInfo() {
            device::io.send_data("info");
        }

        static void reset() {
            device::io.send_data("reset");
        }

        static int isCommandPWM(std::string cmd) {
            std::string pwmCmd = "pwm";
            return cmd.compare(0, pwmCmd.length(), pwmCmd);
        }

        static int isCommandInfo(std::string cmd) {
            std::string infoCmd = "info";
            return cmd.compare(0, infoCmd.length(), infoCmd);
        }

        static int isCommandReset(std::string cmd) {
            std::string resetCmd = "reset";
            return cmd.compare(0, resetCmd.length(), resetCmd);
        }

        static void receiveArduinoOutput() {

            while(1) {
                std::string data = device::io.recv_line(1000);
                std::cout << data << std::endl;

                if(data == std::string("*")) {
                    break;
                }
            }
        }

    };


}