// SPDX-FileCopyrightText: 2017-2022 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "libCZI.h"
#include "CziParse.h"
#include "CziStructs.h"
#include <cassert>
#include <cstddef>
#include "Site.h"

using namespace std;
using namespace libCZI;

/*static*/const std::uint8_t CCZIParse::FILEHDRMAGIC[16] = { 'Z','I','S','R','A','W','F','I','L','E','\0','\0','\0','\0','\0','\0' };
/*static*/const std::uint8_t CCZIParse::SUBBLKDIRMAGIC[16] = { 'Z','I','S','R','A','W','D','I','R','E','C','T','O','R','Y','\0' };
/*static*/const std::uint8_t CCZIParse::SUBBLKMAGIC[16] = { 'Z','I','S','R','A','W','S','U','B','B','L','O','C','K' ,'\0','\0' };
/*static*/const std::uint8_t CCZIParse::METADATASEGMENTMAGIC[16] = { 'Z','I','S','R','A','W','M','E','T','A','D','A','T','A' ,'\0','\0' };
/*static*/const std::uint8_t CCZIParse::ATTACHMENTSDIRMAGC[16] = { 'Z','I','S','R','A','W','A','T','T','D','I','R','\0','\0' ,'\0','\0' };
/*static*/const std::uint8_t CCZIParse::ATTACHMENTBLKMAGIC[16] = { 'Z','I','S','R','A','W','A','T','T','A','C','H','\0','\0' ,'\0','\0' };
/*static*/const std::uint8_t CCZIParse::DELETEDSEGMENTMAGIC[16] = { 'D','E','L','E','T','E','D','\0','\0','\0' ,'\0','\0','\0','\0' ,'\0','\0' };

/*static*/FileHeaderSegmentData CCZIParse::ReadFileHeaderSegment(libCZI::IStream* str)
{
    FileHeaderSegment fileHeaderSegment;

    std::uint64_t bytesRead;
    try
    {
        str->Read(0, &fileHeaderSegment, sizeof(fileHeaderSegment), &bytesRead);
    }
    catch (const std::exception&)
    {
        std::throw_with_nested(LibCZIIOException("Error reading FileHeaderSegment", 0, sizeof(fileHeaderSegment)));
    }

    if (bytesRead != sizeof(fileHeaderSegment))
    {
        CCZIParse::ThrowNotEnoughDataRead(0, sizeof(fileHeaderSegment), bytesRead);
    }

    ConvertToHostByteOrder::Convert(&fileHeaderSegment);

    if (memcmp(fileHeaderSegment.header.Id, CCZIParse::FILEHDRMAGIC, 16) != 0)
    {
        CCZIParse::ThrowIllegalData(0, "Invalid FileHdr-magic");
    }

    return fileHeaderSegment.data;
}

/*static*/CFileHeaderSegmentData CCZIParse::ReadFileHeaderSegmentData(libCZI::IStream* str)
{
    auto fileHeaderSegment = CCZIParse::ReadFileHeaderSegment(str);
    CFileHeaderSegmentData fileHdr{ &fileHeaderSegment };
    return fileHdr;
}

/*static*/CCziSubBlockDirectory CCZIParse::ReadSubBlockDirectory(libCZI::IStream* str, std::uint64_t offset, const SubblockDirectoryParseOptions& options)
{
    CCziSubBlockDirectory subBlkDir;
    CCZIParse::ReadSubBlockDirectory(str, offset, subBlkDir, options);
    subBlkDir.AddingFinished();
    return subBlkDir;
}

/*static*/void CCZIParse::ReadSubBlockDirectory(libCZI::IStream* str, std::uint64_t offset, const std::function<void(const CCziSubBlockDirectoryBase::SubBlkEntry&)>& addFunc, const SubblockDirectoryParseOptions& options, SegmentSizes* segmentSizes /*= nullptr*/)
{
    SubBlockDirectorySegment subBlckDirSegment;
    std::uint64_t bytesRead;
    try
    {
        str->Read(offset, &subBlckDirSegment, sizeof(subBlckDirSegment), &bytesRead);
    }
    catch (const std::exception&)
    {
        std::throw_with_nested(LibCZIIOException("Error reading SubBlkDirectorySegment", offset, sizeof(subBlckDirSegment)));
    }

    if (bytesRead != sizeof(subBlckDirSegment))
    {
        CCZIParse::ThrowNotEnoughDataRead(offset, sizeof(subBlckDirSegment), bytesRead);
    }

    ConvertToHostByteOrder::Convert(&subBlckDirSegment);

    if (memcmp(subBlckDirSegment.header.Id, CCZIParse::SUBBLKDIRMAGIC, 16) != 0)
    {
        CCZIParse::ThrowIllegalData(offset, "Invalid SubBlkDirectory-magic");
    }

    // TODO: possible consistency check ->
    // subBlckDirSegment.header.UsedSize <= subBlckDirSegment.header.AllocatedSize

    std::uint64_t subBlkDirSize = subBlckDirSegment.header.UsedSize;
    if (subBlkDirSize == 0)
    {
        // allegedly, "UsedSize" may not be valid in early versions
        subBlkDirSize = subBlckDirSegment.header.AllocatedSize;
    }

    if (subBlkDirSize < sizeof(SubBlockDirectorySegmentData))
    {
        CCZIParse::ThrowIllegalData(offset, "Invalid SubBlkDirectory-Allocated-Size");
    }

    subBlkDirSize -= sizeof(SubBlockDirectorySegmentData);

    if (segmentSizes != nullptr)
    {
        segmentSizes->AllocatedSize = subBlckDirSegment.header.AllocatedSize;
        segmentSizes->UsedSize = subBlckDirSegment.header.UsedSize;
    }

    // now read the used-size from stream
    std::unique_ptr<void, decltype(free)*> pBuffer(malloc((size_t)subBlkDirSize), free);
    try
    {
        str->Read(offset + sizeof(subBlckDirSegment), pBuffer.get(), subBlkDirSize, &bytesRead);
    }
    catch (const std::exception&)
    {
        std::throw_with_nested(LibCZIIOException("Error reading FileHeaderSegment", offset + sizeof(subBlckDirSegment), subBlkDirSize));
    }

    if (bytesRead != subBlkDirSize)
    {
        CCZIParse::ThrowNotEnoughDataRead(offset + sizeof(subBlckDirSegment), subBlkDirSize, bytesRead);
    }

    int currentOffset = 0;
    CCZIParse::ParseThroughDirectoryEntries(
        subBlckDirSegment.data.EntryCount,
        [&](int numberOfBytes, void* ptr)->void
        {
            if (currentOffset + numberOfBytes <= subBlkDirSize)
            {
                memcpy(ptr, ((char*)pBuffer.get()) + currentOffset, numberOfBytes);
                currentOffset += numberOfBytes;
            }
            else
            {
                CCZIParse::ThrowIllegalData(offset + sizeof(subBlckDirSegment) + currentOffset, "SubBlockDirectory data too small");
            }
        },
        [&](const SubBlockDirectoryEntryDE* subBlkDirDE, const SubBlockDirectoryEntryDV* subBlkDirDV)->void
        {
            if (subBlkDirDE != nullptr)
            {
                CCZIParse::AddEntryToSubBlockDirectory(subBlkDirDE, addFunc);
            }
            else if (subBlkDirDV != nullptr)
            {
                CCZIParse::AddEntryToSubBlockDirectory(subBlkDirDV, addFunc, options);
            }
        });
}

