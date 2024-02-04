// Copy command

#include "Options.h"
#include "DiskUtil.h"
#include "Image.h"
#include "MemFile.h"
#include "SAMdisk.h"
#include "SAMCoupe.h"
#include "Trinity.h"
//#include "SpectrumPlus3.h"
#include "Util.h"

#include <cstring>
#include <strings.h>

static auto& opt_encoding = getOpt<Encoding>("encoding");
static auto& opt_fix = getOpt<int>("fix");
static auto& opt_merge = getOpt<int>("merge");
static auto& opt_minimal = getOpt<int>("minimal");
static auto& opt_nozip = getOpt<int>("nozip");
static auto& opt_quick = getOpt<int>("quick");
static auto& opt_range = getOpt<Range>("range");
static auto& opt_repair = getOpt<int>("repair");
static auto& opt_resize = getOpt<int>("resize");
static auto& opt_step = getOpt<int>("step");
static auto& opt_verbose = getOpt<int>("verbose");

bool ImageToImage(const std::string& src_path, const std::string& dst_path)
{
    auto src_disk = std::make_shared<Disk>();
    auto dst_disk = std::make_shared<Disk>();
    ScanContext context;

    // Force Jupiter Ace reading mode if the output file extension is .dti
    // ToDo: add mechanism for target to hint how source should be read?
    if (opt_encoding != Encoding::Ace && IsFileExt(dst_path, "dti"))
    {
        opt_encoding = Encoding::Ace;
        Message(msgInfo, "assuming --encoding=Ace due to .dti output image");
    }

    // Read the source image
    if (!ReadImage(src_path, src_disk))
        return false;

    // For merge or repair, read any existing target image
    if (opt_merge || opt_repair)
    {
        // Read the image, ignore merge or repair if that fails
        if (!ReadImage(dst_path, dst_disk, false))
            throw util::exception("target image not found");
    }

    // Limit to our maximum geometry, and default to copy everything present in the source
    ValidateRange(opt_range, MAX_TRACKS, MAX_SIDES, opt_step, src_disk->cyls(), src_disk->heads());

    if (opt_minimal)
        TrackUsedInit(*src_disk);

    const bool skip_stable_sectors = opt.skip_stable_sectors && !src_disk->is_constant_disk() ? true : false;

    // If repair mode and normal disk then determine normal track size by calculating the average track size.
    int normal_track_size = 0;
    int normal_first_sector_id = 1;
    if (opt.repair && opt.normal_disk) {
        // If sectors is specified then it is the normal track size.
        if (opt.sectors > 0)
        {
            normal_track_size = opt.sectors;
        }
        else
        {
            int dst_track_amount = 0;
            int sum_dst_track_size = 0;
            opt.range.each([&](const CylHead& cylhead) {
                Track dst_track = dst_disk->read_track(cylhead);
                NormaliseTrack(cylhead, dst_track);
                auto track_normal_probable_size = dst_track.normal_probable_size();
                if (track_normal_probable_size > 0) {
                    sum_dst_track_size += track_normal_probable_size;
                    dst_track_amount++;
                }
            });
            if (dst_track_amount > 0)
                normal_track_size = static_cast<int>(std::round(static_cast<double>(sum_dst_track_size) / dst_track_amount));
        }
        if (opt.base > 0)
            normal_first_sector_id = opt.base;
    }

    // Copy the range of tracks to the target image
    opt_range.each([&](const CylHead& cylhead) {
        // In minimal reading mode, skip unused tracks
        if (opt_minimal && !IsTrackUsed(cylhead.cyl, cylhead.head))
            return;

        Track dst_track;
        Message(msgStatus, "Reading %s", CH(cylhead.cyl, cylhead.head));
        if (opt_repair) // Read dst track early so we can check if it has bad sectors.
        {
            dst_track = dst_disk->read_track(cylhead);
            NormaliseTrack(cylhead, dst_track);
        }

        Headers headers_of_stable_sectors;
        // If repair mode and user specified skip_stable_sectors then skip checking those.
        if (opt_repair && skip_stable_sectors) // Repair mode => dst track can be checked.
        {
            headers_of_stable_sectors = dst_track.stable_sectors().headers();
            // If repair mode and user specified normal-disk then do not repair tracks
            // which has track size amount of id sequence at least.
            if (opt.normal_disk && normal_track_size > 0
                && headers_of_stable_sectors.has_id_sequence(normal_first_sector_id, normal_track_size))
                return;
        }
        if (opt_verbose && opt_repair && !headers_of_stable_sectors.empty()) {
            Message(msgInfo, "Ignoring already good sectors on %s: %s",
                CH(cylhead.cyl, cylhead.head), headers_of_stable_sectors.sector_ids_to_string().c_str());
        }

        auto src_data = src_disk->read(cylhead * opt_step);
        auto src_track = src_data.track();

        if (src_data.has_bitstream())
        {
            auto bitstream = src_data.bitstream();
            if (NormaliseBitstream(bitstream))
            {
                src_data = TrackData(src_data.cylhead, std::move(bitstream));
                src_track = src_data.track();
            }
        }

        bool changed = NormaliseTrack(cylhead, src_track);

        if (opt_verbose)
            ScanTrack(cylhead, src_track, context);

        // Repair or copy?
        if (opt_repair)
        {
            // Repair the target track using the source track.
            RepairTrack(cylhead, dst_track, src_track);

            dst_disk->write(cylhead, std::move(dst_track));
        }
        else
        {
            // If the source track was modified it becomes the only track data.
            if (changed)
                dst_disk->write(cylhead, std::move(src_track));
            else
            {
                // Preserve any source data.
                src_data.cylhead = cylhead;
                dst_disk->write(std::move(src_data));
            }
        }
        }, opt_verbose != 0);

    // Copy any metadata not already present in the target (emplace doesn't replace)
    for (const auto& m : src_disk->metadata)
        dst_disk->metadata.emplace(m);

    // Write the new/merged target image
    return WriteImage(dst_path, dst_disk);
}

