/*
 * LED Pseudo Monochromator tool for communicating with Arduino that itself
 * has firmware for controlling the attached LEDs; supports different commands
 */

#include <iostream>
#include <serial.h>
#include <boost/program_options.hpp>
#include <data.h>
#include <lpm.h>

// Supported commands:
// info         (for getting information of all LED pins from arduino
// reset        (for resetting and turning off all LEDs that are on)
// pwm 10,2000  (Turning on one LED on particular pin with pwm)
// shoot        (For triggering IR LED to take picture from camera)

int main(int argc, char **argv) {
    namespace po = boost::program_options;

    std::string device;
    std::string input;

    po::options_description opts("LED Pseudo Monochromatic Command Line tool");
    opts.add_options()
            ("help",    "Flag for Help")
            ("device", po::value<std::string>(&device), "device file for aurdrino")
            ("input", po::value<std::string>(&input), "Specify command (info, pwm, reset, shoot)");

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(opts).run(), vm);
    po::notify(vm);

    if(vm.count("help")) {
        //show program options
        std::cout << opts << std::endl;
        std::cout << "Supported Commands:" << std::endl << std::endl;

        std::cout << "info         (for getting information of all LED pins from arduino" << std::endl;
        std::cout << "reset        (for resetting and turning off all LEDs that are on)" << std::endl;
        std::cout << "pwm 10,2000  (Turning on one LED on particular pin with pwm)" << std::endl;
        std::cout << "shoot        (For triggering IR LED to take picture from camera)" << std::endl;

        std::cout << std::endl;

    } else if (vm.count("device")) {

        std::cout << "Opening device: " << vm["device"].as<std::string>() << std::endl;

        device::lpm lpm = device::lpm::open(device);

        std::cout << "val of fd: " << lpm.io.printfd() << std::endl;
        std::cout << "user input: " << input << std::endl;

        if(lpm.isCommandPWM(input) == 0) {
            // handle PWM
            lpm.setPWM(input);

        } else if(lpm.isCommandInfo(input) == 0) {
            //handle info
            lpm.getInfo();

        } else if(lpm.isCommandReset(input) == 0) {
            //handle reset
            lpm.reset();

        } else if(lpm.isCommandShoot(input) == 0) {
            //handle shoot img
            lpm.shoot();

        } else {
            std::cout <<"Error: Incorrect command! " << std::endl;
            return -1;
        }

        std::cout  << "from arduino: " << std::endl;
        lpm.receiveArduinoOutput();    //print stream from arduino


    } else {
        std::cout << "Not Enough Arguments. ./lpm --device <device file> --input <input>" << std::endl;
    }

    return 0;
}