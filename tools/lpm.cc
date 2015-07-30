#include <iostream>
#include <serial.h>
#include <boost/program_options.hpp>
#include "lpm.h"


int main(int argc, char **argv) {

    std::string device;
    char in2put;
    std::string input2;

    namespace po = boost::program_options;

    po::options_description opts("LED Pseudo Monochromatic Command Line tool");
    opts.add_options()
            ("help",    "Some help stuff")
            ("device", po::value<std::string>(&device), "device file for aurdrino")
            ("input", po::value<std::string>(&input2), "send r/g/b");

    std::stringstream ss;
    ss << input2 << "\n";
    input2 = ss.str();

    std::cout << "setting input2:" << input2 << std::endl;

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(opts).run(), vm);
    po::notify(vm);

    if(vm.count("help")) {
        std::cout << opts << std::endl;
    } else if (vm.count("device")) {
        std::cout << "Opening device: " << vm["device"].as<std::string>() << std::endl;

        device::serial a = device::arduino::open(device);

        std::cout << "val of fd: " << a.printfd() << std::endl;

        std:: cout << "input: " << input2 << std::endl;

        a.send_data(input2);
        //write(a.printfd(),  &input,  1);

        char output;
        int index = 0;
        std::cout  << "from arduino: ";

        while(1) {
            std::string data = a.recv_line(1000);
            std::cout << data << std::endl;

            if(data == std::string("*")) {
                break;
            }
        }
    } else {
        std::cout << "Not Enough Arguments. ./lpm --device <device file> --input <input>" << std::endl;
    }
    return 0;
}