#pragma once

#include "TrackData.h"
#include "Format.h"
#include "DeviceReadingPolicy.h"

#include <cstdint>
#include <functional>
#include <vector>
#include <map>
#include <mutex>

#include <cstdint>
#include <functional>
#include <vector>
#include <map>
#include <mutex>

class Disk
{
public:
    Disk() = default;
    virtual ~Disk() = default;

    explicit Disk(Format& format);

    virtual bool preload(const Range& range, int cyl_step);
    virtual void clear();

    virtual bool is_constant_disk() const;
    virtual void disk_is_read();

    virtual const TrackData& read(const CylHead& cylhead, bool uncached = false, int with_head_seek_to = -1, const DeviceReadingPolicy& deviceReadingPolicy = DeviceReadingPolicy{});
    const Track& read_track(const CylHead& cylhead, bool uncached = false);
    const BitBuffer& read_bitstream(const CylHead& cylhead, bool uncached = false);
    const FluxData& read_flux(const CylHead& cylhead, bool uncached = false);

    virtual const TrackData& write(TrackData&& trackdata);
    const Track& write(const CylHead& cylhead, Track&& track);
    const BitBuffer& write(const CylHead& cylhead, BitBuffer&& bitbuf);
    const FluxData& write(const CylHead& cylhead, FluxData&& flux_revs, bool normalised = false);

    void each(const std::function<void(const CylHead & cylhead, const Track & track)>& func, bool cyls_first = false);

    void format(const RegularFormat& reg_fmt, const Data& data = Data(), bool cyls_first = false);
    void format(const Format& fmt, const Data& data = Data(), bool cyls_first = false);
    void flip_sides();
    void resize(int cyls, int heads);

    bool find(const Header& header, const Sector*& found_sector);
    const Sector& get_sector(const Header& header);

    Range range() const;
    int cyls() const;
    int heads() const;

    Format fmt{};
    std::map<std::string, std::string> metadata{};
    std::string strType = "<unknown>";

protected:
    std::map<CylHead, TrackData> m_trackdata{};
    std::mutex m_trackdata_mutex{};
};
