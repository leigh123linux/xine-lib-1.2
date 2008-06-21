/// \file streaminfo.h

#ifndef _musepack_streaminfo_h_
#define _musepack_streaminfo_h_

typedef mpc_int32_t mpc_streaminfo_off_t;

/// \brief mpc stream properties structure
///
/// Structure containing all the properties of an mpc stream.  Populated
/// by the streaminfo_read function.
typedef struct mpc_streaminfo {
    /// @name core mpc stream properties
    //@{
    mpc_uint32_t         sample_freq;        ///< sample frequency of stream
    mpc_uint32_t         channels;           ///< number of channels in stream
    mpc_streaminfo_off_t header_position;    ///< byte offset of position of header in stream
    mpc_uint32_t         stream_version;     ///< streamversion of stream
    mpc_uint32_t         bitrate;            ///< bitrate of stream file (in bps)
    double               average_bitrate;    ///< average bitrate of stream (in bits/sec)
    mpc_uint32_t         frames;             ///< number of frames in stream
    mpc_int64_t          pcm_samples;
    mpc_uint32_t         max_band;           ///< maximum band-index used in stream (0...31)
    mpc_uint32_t         is;                 ///< intensity stereo (0: off, 1: on)
    mpc_uint32_t         ms;                 ///< mid/side stereo (0: off, 1: on)
    mpc_uint32_t         block_size;         ///< only needed for SV4...SV6 -> not supported
    mpc_uint32_t         profile;            ///< quality profile of stream
    const char*          profile_name;       ///< name of profile used by stream
    //@}

    /// @name replaygain related fields
    //@{
    mpc_int16_t         gain_title;          ///< replaygain title value 
    mpc_int16_t         gain_album;          ///< replaygain album value
    mpc_uint16_t        peak_album;          ///< peak album loudness level
    mpc_uint16_t        peak_title;          ///< peak title loudness level
    //@}

    /// @name true gapless support data
    //@{
    mpc_uint32_t        is_true_gapless;     ///< true gapless? (0: no, 1: yes)
    mpc_uint32_t        last_frame_samples;  ///< number of valid samples within last frame

    mpc_uint32_t        encoder_version;     ///< version of encoder used
    char                encoder[256];        ///< encoder name

    mpc_streaminfo_off_t tag_offset;         ///< offset to file tags
    mpc_streaminfo_off_t total_file_length;  ///< total length of underlying file
    //@}
} mpc_streaminfo;

#endif // _musepack_streaminfo_h_
