#include <iostream>
#include <serial.h>
#include <boost/thread.hpp>
#include <boost/program_options.hpp>

int commandHelper(std::string const& cmd) {
    return system(cmd.c_str());
}

int main(int argc, char **argv) {

    namespace po = boost::program_options;

    std::string arduinoDevFile;
    std::string pr655DevFile;
    std::string input;

    po::options_description opts("IRIS LED Photo Spectrum Tool");
    opts.add_options()
            ("help",    "Some help stuff")
            ("arduino", po::value<std::string>(&arduinoDevFile), "Device file for Aurdrino")
            ("pr655", po::value<std::string>(&pr655DevFile), "Device file for pr655 Spectrometer")
            ("input", po::value<std::string>(&input), "send <pin no>,<PWM>");

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(opts).run(), vm);
    po::notify(vm);

    if(vm.count("help")) {
        std::cout << opts << std::endl;
        return 0;

    } else if(vm.count("arduino") && vm.count("pr655") && vm.count("input")) {

        std::cout << "Hello IRIS: " << std::endl;
        std::cout << "main waiting for threads" << std::endl;

        /*
         * Turn on LED
         */
        std::string cmd = "";
        cmd = "./lpm --device " + arduinoDevFile + " --input \"pwm " + input + "\"" ;
        boost::thread lpmThread(commandHelper, cmd);
        sleep(1);
        lpmThread.join();

        /*
         * Take the picture     (change command after implementing iris camera)
         */
        cmd = "./lpm --device " + arduinoDevFile + " --input \"pwm " + "5,4096" + "\"" ;
        boost::thread photographThread(commandHelper, cmd);
        sleep(1);
        photographThread.join();

        /*
         * Take the Spectrum    (change command for pr655)
         */
        cmd = "./lpm --device " + arduinoDevFile + " --input \"pwm " + "1,4096" + "\"" ;
        boost::thread pr655Thread(commandHelper, cmd);
        sleep(1);
        pr655Thread.join();

        /*
         * Reset the LED
         */
        cmd = "./lpm --device " + arduinoDevFile + " --input \"reset" + "\"" ;
        boost::thread lpmResetThread(commandHelper, cmd);
        sleep(1);
        lpmResetThread.join();

        std::cout << "All Threads returned -- Main done" << std::endl;

    } else {
        std::cout << "Not Enough Arguments. call --help for help" << std::endl;
    }

    return 0;
}