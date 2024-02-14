// Format and unformat commands

#include "Options.h"
#if 0
#include "types/record.h"
#endif
#include "Image.h"
#include "DiskUtil.h"
#include "SAMCoupe.h"
#include "Util.h"

#include <cstring>
#include <algorithm>

static auto& opt_boot = getOpt<std::string>("boot");
static auto& opt_byteswap = getOpt<int>("byteswap");
static auto& opt_cpm = getOpt<int>("cpm");
static auto& opt_encoding = getOpt<Encoding>("encoding");
static auto& opt_quick = getOpt<int>("quick");
static auto& opt_nosig = getOpt<int>("nosig");

/*
uint8_t abAtomLiteBoot[] =
{
    0xEB, 0x3C, 0x90, 0x41, 0x4C, 0x42, 0x32, 0x2E, 0x34, 0x18, 0x35, 0x00, 0x02, 0x02, 0x8E, 0x04,
    0x02, 0x20, 0x00, 0x00, 0x40, 0xF8, 0x01, 0x00, 0x3F, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x29, 0x00, 0x00, 0x00, 0x00, 0x53, 0x41, 0x4D, 0x43, 0x4F,
    0x55, 0x50, 0x45, 0x2D, 0x43, 0x46, 0x46, 0x41, 0x54, 0x31, 0x36, 0x20, 0x20, 0x20, 0xEB, 0xFE,
    0xD5, 0xC5, 0x21, 0x4F, 0x80, 0x11, 0x4F, 0x4F, 0x01, 0xB1, 0x01, 0xD5, 0xED, 0xB0, 0xC9, 0xC1,
    0x21, 0xF0, 0x4F, 0x7E, 0x23, 0xDB, 0xFE, 0xA6, 0x23, 0x7E, 0x23, 0x20, 0x07, 0xB9, 0xC0, 0x7E,
    0xB8, 0xC0, 0x18, 0x03, 0x2C, 0x20, 0xEC, 0xED, 0x5B, 0x08, 0x50, 0x7A, 0xB3, 0x28, 0x26, 0x3E,
    0x1F, 0xD3, 0xFA, 0xDB, 0xFB, 0x3C, 0xD3, 0xFB, 0x21, 0xF7, 0x7F, 0xCD, 0x36, 0x50, 0xC5, 0x01,
    0xF8, 0x10, 0x21, 0x0F, 0xE0, 0xED, 0xBB, 0xC1, 0x3A, 0x9F, 0x5C, 0xD3, 0xFC, 0xD6, 0x61, 0xD3,
    0xFB, 0x3E, 0x5F, 0xD3, 0xFA, 0xED, 0x5B, 0x0A, 0x50, 0x7A, 0xB3, 0xC8, 0x21, 0x00, 0x80, 0xCD,
    0x36, 0x50, 0x21, 0x03, 0x81, 0x11, 0x03, 0x50, 0x1A, 0xAE, 0xE6, 0x5F, 0xC0, 0x1B, 0x2D, 0xF2,
    0xA8, 0x4F, 0xD1, 0xDB, 0xFB, 0x32, 0xC2, 0x5B, 0x26, 0x51, 0x6F, 0x36, 0x60, 0x3C, 0x32, 0x0A,
    0x81, 0x3C, 0x32, 0x1C, 0x81, 0x3E, 0xA0, 0xCB, 0x61, 0x20, 0x06, 0xAF, 0x32, 0x0F, 0x50, 0x3E,
    0xA1, 0xB0, 0x32, 0x30, 0x50, 0x2A, 0x0C, 0x50, 0x22, 0x00, 0x80, 0xEB, 0x21, 0x0E, 0x50, 0x01,
    0x27, 0x00, 0xED, 0xB0, 0xC3, 0xAC, 0x50, 0x48, 0x69, 0x20, 0x53, 0x69, 0x6D, 0x6F, 0x6E, 0x21,
    0xF7, 0x01, 0xF5, 0x00, 0xF7, 0x02, 0xF5, 0x10, 0xF7, 0x04, 0xE5, 0x00, 0xF7, 0x08, 0xE5, 0x10,
    0x42, 0x4F, 0x4F, 0x54, 0x00, 0x00, 0x00, 0x00, 0x01, 0x06, 0x01, 0x04, 0xB7, 0x40, 0x07, 0xD0,
    0xD0, 0x44, 0x02, 0x20, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00,
    0xA0, 0xC9, 0x00, 0x00, 0x01, 0x00, 0xE5, 0x7A, 0x16, 0x00, 0x1D, 0x87, 0x30, 0x02, 0xC6, 0xA0,
    0x6F, 0x7A, 0x8A, 0x67, 0xEB, 0x19, 0xEB, 0x29, 0x29, 0x19, 0xED, 0x5B, 0x04, 0x50, 0x19, 0xED,
    0x5B, 0x06, 0x50, 0x30, 0x01, 0x13, 0x3E, 0xF2, 0xED, 0x79, 0x3E, 0x01, 0x0C, 0xED, 0x79, 0x0D,
    0x3E, 0xF3, 0xED, 0x79, 0x0C, 0xED, 0x69, 0x0D, 0x3C, 0xED, 0x79, 0x0C, 0xED, 0x61, 0x0D, 0x3C,
    0xED, 0x79, 0x0C, 0xED, 0x59, 0x0D, 0x3C, 0xED, 0x79, 0x0C, 0x78, 0xB2, 0xED, 0x79, 0x0D, 0xE1,
    0x3E, 0xF7, 0xED, 0x79, 0x0C, 0xED, 0x78, 0xFE, 0x50, 0x20, 0xFA, 0x3E, 0x20, 0xED, 0x79, 0xED,
    0x78, 0xFE, 0x58, 0x20, 0xFA, 0x3E, 0xF0, 0x0D, 0xED, 0x79, 0xC5, 0x0C, 0x06, 0xFE, 0xED, 0xB2,
    0xED, 0xB2, 0xED, 0x50, 0x7A, 0xED, 0x58, 0xC1, 0xB3, 0x20, 0x8B, 0xC9, 0x60, 0x68, 0x39, 0xDB,
    0xFB, 0x3D, 0xD3, 0xFB, 0x31, 0x00, 0xC0, 0xC5, 0xC5, 0xC5, 0xC5, 0xC5, 0xC5, 0xC5, 0xC5, 0xC5,
    0xC5, 0xC5, 0xC5, 0xC5, 0xC5, 0xC5, 0xC5, 0xC5, 0xC5, 0xC5, 0xC5, 0xC5, 0xC5, 0xC5, 0xC5, 0xC5,
    0xC5, 0xC5, 0xC5, 0xC5, 0xC5, 0xC5, 0xC5, 0x10, 0xDE, 0xFE, 0x04, 0x20, 0xD4, 0xF9, 0x2A, 0x11,
    0x50, 0x22, 0x06, 0x5A, 0x3E, 0xEF, 0x32, 0xAA, 0x59, 0x3A, 0x35, 0x50, 0xA7, 0xC8, 0x32, 0x08,
    0x5C, 0xC9, 0x45, 0x64, 0x77, 0x69, 0x6E, 0x20, 0x42, 0x6C, 0x69, 0x6E, 0x6B, 0x2E, 0x55, 0xAA
};
*/

