#include "globals.h"

void shutdown(char *)
{
    running = false;
}

bool initsdl()
{
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
	if (SDL_Init(SDL_INIT_GAMECONTROLLER) != 0) {
		debugPrint("Unable to intialize SDL!\n");
		Sleep(10000);
		return 0;
	}

	// We assume the user has a controller plugged into port 1
	controller = SDL_GameControllerOpen(0);
	if (controller == NULL) {
		debugPrint("Unable to initialize controller!\n");
		Sleep(10000);
		return 0;
	}
    return 1;
}

void togglel2cache(char *)
{
    __asm
    {
        push eax
        push edx
        push ecx
        sfence
        mov ecx, 0x11E // BBL_CR_CTL3
        rdmsr
        or eax, 1 << 8 // Default enable L2
        mov ecx, l2State // Read in our bool
        xor ecx, 1 // Flip it
        mov l2State, ecx
        test ecx, ecx
        jnz SKIP_DISABLE
        and eax, ~(1 << 8)
    SKIP_DISABLE:
        mov ecx, 0x11E
        wrmsr
        wbinvd
        pop ecx
        pop edx
        pop eax
    }

#ifdef DISABLE_ALL_CACHE
    __asm
    {
        push eax
        mov eax, cr0
        or eax, 0x40000000
        mov cr0, eax
        pop eax
        sfence
        wbinvd
    }
#endif

    debugPrint("L2 Cache: %s\n", l2State ? "ENABLED" : "DISABLED");
    Sleep(500);
}

void setspindlespeed(char *)
{
    HANDLE handle;
    NTSTATUS status;
    ANSI_STRING cdrom;
    IO_STATUS_BLOCK io_status;
    OBJECT_ATTRIBUTES obj_attr;

    RtlInitAnsiString(&cdrom, "\\Device\\CdRom0");
    InitializeObjectAttributes(&obj_attr, &cdrom, OBJ_CASE_INSENSITIVE, NULL, NULL);
    status = NtOpenFile(&handle, GENERIC_READ | SYNCHRONIZE, &obj_attr, &io_status, FILE_SHARE_READ, FILE_SYNCHRONOUS_IO_NONALERT);
    if (NT_SUCCESS(status)) {
        spindle++;
        if (spindle > 2)
            spindle = 0;
        status = NtDeviceIoControlFile(handle, NULL, NULL, NULL, &io_status, 0x24084, &spindle, sizeof(spindle), NULL, 0);
        if (!NT_SUCCESS(status)) {
            debugPrint("Failed to set spindle speed! Status: 0x%08lX\n", status);
        }
    }
    NtClose(handle);
    debugPrint("Set DVD spindle speed: %lu\n", spindle);
    Sleep(500);
}

void toggleudmamode(char *)
{
    UCHAR data = 0x03;
    WRITE_PORT_BUFFER_UCHAR((PUCHAR)0x01F1, &data, 1);
    udma++;
    if (udma > 0x46)
        udma = 0x40;
    data = udma;
    WRITE_PORT_BUFFER_UCHAR((PUCHAR)0x01F2, &data, 1);
    data = 0xEF;
    WRITE_PORT_BUFFER_UCHAR((PUCHAR)0x01F7, &data, 1);

    debugPrint("Set UMDA: %lX\n", udma - 0x40);
    Sleep(500);
}