bool Image2Trinity(const std::string& path, const std::string&/*trinity_path*/) // ToDo: use trinity_path for record
{
    auto disk = std::make_shared<Disk>();
    const Sector* s = nullptr;
    const MGT_DIR* pdir = nullptr;
    std::string name;

    if (!ReadImage(path, disk))
        return false;

    // Scanning the first 10 directory sectors should be enough
    for (auto sector = 1; !pdir && sector <= MGT_SECTORS; ++sector)
    {
        if (!disk->find(Header(0, 0, sector, 2), s))
            break;

        // Each sector contains 2 MGT directory entries
        for (auto entry = 0; !pdir && entry < 2; ++entry)
        {
            pdir = reinterpret_cast<const MGT_DIR*>(s->data_copy().data() + 256 * entry);

            // Extract the first 4 characters of a potential MGT filename
            name = std::string(reinterpret_cast<const char*>(pdir->abName), 10);

            // Reject if not a code file, or no auto-execute, or not named AUTO*
            if ((pdir->bType & 0x3f) != 19 || pdir->bExecutePage == 0xff ||
                strcasecmp(name.substr(0, 4).c_str(), "auto"))
                pdir = nullptr;
        }
    }

    if (!pdir)
        throw util::exception("no suitable auto-executing code found");

    auto start_addr = TPeek(&pdir->bStartPage, 16384);
    auto exec_addr = TPeek(&pdir->bExecutePage);
    auto file_length = TPeek(&pdir->bLengthInPages);

    // Check the code wouldn't overwrite TrinLoad
    if (start_addr < 0x8000 && (start_addr + file_length) >= 0x6000)
        throw util::exception("code overlaps 6000-7fff range used by TrinLoad");

    auto uPos = 0;
    MEMORY mem(file_length);

    // The file start sector is taken from the directory entry.
    // Code files have a 9-byte metadata header, which must be skipped.
    auto trk = pdir->bStartTrack;
    auto sec = pdir->bStartSector;
    auto uOffset = MGT_FILE_HEADER_SIZE;

    // Loop until we've read the full file
    while (uPos < file_length)
    {
        // Bit 7 of the track number indicates head 1
        uint8_t cyl = (trk & 0x7f);
        uint8_t head = trk >> 7;

        if (!disk->find(Header(cyl, head, sec, 2), s) || s->data_size() != SECTOR_SIZE)
            throw util::exception("end of file reading ", CylHead(cyl, head), " sector", sec);

        // Determine the size of data in this sector, which is at most 510 bytes.
        // The final 2 bytes contain the location of the next sector.
        auto uBlock = std::min(file_length - uPos, 510 - uOffset);
        auto& data = s->data_copy();

        memcpy(mem + uPos, data.data() + uOffset, uBlock);
        uPos += uBlock;
        uOffset = 0;

        // Follow the chain to the next sector
        trk = data[510];
        sec = data[511];
    }

    // Scan the local network for SAM machines running TrinLoad
    auto trinity = Trinity::Open();
    auto devices = trinity->devices();

    // Send the code file to the first device that responded
    Message(msgStatus, "Sending %s to %s...", name.c_str(), devices[0].c_str());
    trinity->send_file(mem, mem.size, start_addr, exec_addr);

    return true;
}

