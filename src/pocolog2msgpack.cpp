#include <cstdlib>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <vector>
#include <list>
#include <string>
#include <stdexcept>
#include <iosfwd>
#include <msgpack.h>
#include <msgpack/fbuffer.h>
#include <pocolog_cpp/MultiFileIndex.hpp>
#include <pocolog_cpp/Stream.hpp>
#include <pocolog_cpp/InputDataStream.hpp>
#include <typelib/typemodel.hh>
#include <typelib/typevisitor.hh>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/join.hpp>


namespace po = boost::program_options;


/**
 * Actual converter logic.
 *
 * The converter loads a single type from its serialized typelib representation
 * and stores it in MessagePack format. The typelib type information must be
 * provided.
 */
class Converter : public Typelib::TypeVisitor
{
    /** Pointer to the serialized typelib type. */
    uint8_t* data;
    /** Size of the size type for containers in the logfile. */
    int size;
    /** Current memory offset of the converter, e.g. might point to a field of a compound. */
    size_t offset;
    /** Flag that is true while converting container types. */
    bool part;
    /** Stores the last integer. Is required to build strings from its characters. */
    unsigned int lastNumber;
    /** Stores the last floating point number. Is required to build arrays from its entries. */
    double lastFloat;
    /** Current field name of a compound type. */
    std::list<std::string> fieldName;
    /** All data will be put in this packer. */
    msgpack_packer& pk;
    /** Verbosity level. */
    int verbose;
    /** Current depth in the type hierarchy. */
    int depth;
    /** A constant that will be used to format debug output on stdout. */
    const int indentation;
protected:
    bool visit_(Typelib::OpaqueType const& type)
    {
        return true;
    }

    bool visit_(Typelib::Numeric const& type)
    {
        switch(type.getNumericCategory())
        {
        case Typelib::Numeric::SInt:
            lastNumber = getSignedInt(type, part);
            break;
        case Typelib::Numeric::UInt:
            lastNumber = getUnsignedInt(type, part);
            break;
        case Typelib::Numeric::Float:
            lastFloat = getFloat(type, part);
            break;
        default:
            throw std::runtime_error("unknown numeric type");
        }
        offset += type.getSize();
        return true;
    }

    unsigned int getUnsignedInt(Typelib::Numeric const& type, bool part)
    {
        if(!part && verbose >= 3 + depth)
            std::cout << "[pocolog2msgpack]"
                << std::setfill(' ') << std::setw(indentation + depth) << " "
                << "uint" << 8 * type.getSize();

        unsigned int i = 0;
        switch(type.getSize())
        {
        case 1:
            i = *reinterpret_cast<uint8_t*>(data + offset);
            if(!part)
                msgpack_pack_uint8(&pk, *reinterpret_cast<uint8_t*>(data + offset));
            break;
        case 2:
            i = *reinterpret_cast<uint16_t*>(data + offset);
            if(!part)
                msgpack_pack_uint16(&pk, *reinterpret_cast<uint16_t*>(data + offset));
            break;
        case 4:
            i = *reinterpret_cast<uint32_t*>(data + offset);
            if(!part)
                msgpack_pack_uint32(&pk, *reinterpret_cast<uint32_t*>(data + offset));
            break;
        case 8:
            i = *reinterpret_cast<uint64_t*>(data + offset);
            if(!part)
                msgpack_pack_uint64(&pk, *reinterpret_cast<uint64_t*>(data + offset));
            break;
        default:
            throw std::runtime_error(
                "unknown uint size: " +
                boost::lexical_cast<std::string>(type.getSize()));
        }

        if(!part && verbose >= 3 + depth)
        {
            if(verbose >= 4 + depth)
                std::cout << ": " << i;
            std::cout << std::endl;
        }

        return i;
    }

    signed int getSignedInt(Typelib::Numeric const& type, bool part)
    {
        if(!part && verbose >= 3 + depth)
            std::cout << "[pocolog2msgpack]"
                << std::setfill(' ') << std::setw(indentation + depth) << " "
                << "int" << 8 * type.getSize();

        signed int i = 0;
        switch(type.getSize())
        {
        case 1:
            i = *reinterpret_cast<int8_t*>(data + offset);
            if(!part)
                msgpack_pack_int8(&pk, *reinterpret_cast<int8_t*>(data + offset));
            break;
        case 2:
            i = *reinterpret_cast<int16_t*>(data + offset);
            if(!part)
                msgpack_pack_int16(&pk, *reinterpret_cast<int16_t*>(data + offset));
            break;
        case 4:
            i = *reinterpret_cast<int32_t*>(data + offset);
            if(!part)
                msgpack_pack_int32(&pk, *reinterpret_cast<int32_t*>(data + offset));
            break;
        case 8:
            i = *reinterpret_cast<int64_t*>(data + offset);
            if(!part)
                msgpack_pack_int64(&pk, *reinterpret_cast<int64_t*>(data + offset));
            break;
        default:
            throw std::runtime_error(
                "unknown sint size: " +
                boost::lexical_cast<std::string>(type.getSize()));
        }

        if(!part && verbose >= 3 + depth)
        {
            if(verbose >= 4 + depth)
                std::cout << ": " << i;
            std::cout << std::endl;
        }

        return i;
    }

