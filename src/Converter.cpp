#include "Converter.hpp"
#include <pocolog_cpp/MultiFileIndex.hpp>
#include <pocolog_cpp/Stream.hpp>
#include <pocolog_cpp/InputDataStream.hpp>
#include <pocolog_cpp/named_vector_helpers.hpp>
#include <msgpack.h>
#include <msgpack/fbuffer.h>
#include <msgpack/zbuffer.h>
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
    const int containerLimit, const int start, const int end, const bool flatten, const bool align_named_vector,
    const int verbose);

FILE* fp;
msgpack_zbuffer zbuf;

bool writeOutZBuffer(bool force=true, bool flush=false) {
    
    if (flush) {
        auto dat = msgpack_zbuffer_flush(&zbuf);
        if (dat == NULL) {
            throw std::runtime_error("msgpack_zbuffer_flush failed");
        }
    }
    
    auto cur_size = msgpack_zbuffer_size(&zbuf);
    if ( (force || flush || zbuf.stream.avail_out < 128*1024) && cur_size > 0) {
        auto written = fwrite( msgpack_zbuffer_data(&zbuf), sizeof(char), cur_size, fp);
        if (written != cur_size) {
            //std::cout << "commanded write " << cur_size << ", written " << written << std::endl;
            throw std::runtime_error("fwrite failed");
        }
        //std::cout << "success writing " << cur_size << " bytes\n";
        msgpack_zbuffer_reset_buffer(&zbuf);
    }
    
    if (flush) {
        msgpack_zbuffer_reset(&zbuf);
    }
    
    return true;
}

int convert(
    const std::vector<std::string>& logfiles, const std::string& output,
    const int containerLimit, const std::string& only,
    const int start, const int end, const bool flatten, const bool align_named_vector,
    const bool compress, const int verbose
) {
    pocolog_cpp::MultiFileIndex* multiIndex = new pocolog_cpp::MultiFileIndex();
    multiIndex->createIndex(logfiles);
    std::vector<pocolog_cpp::Stream*> streams = multiIndex->getAllStreams();
    std::vector<pocolog_cpp::InputDataStream*> dataStreams;
    addValidInputDataStreams(streams, dataStreams, only);
    if(verbose >= 1)
        std::cout << "[pocolog2msgpack] " << dataStreams.size() << " streams"
                  << std::endl;

    /*FILE**/ fp = fopen(output.c_str(), "w");
    msgpack_packer packer;

    if (compress) {
        msgpack_zbuffer_init(&zbuf, 9 , 32*1024*1024);
        msgpack_packer_init(&packer, &zbuf, msgpack_zbuffer_write);
    } else {
        msgpack_packer_init(&packer, fp, msgpack_fbuffer_write);
    }
    

    if (!flatten) {
        // We will store both the logdata and its meta data in a map
        msgpack_pack_map(&packer, 2 * dataStreams.size());
    }

    int exitStatus = convertStreams(
        packer, 
        dataStreams, 
        containerLimit, 
        start, end, 
        flatten, 
        align_named_vector, 
        verbose
    );
    
    
    if (compress) {
        writeOutZBuffer(true, true);
        msgpack_zbuffer_destroy(&zbuf);
    }
    
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
    const int containerLimit, const int start, const int end, const bool flatten, const bool align_named_vector,
    const int verbose)
{
    int exitStatus = EXIT_SUCCESS;
    const bool sliceOutput = dataStreams.size() == 1;

    if (flatten) {
        int pathCount = 0;
    
        for (const auto & s : dataStreams) {
            std::vector<std::string> toplevelPath;
            std::vector<std::vector<std::string>> paths;
            
            Converter::inspectTypeLevels(*(s->getType()), toplevelPath, paths);
            pathCount += paths.size();
        }
        
        pathCount += dataStreams.size()*2;
        std::cout << "[pocolog2msgpack] Number of paths " << pathCount << std::endl;
        
        msgpack_pack_map(&packer, pathCount);
    }
    
    
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

        Converter conv(*stream->getType(), packer,
                       containerLimit, flatten, align_named_vector, verbose);

        //if (conv.convertNamedVector(stream, start, realEnd, verbose)) continue;
        
        exitStatus += conv.convertSamples(stream, start, realEnd);
        exitStatus += conv.convertMetaData(stream, start, realEnd);
    }

    return exitStatus;
}


