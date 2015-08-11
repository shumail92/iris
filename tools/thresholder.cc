#include <iostream>
#include <fstream>
#include <serial.h>
#include <math.h>
#include <boost/program_options.hpp>
#include <data.h>

#include <lpm.h>
#include <pr655.h>

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

    } else if(vm.count("arduino") && vm.count("pr655")) {

        std::cout << "Opening Arduino Device File" << std::endl;
        device::lpm lpm = device::lpm::open(arduinoDevFile);    //connect to arduino

        std::cout << "Opening pr655 Device File" << std::endl;
        device::pr655 meter = device::pr655::open(pr655DevFile);  //connect to pr655

        /*
         * build the <LED Pin, Wavelength> map from YAML Config file
         */
        iris::data::store store = iris::data::store::default_store();
        const std::map<uint8_t, uint16_t> ledMap = store.lpm_leds();

        std::cout << "Traversing the map; generated from LED PIN & Wavelength YAML Config File: " << std::endl;
        for(auto elem : ledMap)    {
            std::cout << "pin: " << unsigned(elem.first) << "  --  Wavelength: " << unsigned(elem.second) << "nm \n";
        }

        std::cout << std::endl << "Starting Process for all available LEDs..." << std::endl << std::endl;

        std::map<uint16_t, spectral_data> spectrumData;

        std::map<uint16_t, uint16_t> led_pin_pwm;

        float threshold = 0.000025;
        bool thresholdFlag = false;

        int pwmDecrementStepSize = 500;
        uint16_t previousPWMVal = 4096;  //initially 4096

        while(!thresholdFlag) {

            for(auto elem : ledMap)    {

                std::cout << "$: Turning on " << unsigned(elem.second) << "nm LED on pin " << unsigned(elem.first) << std::endl;

                bool currentLedThresholdFlag = false;

                while(!currentLedThresholdFlag) {

                    std::string pwmCmd = "pwm " + std::to_string(unsigned(elem.first)) + "," + std::to_string(previousPWMVal) + "";

                    std::cout << "executing: " << pwmCmd << std::endl;

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
                        meter.measure();
                        device::pr655::cfg config = meter.config();
                        spectral_data data = meter.spectral();

                        std::cout << "1" << std::endl;

                        std::vector<float> spectraValues;

                        std::cout << "2" << std::endl;

                        for (size_t i = 0; i < data.data.size(); i++) {
                            std::vector<float>::iterator it = spectraValues.begin();
                            spectraValues.insert(it, data.data[i]);
                        }

                        std::cout << "3" << std::endl;

                        float maxima = *std::max_element(spectraValues.begin(), spectraValues.end());
                        std::cout << "Peak at: " << maxima << std::endl;
                        float diff = fabs(maxima - threshold);
                        std::cout << "Difference in Threshold and Peak: " << diff << std::endl;

                        if( diff  < 0.000005 || previousPWMVal < 1000) {        // it's ok now
                            currentLedThresholdFlag = true;
                            led_pin_pwm.insert(std::pair<uint16_t, uint16_t>(elem.second, previousPWMVal));
                            spectrumData.insert( std::pair<uint16_t , spectral_data>(elem.second, data) );
                            previousPWMVal = 4096;
                        } else {
                            previousPWMVal = previousPWMVal - pwmDecrementStepSize;
                        }
                        spectrumData.insert( std::pair<uint16_t , spectral_data>(elem.second, data) );

                    } catch (const std::exception &e) {
                        std::cerr << e.what() << std::endl;
                    }

                    meter.stop();
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

            thresholdFlag = true;   //all pwm values are good now
        }


        std::ofstream pwmout("data/pwm.txt");

        pwmout << "pwm:" << std::endl;

        for(auto elem : led_pin_pwm) {
            pwmout << "  " << unsigned(elem.first) << " : " << unsigned(elem.second) << std::endl;
        }
        pwmout.close();

        std::ofstream fout("spectral.txt");

        fout << "xx,";

        for(auto elem : spectrumData)    {
            spectral_data temp = elem.second;

            for (size_t i = 0; i < temp.data.size(); i++) {
                fout << temp.wl_start + i * temp.wl_step << ",";
            }
            break;
        }

        fout << std::endl;

        for(auto elem : spectrumData)    {
            fout << unsigned(elem.first) << ",";
            spectral_data temp = elem.second;

            for (size_t i = 0; i < temp.data.size(); i++) {
                fout << temp.data[i] << ",";
            }

            fout << std::endl;

        }

    } else {
        std::cout << "Not Enough Arguments. call --help for help" << std::endl;
    }

    return 0;
}