#include "../tools/globals.h"

#define KB(x)   ((size_t) (x) << 10)
#define MB(x)   ((size_t) (x) << 20)
#define BUFFER_SIZE KB(512)
#define READ_SIZE MB(32)
#define BLOCK_SIZE MB(256)
#define MAX_RETRIES 3
#define MAX_READ_SECTORS 32

// http://www.ioctls.net/
#define IOCTL_SCSI_PASS_THROUGH_DIRECT 0x4D014
#define SCSI_IOCTL_DATA_IN 1  // read data
#define SCSIOP_READ 0x28

typedef struct _SCSI_PASS_THROUGH_DIRECT
{
    USHORT Length;
    UCHAR ScsiStatus;
    UCHAR PathId;
    UCHAR TargetId;
    UCHAR Lun;
    UCHAR CdbLength;
    UCHAR SenseInfoLength;
    UCHAR DataIn;
    ULONG DataTransferLength;
    ULONG TimeOutValue;
    PVOID DataBuffer;
    ULONG SenseInfoOffset;
    UCHAR Cdb[16];
} SCSI_PASS_THROUGH_DIRECT, *PSCSI_PASS_THROUGH_DIRECT;

#pragma pack(push, scsi, 1)
typedef union _CDB
{
    struct _CDB10 {
        UCHAR OperationCode;
        UCHAR RelativeAddress : 1;
        UCHAR Reserved1 : 2;
        UCHAR ForceUnitAccess : 1;
        UCHAR DisablePageOut : 1;
        UCHAR Lun : 3;
        //UCHAR LBA[4];
        ULONG LBA;
        UCHAR Reserved2;
        //UCHAR TransferBlocks[2];
        USHORT TransferBlocks;
        UCHAR Control;
    } CDB10, *PCDB10;
} CDB, *PCDB;
#pragma pack(pop, scsi)

FILE_FS_SIZE_INFORMATION getntfsinfo(ANSI_STRING dev_name)
{
    HANDLE handle;
    NTSTATUS status;
    IO_STATUS_BLOCK io_status;
    OBJECT_ATTRIBUTES obj_attr;
    FILE_FS_SIZE_INFORMATION size = {};

    InitializeObjectAttributes(&obj_attr, &dev_name, OBJ_CASE_INSENSITIVE, NULL, NULL);

    status = NtOpenFile(&handle, GENERIC_READ | SYNCHRONIZE, &obj_attr, &io_status, FILE_SHARE_READ, FILE_OPEN_FOR_FREE_SPACE_QUERY | FILE_SYNCHRONOUS_IO_NONALERT);
    if (!NT_SUCCESS(status)) {
        debugPrint("Couldn't open device for reading\n");
        return size;
    }

    NtQueryVolumeInformationFile(handle, &io_status, &size, sizeof(size), FileFsSizeInformation);
    NtClose(handle);

    return size;
}

// FIXME: Right now this doesn't do anything too interesting, just moves the read head 1/4th of the disc and back, also does not work with HDD?
void dorawread(char *dev_name)
{
    NTSTATUS status;
    LARGE_INTEGER freq, start, end;
    PDEVICE_OBJECT pDevice;
    ULONG returnSize;

    ANSI_STRING name;
    char *abc = "\\Device\\CdRom0";
    RtlInitAnsiString(&name, abc);

    status = ObReferenceObjectByName(&name, 0, &IoDeviceObjectType, 0, (void**)&pDevice);
    if (!NT_SUCCESS(status)) {
        debugPrint("Couldn't open %s for reading\n", name.Buffer);
        goto cleanup;
    }

    FILE_FS_SIZE_INFORMATION geometry = getntfsinfo(name);
    int size = 1 * geometry.BytesPerSector;
    int max_sectors = geometry.TotalAllocationUnits.QuadPart / 4;

    char *buffer = malloc(size + MAX_READ_SECTORS);
    if (!buffer) {
        debugPrint("Couldn't allocate buffer memory!\n");
        goto cleanup;
    }

    SCSI_PASS_THROUGH_DIRECT direct;
    memset(&direct, 0, sizeof(SCSI_PASS_THROUGH_DIRECT));
    direct.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
    direct.DataIn = SCSI_IOCTL_DATA_IN;
    direct.DataBuffer = buffer;
    direct.DataTransferLength = size;

    PCDB cdb = (PCDB)&direct.Cdb;
    cdb->CDB10.OperationCode = SCSIOP_READ;
    cdb->CDB10.ForceUnitAccess = 1;
    cdb->CDB10.LBA = __builtin_bswap32(0);
    cdb->CDB10.TransferBlocks = __builtin_bswap16(1);

    QueryPerformanceFrequency(&freq);
    for (int i = 0; i < 100; i++) {
        if (__builtin_bswap32(cdb->CDB10.LBA) == max_sectors) {
            cdb->CDB10.LBA = __builtin_bswap32(0);
        } else {
            cdb->CDB10.LBA = __builtin_bswap32(max_sectors);
        }

        QueryPerformanceCounter(&start);
        status = IoSynchronousDeviceIoControlRequest(IOCTL_SCSI_PASS_THROUGH_DIRECT, pDevice, &direct, sizeof(SCSI_PASS_THROUGH_DIRECT), NULL, 0, &returnSize, FALSE);
        QueryPerformanceCounter(&end);

        if (!NT_SUCCESS(status)) {
            debugPrint("\nFailed. %lX\n", status);
            Sleep(10000);
            goto cleanup;
        } else {
            double speed = size * freq.QuadPart;
            speed /= (end.QuadPart - start.QuadPart);
            //speed /= 1000;
            //speed /= 1024; // KB/s is more useful than B/s

            debugPrint("Read Block: %lu %lu\n", (ULONG)speed, returnSize);
        }
    }

cleanup:
    free(buffer);
    debugPrint("Returning...\n");
    Sleep(10000);
}

