#pragma once

#include "Disk.h"

#include <memory>

enum { ftUnknown, ftFloppy, ftRAW, ftDSK, ftMGT, ftSAD, ftTRD, ftSSD, ftD2M, ftD81, ftD88, ftIMD, ftMBD, ftOPD, ftS24, ftFDI, ftCPM, ftLIF, ftDS2, ftQDOS, ftRecord, ftLast };

void ReadImage(const std::string& path, std::shared_ptr<Disk>& disk, bool normalise = true);
bool WriteImage(const std::string& path, std::shared_ptr<Disk>& disk);