bool Converter::inspectTypeLevels(
    const Typelib::Type &t, 
    std::vector<std::string> toplevelPath, 
    std::vector<std::vector<std::string>> & paths
) {
    
    switch (t.getCategory()) {            
            
        case Typelib::Type::NullType:
            paths.push_back(toplevelPath);
            break;
        case Typelib::Type::Array:
            {
                auto & array_type = dynamic_cast<const Typelib::Array*>(&t)->getIndirection();
                inspectTypeLevels(array_type, toplevelPath, paths);
            }           
            break;
        case Typelib::Type::Pointer:
            {
                auto & array_type = dynamic_cast<const Typelib::Pointer*>(&t)->getIndirection();
                inspectTypeLevels(array_type, toplevelPath, paths);
            }       
            break;
        case Typelib::Type::Numeric:
            paths.push_back(toplevelPath);
            break;
        case Typelib::Type::Enum:
            paths.push_back(toplevelPath);
            break;
        case Typelib::Type::Compound:
            {    
                std::list<Typelib::Field> fields = dynamic_cast<const Typelib::Compound*>(&t)->getFields();
                
                for ( auto f : fields ) {
                    auto & type_field = f.getType();
                    auto new_path = toplevelPath;
                    new_path.push_back(f.getName());
                    inspectTypeLevels(type_field, new_path, paths);
                }
            }
            break;
        case Typelib::Type::Opaque:
            paths.push_back(toplevelPath);            
            break;
        case Typelib::Type::Container:
            {
                if (t.getName() == "/std/string") {
                    paths.push_back(toplevelPath);
                } else {
                    auto & array_type = dynamic_cast<const Typelib::Container*>(&t)->getIndirection();
                    inspectTypeLevels(array_type, toplevelPath, paths);
                }
            }       
            break;
            
        default:
            break;
            
            
    }
    
    return true;
}


bool Converter::isNamedVectorField(
    const Typelib::Type &t, 
    std::vector<std::string> toplevelPath, 
    std::vector<std::string> targetPath
) {
    if (targetPath.size() < 1) {
        throw std::runtime_error("targetPath must contain at least the field name");
    }
    
    targetPath.pop_back();
    
    return Converter::isNamedVector(t, toplevelPath, targetPath);
}


bool Converter::isNamedVector(
    const Typelib::Type &t, 
    std::vector<std::string> toplevelPath, 
    std::vector<std::string> targetPath
) {
    
    
    if (targetPath.size() < toplevelPath.size()) {
        throw std::runtime_error("targetPath.size() cannot be smaller than toplevelPath.size()");
    }
        
    if (targetPath.size() == toplevelPath.size()) {
        if (targetPath != toplevelPath) {
            throw std::runtime_error("targetPath and toplevelPath mismatch on same level");
        }
        
        return pocolog_cpp::is_named_vector(t);
    }
    
    switch (t.getCategory()) {            
            
        case Typelib::Type::NullType:
            break;
        case Typelib::Type::Array:
            {
                auto & array_type = dynamic_cast<const Typelib::Array*>(&t)->getIndirection();
                return isNamedVector(array_type, toplevelPath, targetPath);
            }           
            break;
        case Typelib::Type::Pointer:
            {
                auto & array_type = dynamic_cast<const Typelib::Pointer*>(&t)->getIndirection();
                return isNamedVector(array_type, toplevelPath, targetPath);
            }       
            break;
        case Typelib::Type::Numeric:
            break;
        case Typelib::Type::Enum:
            break;
        case Typelib::Type::Compound:
            {    
                std::string targetField = targetPath[toplevelPath.size()];   
                auto new_path = toplevelPath;
                new_path.push_back(targetField);

                auto f = dynamic_cast<const Typelib::Compound*>(&t)->getField(targetField);
                auto & type_field = f->getType();
                return isNamedVector(type_field, new_path, targetPath);                
            }
            break;
        case Typelib::Type::Opaque:
            break;
        case Typelib::Type::Container:
            {
                if (t.getName() == "/std/string") {
                    return false;
                } else {
                    auto & array_type = dynamic_cast<const Typelib::Container*>(&t)->getIndirection();                    
                    return isNamedVector(array_type, toplevelPath, targetPath);
                }
            }       
            break;
            
        default:
            break;            
            
    }
    
    return false;
}