/*static*/void CCZIParse::InplacePatchSubBlockDirectory(libCZI::IInputOutputStream* stream, std::uint64_t offset, const std::function<bool(int sub_block_index, char dimension_identifier, int32_t size, int32_t& new_size)>& patchFunc)
{
    SubBlockDirectorySegment subBlckDirSegment;
    std::uint64_t bytesRead;
    try
    {
        stream->Read(offset, &subBlckDirSegment, sizeof(subBlckDirSegment), &bytesRead);
    }
    catch (const std::exception&)
    {
        std::throw_with_nested(LibCZIIOException("Error reading SubBlkDirectorySegment", offset, sizeof(subBlckDirSegment)));
    }

    if (bytesRead != sizeof(subBlckDirSegment))
    {
        CCZIParse::ThrowNotEnoughDataRead(offset, sizeof(subBlckDirSegment), bytesRead);
    }

    ConvertToHostByteOrder::Convert(&subBlckDirSegment);

    if (memcmp(subBlckDirSegment.header.Id, CCZIParse::SUBBLKDIRMAGIC, 16) != 0)
    {
        CCZIParse::ThrowIllegalData(offset, "Invalid SubBlkDirectory-magic");
    }

    // TODO: possible consistency check ->
    // subBlckDirSegment.header.UsedSize <= subBlckDirSegment.header.AllocatedSize

    std::uint64_t subBlkDirSize = subBlckDirSegment.header.UsedSize;
    if (subBlkDirSize == 0)
    {
        // allegedly, "UsedSize" may not be valid in early versions
        subBlkDirSize = subBlckDirSegment.header.AllocatedSize;
    }

    if (subBlkDirSize < sizeof(SubBlockDirectorySegmentData))
    {
        CCZIParse::ThrowIllegalData(offset, "Invalid SubBlkDirectory-Allocated-Size");
    }

    subBlkDirSize -= sizeof(SubBlockDirectorySegmentData);

    //if (segmentSizes != nullptr)
    //{
    //    segmentSizes->AllocatedSize = subBlckDirSegment.header.AllocatedSize;
    //    segmentSizes->UsedSize = subBlckDirSegment.header.UsedSize;
    //}

    //// now read the used-size from stream
    //std::unique_ptr<void, decltype(free)*> pBuffer(malloc((size_t)subBlkDirSize), free);
    //try
    //{
    //    stream->Read(offset + sizeof(subBlckDirSegment), pBuffer.get(), subBlkDirSize, &bytesRead);
    //}
    //catch (const std::exception&)
    //{
    //    std::throw_with_nested(LibCZIIOException("Error reading FileHeaderSegment", offset + sizeof(subBlckDirSegment), subBlkDirSize));
    //}

    //if (bytesRead != subBlkDirSize)
    //{
    //    CCZIParse::ThrowNotEnoughDataRead(offset + sizeof(subBlckDirSegment), subBlkDirSize, bytesRead);
    //}

    offset = offset + sizeof(subBlckDirSegment);
    for (int i = 0; i < subBlckDirSegment.data.EntryCount; ++i)
    {
        char schemaType[2];
        stream->Read(offset, schemaType, 2, &bytesRead);
        offset += 2;
        if (schemaType[0] == 'D' && schemaType[1] == 'V')
        {
            SubBlockDirectoryEntryDV dv;
            dv.SchemaType[0] = schemaType[0]; dv.SchemaType[1] = schemaType[1];
            //funcRead(4 + 8 + 4 + 4 + 6 + 4, reinterpret_cast<uint8_t*>(&dv) + 2);
            stream->Read(offset, reinterpret_cast<uint8_t*>(&dv) + 2, 4 + 8 + 4 + 4 + 6 + 4, &bytesRead);
            offset += 4 + 8 + 4 + 4 + 6 + 4;
            ConvertToHostByteOrder::Convert(&dv);

            for (int dimension = 0; dimension < dv.DimensionCount; ++dimension)
            {
                DimensionEntryDV dimension_entry;
                stream->Read(offset, &dimension_entry, sizeof(DimensionEntryDV), &bytesRead);
                offset += sizeof(DimensionEntryDV);
                ConvertToHostByteOrder::Convert(&dimension_entry, 1);

                char dimension_identifier;
                if (IsXDimension(dimension_entry.Dimension, sizeof(dimension_entry.Dimension)))
                {
                    dimension_identifier = 'X';
                }
                else if (IsYDimension(dimension_entry.Dimension, sizeof(dimension_entry.Dimension)))
                {
                    dimension_identifier = 'Y';
                }
                else if (IsMDimension(dimension_entry.Dimension, sizeof(dimension_entry.Dimension)))
                {
                    dimension_identifier = 'M';
                }
                else
                {
                    dimension_identifier = Utils::DimensionToChar(CCZIParse::DimensionCharToDimensionIndex(dimension_entry.Dimension, sizeof(dimension_entry.Dimension)));
                }

                int32_t new_size;
                bool was_patched = patchFunc(i, dimension_identifier, dimension_entry.StoredSize, new_size);
                if (was_patched == true)
                {
                    dimension_entry.Size = new_size;
                    stream->Write(
                        offset - sizeof(DimensionEntryDV) + offsetof(DimensionEntryDV, StoredSize),
                        &new_size,
                        sizeof(int32_t),
                        nullptr);
                }
            }
        }
    }
}

/*static*/void CCZIParse::ReadSubBlockDirectory(libCZI::IStream* str, std::uint64_t offset, CCziSubBlockDirectory& subBlkDir, const SubblockDirectoryParseOptions& options)
{
    CCZIParse::ReadSubBlockDirectory(str, offset, [&](const CCziSubBlockDirectoryBase::SubBlkEntry& e)->void {subBlkDir.AddSubBlock(e); }, options, nullptr);
}