    double getFloat(Typelib::Numeric const& type, bool part)
    {
        if(!part && verbose >= 3 + depth)
            std::cout << "[pocolog2msgpack]"
                << std::setfill(' ') << std::setw(indentation + depth) << " "
                << "float" << 8 * type.getSize();

        double i = NAN;
        switch(type.getSize())
        {
        case 4:
            i = *reinterpret_cast<float*>(data + offset);
            msgpack_pack_float(&pk, *reinterpret_cast<float*>(data + offset));
            break;
        case 8:
            i = *reinterpret_cast<double*>(data + offset);
            msgpack_pack_double(&pk, *reinterpret_cast<double*>(data + offset));
            break;
        default:
            throw std::runtime_error(
                "unknown float size: "
                + boost::lexical_cast<std::string>(type.getSize()));
        }

        if(!part && verbose >= 3 + depth)
        {
            if(verbose >= 4 + depth)
                std::cout << ": " << i;
            std::cout << std::endl;
        }

        return i;
    }

    bool visit_(Typelib::Enum const&)
    {
        if(verbose >= 3 + depth)
            std::cout << "[pocolog2msgpack]"
                << std::setfill(' ') << std::setw(indentation + depth) << " "
                << "enum" << std::endl;

        return true;
    }

    bool visit_(Typelib::Pointer const& type)
    {
        if(!part && verbose >= 3 + depth)
            std::cout << "[pocolog2msgpack]"
                << std::setfill(' ') << std::setw(indentation + depth) << " "
                << "pointer" << std::endl;

        depth += 1;
        TypeVisitor::visit_(type);
        depth -= 1;
        return true;
    }

    bool visit_(Typelib::Array const& type)
    {
        const size_t numElements = type.getDimension();
        double d[numElements];

        if(verbose >= 3 + depth)
            std::cout << "[pocolog2msgpack]"
                << std::setfill(' ') << std::setw(indentation + depth) << " "
                << "array[" << numElements << "]" << std::endl;

        msgpack_pack_array(&pk, numElements);
        depth += 1;
        for(size_t i = 0; i < numElements; i++)
        {
            visit_(type.getIndirection());
            d[i] = lastFloat;
        }
        depth -= 1;
        return true;
    }

    bool visit_(Typelib::Container const& type)
    {
        // TODO Is there a way to determine 'size' automatically?
        size_t numElements;
        if(size == 1)
            numElements = *reinterpret_cast<uint8_t*>(data + offset);
        else if(size == 2)
            numElements = *reinterpret_cast<uint16_t*>(data + offset);
        else if(size == 4)
            numElements = *reinterpret_cast<uint32_t*>(data + offset);
        else if(size == 8)
            numElements = *reinterpret_cast<uint64_t*>(data + offset);
        else
            throw std::runtime_error(
                "Unknown size type size, should be one of 1, 2, 4, 8");
        offset += size;

        if(verbose >= 3 + depth)
            std::cout << "[pocolog2msgpack]"
                << std::setfill(' ') << std::setw(indentation + depth) << " "
                << type.kind() << "[" << numElements << "]" << std::endl;

        if(type.kind() == "/std/string")
        {
            part = true;
            std::string s;
            depth += 1;
            for(size_t i = 0; i < numElements; i++)
            {
                visit_(type.getIndirection());
                s += (char) lastNumber;
            }
            part = false;
            depth -= 1;

            msgpack_pack_str(&pk, numElements);
            msgpack_pack_str_body(&pk, s.c_str(), numElements);
        }
        else if(type.kind() == "/std/vector")
        {
            msgpack_pack_array(&pk, numElements);

            depth += 1;
            for(size_t i = 0; i < numElements; i++)
                visit_(type.getIndirection());
            depth -= 1;
        }
        else
        {
            throw std::runtime_error("unknown container");
        }
        return true;
    }

    bool visit_(Typelib::Compound const& type)
    {
        if(verbose >= 3 + depth)
            std::cout << "[pocolog2msgpack]"
                << std::setfill(' ') << std::setw(indentation + depth) << " "
                << "compound '" << type.getName() << "'" << std::endl;

        msgpack_pack_map(&pk, type.getFields().size());
        depth += 1;
        TypeVisitor::visit_(type);
        depth -= 1;
        return true;
    }