bool Converter::extractDataByPath(
    const Typelib::Value &v, 
    std::vector<std::string> toplevelPath, 
    std::vector<std::string> targetPath, 
    std::shared_ptr<std::vector<size_t>> element_order
) {

    if (verbose >=3) {
        std::string curPath = "";
            
        for (auto dir : targetPath) {
            curPath = curPath + "/" + dir;
        }
        std::cout << "At path: ";
        std::cout << curPath;
        std::cout << std::endl;
    }
    
    auto & t = v.getType();
    
    if (targetPath.size() < toplevelPath.size()) {
        throw std::runtime_error("targetPath.size() cannot be smaller than toplevelPath.size()");
    }
        
    if (targetPath.size() == toplevelPath.size()) {
        if (targetPath != toplevelPath) {
            throw std::runtime_error("targetPath and toplevelPath mismatch on same level");
        }
        
        apply(v);
        
        if (verbose >=3) {
            std::cout << "...applying" << std::endl;
        }
        return true;
    }
   
   
    switch (t.getCategory()) {            
            
        case Typelib::Type::NullType:
            throw(std::runtime_error("targetPath cannot be reached, final type is NullType"));
            break;
        case Typelib::Type::Array:
            {
                const Typelib::Array* valueArray = dynamic_cast<const Typelib::Array*>(&t);                
                auto & array_type = valueArray->getIndirection();                           
                auto num_elements = valueArray->getDimension();
                
                
                uint8_t* base = static_cast<uint8_t*>(v.getData());
                uint8_t* elementData;
                
                if (verbose >=3) {
                    std::cout << "...putting array<\n";
                }
                
                if (element_order) {
                    msgpack_pack_array(&pk, element_order->size());
                    for (size_t i = 0; i < element_order->size(); ++i) {
                        if ( element_order->at(i) >= num_elements ) {
                            throw(std::invalid_argument("invalid index in element order"));
                        }
                        elementData = base + array_type.getSize() * element_order->at(i);
                        Typelib::Value element(elementData, array_type);
                        extractDataByPath(element, toplevelPath, targetPath);                        
                    }
                } else {                     
                    msgpack_pack_array(&pk, num_elements);
                    
                    for (size_t i = 0; i < num_elements; ++i)
                    {
                        elementData = base + array_type.getSize() * i;
                        Typelib::Value element(elementData, array_type);
                        extractDataByPath(element, toplevelPath, targetPath);
                    }                
                }
            }           
            break;
        case Typelib::Type::Pointer:            
            throw(std::runtime_error("targetPath cannot be reached, type Pointer is currently unsupported"));      
            break;
        case Typelib::Type::Numeric:
            throw(std::runtime_error("targetPath cannot be reached, final type is Numeric"));
            break;
        case Typelib::Type::Enum:
            throw(std::runtime_error("targetPath cannot be reached, final type is Enum"));
            break;
        case Typelib::Type::Compound:
            {              
                
                std::string targetField = targetPath[toplevelPath.size()];   
                auto new_path = toplevelPath;
                new_path.push_back(targetField);
                
                Typelib::Value fieldValue = Typelib::FieldGetter().apply(v, targetField);
                
                if(align_named_vector && targetField == "names" && pocolog_cpp::is_named_vector(t)) {
                    // in align mode, write out names only once
                    // since they are all the same for all samples (or conversion of elements fails)
                    // so check if it was written
                    if ( !names_written.count(toplevelPath) ) {
                        // it has not been written yet
                        // write it
                        if (verbose >=3) {
                            std::cout << "...descending to " << targetField << "\n";
                        }
                        extractDataByPath(fieldValue, new_path, targetPath);
                        // save that we did
                        names_written.insert(toplevelPath);
                    }
                    
                } else if(align_named_vector && targetField == "elements" && pocolog_cpp::is_named_vector(t)) {
                
                    if (verbose >=3) {
                        std::cout << "is named vector\n";                    
                    }
                    
                    // try to find the saved order of elements/names
                    auto name2idx = path_to_name_orders.find(toplevelPath);
                    
                    if (name2idx == path_to_name_orders.end()) {    
                        // not yet there, so this must be the first occurence
                        // save the order into our store
                        if (verbose >=3) {
                            std::cout << "creating reference order\n";
                        }

                        std::map<std::string, size_t> named_vector_sorting_map;
                        std::vector <std::string> order = pocolog_cpp::extract_names(v);
                        for(size_t idx = 0; idx < order.size(); idx++)
                        {
                            if (named_vector_sorting_map.find(order[idx]) != named_vector_sorting_map.end()) {
                                throw(std::invalid_argument("names are not unique"));
                            }
                            named_vector_sorting_map[order[idx]] = idx;
                        }

                        path_to_name_orders[toplevelPath] = named_vector_sorting_map;   
                        name2idx = path_to_name_orders.find(toplevelPath);
                    }
                    
                    std::vector <std::string> names = pocolog_cpp::extract_names(v);
                    
                    if(names.size() != name2idx->second.size()){
                        throw(std::invalid_argument("names and order must be of same size"));
                    }

                    auto element_reorder = std::make_shared<std::vector<size_t>>(names.size());
                    
                    for (size_t idx_name=0; idx_name < names.size(); ++idx_name) {
                    
                        auto targetIdx_it = name2idx->second.find(names[idx_name]);
                        if(targetIdx_it == name2idx->second.end()){
                            throw(std::invalid_argument(names[idx_name]+" could not be found the given sorting order"));
                        }
                        (*element_reorder)[idx_name] = targetIdx_it->second;
                        
                    }   
                    
                    if (verbose >=3) {
                        std::cout << "...descending to " << targetField << " with order\n";
                    }
                    
                    extractDataByPath(fieldValue, new_path, targetPath, element_reorder);                 
                    
                } else {                
                    if (verbose >=3) {
                        std::cout << "...descending to " << targetField << "\n";
                    }
                    extractDataByPath(fieldValue, new_path, targetPath);
                }
            }
            break;
        case Typelib::Type::Opaque:
            throw(std::runtime_error("targetPath cannot be reached, final type is Opaque"));          
            break;
        case Typelib::Type::Container:
            {                
                const Typelib::Container* container = dynamic_cast<const Typelib::Container*>(&t);                
                auto num_elements = container->getElementCount(v.getData());
                
                
               /* if (remainingDescents == 1 && t.getName() == "/std/string") {
                    std::cout << "...putting str\n";
                    std::string* string_ptr = reinterpret_cast< std::string* >(v.getData());                    
                    msgpack_pack_str(&pk, string_ptr->size());
                    msgpack_pack_str_body(&pk, string_ptr->c_str(), string_ptr->size());
                } else*/ 
                {    
                    if (verbose >=3) {
                        std::cout << "...putting Container\n";
                    }
                
                    if (element_order) {
                        
                        if (verbose >=3) {
                            std::cout << "...with order\n";
                        }
                        
                        msgpack_pack_array(&pk, element_order->size());
                        for (size_t i = 0; i < element_order->size(); ++i) {
                            if ( element_order->at(i) >= num_elements ) {
                                throw(std::invalid_argument("invalid index in element order"));
                            }
                            auto curElem = container->getElement(v.getData(), element_order->at(i));
                            extractDataByPath(curElem, toplevelPath, targetPath);                      
                        }
                    } else {                
                        msgpack_pack_array(&pk, num_elements); 
                        
                        for (size_t i = 0; i < num_elements; ++i) {
                            auto curElem = container->getElement(v.getData(), i);
                            extractDataByPath(curElem, toplevelPath, targetPath);
                        }                  
                    }                   
                }
            }
            break;
            
        default:
            throw(std::runtime_error("targetPath cannot be reached, type unknown"));   
            break;
            
            
    }
    
    return true;
}

    