/*static*/CCziAttachmentsDirectory CCZIParse::ReadAttachmentsDirectory(libCZI::IStream* str, std::uint64_t offset)
{
    CCziAttachmentsDirectory attDir;
    CCZIParse::ReadAttachmentsDirectory(str, offset, [&](const CCziAttachmentsDirectoryBase::AttachmentEntry& ae)->void {attDir.AddAttachmentEntry(ae); }, nullptr);
    return attDir;
}

/*static*/void CCZIParse::ReadAttachmentsDirectory(libCZI::IStream* str, std::uint64_t offset, const std::function<void(const CCziAttachmentsDirectoryBase::AttachmentEntry&)>& addFunc, SegmentSizes* segmentSizes/*= nullptr*/)
{
    AttachmentDirectorySegment attachmentDirSegment;
    uint64_t bytesRead;
    try
    {
        str->Read(offset, &attachmentDirSegment, sizeof(attachmentDirSegment), &bytesRead);
    }
    catch (const std::exception&)
    {
        std::throw_with_nested(LibCZIIOException("Error reading AttachmentDirectorySegment", offset, sizeof(attachmentDirSegment)));
    }

    if (bytesRead != sizeof(attachmentDirSegment))
    {
        CCZIParse::ThrowNotEnoughDataRead(offset, sizeof(attachmentDirSegment), bytesRead);
    }

    ConvertToHostByteOrder::Convert(&attachmentDirSegment);

    if (memcmp(attachmentDirSegment.header.Id, CCZIParse::ATTACHMENTSDIRMAGC, 16) != 0)
    {
        CCZIParse::ThrowIllegalData(offset, "Invalid AttachmentDirectory-magic");
    }

    if (segmentSizes != nullptr)
    {
        segmentSizes->UsedSize = attachmentDirSegment.header.UsedSize;
        segmentSizes->AllocatedSize = attachmentDirSegment.header.AllocatedSize;
    }

    // TODO: we can add a couple of consistency checks here

    // an empty attachment-directory (with zero entries) is valid and in this
    //  case we can skip the rest of the processing
    if (attachmentDirSegment.data.EntryCount > 0)
    {
        // now read the AttachmentEntries
        const uint64_t attachmentEntriesSize = static_cast<uint64_t>(attachmentDirSegment.data.EntryCount) * sizeof(AttachmentEntryA1);
        unique_ptr<AttachmentEntryA1, decltype(free)*> pBuffer(static_cast<AttachmentEntryA1*>(malloc((size_t)attachmentEntriesSize)), free);

        // now read the attachment-entries
        try
        {
            str->Read(offset + sizeof(attachmentDirSegment), pBuffer.get(), attachmentEntriesSize, &bytesRead);
        }
        catch (const std::exception&)
        {
            std::throw_with_nested(LibCZIIOException("Error reading FileHeaderSegment", offset + sizeof(attachmentDirSegment), attachmentEntriesSize));
        }

        if (bytesRead != attachmentEntriesSize)
        {
            CCZIParse::ThrowNotEnoughDataRead(offset + sizeof(attachmentDirSegment), attachmentEntriesSize, bytesRead);
        }

        for (int i = 0; i < attachmentDirSegment.data.EntryCount; ++i)
        {
            ConvertToHostByteOrder::Convert(pBuffer.get() + i);

            const AttachmentEntryA1* pSrc = pBuffer.get() + i;
            CCziAttachmentsDirectoryBase::AttachmentEntry ae;
            bool b = CheckAttachmentSchemaType(reinterpret_cast<const char*>(&pSrc->SchemaType[0]), 2);
            if (b == false)
            {
                // TODO: what do we do if we encounter this...?
                continue;
            }

            ae.FilePosition = pSrc->FilePosition;
            ae.ContentGuid = pSrc->ContentGuid;
            memcpy(&ae.ContentFileType[0], &pSrc->ContentFileType[0], sizeof(AttachmentEntryA1::ContentFileType));
            memcpy(&ae.Name[0], &pSrc->Name[0], sizeof(AttachmentEntryA1::Name));
            ae.Name[sizeof(AttachmentEntryA1::Name) - 1] = '\0';
            addFunc(ae);
        }
    }
}