void doswread(char* dev_name)
{
    ULONG maxBlocks;
    NTSTATUS status;
    IO_STATUS_BLOCK io_status;
    OBJECT_ATTRIBUTES obj_attr;
    HANDLE handle = INVALID_HANDLE_VALUE;
    LARGE_INTEGER freq, maxOffset, devOffset = {{0, 0}}, start, end;
    FILE_FS_SIZE_INFORMATION geometry;

    ANSI_STRING device;
    RtlInitAnsiString(&device, dev_name);

    void *buffer = (void *)malloc(BUFFER_SIZE);
    if (!buffer) {
        debugPrint("Couldn't allocate buffer memory!\n");
        return;
    }

    QueryPerformanceFrequency(&freq);

    geometry = getntfsinfo(device);
    maxOffset.QuadPart = geometry.TotalAllocationUnits.QuadPart * geometry.SectorsPerAllocationUnit * geometry.BytesPerSector;

    if (!maxOffset.QuadPart) {
        debugPrint("Couldn't query FileFsSizeInformation on %s\n", device.Buffer);
        goto cleanup;
    }

    maxBlocks = maxOffset.QuadPart / BLOCK_SIZE;
    debugPrint("Block Size: %d MB\n", (int)(BLOCK_SIZE / 1024 / 1024));

    InitializeObjectAttributes(&obj_attr, &device, OBJ_CASE_INSENSITIVE, NULL, NULL);
    status = NtOpenFile(&handle, GENERIC_READ | SYNCHRONIZE, &obj_attr, &io_status, FILE_SHARE_READ, FILE_SYNCHRONOUS_IO_NONALERT);
    if (!NT_SUCCESS(status)) {
        debugPrint("Couldn't open %s for reading\n", device.Buffer);
        goto cleanup;
    }

    debugPrint("Starting read test...\n");

    int retries = 0;
    for (ULONG block = 0; block < maxBlocks; block++, devOffset.QuadPart += BLOCK_SIZE)
    {
        QueryPerformanceCounter(&start);
        for (LARGE_INTEGER offset = devOffset; offset.QuadPart - devOffset.QuadPart < READ_SIZE; offset.QuadPart += BUFFER_SIZE)
        {
retry:
            status = NtReadFile(handle, 0, NULL, NULL, &io_status, buffer, BUFFER_SIZE, &offset);
            if (!NT_SUCCESS(status)) {
                // Some sectors on title discs return this, perhaps security sectors? Everything else is an error.
                if (status != (NTSTATUS)0xC0000010) {
                    debugPrint(".");
                    if (retries > MAX_RETRIES) {
                        debugPrint("\nFailed to read at offset: %llX %lX\n", offset.QuadPart, status);
                        goto cleanup;
                    }
                    retries++;
                    Sleep(250);
                    goto retry;
                }
            }
        }
        QueryPerformanceCounter(&end);

        double speed = READ_SIZE * freq.QuadPart;
        speed /= (end.QuadPart - start.QuadPart);
        speed /= 1024; // KB/s is more useful than B/s

        debugPrint("Read Block: %lu / %lu %lu KB/s\n", block, maxBlocks - 1, (ULONG)speed);
    }

cleanup:
    free(buffer);
    NtClose(handle);
    debugPrint("Returning...\n");
    Sleep(10000);
}