int Converter::convertMetaData(
    pocolog_cpp::InputDataStream* stream,
    const int start, const int end
) {
    int exitStatus = EXIT_SUCCESS;
    std::string timeKey = "timestamps";
    std::string typeKey = "type";

    pocolog_cpp::Index& streamIndex = stream->getFileIndex();

    const std::string key = stream->getName() + ".meta";

    const int realEnd = computeRangeEnd(start, end, stream->getSize(), false);
    int exportedSize = realEnd - start;

    if(exportedSize < 0)
    {
        std::cerr << "[pocolog2msgpack] No samples in requested range for meta stream \""
                    << key << "\"." << std::endl;
        exportedSize = 0;
    }

    if (flatten) {
        typeKey = key + "/" + typeKey;
        timeKey = key + "/" + timeKey;
    } else {
        msgpack_pack_str(&pk, key.size());
        msgpack_pack_str_body(&pk, key.c_str(), key.size());

        msgpack_pack_map(&pk, 2);
    }

    msgpack_pack_str(&pk, timeKey.size());
    msgpack_pack_str_body(&pk, timeKey.c_str(), timeKey.size());


    msgpack_pack_array(&pk, exportedSize);

    if(exportedSize > 0)
    {
        for(size_t t = start; t < realEnd; t++)
        {
            msgpack_pack_int64(&pk, streamIndex.getSampleTime(t).microseconds);
        }
    }

    msgpack_pack_str(&pk, typeKey.size());
    msgpack_pack_str_body(&pk, typeKey.c_str(), typeKey.size());
    const std::string typeName = stream->getType()->getName();
    msgpack_pack_str(&pk, typeName.size());
    msgpack_pack_str_body(&pk, typeName.c_str(), typeName.size());
    

    return exitStatus;
}

