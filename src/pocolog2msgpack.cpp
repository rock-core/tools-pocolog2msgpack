#include "Converter.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <boost/program_options.hpp>


namespace po = boost::program_options;


int main(int argc, char *argv[])
{
    po::options_description desc("Options");
    desc.add_options()
        ("help,h", "Print help message")
        ("verbose,v", boost::program_options::value<int>()->default_value(0),
            "Verbosity level")
        ("logfile,l",
            boost::program_options::value<std::vector<std::string> >()->multitoken(),
            "Logfiles")
        ("output,o", boost::program_options::value<std::string>()->default_value("output.msg"),
            "Output file")
        ("size,s", boost::program_options::value<int>()->default_value(8),
            "Length of the size type. This should be 8 for most machines, "
            "but it can be 1, e.g. on robots.")
        ("container-limit,c", boost::program_options::value<int>()->default_value(10000),
            "Maximum lenght of a container that will be converted. This option "
            "should only be used if you have old logfiles from which we can't "
            "read the container size properly.")
        ("only", boost::program_options::value<std::string>()->default_value(""),
            "Only convert the port given by this argument.")
        ("start", boost::program_options::value<int>()->default_value(0),
            "Index of the first sample that will be exported. This option "
            "is only valid if only one stream will be exported.")
        ("end", boost::program_options::value<int>()->default_value(-1),
            "Index after the last sample that will be exported. This option "
            "is only valid if only one stream will be exported.")
    ;
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if(vm.count("help"))
    {
        std::cout << desc << std::endl;
        return EXIT_FAILURE;
    }

    if(!vm.count("logfile"))
    {
        std::cerr << "[pocolog2msgpack] No logfile given" << std::endl;
        std::cout << desc << std::endl;
        return EXIT_FAILURE;
    }

    const int verbose = vm["verbose"].as<int>();
    const std::vector<std::string> logfiles = vm["logfile"].as<std::vector<std::string> >();
    const std::string output = vm["output"].as<std::string>();
    const int size = vm["size"].as<int>();
    const int containerLimit = vm["container-limit"].as<int>();
    const std::string only = vm["only"].as<std::string>();
    const int start = vm["start"].as<int>();
    const int end = vm["end"].as<int>();

    if(verbose >= 1)
    {
        std::cout << "[pocolog2msgpack] Verbosity level is "
            << verbose << "." << std::endl;
    }

    if(only != "")
        std::cout << "[pocolog2msgpack] Only converting port '" << only
            << "'." << std::endl;

    if(end >= 0 && start > end)
    {
        std::cerr << "[pocolog2msgpack] start = " << start
            << " after end = " << end << std::endl;
        return EXIT_FAILURE;
    }

    return convert(logfiles, output, size, containerLimit, only, start, end,
                   verbose);
}