bool Hdd2Hdd(const std::string& src_path, const std::string& dst_path)
{
    bool f = false;
    bool fCreated = false;

    auto src_hdd = HDD::OpenDisk(src_path);
    if (!src_hdd)
    {
        Error("open");
        return false;
    }

    auto dst_hdd = HDD::OpenDisk(dst_path);
    if (!dst_hdd)
    {
        dst_hdd = HDD::CreateDisk(dst_path, src_hdd->total_bytes, &src_hdd->sIdentify);
        fCreated = true;
    }

    if (!dst_hdd)
        Error("create");
    else if (src_hdd->total_sectors != dst_hdd->total_sectors && !opt_resize)
        throw util::exception("Source size (", src_hdd->total_sectors, " sectors) does not match target (", dst_hdd->total_sectors, " sectors)");
    else if (src_hdd->sector_size != dst_hdd->sector_size)
        throw util::exception("Source sector size (", src_hdd->sector_size, " bytes) does not match target (", dst_hdd->sector_size, " bytes)");
    else if ((fCreated || dst_hdd->SafetyCheck()) && dst_hdd->Lock())
    {
        BDOS_CAPS bdcSrc, bdcDst;

        if (opt_resize && IsBDOSDisk(*src_hdd, bdcSrc))
        {
            GetBDOSCaps(dst_hdd->total_sectors, bdcDst);

            auto uBase = std::min(bdcSrc.base_sectors, bdcDst.base_sectors);
            auto uData = std::min(src_hdd->total_sectors - bdcSrc.base_sectors, dst_hdd->total_sectors - bdcDst.base_sectors);
            auto uEnd = opt_quick ? uBase + uData : dst_hdd->total_sectors;

            // Copy base sectors, clear unused base sector area, copy record data sectors
            f = dst_hdd->Copy(src_hdd.get(), uBase, 0, 0, uEnd);
            f &= dst_hdd->Copy(nullptr, bdcDst.base_sectors - uBase, 0, uBase, uEnd);
            f &= dst_hdd->Copy(src_hdd.get(), uData, bdcSrc.base_sectors, bdcDst.base_sectors, uEnd);

            MEMORY mem(dst_hdd->sector_size);

            // If the record count isn't a multiple of 32, the final record list entry won't be full
            if (bdcDst.records % (dst_hdd->sector_size / BDOS_LABEL_SIZE) &&
                dst_hdd->Seek(bdcDst.base_sectors - 1) && dst_hdd->Read(mem, 1))
            {
                // Determine the number of used entries in the final record list sector
                auto used_bytes = (bdcDst.records * BDOS_LABEL_SIZE) & (dst_hdd->sector_size - 1);

                // Clear the unused space to remove any entries lost by resizing, and write it back
                memset(mem + used_bytes, 0, dst_hdd->sector_size - used_bytes);
                dst_hdd->Seek(bdcDst.base_sectors - 1);
                dst_hdd->Write(mem, 1);
            }

            // If this isn't a quick copy, clear the remaining destination space
            if (!opt_quick)
                f &= dst_hdd->Copy(nullptr, dst_hdd->total_sectors - (bdcDst.base_sectors + uData), 0x00, bdcDst.base_sectors + uData, uEnd);

            // If the source disk is bootable, consider updating the BDOS variables
            if (opt_fix != 0 && bdcSrc.bootable && dst_hdd->Seek(0) && dst_hdd->Read(mem, 1))
            {
                // Check for compatible DOS version
                if (UpdateBDOSBootSector(mem, *dst_hdd))
                {
                    dst_hdd->Seek(0);
                    dst_hdd->Write(mem, 1);
                }
            }
        }
        else
        {
            auto uCopy = std::min(src_hdd->total_sectors, dst_hdd->total_sectors);
            f = dst_hdd->Copy(src_hdd.get(), uCopy);
        }

        dst_hdd->Unlock();
    }

    return f;
}