Converter::Converter(
    Typelib::Type const& type, 
    msgpack_packer& pk, 
    int containerLimit, 
    bool flatten, 
    bool align_named_vector, 
    int verbose
) : type(type), 
    pk(pk),
    containerLimit(containerLimit), 
    flatten(flatten),
    align_named_vector(align_named_vector),
    verbose(verbose),
    depth(0),
    indentation(1)
{

    debug = (verbose > 3);

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

int Converter::convertSamples(
    pocolog_cpp::InputDataStream* stream,
    const int start, const int end
) {
    const float reportProgressDelta = 0.1;
    float nextReportProgress = reportProgressDelta;

    auto number_of_samples = end - start;
    
    
    if (!flatten) {
        
        const std::string streamName = stream->getName();
        fieldName.push_back(streamName);        
        msgpack_pack_str(&pk, streamName.size());
        msgpack_pack_str_body(&pk, streamName.c_str(), streamName.size());            
        msgpack_pack_array(&pk, number_of_samples);
            
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
                std::cout << "[pocolog2msgpack] Converting sample of size "
                        << curSampleData.size() << std::endl;
            }

            if(verbose >= 4)
            {
                for(size_t i = 0; i < curSampleData.size(); i++)
                {
                    if(i % 32 == 0 && i > 0) std::cout << std::endl;
                    std::ios_base::fmtflags f(std::cout.flags());
                    std::cout << std::setfill('0') << std::setw(2) << std::right << std::hex
                            << (unsigned int)(curSampleData[i]) << " ";
                    std::cout.flags(f);
                    std::cout << " ";

                    if(i > 128)
                    {
                        std::cout << "...";
                        break;
                    }
                }
                std::cout << std::endl;
            }

            convertSample(curSampleData);
            
            if (compress) {
                writeOutZBuffer();
            }
            
            const float progress = ((float)(t - start)) / (end - start);
            if(progress >= nextReportProgress)
            {
                nextReportProgress += reportProgressDelta;
                if(verbose >= 1)
                {
                    std::cout << "[pocolog2msgpack] " << (int)(progress * 100)
                            << "% of stream done." << std::endl;
                }
            }
        }
    } else {
        
        
        const Typelib::Type * cur_type = stream->getType();
               
        
        std::vector<std::vector<std::string>> paths;
        std::vector<std::vector<std::string>> nv_paths;
        std::vector<std::string> toplevelPath = {stream->getName()};
        inspectTypeLevels(*cur_type, toplevelPath, paths); 
        
        std::string curPath;
        
        //msgpack_pack_map(&pk, paths.size());
        int n_path_finished = 0;        
    
        std::vector<uint8_t> buffer;
        buffer.resize(cur_type->getSize()); 
        
        std::vector<std::vector<uint8_t>> buffer_cache;
        std::vector<Typelib::Value> value_cache;
        buffer_cache.reserve(end-start);
        value_cache.reserve(end-start);
            
        for (auto p : paths) {
            curPath = "";
            
            bool first = true;
            
            for (auto dir : p) {
                if (first)                    
                    curPath = dir;
                else
                    curPath = curPath + "/" + dir;
                
                first = false;
            }
            std::cout << "[pocolog2msgpack] Extracting path \"";
            std::cout << curPath << "\"...";
            std::cout << std::endl;
                    
            msgpack_pack_str(&pk, curPath.size());
            msgpack_pack_str_body(&pk, curPath.c_str(), curPath.size());
            
        
            if (align_named_vector && 
                p.back() == "names" && 
                Converter::isNamedVectorField(*cur_type, toplevelPath, p)
            ) {
                // just one sample with the names
                // so...nothing to prepare
            } else {
                // an array of all samples
                msgpack_pack_array(&pk, number_of_samples);
            }
            
            // todo: end < or <= ??
            for (size_t cur_idx=start; cur_idx<end; cur_idx++) { 
                
//                 if (buffer_cache.size() < cur_idx-start+1) {
//                     buffer_cache.emplace_back(cur_type->getSize());
//                     
//                     auto & cur_buf = buffer_cache[cur_idx-start];
//                     stream->getTyplibValue(cur_buf.data(), stream->getTypeMemorySize(), cur_idx);
//                     value_cache.emplace_back(cur_buf.data(), *stream->getType());
//                 }
//                 
//                 extractDataByPath(value_cache[cur_idx-start], toplevelPath, p);
                
                stream->getTyplibValue(buffer.data(), stream->getTypeMemorySize(), cur_idx);
                Typelib::Value v(buffer.data(), *stream->getType());                                
                extractDataByPath(v, toplevelPath, p);
                Typelib::destroy(v);
                
                if (compress) {
                    writeOutZBuffer();
                }
                
                
                float progress = ((float)(cur_idx - start)) / (end - start);
                progress *=  n_path_finished;
                progress /= paths.size();
                
                if(progress >= nextReportProgress)
                {
                    nextReportProgress += reportProgressDelta;
                    if(verbose >= 1)
                    {
                        std::cout << "[pocolog2msgpack] " << (int)(progress * 100)
                                << "% of stream done." << std::endl;
                    }
                }
            }
            
            ++n_path_finished;
            
        }
        
        for ( auto & v : value_cache) {
            Typelib::destroy(v);
        }
        value_cache.clear();
        buffer_cache.clear();
        
    }
    return EXIT_SUCCESS;
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