bool FormatImage(const std::string& path, Range range)
{
    auto disk = std::make_shared<Disk>();
    ReadImage(path, disk);

    ValidateRange(range, MAX_TRACKS, MAX_SIDES, 1, disk->cyls(), disk->heads());

    // Start with MGT or ProDos format, but with automatic gap3.
    Format fmt{ !opt_cpm ? RegularFormat::MGT : RegularFormat::ProDos };
    fmt.gap3 = 0;

    // Halve the default sector count in FM to ensure it fits.
    if (opt_encoding == Encoding::FM)
        fmt.sectors /= 2;

    // Allow everything to be overridden, but check it's sensible.
    fmt.Override(true);
    fmt.Validate();

    util::cout <<
        util::fmt("%s %s, %2u cyls, %u heads, %2u sectors/track, %4u bytes/sector\n",
            to_string(fmt.datarate).c_str(), to_string(fmt.encoding).c_str(),
            range.cyls(), range.heads(), fmt.sectors, fmt.sector_size());

    range.each([&](const CylHead& cylhead) {
        Track track;
        track.format(cylhead, fmt);
        Message(msgStatus, "Formatting %s", strCH(cylhead.cyl, cylhead.head).c_str());
        disk->write(cylhead, std::move(track));
        }, fmt.cyls_first);

    return WriteImage(path, disk);
}

bool UnformatImage(const std::string& path, Range range)
{
    auto disk = std::make_shared<Disk>();
    ReadImage(path, disk);

    ValidateRange(range, MAX_TRACKS, MAX_SIDES, 1, disk->cyls(), disk->heads());

    range.each([&](const CylHead& cylhead) {
        Message(msgStatus, "Unformatting %s", strCH(cylhead.cyl, cylhead.head).c_str());
        disk->write(cylhead, Track());
        });

    return WriteImage(path, disk);
}

