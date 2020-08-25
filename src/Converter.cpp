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
#include <typelib/value_ops.hh>
#include <ios>

void addValidInputDataStreams(
    const std::vector<pocolog_cpp::Stream*>& streams,
    std::vector<pocolog_cpp::InputDataStream*>& dataStreams,
    const std::string& only);
int convertStreams(
    msgpack_packer& packer, std::vector<pocolog_cpp::InputDataStream*>& dataStreams,
    const int size, const int containerLimit, const int start, const int end,
    const int verbose);
int convertSamples(Converter& conv, pocolog_cpp::InputDataStream* stream,
                   const int start, const int end, const int verbose);
int convertMetaData(
    msgpack_packer& packer, std::vector<pocolog_cpp::InputDataStream*>& dataStreams,
    const int start, const int end, const int verbose);


int convert(const std::vector<std::string>& logfiles, const std::string& output,
            const int size, const int containerLimit, const std::string& only,
            const int start, const int end, const int verbose)
{
    pocolog_cpp::MultiFileIndex* multiIndex = new pocolog_cpp::MultiFileIndex();
    multiIndex->createIndex(logfiles);
    std::vector<pocolog_cpp::Stream*> streams = multiIndex->getAllStreams();
    std::vector<pocolog_cpp::InputDataStream*> dataStreams;
    addValidInputDataStreams(streams, dataStreams, only);
    if(verbose >= 1)
        std::cout << "[pocolog2msgpack] " << dataStreams.size() << " streams"
                  << std::endl;

    FILE* fp = fopen(output.c_str(), "w");

    msgpack_packer packer;
    msgpack_packer_init(&packer, fp, msgpack_fbuffer_write);

    // We will store both the logdata and its meta data in a map
    msgpack_pack_map(&packer, 2 * dataStreams.size());

    int exitStatus = convertStreams(
                         packer, dataStreams, size, containerLimit, start, end, verbose);
    exitStatus += convertMetaData(
                      packer, dataStreams, start, end, verbose);

    fclose(fp);
    delete multiIndex;

    return exitStatus;
}