/*static*/CCZIParse::SubBlockData CCZIParse::ReadSubBlock(libCZI::IStream* str, std::uint64_t offset, const SubBlockStorageAllocate& allocateInfo)
{
    SubBlockSegment subBlckSegment;
    std::uint64_t bytesRead;

    // the minimum guaranteed size we can read here is "sizeof(SegmentHeader) + SIZE_SUBBLOCKDATA_MINIMUM", it is NOT sizeof(SubBlockSegment)
    const uint64_t MinSizeSubBlockSegment = sizeof(SegmentHeader) + SIZE_SUBBLOCKDATA_MINIMUM;

    try
    {
        str->Read(offset, &subBlckSegment, MinSizeSubBlockSegment, &bytesRead);
    }
    catch (const std::exception&)
    {
        std::throw_with_nested(LibCZIIOException("Error reading SubBlock-Segment", offset, MinSizeSubBlockSegment));
    }

    if (bytesRead != MinSizeSubBlockSegment)
    {
        CCZIParse::ThrowNotEnoughDataRead(offset, sizeof(subBlckSegment), bytesRead);
    }

    ConvertToHostByteOrder::Convert(&subBlckSegment);

    if (memcmp(subBlckSegment.header.Id, CCZIParse::SUBBLKMAGIC, 16) != 0)
    {
        CCZIParse::ThrowIllegalData(offset, "Invalid SubBlock-magic");
    }

    uint32_t lengthSubblockSegmentData = 0;
    SubBlockData sbd;
    if (subBlckSegment.data.entrySchema[0] == 'D' && subBlckSegment.data.entrySchema[1] == 'V')
    {
        ConvertToHostByteOrder::Convert(&subBlckSegment.data.entryDV);
        sbd.compression = subBlckSegment.data.entryDV.Compression;
        sbd.pixelType = subBlckSegment.data.entryDV.PixelType;
        sbd.mIndex = (std::numeric_limits<int>::max)();
        memcpy(sbd.spare, subBlckSegment.data.entryDV._spare, sizeof(sbd.spare));

        if (subBlckSegment.data.entryDV.DimensionCount > MAXDIMENSIONS)
        {
            stringstream ss;
            ss << "'DimensionCount' was found to be " << subBlckSegment.data.entryDV.DimensionCount << ", where the maximum allowed is " << MAXDIMENSIONS << ".";
            CCZIParse::ThrowIllegalData(
                offset + sizeof(SegmentHeader) + SIZE_SUBBLOCKDATA_FIXEDPART + offsetof(SubBlockDirectoryEntryDV, DimensionCount),
                ss.str().c_str());
        }

        // now we can calculate the exact size of the "SubBlockSegmentData" we need here
        lengthSubblockSegmentData = SIZE_SUBBLOCKDATA_FIXEDPART + 32 + subBlckSegment.data.entryDV.DimensionCount * sizeof(DimensionEntryDV);
        if (lengthSubblockSegmentData + sizeof(SegmentHeader) > MinSizeSubBlockSegment)
        {
            // the required size is larger than the minimal size, so now we need to read the additional data (beyond the minimum size)
            const uint64_t remainingBytesToRead = lengthSubblockSegmentData + sizeof(SegmentHeader) - MinSizeSubBlockSegment;
            try
            {
                str->Read(
                    offset + MinSizeSubBlockSegment,
                    ((uint8_t*)(&subBlckSegment)) + MinSizeSubBlockSegment,
                    remainingBytesToRead,
                    &bytesRead);
            }
            catch (const std::exception&)
            {
                std::throw_with_nested(LibCZIIOException("Error reading additional data from SubBlock-Segment", offset + MinSizeSubBlockSegment, remainingBytesToRead));
            }

            if (bytesRead != remainingBytesToRead)
            {
                CCZIParse::ThrowNotEnoughDataRead(offset + MinSizeSubBlockSegment, remainingBytesToRead, bytesRead);
            }
        }

        ConvertToHostByteOrder::Convert(subBlckSegment.data.entryDV.DimensionEntries, subBlckSegment.data.entryDV.DimensionCount);

        for (int i = 0; i < subBlckSegment.data.entryDV.DimensionCount; ++i)
        {
            const DimensionEntryDV* dimEntry = subBlckSegment.data.entryDV.DimensionEntries + i;
            if (IsMDimension(dimEntry->Dimension, sizeof(dimEntry->Dimension)))
            {
                sbd.mIndex = dimEntry->Start;
            }
            else if (IsXDimension(dimEntry->Dimension, sizeof(dimEntry->Dimension)))
            {
                sbd.logicalRect.x = dimEntry->Start;
                sbd.logicalRect.w = dimEntry->Size;
                sbd.physicalSize.w = dimEntry->StoredSize;
            }
            else if (IsYDimension(dimEntry->Dimension, sizeof(dimEntry->Dimension)))
            {
                sbd.logicalRect.y = dimEntry->Start;
                sbd.logicalRect.h = dimEntry->Size;
                sbd.physicalSize.h = dimEntry->StoredSize;
            }
            else
            {
                libCZI::DimensionIndex dim = CCZIParse::DimensionCharToDimensionIndex(dimEntry->Dimension, sizeof(dimEntry->Dimension));
                sbd.coordinate.Set(dim, dimEntry->Start);
            }
        }
    }
    else if (subBlckSegment.data.entrySchema[0] == 'D' && subBlckSegment.data.entrySchema[1] == 'E')
    {
        sbd.compression = subBlckSegment.data.entryDE.Compression;
        sbd.pixelType = subBlckSegment.data.entryDE.PixelType;

        lengthSubblockSegmentData = sizeof(SubBlockDirectoryEntryDE);
        // TODO...
    }
    else
    {
        CCZIParse::ThrowIllegalData(offset, "Invalid schema");
    }

    // the minimal size of the "subblock-segment-data" is given by "SIZE_SUBBLOCKDATA_MINIMUM", but the actual size of the
    //  data-structure (SubBlockDirectoryEntryDV) may be larger than that - so we need to take the max of the actual size and
    //  the reserved (minimal) size here
    lengthSubblockSegmentData = max(lengthSubblockSegmentData, (uint32_t)SIZE_SUBBLOCKDATA_MINIMUM);

    // TODO: if subBlckSegment.data.DataSize > size_t (=4GB for 32Bit) then bail out gracefully
    auto deleter = [&](void* ptr) -> void {allocateInfo.free(ptr); };
    std::unique_ptr<void, decltype(deleter)> pMetadataBuffer(subBlckSegment.data.MetadataSize > 0 ? allocateInfo.alloc(subBlckSegment.data.MetadataSize) : nullptr, deleter);
    std::unique_ptr<void, decltype(deleter)> pDataBuffer(subBlckSegment.data.DataSize > 0 ? allocateInfo.alloc(static_cast<size_t>(subBlckSegment.data.DataSize)) : nullptr, deleter);
    std::unique_ptr<void, decltype(deleter)> pAttachmentBuffer(subBlckSegment.data.AttachmentSize > 0 ? allocateInfo.alloc(subBlckSegment.data.AttachmentSize) : nullptr, deleter);

    // TODO: now get the information from the SubBlockDirectoryEntryDV/DE structure, and figure out their size
    // TODO: compare this information against the information from the SubBlock-directory
    if (pMetadataBuffer)
    {
        try
        {
            str->Read(offset + lengthSubblockSegmentData + sizeof(SegmentHeader), pMetadataBuffer.get(), subBlckSegment.data.MetadataSize, &bytesRead);
        }
        catch (const std::exception&)
        {
            std::throw_with_nested(LibCZIIOException("Error reading FileHeaderSegment", offset + lengthSubblockSegmentData + sizeof(SegmentHeader), subBlckSegment.data.MetadataSize));
        }

        if (bytesRead != subBlckSegment.data.MetadataSize)
        {
            CCZIParse::ThrowNotEnoughDataRead(offset + lengthSubblockSegmentData + sizeof(SegmentHeader), subBlckSegment.data.MetadataSize, bytesRead);
        }
    }

    if (pDataBuffer)
    {
        try
        {
            str->Read(offset + lengthSubblockSegmentData + sizeof(SegmentHeader) + subBlckSegment.data.MetadataSize, pDataBuffer.get(), subBlckSegment.data.DataSize, &bytesRead);
        }
        catch (const std::exception&)
        {
            std::throw_with_nested(LibCZIIOException("Error reading FileHeaderSegment", offset + lengthSubblockSegmentData + sizeof(SegmentHeader) + subBlckSegment.data.MetadataSize, subBlckSegment.data.DataSize));
        }

        if (bytesRead != subBlckSegment.data.DataSize)
        {
            CCZIParse::ThrowNotEnoughDataRead(offset + lengthSubblockSegmentData + sizeof(SegmentHeader) + subBlckSegment.data.MetadataSize, subBlckSegment.data.DataSize, bytesRead);
        }
    }

    if (pAttachmentBuffer)
    {
        try
        {
            str->Read(offset + lengthSubblockSegmentData + sizeof(SegmentHeader) + subBlckSegment.data.MetadataSize + subBlckSegment.data.DataSize, pAttachmentBuffer.get(), subBlckSegment.data.AttachmentSize, &bytesRead);
        }
        catch (const std::exception&)
        {
            std::throw_with_nested(LibCZIIOException("Error reading FileHeaderSegment", offset + lengthSubblockSegmentData + sizeof(SegmentHeader) + subBlckSegment.data.MetadataSize + subBlckSegment.data.DataSize, subBlckSegment.data.AttachmentSize));
        }

        if (bytesRead != subBlckSegment.data.AttachmentSize)
        {
            CCZIParse::ThrowNotEnoughDataRead(offset + lengthSubblockSegmentData + sizeof(SegmentHeader) + subBlckSegment.data.MetadataSize + subBlckSegment.data.DataSize, subBlckSegment.data.AttachmentSize, bytesRead);
        }
    }

    sbd.ptrData = pDataBuffer.release();
    sbd.dataSize = subBlckSegment.data.DataSize;
    sbd.ptrAttachment = pAttachmentBuffer.release();
    sbd.attachmentSize = subBlckSegment.data.AttachmentSize;
    sbd.ptrMetadata = pMetadataBuffer.release();
    sbd.metaDataSize = subBlckSegment.data.MetadataSize;
    return sbd;
}