bool Hdd2Boot(const std::string& path, const std::string& boot_path)
{
    bool fRet = false;
    FILE* file = nullptr;

    auto hdd = HDD::OpenDisk(path.substr(0, path.rfind(':')));

    if (!hdd)
        Error("open");
    else
    {
        MEMORY mem(hdd->sector_size);

        if (!hdd->Seek(0) || !hdd->Read(mem, 1))
            Error("read");

        file = fopen(boot_path.c_str(), "wb");
        if (!file)
            Error("open");
        else if (!fwrite(mem, mem.size, 1, file))
            Error("write");
        else
            fRet = true;
    }

    if (file) fclose(file);
    return fRet;
}

bool Boot2Hdd(const std::string& boot_path, const std::string& hdd_path)
{
    bool fRet = false;
    MemFile file;

    auto hdd = HDD::OpenDisk(hdd_path.substr(0, hdd_path.rfind(':')));

    if (!hdd || !hdd->Seek(0))
        Error("open");
    else if (!file.open(boot_path, !opt_nozip))
        /* already reported */;
    else if (file.size() != hdd->sector_size)
        throw util::exception("boot sector must match sector size (", hdd->sector_size, " bytes)");
    else
    {
        MEMORY mem(hdd->sector_size);

        if (!file.read(mem, mem.size))
            Error("read");
        else if (hdd->SafetyCheck() && hdd->Lock())
        {
            // If we're writing a compatible AL+ boot sector, consider updating the BDOS variables
            if (opt_fix != 0)
                UpdateBDOSBootSector(mem, *hdd);

            if (!hdd->Write(mem, 1))
                Error("write");
            else
                fRet = true;

            hdd->Unlock();
        }
    }

    return fRet;
}

bool Boot2Boot(const std::string& src_path, const std::string& dst_path)
{
    bool fRet = false;

    auto hdd = HDD::OpenDisk(src_path.substr(src_path.rfind(':')));

    if (!hdd.get())
        Error("open");
    else
    {
        MEMORY mem(hdd->sector_size);

        if (!hdd->Seek(0) || !hdd->Read(mem, 1))
            Error("read");
        else
        {
            auto hdd2 = HDD::OpenDisk(dst_path.substr(dst_path.rfind(':')));

            if (!hdd2.get())
                Error("open2");
            else if (hdd2->sector_size != hdd->sector_size)
                throw util::exception("Source sector size (", hdd->sector_size, " bytes) does not match target (", hdd2->sector_size, " bytes)");
            else if (hdd2->SafetyCheck() && hdd2->Lock())
            {
                // If we're writing a compatible AL+ boot sector, consider updating the BDOS variables
                if (opt_fix != 0)
                    UpdateBDOSBootSector(mem, *hdd2);

                if (!hdd2->Seek(0) || !hdd2->Write(mem, 1))
                    Error("write");
                else
                    fRet = true;

                hdd2->Unlock();
            }
        }
    }

    return fRet;
}