bool Converter::visit_ (int8_t & v)
{
    if(debug)
    {
        printBegin();
        std::cout << "got int8 " << (int)v << std::endl;
    }

    if(modeNumericToString)
    {
        numericToStringBuffer += (char)v;
        return true;
    }

    msgpack_pack_int8(&pk, v);
    return true;
}

bool Converter::visit_ (uint8_t & v)
{
    if(debug)
    {
        printBegin();
        std::cout << "got uint8 " << (unsigned int) v << std::endl;
    }

    if(modeNumericToString)
    {
        numericToStringBuffer += (char)v;
        return true;
    }

    msgpack_pack_uint8(&pk, v);
    return true;
}

bool Converter::visit_ (int16_t & v)
{
    if(debug)
    {
        printBegin();
        std::cout << __FUNCTION__ << "(int16_t & v) got " << v << std::endl;
    }
    msgpack_pack_int16(&pk, v);
    return true;
}
bool Converter::visit_ (uint16_t & v)
{
    if(debug)
    {
        printBegin();
        std::cout << __FUNCTION__ << "(uint16_t & v) got " << v << std::endl;
    }
    msgpack_pack_uint16(&pk, v);
    return true;
}
bool Converter::visit_ (int32_t & v)
{
    if(debug)
    {
        printBegin();
        std::cout << __FUNCTION__ << "(int32_t & v) got " << v << std::endl;
    }
    msgpack_pack_int32(&pk, v);
    return true;
}
bool Converter::visit_ (uint32_t& v)
{
    if(debug)
    {
        printBegin();
        std::cout << __FUNCTION__ << "(uint32_t& v) got " << v << std::endl;
    }
    msgpack_pack_uint32(&pk, v);
    return true;
}
bool Converter::visit_ (int64_t & v)
{
    if(debug)
    {
        printBegin();
        std::cout << __FUNCTION__ << "(int64_t & v) got " << v << std::endl;
    }
    msgpack_pack_int64(&pk, v);
    return true;
}
bool Converter::visit_ (uint64_t& v)
{
    if(debug)
    {
        printBegin();
        std::cout << __FUNCTION__ << "(uint64_t& v) got " << v << std::endl;
    }
    msgpack_pack_uint64(&pk, v);
    return true;
}
bool Converter::visit_ (float & v)
{
    if(debug)
    {
        printBegin();
        std::cout << __FUNCTION__ << "(float & v) got " << v << std::endl;
    }
    msgpack_pack_float(&pk, v);
    return true;
}
bool Converter::visit_ (double & v)
{
    if(debug)
    {
        printBegin();
        std::cout << __FUNCTION__ << "(double & v) got " << v << std::endl;
    }
    msgpack_pack_double(&pk, v);

    return true;
}