/*static*/void CCZIParse::InplacePatchSubblock(
                    libCZI::IInputOutputStream* stream,
                    std::uint64_t offset,
                    const std::function<bool(char dimension_identifier, std::int32_t size, std::int32_t& new_coordinate)>& patchFunc)
{
    SubBlockSegment subBlckSegment;
    std::uint64_t bytesRead;

    // the minimum guaranteed size we can read here is "sizeof(SegmentHeader) + SIZE_SUBBLOCKDATA_MINIMUM", it is NOT sizeof(SubBlockSegment)
    const uint64_t MinSizeSubBlockSegment = sizeof(SegmentHeader) + SIZE_SUBBLOCKDATA_MINIMUM;

    try
    {
        stream->Read(offset, &subBlckSegment, MinSizeSubBlockSegment, &bytesRead);
        size_t s1 = offsetof(SubBlockSegment, data);
        size_t s2 = offsetof(SubBlockSegmentData, entryDV);
        offset += offsetof(SubBlockSegment, data) + offsetof(SubBlockSegmentData, entryDV) + offsetof(SubBlockDirectoryEntryDV, DimensionEntries[0]);
    }
    catch (const std::exception&)
    {
        std::throw_with_nested(LibCZIIOException("Error reading SubBlock-Segment", offset, MinSizeSubBlockSegment));
    }

    if (bytesRead != MinSizeSubBlockSegment)
    {
        CCZIParse::ThrowNotEnoughDataRead(offset, sizeof(subBlckSegment), bytesRead);
    }

    ConvertToHostByteOrder::Convert(&subBlckSegment);

    if (memcmp(subBlckSegment.header.Id, CCZIParse::SUBBLKMAGIC, 16) != 0)
    {
        CCZIParse::ThrowIllegalData(offset, "Invalid SubBlock-magic");
    }

    uint32_t lengthSubblockSegmentData = 0;
    SubBlockData sbd;
    if (subBlckSegment.data.entrySchema[0] == 'D' && subBlckSegment.data.entrySchema[1] == 'V')
    {
        ConvertToHostByteOrder::Convert(&subBlckSegment.data.entryDV);
        sbd.compression = subBlckSegment.data.entryDV.Compression;
        sbd.pixelType = subBlckSegment.data.entryDV.PixelType;
        sbd.mIndex = (std::numeric_limits<int>::max)();
        memcpy(sbd.spare, subBlckSegment.data.entryDV._spare, sizeof(sbd.spare));

        if (subBlckSegment.data.entryDV.DimensionCount > MAXDIMENSIONS)
        {
            stringstream ss;
            ss << "'DimensionCount' was found to be " << subBlckSegment.data.entryDV.DimensionCount << ", where the maximum allowed is " << MAXDIMENSIONS << ".";
            CCZIParse::ThrowIllegalData(
                offset + sizeof(SegmentHeader) + SIZE_SUBBLOCKDATA_FIXEDPART + offsetof(SubBlockDirectoryEntryDV, DimensionCount),
                ss.str().c_str());
        }

        for (int dimension = 0; dimension < subBlckSegment.data.entryDV.DimensionCount; ++dimension)
        {
            DimensionEntryDV dimension_entry;
            stream->Read(offset, &dimension_entry, sizeof(DimensionEntryDV), &bytesRead);
            offset += sizeof(DimensionEntryDV);
            ConvertToHostByteOrder::Convert(&dimension_entry, 1);

            char dimension_identifier;
            if (IsXDimension(dimension_entry.Dimension, sizeof(dimension_entry.Dimension)))
            {
                dimension_identifier = 'X';
            }
            else if (IsYDimension(dimension_entry.Dimension, sizeof(dimension_entry.Dimension)))
            {
                dimension_identifier = 'Y';
            }
            else if (IsMDimension(dimension_entry.Dimension, sizeof(dimension_entry.Dimension)))
            {
                dimension_identifier = 'M';
            }
            else
            {
                dimension_identifier = Utils::DimensionToChar(CCZIParse::DimensionCharToDimensionIndex(dimension_entry.Dimension, sizeof(dimension_entry.Dimension)));
            }

            int32_t new_size;
            bool was_patched = patchFunc(dimension_identifier, dimension_entry.StoredSize, new_size);
            if (was_patched == true)
            {
                dimension_entry.Size = new_size;
                stream->Write(
                    offset - sizeof(DimensionEntryDV) + offsetof(DimensionEntryDV, StoredSize),
                    &new_size,
                    sizeof(int32_t),
                    nullptr);
            }
        }
    }
}

