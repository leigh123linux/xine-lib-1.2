/// \file reader.h

#ifndef _musepack_reader_h_
#define _musepack_reader_h_

/// \brief Stream reader interface structure.
///
/// This is the structure you must supply to the musepack decoding library
/// to feed it with raw data.  Implement the five member functions to provide
/// a functional reader.
typedef struct mpc_reader_t {
    /// Reads size bytes of data into buffer at ptr.
	mpc_int32_t (*read)(void *t, void *ptr, mpc_int32_t size);

    /// Seeks to byte position offset.
	mpc_bool_t (*seek)(void *t, mpc_int32_t offset);

    /// Returns the current byte offset in the stream.
	mpc_int32_t (*tell)(void *t);

    /// Returns the total length of the source stream, in bytes.
	mpc_int32_t (*get_size)(void *t);

    /// True if the stream is a seekable stream.
	mpc_bool_t (*canseek)(void *t);

    /// Optional field that can be used to identify a particular instance of
    /// reader or carry along data associated with that reader.
    void *data;

    // These are used by provided internal standard file-based reader implementation.
    // You shouldn't touch them.  They're included in the main struct to avoid
    // malloc/free.
    FILE *file;
    long file_size;
    mpc_bool_t is_seekable;
} mpc_reader;

/// Initializes reader with default stdio file reader implementation.  Use
/// this if you're just reading from a plain file.
///
/// \param r reader struct to initalize
/// \param input input stream to attach to the reader
void mpc_reader_setup_file_reader(mpc_reader *r, FILE *input);

#endif // _musepack_reader_h_
