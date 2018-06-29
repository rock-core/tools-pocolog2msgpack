#pragma once
#include <msgpack.h>
#include <msgpack/fbuffer.h>
#include <typelib/typemodel.hh>
#include <typelib/typevisitor.hh>


/**
 * Convert logfile.
 * @param logfiles names of the pocolog logfiles
 * @param output name of the MsgPack logfile, will be created
 * @param size size of the size type for containers in the logfile
 * @param containerLimit maximum lenght of a container that will be converted
 * @param exclude exclude port from conversion
 * @param only only convert the port given by this argument
 * @param start index of the first sample that will be exported
 * @param end index after the last sample that will be exported
 * @param verbose verbosity level
 * @return exit status of the program
 */
int convert(const std::vector<std::string>& logfiles, const std::string& output,
            const int size, const int containerLimit, const std::string& exclude,
            const std::string& only, const int start, const int end, const int verbose);


/**
 * Actual converter logic for one sample.
 *
 * The converter loads a single type from its serialized typelib representation
 * and stores it in MessagePack format. The typelib type information must be
 * provided.
 */
class Converter : public Typelib::TypeVisitor
{
    /** Pointer to the serialized typelib type. */
    uint8_t* data;
    /** Typelib's type description. */
    Typelib::Type const& type;
    /** Size of the size type for containers in the logfile. */
    int size;
    /** Maximum lenght of a container that will be converted. */
    int containerLimit;
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
public:
    Converter(std::string const& basename, Typelib::Type const& type, msgpack_packer& pk, int size, int containerLimit, int verbose);
    virtual ~Converter();
    void convertSample(uint8_t* data);
protected:
    bool visit_(Typelib::OpaqueType const& type);
    bool visit_(Typelib::Numeric const& type);
    unsigned int getUnsignedInt(Typelib::Numeric const& type, bool part);
    signed int getSignedInt(Typelib::Numeric const& type, bool part);
    double getFloat(Typelib::Numeric const& type, bool part);
    bool visit_(Typelib::Enum const&);
    bool visit_(Typelib::Pointer const& type);
    bool visit_(Typelib::Array const& type);
    bool visit_(Typelib::Container const& type);
    bool visit_(Typelib::Compound const& type);
    bool visit_(Typelib::Compound const& type, Typelib::Field const& field);

    using TypeVisitor::visit_;
private:
    void reset();
    void printBegin();
};