/*static*/CCZIParse::AttachmentData CCZIParse::ReadAttachment(libCZI::IStream* str, std::uint64_t offset, const SubBlockStorageAllocate& allocateInfo)
{
    AttachmentSegment attchmntSegment;
    std::uint64_t bytesRead;
    try
    {
        str->Read(offset, &attchmntSegment, sizeof(attchmntSegment), &bytesRead);
    }
    catch (const std::exception&)
    {
        std::throw_with_nested(LibCZIIOException("Error reading Attachment-Segment", offset, sizeof(attchmntSegment)));
    }

    if (bytesRead != sizeof(attchmntSegment))
    {
        CCZIParse::ThrowNotEnoughDataRead(offset, sizeof(attchmntSegment), bytesRead);
    }

    ConvertToHostByteOrder::Convert(&attchmntSegment);

    if (memcmp(attchmntSegment.header.Id, CCZIParse::ATTACHMENTBLKMAGIC, 16) != 0)
    {
        CCZIParse::ThrowIllegalData(offset, "Invalid Attachment-magic");
    }

    // TODO: if subBlckSegment.data.DataSize > size_t (=4GB for 32Bit) then bail out gracefully
    auto deleter = [&](void* ptr) -> void {allocateInfo.free(ptr); };
    std::unique_ptr<void, decltype(deleter)> pAttchmntBuffer(attchmntSegment.data.DataSize > 0 ? allocateInfo.alloc(static_cast<size_t>(attchmntSegment.data.DataSize)) : nullptr, deleter);

    if (pAttchmntBuffer)
    {
        try
        {
            str->Read(offset + 256 + sizeof(SegmentHeader), pAttchmntBuffer.get(), attchmntSegment.data.DataSize, &bytesRead);
        }
        catch (const std::exception&)
        {
            std::throw_with_nested(LibCZIIOException("Error reading AttachmentSegment", offset + 256 + sizeof(SegmentHeader), attchmntSegment.data.DataSize));
        }

        if (bytesRead != attchmntSegment.data.DataSize)
        {
            CCZIParse::ThrowNotEnoughDataRead(offset + 256 + sizeof(SegmentHeader), attchmntSegment.data.DataSize, bytesRead);
        }
    }

    AttachmentData ad;
    ad.ptrData = pAttchmntBuffer.release();
    ad.dataSize = attchmntSegment.data.DataSize;
    return ad;
}

/*static*/void CCZIParse::ParseThroughDirectoryEntries(int count, const std::function<void(int, void*)>& funcRead, const std::function<void(const SubBlockDirectoryEntryDE*, const SubBlockDirectoryEntryDV*)>& funcAddEntry)
{
    for (int i = 0; i < count; ++i)
    {
        char schemaType[2];
        funcRead(2, schemaType);
        if (schemaType[0] == 'D' && schemaType[1] == 'V')
        {
            SubBlockDirectoryEntryDV dv;
            dv.SchemaType[0] = schemaType[0]; dv.SchemaType[1] = schemaType[1];
            funcRead(4 + 8 + 4 + 4 + 6 + 4, reinterpret_cast<uint8_t*>(&dv) + 2);
            ConvertToHostByteOrder::Convert(&dv);

            int sizeToRead = dv.DimensionCount * sizeof(DimensionEntryDV);
            // TODO: check for max size etc.
            funcRead(sizeToRead, &dv.DimensionEntries[0]);
            ConvertToHostByteOrder::Convert(&dv.DimensionEntries[0], dv.DimensionCount);

            funcAddEntry(nullptr, &dv);
        }
        else if (schemaType[0] == 'D' && schemaType[1] == 'E')
        {
            SubBlockDirectoryEntryDE de;
            funcRead(sizeof(de), &de);
            ConvertToHostByteOrder::Convert(&de);
            funcAddEntry(&de, nullptr);
        }
    }
}

/*static*/void CCZIParse::AddEntryToSubBlockDirectory(const SubBlockDirectoryEntryDE* subBlkDirDE, const std::function<void(const CCziSubBlockDirectoryBase::SubBlkEntry&)>& addFunc)
{
    // TODO
    throw std::logic_error("not (yet) implemented");
}

/*static*/void CCZIParse::AddEntryToSubBlockDirectory(const SubBlockDirectoryEntryDV* subBlkDirDV, const std::function<void(const CCziSubBlockDirectoryBase::SubBlkEntry&)>& addFunc, const SubblockDirectoryParseOptions& options)
{
    CCziSubBlockDirectory::SubBlkEntry entry;
    entry.Invalidate();

    bool x_was_given = false;
    bool y_was_given = false;
    bool size_of_m_was_not_1 = false;       // we will note here whether size for M-dimension was not 1
    int size_of_m_in_case_it_was_not_1 = 0; // ...and, if this is the case, we will note the size here
    for (int i = 0; i < subBlkDirDV->DimensionCount; ++i)
    {
        if (CCZIParse::IsXDimension(subBlkDirDV->DimensionEntries[i].Dimension, 4))
        {
            entry.x = subBlkDirDV->DimensionEntries[i].Start;
            entry.width = subBlkDirDV->DimensionEntries[i].Size;
            entry.storedWidth = subBlkDirDV->DimensionEntries[i].StoredSize;
            x_was_given = true;
        }
        else if (CCZIParse::IsYDimension(subBlkDirDV->DimensionEntries[i].Dimension, 4))
        {
            entry.y = subBlkDirDV->DimensionEntries[i].Start;
            entry.height = subBlkDirDV->DimensionEntries[i].Size;
            entry.storedHeight = subBlkDirDV->DimensionEntries[i].StoredSize;
            y_was_given = true;
        }
        else if (CCZIParse::IsMDimension(subBlkDirDV->DimensionEntries[i].Dimension, 4))
        {
            entry.mIndex = subBlkDirDV->DimensionEntries[i].Start;
            if (subBlkDirDV->DimensionEntries[i].Size != 1)
            {
                if (options.GetDimensionMMustHaveSizeOne())
                {
                    // In this case we can immediately throw an exception (i.e. this options requires that the size of M is 1 for all subblocks).
                    stringstream string_stream;
                    string_stream << "Size for dimension 'M' is expected to be 1, but found " << subBlkDirDV->DimensionEntries[i].Size << " (file-offset:" << subBlkDirDV->FilePosition << ").";
                    throw LibCZICZIParseException(string_stream.str().c_str(), LibCZICZIParseException::ErrorCode::NonConformingSubBlockDimensionEntry);
                }
                else
                {
                    // ...but, for the option "MMustHaveSizeOneExceptForPyramidSubblocks" we have to check first if this a pyramid-subblock,
                    //  which means that we must have the information for X and Y first. We do not want to assume a specific order of the dimension
                    //  entries here, so we just take not of this fact and check it later.
                    size_of_m_was_not_1 = true;
                    size_of_m_in_case_it_was_not_1 = subBlkDirDV->DimensionEntries[i].Size;
                }
            }
        }
        else
        {
            libCZI::DimensionIndex dim = CCZIParse::DimensionCharToDimensionIndex(subBlkDirDV->DimensionEntries[i].Dimension, 4);
            entry.coordinate.Set(dim, subBlkDirDV->DimensionEntries[i].Start);

            if (options.GetPhysicalDimensionOtherThanMMustHaveSizeOne())
            {
                auto physicalSize = subBlkDirDV->DimensionEntries[i].StoredSize;
                if (physicalSize != 1)
                {
                    stringstream string_stream;
                    string_stream
                        << "Physical size for dimension '" << Utils::DimensionToChar(dim)
                        << "' is expected to be 1, but found " << physicalSize
                        << " (file-offset:" << subBlkDirDV->FilePosition << ").";
                    throw LibCZICZIParseException(string_stream.str().c_str(), LibCZICZIParseException::ErrorCode::NonConformingSubBlockDimensionEntry);
                }
            }

            if (options.GetDimensionOtherThanMMustHaveSizeOne() && subBlkDirDV->DimensionEntries[i].Size != 1)
            {
                stringstream string_stream;
                string_stream << "Size for dimension '" << Utils::DimensionToChar(dim) << "' is expected to be 1, but found " << subBlkDirDV->DimensionEntries[i].Size << " (file-offset:" << subBlkDirDV->FilePosition << ").";
                throw LibCZICZIParseException(string_stream.str().c_str(), LibCZICZIParseException::ErrorCode::NonConformingSubBlockDimensionEntry);
            }
        }
    }

    if (options.GetDimensionXyMustBePresent() && (!x_was_given || !y_was_given))
    {
        stringstream string_stream;
        string_stream << "No coordinate/size given for ";
        if (!x_was_given && y_was_given)
        {
            string_stream << "'X'";
        }
        else if (!y_was_given && x_was_given)
        {
            string_stream << "'Y'";
        }
        else
        {
            string_stream << "'X' and 'Y'";
        }

        string_stream << " (file-offset:" << subBlkDirDV->FilePosition << ").";
        throw LibCZICZIParseException(string_stream.str().c_str(), LibCZICZIParseException::ErrorCode::NonConformingSubBlockDimensionEntry);
    }

    if (size_of_m_was_not_1 && options.GetDimensionMMustHaveSizeOneForPyramidSubblocks())
    {
        // Ok, so now check if this is a pyramid-subblock (and if so, we will ignore the error).
        // In turns out that there are quite a few files out there which erroneously have a non-1 size for the M-dimension of a pyramid-tile, 
        // as some software used to write it that way). If we ignore this error, then those files work perfectly fine.
        if (entry.IsStoredSizeEqualLogicalSize())
        {
            // this is not a pyramid-subblock, so we throw the exception
            stringstream string_stream;
            string_stream << "Size for dimension 'M' for non-pyramid-subblock is expected to be 1, but found " << size_of_m_in_case_it_was_not_1 << " (file-offset:" << subBlkDirDV->FilePosition << ").";
            throw LibCZICZIParseException(string_stream.str().c_str(), LibCZICZIParseException::ErrorCode::NonConformingSubBlockDimensionEntry);
        }
    }

    entry.FilePosition = subBlkDirDV->FilePosition;
    entry.PixelType = subBlkDirDV->PixelType;
    entry.Compression = subBlkDirDV->Compression;
    entry.pyramid_type_from_spare = subBlkDirDV->_spare[0];

    addFunc(entry);
}