bool FormatHdd(const std::string& path)
{
    bool f = false;
    auto hdd = HDD::OpenDisk(path);

    /*
        MEMORY mem(SECTOR_SIZE);

        // Disk to be made BDOS-bootable?
        if (opt_boot)
        {
            // Provide a default boot sector
            memcpy(mem, abAtomLiteBoot, sizeof(abAtomLiteBoot));

            // Boot sector file supplied?
            if (*opt_boot)
            {
                MFILE file;

                if (!file.Open(opt_boot))
                    return Error("boot");
                else if (msize(&file) != SECTOR_SIZE)
                {
                    throw util::exception("boot sector must be exactly 512 bytes");
                    return false;
                }
                else
                    mread(mem, mem.size, 1, &file);
            }
        }
    */

    if (!hdd)
        Error("open");
    else if (hdd->SafetyCheck() && hdd->Lock())
    {
        BDOS_CAPS bdc;
        GetBDOSCaps(hdd->total_sectors, bdc);
        bdc.need_byteswap = !!opt_byteswap;

        // A quick format stops after the MGT boot sector in record 1
        int64_t total_sectors = opt_quick ? bdc.base_sectors + MGT_DIR_TRACKS * MGT_SECTORS + 1 : hdd->total_sectors;

        // Format the boot sector and record list
        f = hdd->Copy(nullptr, bdc.base_sectors, 0, 0, total_sectors, "Formatting");

        MEMORY mem(MGT_DISK_SIZE);
        if (!opt_nosig) memcpy(mem + 232, "BDOS", 4);

        // Format the record data area
        for (int64_t uPos = bdc.base_sectors; ; uPos += MGT_DISK_SECTORS)
        {
            // Determine how much to transfer in one go
            if (uPos > total_sectors) uPos = total_sectors;
            auto block_size = std::min(static_cast<int>(total_sectors - uPos), MGT_DISK_SECTORS);

            Message(msgStatus, "Formatting... %u%%", static_cast<unsigned>(uPos * 100 / total_sectors));

            if (!block_size)
                break;

            // Locate and write the record block
            if (!hdd->Seek(uPos) || !hdd->Write(mem, block_size, bdc.need_byteswap))
            {
                // If this is a raw format, report the sector offset
                if (opt_nosig)
                    Message(msgStatus, "Write error at sector %u: %s", uPos, LastError());
                else
                {
                    // Report the record containing the error
                    Message(msgStatus, "Write error in record %u: %s", 1 + (uPos - bdc.base_sectors) / MGT_DISK_SECTORS, LastError());

                    // Attempt to clear the BDOS signature in the bad record, to prevent its use
                    MEMORY memblank(hdd->sector_size);
                    hdd->Seek(uPos);
                    hdd->Write(memblank, 1);
                }
            }
        }

        /*
                if (opt_boot)
                {
                    UpdateBDOSBootSector(mem, &hdd);

                    if (!hdd->Seek(0) || !hdd->Write(mem, 1, bdc.fNeedSwap))
                        return Error("boot");
                }
        */
        hdd->Unlock();
    }

    return f;
}

bool FormatBoot(const std::string& path)
{
    bool fRet = false;

    // Strip ":0" from end of string
    auto hdd = HDD::OpenDisk(path.substr(0, path.rfind(':')));

    if (!hdd.get() || !hdd->Seek(0))
        Error("open");
    else if (hdd->SafetyCheck() && hdd->Lock())
    {
        MEMORY mem(hdd->sector_size);

        if (!hdd->Write(mem, 1))
            Error("write");
        else
            fRet = true;

        hdd->Unlock();
    }

    return fRet;
}

bool FormatRecord(const std::string& path)
{
    auto record = 0;
    if (!IsRecord(path, &record))
        throw util::exception("invalid record path");

    throw std::logic_error("BDOS record formatting not implemented");
#if 0
    auto hdd_path = path.substr(0, path.rfind(':'));
    auto hdd = HDD::OpenDisk(hdd_path);

    if (!hdd)
        return Error("open");

    // The disk signature is part of record 1, so unformatting is dangerous
    if (record == 1 && opt_nosig == 1)
        throw util::exception("unformatting record 1 would destroy BDOS signature!");

    auto olddisk = std::make_shared<DISK>();
    MEMORY mem(MGT_TRACK_SIZE);

    for (BYTE cyl = 0; cyl < NORMAL_TRACKS; ++cyl)
        for (BYTE head = 0; head < NORMAL_SIDES; ++head)
            olddisk->FormatRegularTrack(cyl, head, &fmtMGT, mem);

    return !WriteRecord(hdd.get(), record, *olddisk, true);
#endif
}