void addValidInputDataStreams(
    const std::vector<pocolog_cpp::Stream*>& streams,
    std::vector<pocolog_cpp::InputDataStream*>& dataStreams,
    const std::string& only)
{
    dataStreams.reserve(streams.size());
    for(size_t i = 0; i < streams.size(); i++)
    {
        if(only != "" && only != streams[i]->getName())
            continue;

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
    if(only != "" && dataStreams.size() == 0)
        std::cerr << "[pocolog2msgpack] Did not find the stream '" << only
                  << "'." << std::endl;
}

const int computeRangeEnd(const int userStart, const int userEnd,
                          const int streamLength, const bool printWarning)
{
    int realEnd = userEnd < 0 ? streamLength : userEnd;
    if(streamLength < userEnd)
    {
        if(printWarning)
        {
            std::cerr << "[pocolog2msgpack] Requested samples [" << userStart
                      << ", " << userEnd << "). This stream only has "
                      << streamLength << " samples. Range will be cut to ["
                      << userStart << ", " << streamLength << ")" << std::endl;
        }
        realEnd = streamLength;
    }
    return realEnd;
}

int convertStreams(
    msgpack_packer& packer, std::vector<pocolog_cpp::InputDataStream*>& dataStreams,
    const int size, const int containerLimit, const int start, const int end,
    const int verbose)
{
    int exitStatus = EXIT_SUCCESS;
    const bool sliceOutput = dataStreams.size() == 1;

    for(size_t i = 0; i < dataStreams.size(); i++)
    {
        pocolog_cpp::InputDataStream* stream = dataStreams[i];
        assert(stream);
        const std::string streamName = stream->getName();
        if(verbose >= 1)
            std::cout << "[pocolog2msgpack] Stream #" << i << " ("
                      << streamName << "): " << stream->getSize()
                      << " samples" << std::endl;

        const int realEnd = computeRangeEnd(start, end, stream->getSize(), true);
        int exportedSize = realEnd - start;

        if (exportedSize < 0)
        {
            std::cerr << "[pocolog2msgpack] No samples in requested range for stream \""
                      << streamName << "\"." << std::endl;
            exportedSize = 0;
        }
        msgpack_pack_str(&packer, streamName.size());
        msgpack_pack_str_body(&packer, streamName.c_str(), streamName.size());

        msgpack_pack_array(&packer, exportedSize);

        if (exportedSize == 0)
            continue;

        Converter conv(streamName, *stream->getType(), packer,
                       containerLimit, verbose);

        exitStatus += convertSamples(conv, stream, start, realEnd, verbose);
    }

    return exitStatus;
}

int convertSamples(Converter& conv, pocolog_cpp::InputDataStream* stream,
                   const int start, const int end, const int verbose)
{

    float next_report_progress = 0.0;
    float report_progress_delta = 0.1;

    for(size_t t = start; t < end; t++)
    {

        if(verbose >= 2)
        {
            std::cout << "[pocolog2msgpack] Converting sample #" << t
                      << std::endl;
        }
        std::vector<uint8_t> curSampleData;
        const bool ok = stream->getSampleData(curSampleData, t);

        if(!ok)
        {
            std::cerr << "[pocolog2msgpack] ERROR: Could not read sample data."
                      << std::endl;
            return EXIT_FAILURE;
        }

        if(verbose >= 3)
        {
            std::cout << "Converting sample of size " << curSampleData.size() << std::endl;
        }

        if(verbose >= 4)
        {
            for ( size_t i=0; i < curSampleData.size() ; i++ )
            {
                if (i % 32 == 0) std::cout << std::endl;
                std::ios_base::fmtflags f( std::cout.flags() );
                std::cout << std::setfill('0') << std::setw(2) << std::right << std::hex << (unsigned int)(curSampleData[i]) << " ";
                std::cout.flags( f );
                std::cout << " ";

                if (i > 128)
                {
                    std::cout << "...";
                    break;
                }
            }
            std::cout << std::endl;
        }

        conv.convertSample(curSampleData);

        float progress = ((float)(t-start))/(end-start);

        if ( progress >= next_report_progress )
        {
            next_report_progress += report_progress_delta;
            if (verbose >= 1)
            {
                std::cout << "[pocolog2msgpack] " << (int)(progress*100) << "% of stream done." << std::endl;
            }
        }


    }
    return EXIT_SUCCESS;
}

int convertMetaData(
    msgpack_packer& packer, std::vector<pocolog_cpp::InputDataStream*>& dataStreams,
    const int start, const int end, const int verbose)
{
    int exitStatus = EXIT_SUCCESS;
    const std::string timeKey = "timestamps";
    const std::string typeKey = "type";

    for(size_t i = 0; i < dataStreams.size(); i++)
    {
        pocolog_cpp::InputDataStream* stream = dataStreams[i];
        assert(stream);
        pocolog_cpp::Index& streamIndex = stream->getFileIndex();

        const std::string key = stream->getName() + ".meta";

        const int realEnd = computeRangeEnd(start, end, stream->getSize(), false);
        int exportedSize = realEnd - start;

        if (exportedSize < 0)
        {
            std::cerr << "[pocolog2msgpack] No samples in requested range for meta stream \""
                      << key << "\"." << std::endl;
            exportedSize = 0;
        }

        msgpack_pack_str(&packer, key.size());
        msgpack_pack_str_body(&packer, key.c_str(), key.size());

        msgpack_pack_map(&packer, 2);

        msgpack_pack_str(&packer, timeKey.size());
        msgpack_pack_str_body(&packer, timeKey.c_str(), timeKey.size());


        msgpack_pack_array(&packer, exportedSize);

        if (exportedSize > 0)
        {
            for(size_t t = start; t < realEnd; t++)
            {
                msgpack_pack_int64(&packer, streamIndex.getSampleTime(t).microseconds);
            }
        }

        msgpack_pack_str(&packer, typeKey.size());
        msgpack_pack_str_body(&packer, typeKey.c_str(), typeKey.size());
        const std::string typeName = stream->getType()->getName();
        msgpack_pack_str(&packer, typeName.size());
        msgpack_pack_str_body(&packer, typeName.c_str(), typeName.size());
    }

    return exitStatus;
}

Converter::Converter(std::string const& basename, Typelib::Type const& type, msgpack_packer& pk, int containerLimit, int verbose)
    : type(type), pk(pk),
      containerLimit(containerLimit), verbose(verbose), depth(0),
      indentation(1)
{

    debug = (verbose > 3);

    fieldName.push_back(basename);
}

Converter::~Converter()
{
}

void Converter::convertSample(std::vector<uint8_t>& data)
{
    std::vector<uint8_t> val_buffer;
    val_buffer.resize(type.getSize());
    auto val = Typelib::Value( val_buffer.data(), type );
    Typelib::init( val );
    Typelib::load( val, data.data(), data.size() );

    reset();
    ValueVisitor::apply(val);
    Typelib::destroy( val );

}

void Converter::reset()
{
    depth = 0;
}

void Converter::printBegin()
{
    std::cout << "[pocolog2msgpack]"
              << std::setfill(' ') << std::setw(indentation + depth) << " ";
}

bool Converter::visit_ (int8_t  & v)
{
    if (debug)
    {
        printBegin();
        std::cout << "got int8 " << (int)v << std::endl;
    }

    if (mode_numeric_to_string)
    {
        numeric_to_string_buffer += (char)v;
        return true;
    }

    msgpack_pack_int8(&pk, v);
    return true;
}

bool Converter::visit_ (uint8_t & v)
{
    if (debug)
    {
        printBegin();
        std::cout << "got uint8 " << (unsigned int) v << std::endl;
    }

    if (mode_numeric_to_string)
    {
        numeric_to_string_buffer += (char)v;
        return true;
    }

    msgpack_pack_uint8(&pk, v);
    return true;
}

bool Converter::visit_ (int16_t & v)
{
    if (debug)
    {
        printBegin();
        std::cout << __FUNCTION__ << "(int16_t & v) got "<<  v  << std::endl;
    }
    msgpack_pack_int16(&pk, v);
    return true;
}
bool Converter::visit_ (uint16_t& v)
{
    if (debug)
    {
        printBegin();
        std::cout << __FUNCTION__ << "(uint16_t& v) got "<<  v  << std::endl;
    }
    msgpack_pack_uint16(&pk, v);
    return true;
}
bool Converter::visit_ (int32_t & v)
{
    if (debug)
    {
        printBegin();
        std::cout << __FUNCTION__ << "(int32_t & v) got "<<  v  << std::endl;
    }
    msgpack_pack_int32(&pk, v);
    return true;
}
bool Converter::visit_ (uint32_t& v)
{
    if (debug)
    {
        printBegin();
        std::cout << __FUNCTION__ << "(uint32_t& v) got "<<  v  << std::endl;
    }
    msgpack_pack_uint32(&pk, v);
    return true;
}
bool Converter::visit_ (int64_t & v)
{
    if (debug)
    {
        printBegin();
        std::cout << __FUNCTION__ << "(int64_t & v) got "<<  v  << std::endl;
    }
    msgpack_pack_int64(&pk, v);
    return true;
}
bool Converter::visit_ (uint64_t& v)
{
    if (debug)
    {
        printBegin();
        std::cout << __FUNCTION__ << "(uint64_t& v) got "<<  v  << std::endl;
    }
    msgpack_pack_uint64(&pk, v);
    return true;
}
bool Converter::visit_ (float   & v)
{
    if (debug)
    {
        printBegin();
        std::cout << __FUNCTION__ << "(float   & v) got "<<  v  << std::endl;
    }
    msgpack_pack_float(&pk, v);
    return true;
}
bool Converter::visit_ (double  & v)
{
    if (debug)
    {
        printBegin();
        std::cout << __FUNCTION__ << "(double  & v) got "<<  v  << std::endl;
    }
    msgpack_pack_double(&pk, v);

    return true;
}

bool Converter::visit_(Typelib::Value const& v, Typelib::OpaqueType const& type)
{
    if (debug)
    {
        printBegin();
        std::cout << __FUNCTION__ << "OpaqueType at "<< v.getData() << std::endl;
    }
    return true;
}

bool Converter::visit_(Typelib::Value const& v, Typelib::Pointer const& type)
{
    if (debug)
    {
        printBegin();
        std::cout << __FUNCTION__ << "Pointer at "<< v.getData() <<   std::endl;
    }

    depth += 1;
    auto retval = Typelib::ValueVisitor::visit_(v, type);
    depth -= 1;

    return retval;
}

bool Converter::visit_(Typelib::Value const& v, Typelib::Array const& type)
{
    if (debug)
    {
        printBegin();
        std::cout << __FUNCTION__ << "Array at "<< v.getData() <<  std::endl;
        printBegin();
        std::cout << "array[" << type.getDimension() << "]" << std::endl;
    }

    msgpack_pack_array(&pk, type.getDimension());

    depth += 1;
    auto retval = Typelib::ValueVisitor::visit_(v, type);
    depth -= 1;

    return retval;
}

bool Converter::visit_(Typelib::Value const& v, Typelib::Container const& type)
{
    size_t numElements = type.getElementCount(v.getData());

    if (debug)
    {
        printBegin();
        std::cout << __FUNCTION__ << "Container at "<< v.getData() << std::endl;
        printBegin();
        std::cout << "numElements: " <<numElements << std::endl;
        printBegin();
        std::cout << type.kind() << "[" << numElements << "]" << std::endl;
    }

    if(numElements > containerLimit)
    {
        throw std::runtime_error("too many elements");
    }

    bool retval = false;

    if(type.kind() == "/std/string")
    {
        numeric_to_string_buffer = "";
        mode_numeric_to_string = true;

        depth += 1;
        retval = Typelib::ValueVisitor::visit_(v, type);
        depth -= 1;

        mode_numeric_to_string = false;

        if (debug)
        {
            std::cout << "got str: \"" << numeric_to_string_buffer << "\""  << std::endl;
        }

        msgpack_pack_str(&pk, numeric_to_string_buffer.size());
        msgpack_pack_str_body(&pk, numeric_to_string_buffer.c_str(), numeric_to_string_buffer.size());

    }
    else
    {

        msgpack_pack_array(&pk, numElements);

        depth += 1;
        retval = Typelib::ValueVisitor::visit_(v, type);
        depth -= 1;
    }

    return retval;
}

bool Converter::visit_(Typelib::Value const& v, Typelib::Compound const& type)
{
    if (debug)
    {
        printBegin();
        std::cout << "compound '" << type.getName() << "' with " << type.getFields().size() << " fields" << std::endl;
    }

    msgpack_pack_map(&pk, type.getFields().size());

    depth += 1;
    auto retval = Typelib::ValueVisitor::visit_(v, type);
    depth -= 1;

    return retval;
}

bool Converter::visit_(Typelib::Value const& v, Typelib::Compound const& type, Typelib::Field const& field)
{
    if (debug)
    {
        printBegin();
        std::cout << "field '" << field.getName() << "'" << std::endl;
    }

    msgpack_pack_str(&pk, field.getName().size());
    msgpack_pack_str_body(&pk, field.getName().c_str(), field.getName().size());

    fieldName.push_back(field.getName());

    depth += 1;
    auto retval = Typelib::ValueVisitor::visit_(v, type, field);
    depth -= 1;

    fieldName.pop_back();

    return retval;
}

bool Converter::visit_(Typelib::Enum::integral_type& intValue, Typelib::Enum const& e)
{
    try
    {
        std::string value = e.get(intValue);

        msgpack_pack_str(&pk, value.size());
        msgpack_pack_str_body(&pk, value.c_str(), value.size());
    }
    catch(Typelib::Enum::ValueNotFound& e)
    {
        msgpack_pack_int(&pk, intValue);
        std::cerr << "[pocolog2msgpack] Could not find a string representation "
                  << "for enum value " << intValue << "." << std::endl;
    }

    return true;
}