/*static*/CCZIParse::MetadataSegmentData CCZIParse::ReadMetadataSegment(libCZI::IStream* str, std::uint64_t offset, const SubBlockStorageAllocate& allocateInfo)
{
    MetadataSegment metadataSegment;
    std::uint64_t bytesRead;
    try
    {
        str->Read(offset, &metadataSegment, sizeof(metadataSegment), &bytesRead);
    }
    catch (const std::exception&)
    {
        std::throw_with_nested(LibCZIIOException("Error reading MetaDataSegment", offset, sizeof(metadataSegment)));
    }

    if (bytesRead != sizeof(metadataSegment))
    {
        CCZIParse::ThrowNotEnoughDataRead(offset, sizeof(metadataSegment), bytesRead);
    }

    ConvertToHostByteOrder::Convert(&metadataSegment);

    if (memcmp(metadataSegment.header.Id, CCZIParse::METADATASEGMENTMAGIC, 16) != 0)
    {
        CCZIParse::ThrowIllegalData(offset, "Invalid MetadataSegment-magic");
    }

    // TODO: perform consistency checks...
    auto deleter = [&](void* ptr) -> void {allocateInfo.free(ptr); };
    std::unique_ptr<void, decltype(deleter)> pXmlBuffer(metadataSegment.data.XmlSize > 0 ? allocateInfo.alloc(metadataSegment.data.XmlSize) : nullptr, deleter);
    std::unique_ptr<void, decltype(deleter)> pAttachmentBuffer(metadataSegment.data.AttachmentSize > 0 ? allocateInfo.alloc(static_cast<size_t>(metadataSegment.data.AttachmentSize)) : nullptr, deleter);
    if (pXmlBuffer)
    {
        try
        {
            str->Read(offset + sizeof(metadataSegment), pXmlBuffer.get(), metadataSegment.data.XmlSize, &bytesRead);
        }
        catch (const std::exception&)
        {
            std::throw_with_nested(LibCZIIOException("Error reading MetaDataSegment", offset + sizeof(metadataSegment), metadataSegment.data.XmlSize));
        }

        if (bytesRead != metadataSegment.data.XmlSize)
        {
            CCZIParse::ThrowNotEnoughDataRead(offset + sizeof(metadataSegment), metadataSegment.data.XmlSize, bytesRead);
        }
    }

    if (pAttachmentBuffer)
    {
        try
        {
            str->Read(offset + sizeof(metadataSegment) + metadataSegment.data.XmlSize, pAttachmentBuffer.get(), metadataSegment.data.AttachmentSize, &bytesRead);
        }
        catch (const std::exception&)
        {
            std::throw_with_nested(LibCZIIOException("Error reading MetaDataSegment", offset + sizeof(metadataSegment) + metadataSegment.data.XmlSize, metadataSegment.data.AttachmentSize));
        }

        if (bytesRead != metadataSegment.data.AttachmentSize)
        {
            CCZIParse::ThrowNotEnoughDataRead(offset + sizeof(metadataSegment) + metadataSegment.data.XmlSize, metadataSegment.data.AttachmentSize, bytesRead);
        }
    }

    MetadataSegmentData segData;
    segData.ptrXmlData = pXmlBuffer.release();
    segData.xmlDataSize = metadataSegment.data.XmlSize;
    segData.ptrAttachment = pAttachmentBuffer.release();
    segData.attachmentSize = metadataSegment.data.AttachmentSize;
    return segData;
}

/*static*/char CCZIParse::ToUpperCase(char c)
{
    if (isascii(c) != 0 && isupper(c) == 0)
    {
        return (char)toupper(c);
    }

    return c;
}

