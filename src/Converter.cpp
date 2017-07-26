#include "Converter.hpp"
#include <pocolog_cpp/MultiFileIndex.hpp>
#include <pocolog_cpp/Stream.hpp>
#include <pocolog_cpp/InputDataStream.hpp>
#include <msgpack.h>
#include <msgpack/fbuffer.h>
#include <boost/lexical_cast.hpp>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <string>
#include <stdexcept>
#include <cstdlib>
#include <cassert>

void addValidInputDataStreams(
    const std::vector<pocolog_cpp::Stream*>& streams,
    std::vector<pocolog_cpp::InputDataStream*>& dataStreams);
int convertSamples(Converter& conv, pocolog_cpp::InputDataStream* stream,
    const int verbose);


int convert(const std::string& logfile, const std::string& output,
            const int size, const int containerLimit, const int verbose)
{
    pocolog_cpp::MultiFileIndex* multiIndex = new pocolog_cpp::MultiFileIndex();
    std::vector<std::string> filenames(1, logfile);
    multiIndex->createIndex(filenames);

    std::vector<pocolog_cpp::Stream*> streams = multiIndex->getAllStreams();
    std::vector<pocolog_cpp::InputDataStream*> dataStreams;
    addValidInputDataStreams(streams, dataStreams);
    if(verbose >= 1)
        std::cout << "[pocolog2msgpack] " << dataStreams.size() << " streams"
            << std::endl;

    FILE* fp = fopen(output.c_str(), "w");
    msgpack_packer pk;
    msgpack_packer_init(&pk, fp, msgpack_fbuffer_write);

    msgpack_pack_map(&pk, dataStreams.size());
    int exit_status = EXIT_SUCCESS;
    for(size_t i = 0; i < dataStreams.size(); i++)
    {
        pocolog_cpp::InputDataStream* stream = dataStreams[i];
        assert(stream);

        if(verbose >= 1)
            std::cout << "[pocolog2msgpack] Stream #" << i << ": " << stream->getSize()
                << " samples" << std::endl;

        std::string streamName = stream->getName();

        msgpack_pack_str(&pk, streamName.size());
        msgpack_pack_str_body(&pk, streamName.c_str(), streamName.size());

        msgpack_pack_array(&pk, stream->getSize());
        Converter conv(streamName, *stream->getType(), pk, size, containerLimit, verbose);
        exit_status += convertSamples(conv, stream, verbose);
    }
    fclose(fp);

    delete multiIndex;

    return exit_status;
}

void addValidInputDataStreams(
    const std::vector<pocolog_cpp::Stream*>& streams,
    std::vector<pocolog_cpp::InputDataStream*>& dataStreams)
{
    dataStreams.reserve(streams.size());
    for(size_t i = 0; i < streams.size(); i++)
    {
        pocolog_cpp::InputDataStream* dataStream =
            dynamic_cast<pocolog_cpp::InputDataStream*>(streams[i]);
        if(!dataStream)
        {
            std::cerr << "[pocolog2msgpack] Stream #" << i
                << " is not an InputDataStream, will be ignored!" << std::endl;
            continue;
        }
        dataStreams.push_back(dataStream);
    }
}

int convertSamples(Converter& conv, pocolog_cpp::InputDataStream* stream,
    const int verbose)
{
    for(size_t t = 0; t < stream->getSize(); t++)
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
            std::cout << "[pocolog2msgpack] Converting sample #" << t
                << std::endl;
        conv.apply(&data[0]);
    }
    return EXIT_SUCCESS;
}

Converter::Converter(std::string const& basename, Typelib::Type const& type, msgpack_packer& pk, int size, int containerLimit, int verbose)
    : data(NULL), type(type), offset(0), part(false), pk(pk), size(size),
      containerLimit(containerLimit), verbose(verbose), depth(0),
      indentation(1)
{
    fieldName.push_back(basename);
}

Converter::~Converter()
{
    assert(!data);
}

void Converter::apply(uint8_t* data)
{
    assert(data);

    reset();
    this->data = data;
    TypeVisitor::apply(type);
    this->data = NULL;
}

void Converter::reset()
{
    assert(!data);
    offset = 0;
    part = false;
    depth = 0;
}

bool Converter::visit_(Typelib::OpaqueType const& type)
{
    return true;
}

bool Converter::visit_(Typelib::Numeric const& type)
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

unsigned int Converter::getUnsignedInt(Typelib::Numeric const& type, bool part)
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

signed int Converter::getSignedInt(Typelib::Numeric const& type, bool part)
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

double Converter::getFloat(Typelib::Numeric const& type, bool part)
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

bool Converter::visit_(Typelib::Enum const&)
{
    if(verbose >= 3 + depth)
        std::cout << "[pocolog2msgpack]"
            << std::setfill(' ') << std::setw(indentation + depth) << " "
            << "enum" << std::endl;

    return true;
}

bool Converter::visit_(Typelib::Pointer const& type)
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

bool Converter::visit_(Typelib::Array const& type)
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

bool Converter::visit_(Typelib::Container const& type)
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

    if(numElements > containerLimit)
    {
        std::cerr << "[pocolog2msgpack] WARNING: container limit exceeded, "
            << "truncating " << type.kind() << "! (" << numElements << " > "
            << containerLimit << ")" << std::endl;
        numElements = containerLimit;
    }

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

bool Converter::visit_(Typelib::Compound const& type)
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

bool Converter::visit_(Typelib::Compound const& type, Typelib::Field const& field)
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
