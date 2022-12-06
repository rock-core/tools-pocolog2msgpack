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
        ("container-limit,c", boost::program_options::value<int>()->default_value(1000000),
            "Maximum length of a container that will be read and converted. "
            "This option should only be used for debugging purposes to prevent "
            "conversion of large containers. It might not result in a correctly "
            "converted logfile.")
        ("only", boost::program_options::value<std::string>()->default_value(""),
            "Only convert the port given by this argument.")
        ("start", boost::program_options::value<int>()->default_value(0),
            "Index of the first sample that will be exported. This option "
            "is only useful if only one stream will be exported.")
        ("end", boost::program_options::value<int>()->default_value(-1),
            "Index after the last sample that will be exported. This option "
            "is only useful if only one stream will be exported.")
        ("align_named_vector",
            "For named vector types, order the elements by the names of the first sample. "
            "Only one names vector valid for all samples will be included in the output. "
            "Only works with --flatten.")
        ("flatten",
            "With --flatten, all nested field names are joined into top level keys. "
            "This makes the data fields directly accessible.")
        ("compress", "Compress the output using msgpack's zlib compression.")
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
    const int containerLimit = vm["container-limit"].as<int>();
    const std::string only = vm["only"].as<std::string>();
    const int start = vm["start"].as<int>();
    const int end = vm["end"].as<int>();
    const bool align_named_vector = vm.count("align_named_vector");
    const bool flatten = vm.count("flatten");
    const bool compress = vm.count("compress");
    
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

    if(align_named_vector && !flatten)
    {
        std::cerr << "[pocolog2msgpack] align_named_vector only supported in flatten mode." << std::endl;
        return EXIT_FAILURE;
    }
    
    
    return convert(logfiles, output, containerLimit, only, start, end, flatten, align_named_vector, compress, verbose);
}