    bool visit_(Typelib::Compound const& type, Typelib::Field const& field)
    {
        if(verbose >= 3 + depth)
            std::cout << "[pocolog2msgpack]"
                << std::setfill(' ') << std::setw(indentation + depth) << " "
                << "field '" << field.getName() << "'" << std::endl;

        msgpack_pack_str(&pk, field.getName().size());
        msgpack_pack_str_body(&pk, field.getName().c_str(), field.getName().size());

        fieldName.push_back(field.getName());
        depth += 1;
        TypeVisitor::visit_(type, field);
        depth -= 1;
        fieldName.pop_back();
        return true;
    }

    using TypeVisitor::visit_;
public:
    Converter(uint8_t* data, msgpack_packer& pk, int size, int verbose)
        : data(data), offset(0), part(false), pk(pk), size(size),
          verbose(verbose), depth(0), indentation(1)
    {
    }
    void apply(Typelib::Type const& type, std::string const& basename)
    {
        fieldName.push_back(basename);
        TypeVisitor::apply(type);
        fieldName.pop_back();
    }
};

int convert(const std::string& logfile, const std::string& output,
            const int size, const int verbose)
{
    pocolog_cpp::MultiFileIndex* multiIndex = new pocolog_cpp::MultiFileIndex();
    std::vector<std::string> filenames(1, logfile);
    multiIndex->createIndex(filenames);

    std::vector<pocolog_cpp::Stream*> streams = multiIndex->getAllStreams();
    const size_t numStreams = streams.size();
    std::vector<pocolog_cpp::InputDataStream*> dataStreams(numStreams);
    for(size_t i = 0; i < numStreams; i++)
    {
        dataStreams[i] = dynamic_cast<pocolog_cpp::InputDataStream*>(streams[i]);
        if(!dataStreams[i])
        {
            std::cerr << "[pocolog2msgpack] Stream #" << i
                << " is not a data stream!" << std::endl;
            continue;
        }
    }

    if(verbose >= 1)
        std::cout << "[pocolog2msgpack] " << numStreams << " streams"
            << std::endl;

    FILE* fp = fopen(output.c_str(), "w");
    msgpack_packer pk;
    msgpack_packer_init(&pk, fp, msgpack_fbuffer_write);

    msgpack_pack_map(&pk, numStreams);
    for(size_t i = 0; i < numStreams; i++)
    {
        pocolog_cpp::InputDataStream* stream = dataStreams[i];
        if(!stream)
        {
            std::cerr << "[pocolog2msgpack] Could not open stream #" << i
                << std::endl;
            continue;
        }

        const size_t numSamples = stream->getSize();
        if(verbose >= 1)
            std::cout << "[pocolog2msgpack] Stream #" << i << ": " << numSamples
                << " samples" << std::endl;

        const Typelib::Type& type = *stream->getType();
        std::string streamName = stream->getName();

        msgpack_pack_str(&pk, streamName.size());
        msgpack_pack_str_body(&pk, streamName.c_str(), streamName.size());

        msgpack_pack_array(&pk, numSamples);
        for(size_t t = 0; t < numSamples; t++)
        {
            std::vector<uint8_t> data;
            const bool ok = stream->getSampleData(data, t);
            if(!ok)
            {
                std::cerr << "[pocolog2msgpack] Could not read sample data."
                    << std::endl;
                return EXIT_FAILURE;
            }

            if(verbose >= 2)
                std::cout << "[pocolog2msgpack] Converting column = " << i
                    << ", t = " << t << std::endl;

            Converter conv(&data[0], pk, size, verbose);
            conv.apply(type, streamName);
        }
    }
    fclose(fp);

    delete multiIndex;

    return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
    po::options_description desc("Options");
    desc.add_options()
        ("help,h", "Print help message")
        ("verbose,v", boost::program_options::value<int>()->default_value(0),
            "Verbosity level")
        ("logfile,l", boost::program_options::value<std::string>(),
            "Logfile")
        ("output,o", boost::program_options::value<std::string>()->default_value("output.msg"),
            "Output file")
        ("size,s", boost::program_options::value<int>()->default_value(8),
            "Length of the size type. This should be 8 for most machines, "
            "but it can be 1, e.g. on robots.")
    ;
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if(vm.count("help"))
    {
        std::cout << desc << std::endl;
        return EXIT_FAILURE;
    }

    int verbose = vm["verbose"].as<int>();
    if(verbose >= 1)
    {
        std::cout << "[pocolog2msgpack] Verbosity level is "
            << verbose << "." << std::endl;
    }

    if(!vm.count("logfile"))
    {
        std::cerr << "[pocolog2msgpack] No logfile given" << std::endl;
        std::cout << desc << std::endl;
        return EXIT_FAILURE;
    }
    std::string logfile = vm["logfile"].as<std::string>();
    std::string output = vm["output"].as<std::string>();
    const int size = vm["size"].as<int>();

    return convert(logfile, output, size, verbose);
}
