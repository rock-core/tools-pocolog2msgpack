#include <cstdlib>
#include <iostream>
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
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/join.hpp>


std::vector<std::string> getFilenames(int argc, char *argv[])
{
    const int numFiles = argc - 1;
    std::vector<std::string> filenames(numFiles);
    for(int i = 0; i < numFiles; i++)
        filenames[i] = argv[i + 1];
    return filenames;
}


class Converter : public Typelib::TypeVisitor
{
    uint8_t* data;
    size_t offset;
    bool part;
    unsigned int lastNumber;
    double lastFloat;
    std::list<std::string> fieldName;
    msgpack_packer& pk;
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
        return i;
    }

    signed int getSignedInt(Typelib::Numeric const& type, bool part)
    {
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
        return i;
    }

    double getFloat(Typelib::Numeric const& type, bool part)
    {
        double i = 0;  // TODO initialize with nan
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
                "unknown float size: " + boost::lexical_cast<std::string>(type.getSize()));
        }
        return i;
    }

    bool visit_(Typelib::Enum const&)
    {
        return true;
    }

    bool visit_(Typelib::Pointer const& type)
    {
        TypeVisitor::visit_(type);
        return true;
    }

    bool visit_(Typelib::Array const& type)
    {
        const size_t numElements = type.getDimension();
        double d[numElements];

        msgpack_pack_array(&pk, numElements);
        for(size_t i = 0; i < numElements; i++)
        {
            visit_(type.getIndirection());
            d[i] = lastFloat;
        }
        return true;
    }

    bool visit_(Typelib::Container const& type)
    {
        if(type.kind() == "/std/string")
        {
            const size_t numElements = *reinterpret_cast<uint64_t*>(data + offset);
            offset += 8;  // TODO We don't know whether this is always right!!!

            part = true;
            std::string s;
            for(size_t i = 0; i < numElements; i++)
            {
                visit_(type.getIndirection());
                s += (char) lastNumber;
            }
            part = false;

            msgpack_pack_str(&pk, numElements);
            msgpack_pack_str_body(&pk, s.c_str(), numElements);
        }
        else if(type.kind() == "/std/vector")
        {
            std::cerr << "/std/vector is not implemented yet, see https://github.com/orocos-toolchain/typelib/blob/master/lang/csupport/containers.cc#L260" << std::endl;
            msgpack_pack_nil(&pk);
        }
        else
        {
            throw std::runtime_error("unknown container");
        }
        return true;
    }

    bool visit_(Typelib::Compound const& type)
    {
        msgpack_pack_map(&pk, type.getFields().size());
        TypeVisitor::visit_(type);
        return true;
    }

    bool visit_(Typelib::Compound const& type, Typelib::Field const& field)
    {
        msgpack_pack_str(&pk, field.getName().size());
        msgpack_pack_str_body(&pk, field.getName().c_str(), field.getName().size());

        fieldName.push_back(field.getName());
        TypeVisitor::visit_(type, field);
        fieldName.pop_back();
        return true;
    }

    using TypeVisitor::visit_;
public:
    Converter(uint8_t* data, msgpack_packer& pk)
        : data(data), offset(0), part(false), pk(pk)
    {
    }
    void apply(Typelib::Type const& type, std::string const& basename)
    {
        fieldName.push_back(basename);
        TypeVisitor::apply(type);
        fieldName.pop_back();
    }
};


int main(int argc, char *argv[])
{
    if(argc == 1)
    {
        std::cerr << "No logfile given" << std::endl;
        return EXIT_FAILURE;
    }

    pocolog_cpp::MultiFileIndex* multiIndex = new pocolog_cpp::MultiFileIndex();

    std::vector<std::string> filenames = getFilenames(argc, argv);
    multiIndex->createIndex(filenames);

    std::vector<pocolog_cpp::Stream*> streams = multiIndex->getAllStreams();
    const size_t numStreams = streams.size();
    std::vector<pocolog_cpp::InputDataStream*> dataStreams(numStreams);
    for(size_t i = 0; i < numStreams; i++)
    {
        dataStreams[i] = dynamic_cast<pocolog_cpp::InputDataStream*>(streams[i]);
        if(!dataStreams[i])
        {
            std::cerr << "Stream #" << i << " is not a data stream!" << std::endl;
            continue;
        }
    }

    std::cout << numStreams << " streams" << std::endl;

    std::string fileName("output.msg");
    FILE* fp = fopen(fileName.c_str(), "w");
    msgpack_packer pk;
    msgpack_packer_init(&pk, fp, msgpack_fbuffer_write);

    msgpack_pack_map(&pk, numStreams);
    for(size_t i = 0; i < numStreams; i++)
    {
        pocolog_cpp::InputDataStream* stream = dataStreams[i];
        if(!stream) continue;  // TODO warning

        const size_t numSamples = stream->getSize();
        std::cout << numSamples << " samples" << std::endl;

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
                std::cerr << "Could not read sample data." << std::endl;
                return EXIT_FAILURE;
            }

            std::cout << "column = " << i << ", t = " << t << std::endl;

            Converter conv(&data[0], pk);
            conv.apply(type, streamName);
        }
    }
    fclose(fp);

    delete multiIndex;

    return EXIT_SUCCESS;
}