bool Converter::visit_(Typelib::Value const& v, Typelib::OpaqueType const& type)
{
    if(debug)
    {
        printBegin();
        std::cout << __FUNCTION__ << "OpaqueType at " << v.getData() << std::endl;
    }
    return true;
}

bool Converter::visit_(Typelib::Value const& v, Typelib::Pointer const& type)
{
    if(debug)
    {
        printBegin();
        std::cout << __FUNCTION__ << "Pointer at " << v.getData() << std::endl;
    }

    depth += 1;
    bool retval = Typelib::ValueVisitor::visit_(v, type);
    depth -= 1;

    return retval;
}

bool Converter::visit_(Typelib::Value const& v, Typelib::Array const& type)
{
    if(debug)
    {
        printBegin();
        std::cout << __FUNCTION__ << "Array at " << v.getData() << std::endl;
        printBegin();
        std::cout << "array[" << type.getDimension() << "]" << std::endl;
    }

    msgpack_pack_array(&pk, type.getDimension());

    depth += 1;
    bool retval = Typelib::ValueVisitor::visit_(v, type);
    depth -= 1;

    return retval;
}

bool Converter::visit_(Typelib::Value const& v, Typelib::Container const& type)
{
    size_t numElements = type.getElementCount(v.getData());

    if(debug)
    {
        printBegin();
        std::cout << __FUNCTION__ << "Container at " << v.getData() << std::endl;
        printBegin();
        std::cout << "numElements: " << numElements << std::endl;
        printBegin();
        std::cout << type.kind() << "[" << numElements << "]" << std::endl;
    }

    if(numElements > containerLimit)
    {
        std::cerr << "[pocolog2msgpack] Container size " << numElements 
                  << " exceeds limit of " << containerLimit << ". Skipping..."
                  << std::endl;
                  
        return false;
    }

    bool retval = false;

    if(type.kind() == "/std/string")
    {
        numericToStringBuffer = "";
        modeNumericToString = true;

        depth += 1;
        retval = Typelib::ValueVisitor::visit_(v, type);
        depth -= 1;

        modeNumericToString = false;

        if(debug)
        {
            printBegin();
            std::cout << "got str: \"" << numericToStringBuffer << "\"" << std::endl;
        }

        msgpack_pack_str(&pk, numericToStringBuffer.size());
        msgpack_pack_str_body(&pk, numericToStringBuffer.c_str(), numericToStringBuffer.size());

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
    if(debug)
    {
        printBegin();
        std::cout << "compound '" << type.getName() << "' with " << type.getFields().size() << " fields" << std::endl;
    }

    msgpack_pack_map(&pk, type.getFields().size());

    depth += 1;
    bool retval = Typelib::ValueVisitor::visit_(v, type);
    depth -= 1;

    return retval;
}

bool Converter::visit_(Typelib::Value const& v, Typelib::Compound const& type, Typelib::Field const& field)
{
    if(debug)
    {
        printBegin();
        std::cout << "field '" << field.getName() << "'" << std::endl;
    }

    msgpack_pack_str(&pk, field.getName().size());
    msgpack_pack_str_body(&pk, field.getName().c_str(), field.getName().size());

    fieldName.push_back(field.getName());

    depth += 1;
    bool retval = Typelib::ValueVisitor::visit_(v, type, field);
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
