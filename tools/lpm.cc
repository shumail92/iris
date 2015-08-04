#include <iostream>
#include <serial.h>

#include <boost/program_options.hpp>
#include <data.h>

#include "lpm.h"

// info
// reset
// pwm 10,2000
// shoot

int main(int argc, char **argv) {
    namespace po = boost::program_options;

    std::string device;
    std::string input;

    po::options_description opts("LED Pseudo Monochromatic Command Line tool");
    opts.add_options()
            ("help",    "Some help stuff")
            ("device", po::value<std::string>(&device), "device file for aurdrino")
            ("input", po::value<std::string>(&input), "send r/g/b");

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(opts).run(), vm);
    po::notify(vm);

    if(vm.count("help")) {
        //show program options
        std::cout << opts << std::endl;

    } else if (vm.count("device")) {

//        iris::data::store store = iris::data::store::default_store();
//        store.lpm_leds();

        std::cout << "Opening device: " << vm["device"].as<std::string>() << std::endl;

        device::lpm lpm = device::lpm::open(device);

        std::cout << "val of fd: " << lpm.io.printfd() << std::endl;
        std::cout << "input: " << input << std::endl;

        if(lpm.isCommandPWM(input) == 0) {
            // handle PWM
            lpm.setPWM(input);

        } else if(lpm.isCommandInfo(input) == 0) {
            //handle info
            lpm.getInfo();

        } else if(lpm.isCommandReset(input) == 0) {
            //handle reset
            lpm.reset();

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