#pragma once

#include "Range.h"

enum class FdcType { None, PC, WD, Amiga, Apple };
enum class RegularFormat {
    None,
    MGT, ProDos,
    TRDOS, QDOS, OPD, D80,
    PC320, PC360, PC640, PC720, PC1200, PC1232, PC1440, PC2880,
    TO_640K_MFM, TO_320K_MFM, TO_160K_MFM, TO_160K_FM, TO_80K_FM,
    AmigaDOS, AmigaDOSHD,
    MBD820, MBD1804,
    D2M, D4M,
    _2D, D81, LIF, AtariST, DO
};

struct Format
{
    static constexpr int DefaultTracks = 80;
    static constexpr int DefaultSides = 2;

    Format() = default;
    Format(RegularFormat reg_fmt);

    int sector_size() const;
    int track_size() const;
    int side_size() const;
    int disk_size() const;
    int total_sectors() const;
    Range range() const;

    VectorX<int> get_ids(const CylHead& cylhead) const;
    void Validate() const;
    bool TryValidate() const;
    void Override(bool full_control = false);
    bool IsNone() const
    {
        return regular_format == RegularFormat::None;
    }
    bool IsSameCylHeadSectorsSize(const Format& format) const
    {
        return cyls == format.cyls && heads == format.heads && sectors == format.sectors && size == format.size;
    }

    static Format GetFormat(RegularFormat reg_fmt);
    static bool FromSize(int64_t size, Format& fmt);
    static void Validate(int cyls, int heads, int sectors = 1, int sector_size = 512, int max_size = 0);
    static bool TryValidate(int cyls, int heads, int sectors = 1, int sector_size = 512, int max_size = 0);

    std::string ToString(bool onlyRelevantData = true) const
    {
        std::ostringstream ss;
        ss << "cyls=" << cyls << ", heads=" << heads << ", sectors=" << sectors
           << ", size=" << size << ", base=" << base << ", datarate=" << datarate
           << ", encoding=" << encoding;
        if (!onlyRelevantData)
        {
                ss << ", offset=" << offset << ", interleave=" << interleave
                   << ", skew=" << skew << ", gap3=" << gap3 << ", fill=" << fill
                   << ", cyls_first=" << cyls_first << ", format=" << int(regular_format);
        }
        return ss.str();
    }

    friend std::string to_string(const Format& format, bool onlyRelevantData = true)
    {
        std::ostringstream ss;
        ss << format.ToString(onlyRelevantData);
        return ss.str();
    }

    int cyls = DefaultTracks;
    int heads = DefaultSides;

    FdcType fdc = FdcType::PC;              // FDC type for head matching rules
    DataRate datarate = DataRate::Unknown;  // Data rate within encoding
    Encoding encoding = Encoding::Unknown;  // Data encoding scheme
    int sectors = 0, size = 2;              // sectors/track, sector size code
    int base = 1, offset = 0;               // base sector number, offset into cyl 0 head 0
    int interleave = 1, skew = 0;           // sector interleave, track skew
    int head0 = 0, head1 = 1;               // head 0 value, head 1 value
    int gap3 = 0;                           // inter-sector gap
    uint8_t fill = 0x00;                    // Fill byte
    bool cyls_first = false;                // True if media order is cyls on head 0 before head 1
    RegularFormat regular_format = RegularFormat::None; // The regular format used at constructing.
};

inline std::ostream& operator<<(std::ostream& os, const Format& format) { return os << format.ToString(); }
