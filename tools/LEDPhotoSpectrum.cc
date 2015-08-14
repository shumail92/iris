/*
 * Tool for automating the process of turning on all available LEDs one by one and
 * measuring the spectrum of each LED and taking the picture of each using the camera.
 * -c & -s flags are added to skip spectrum measurement or capturing of photograph
 */

#include <iostream>
#include <fstream>
#include <serial.h>
#include <boost/program_options.hpp>
#include <data.h>
#include <lpm.h>
#include <pr655.h>

int main(int argc, char **argv) {

    namespace po = boost::program_options;

    std::string arduinoDevFile;
    std::string pr655DevFile;

    bool pictureFlag =  true;
    bool spectrumFlag =  true;

    device::pr655 meter;

    po::options_description opts("IRIS LED Photo Spectrum Tool");
    opts.add_options()
            ("help",    "Supported Arguments/Flags")
            ("arduino", po::value<std::string>(&arduinoDevFile), "Device file for Aurdrino")
            ("pr655", po::value<std::string>(&pr655DevFile), "Device file for pr655 Spectrometer")
            ("c",    "Specify this flag to skip capturing of photographs from camera")
            ("s",    "Specify this flag to skip measurement of spectrometer");

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(opts).run(), vm);
    po::notify(vm);

    if(vm.count("c")) {
        pictureFlag = false;
    }

    if(vm.count("s")) {
        spectrumFlag = false;
    }

    if(vm.count("help")) {
        std::cout << opts << std::endl;
        return 0;

    } else if(vm.count("arduino")) {

        std::cout << "Opening Arduino Device File" << std::endl;
        device::lpm lpm = device::lpm::open(arduinoDevFile);    //connect to arduino

        if(spectrumFlag) {

            if(vm.count("pr655")) {
                std::cout << "Opening pr655 Device File" << std::endl;
                meter = device::pr655::open(pr655DevFile);  //connect to pr655
            } else {
                std::cout << "Error: Specify pr655 device file" << std::endl;
                return -1;
            }
        }

        /*
         * build the <LED Pin, Wavelength> map from YAML Config file
         */
        iris::data::store store = iris::data::store::default_store();
        const std::map<uint8_t, uint16_t> ledMap = store.lpm_leds();

        /*
         * build the <LED wavelength,PWM> map from YAML Config file
         */
        const std::map<uint16_t, uint16_t> ledPwmMap = store.lpm_pwm();

        std::cout << "Traversing and printing the map; generated from LED PIN & Wavelength YAML Config File: " << std::endl;
        for(auto elem : ledMap)    {
            std::cout << "pin: " << unsigned(elem.first) << "  --  Wavelength: " << unsigned(elem.second) << "nm \n";
        }

        std::cout << std::endl << "Starting Process for all available LEDs..." << std::endl << std::endl;

        std::map<uint16_t, spectral_data> spectrumData;
        std::ofstream errorOut("data/error.txt");

        for(auto elem : ledMap)    {

            std::cout << "$: Turning on " << unsigned(elem.second) << "nm LED on pin " << unsigned(elem.first) << " with PWM: " << ledPwmMap.at(elem.second) <<std::endl;
            std::string pwmCmd = "pwm " + std::to_string(unsigned(elem.first)) + "," + std::to_string(ledPwmMap.at(elem.second)) + "";

            /*
             * Turn on LED
             */
            lpm.setPWM(pwmCmd);
            std::cout << "---------------------------------------" << std::endl;
            std::cout  << "From arduino after turning LED on: " << std::endl;
            lpm.receiveArduinoOutput();    //print stream from arduino
            std::cout << "---------------------------------------" << std::endl << std::endl << std::endl ;

            /*
             * Time delay of 1 sec
             */
            usleep(1000000);

            if(pictureFlag) {

                /*
                 * Take the picture
                 */
                std::cout << "$: Capturing Photograph from Camera" << std::endl;
                lpm.shoot();
                std::cout << "---------------------------------------" << std::endl;
                std::cout  << "From arduino after taking picture " << std::endl;
                lpm.receiveArduinoOutput();    //print stream from arduino
                std::cout << "---------------------------------------" << std::endl << std::endl << std::endl ;

                /*
                 * Time delay of 1 sec
                 */
                usleep(1000000);
            }


            if(spectrumFlag) {

                /*
                 * Measure the Spectrum
                 */
                std::cout << "$: Measuring the spectrum" << std::endl;

                std::string prefix = "# ";
                try {
                    bool could_start = meter.start();
                    if (! could_start) {
                        std::cerr << "Could not start remote mode" << std::endl;
                        meter.stop();
                        return -1;
                    }

                    meter.units(true);
                    bool could_measure = meter.measure();
                    device::pr655::cfg config = meter.config();
                    spectral_data data = meter.spectral();
                    if(could_measure) {
                        spectrumData.insert( std::pair<uint16_t , spectral_data>(elem.second, data) );
                    } else {
                        std::cout << ">>: Unable to measure spectrum of " << unsigned(elem.second) << "nm LED on pin " << unsigned(elem.first) << " with PWM: " << ledPwmMap.at(elem.second) <<std::endl;
                        errorOut << ">>: Unable to measure spectrum of " << unsigned(elem.second) << "nm LED on pin " << unsigned(elem.first) << " with PWM: " << ledPwmMap.at(elem.second) <<std::endl;
                    }

                } catch (const std::exception &e) {
                    std::cerr << e.what() << std::endl;
                }

                meter.stop();

                /*
                 * Time delay of 1 sec
                 */
                usleep(1000000);
            }

            /*
             * Reset the LED
             */
            std::cout << "$: Resetting the LED" << std::endl;
            lpm.reset();
            std::cout << "---------------------------------------" << std::endl;
            std::cout  << "From arduino after resetting" << std::endl;
            lpm.receiveArduinoOutput();    //print stream from arduino
            std::cout << "---------------------------------------" << std::endl << std::endl << std::endl ;

            /*
             * Time delay of 1 sec
             */
            usleep(1000000);
        }

        if(spectrumFlag) {

            std::ofstream fout("data/spectral.txt");

            fout << "led,";

            for(auto elem : spectrumData)    {
                spectral_data temp = elem.second;

                for (size_t i = 0; i < temp.data.size(); i++) {
                    fout << temp.wl_start + i * temp.wl_step ;

                    if(i != (temp.data.size() -1) ) {
                        fout << ",";
                    }
                }
                break;      // just to print top header line in dump file
            }

            fout << std::endl;

            for(auto elem : spectrumData)    {
                fout << unsigned(elem.first) << ",";
                spectral_data temp = elem.second;

                for (size_t i = 0; i < temp.data.size(); i++) {
                    fout << temp.data[i] ;

                    if(i != (temp.data.size() -1) ) {
                        fout << ",";
                    }
                }
                fout << std::endl;
            }

            /*
             * Closing Streams
             */
            errorOut.close();
            fout.close();
        }

    } else {
        std::cout << "Not Enough Arguments. call --help for help" << std::endl;
    }

    return 0;
}