/*static*/libCZI::DimensionIndex CCZIParse::DimensionCharToDimensionIndex(const char* ptr, size_t size)
{
    if (size < 1)
    {
        CCZIParse::ThrowIllegalData("parameter 'size' is illegal");
    }

    static const struct CharAndDim
    {
        char dimChar;
        libCZI::DimensionIndex dimIndex;
    } CharAndDimList[] =
    {
        // important: this list must be sorted (ascending) for "dimChar"
        { 'B', DimensionIndex::B },
        { 'C', DimensionIndex::C },
        { 'H', DimensionIndex::H },
        { 'I', DimensionIndex::I },
        { 'R', DimensionIndex::R },
        { 'S', DimensionIndex::S },
        { 'T', DimensionIndex::T },
        { 'V', DimensionIndex::V },
        { 'Z', DimensionIndex::Z }
    };

    const char c = CCZIParse::ToUpperCase(*ptr);
    const auto it = std::lower_bound(
        std::begin(CharAndDimList),
        std::end(CharAndDimList),
        c,
        [](const CharAndDim& val, char toSearch)->int {return val.dimChar < toSearch; });
    if (it != std::end(CharAndDimList) && !(c < it->dimChar))
    {
        return it->dimIndex;
    }

    CCZIParse::ThrowIllegalData("invalid dimension");
    return DimensionIndex::invalid;     // not reachable, just to silence compiler warning
}

/*static*/bool CCZIParse::IsMDimension(const char* ptr, size_t size)
{
    if (size < 1)
    {
        CCZIParse::ThrowIllegalData("parameter 'size' is illegal");
    }

    char c = CCZIParse::ToUpperCase(*ptr);
    return (c == 'M') ? true : false;
}

/*static*/bool CCZIParse::IsXDimension(const char* ptr, size_t size)
{
    if (size < 1)
    {
        CCZIParse::ThrowIllegalData("parameter 'size' is illegal");
    }

    char c = CCZIParse::ToUpperCase(*ptr);
    return (c == 'X') ? true : false;
}

/*static*/bool CCZIParse::IsYDimension(const char* ptr, size_t size)
{
    if (size < 1)
    {
        CCZIParse::ThrowIllegalData("parameter 'size' is illegal");
    }

    char c = CCZIParse::ToUpperCase(*ptr);
    return (c == 'Y') ? true : false;
}

/*static*/void CCZIParse::ThrowNotEnoughDataRead(std::uint64_t offset, std::uint64_t bytesRequested, std::uint64_t bytesActuallyRead)
{
    stringstream ss;
    ss << "Not enough data read at offset " << offset << " -> requested: " << bytesRequested << " bytes, actually got " << bytesActuallyRead << " bytes.";
    throw LibCZICZIParseException(ss.str().c_str(), LibCZICZIParseException::ErrorCode::NotEnoughData);
}

/*static*/void CCZIParse::ThrowIllegalData(std::uint64_t offset, const char* sz)
{
    stringstream ss;
    ss << "Illegal data detected at offset " << offset << " -> " << sz;
    throw LibCZICZIParseException(ss.str().c_str(), LibCZICZIParseException::ErrorCode::CorruptedData);
}

/*static*/void CCZIParse::ThrowIllegalData(const char* sz)
{
    stringstream ss;
    ss << "Illegal data detected -> " << sz;
    throw LibCZICZIParseException(ss.str().c_str(), LibCZICZIParseException::ErrorCode::CorruptedData);
}

/*static*/bool CCZIParse::CheckAttachmentSchemaType(const char* p, size_t cnt)
{
    assert(cnt >= 2);
    if (*p != 'A' || *(p + 1) != '1')
    {
        if (GetSite()->IsEnabled(LOGLEVEL_SEVEREWARNING))
        {
            GetSite()->Log(LOGLEVEL_SEVEREWARNING, "Unknown Attachment-Schema-Type encountered");
        }

        return false;
    }

    return true;
}

/*static*/CCZIParse::SegmentSizes CCZIParse::ReadSegmentHeader(CCZIParse::SegmentType type, libCZI::IStream* str, std::uint64_t pos)
{
    const std::uint8_t* pMagic;
    switch (type)
    {
    case CCZIParse::SegmentType::SbBlkDirectory:
        pMagic = CCZIParse::SUBBLKDIRMAGIC;
        break;
    case CCZIParse::SegmentType::SbBlk:
        pMagic = CCZIParse::SUBBLKMAGIC;
        break;
    case CCZIParse::SegmentType::AttchmntDirectory:
        pMagic = CCZIParse::ATTACHMENTSDIRMAGC;
        break;
    case CCZIParse::SegmentType::Attachment:
        pMagic = CCZIParse::ATTACHMENTBLKMAGIC;
        break;
    case CCZIParse::SegmentType::Metadata:
        pMagic = CCZIParse::METADATASEGMENTMAGIC;
        break;
    default:
        throw std::logic_error("unknown SegmentType");
    }

    SegmentHeader segmentHdr;
    std::uint64_t bytesRead;
    try
    {
        str->Read(pos, &segmentHdr, sizeof(segmentHdr), &bytesRead);
    }
    catch (const std::exception&)
    {
        std::throw_with_nested(LibCZIIOException("Error reading SegmentHeader", pos, sizeof(segmentHdr)));
    }

    if (bytesRead != sizeof(segmentHdr))
    {
        CCZIParse::ThrowNotEnoughDataRead(pos, sizeof(segmentHdr), bytesRead);
    }

    if (memcmp(segmentHdr.Id, pMagic, 16) != 0)
    {
        CCZIParse::ThrowIllegalData(pos, "Invalid SegmentHeader-magic");
    }

    ConvertToHostByteOrder::Convert(&segmentHdr);

    return SegmentSizes{ segmentHdr.AllocatedSize,segmentHdr.UsedSize };
}

/*static*/CCZIParse::SegmentSizes CCZIParse::ReadSegmentHeaderAny(libCZI::IStream* str, std::uint64_t pos)
{
    SegmentHeader segmentHdr;
    std::uint64_t bytesRead;
    try
    {
        str->Read(pos, &segmentHdr, sizeof(segmentHdr), &bytesRead);
    }
    catch (const std::exception&)
    {
        std::throw_with_nested(LibCZIIOException("Error reading SegmentHeader", pos, sizeof(segmentHdr)));
    }

    if (bytesRead != sizeof(segmentHdr))
    {
        CCZIParse::ThrowNotEnoughDataRead(pos, sizeof(segmentHdr), bytesRead);
    }

    ConvertToHostByteOrder::Convert(&segmentHdr);

    return SegmentSizes{ segmentHdr.AllocatedSize,segmentHdr.UsedSize };
}

void CCZIParse::SubblockDirectoryParseOptions::SetFlag(ParseFlags flag, bool enable)
{
    this->flags.set(static_cast<std::underlying_type<ParseFlags>::type>(flag), enable);
}

bool CCZIParse::SubblockDirectoryParseOptions::GetFlag(ParseFlags flag)const
{
    return this->flags.test(static_cast<std::underlying_type<ParseFlags>::type>(flag));
}
