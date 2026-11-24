#include "fmod_settings.h"

#include "fmod_memory.h"
#include "fmod_os_cdda.h"
#include "fmod_string.h"
#include "fmod_time.h"
#include "fmod_debug.h"

#include <windows.h>
#include <VersionHelpers.h>
#include <stdio.h>
#include <stddef.h>


namespace FMOD
{


//=============================================================================================
//  Constants
//=============================================================================================
#define SAFE_FREE(x) if ((x)) { FMOD_Memory_Free((x)); (x) = 0; }

static const char FMOD_CDDA_ASPIDLLNAME[]          = "wnaspi32.dll";
static const char FMOD_CDDA_GETASPI32SUPPORTINFO[] = "GetASPI32SupportInfo";
static const char FMOD_CDDA_SENDASPI32COMMAND[]    = "SendASPI32Command";

#define SIZEOF_QSUBCHANNEL                  0
#define SIZEOF_CDROM_SECTOR                 2048
#define SIZEOF_CDAUDIO                      (SIZEOF_CDDA_SECTOR - SIZEOF_QSUBCHANNEL)
#define USING_NTSCSI                        (FMOD_CDDA_GetASPI32SupportInfo == FMOD_CDDA_NTSCSI_GetASPI32SupportInfo)

#define METHOD_BUFFERED                     0
#define METHOD_IN_DIRECT                    1
#define METHOD_OUT_DIRECT                   2
#define METHOD_NEITHER                      3

#define FILE_ANY_ACCESS                     0
#ifndef FILE_READ_ACCESS
  #define FILE_READ_ACCESS                  (0x0001)
#endif
#ifndef FILE_WRITE_ACCESS
  #define FILE_WRITE_ACCESS                 (0x0002)
#endif

#define IOCTL_SCSI_BASE                     0x00000004

#define SCSI_IOCTL_DATA_OUT                 0
#define SCSI_IOCTL_DATA_IN                  1
#define SCSI_IOCTL_DATA_UNSPECIFIED         2

#define CTL_CODE( DevType, Function, Method, Access ) (                 \
    ((DevType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method) \
)

#define IOCTL_SCSI_PASS_THROUGH             CTL_CODE( IOCTL_SCSI_BASE, 0x0401, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS )
#define IOCTL_SCSI_MINIPORT                 CTL_CODE( IOCTL_SCSI_BASE, 0x0402, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS )
#define IOCTL_SCSI_GET_INQUIRY_DATA         CTL_CODE( IOCTL_SCSI_BASE, 0x0403, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SCSI_GET_CAPABILITIES         CTL_CODE( IOCTL_SCSI_BASE, 0x0404, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SCSI_PASS_THROUGH_DIRECT      CTL_CODE( IOCTL_SCSI_BASE, 0x0405, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS )
#define IOCTL_SCSI_GET_ADDRESS              CTL_CODE( IOCTL_SCSI_BASE, 0x0406, METHOD_BUFFERED, FILE_ANY_ACCESS )

/*
    Peripheral device type definitions
*/
#define DTYPE_WORM                          0x04        // Write-once read-multiple
#define DTYPE_CDROM                         0x05        // CD-ROM device
#define DTYPE_UNKNOWN                       0x1F        // Unknown or no device type

/*
    SCSI commands
*/
#define SCSI_TST_U_RDY                      0x00        // Test Unit Ready (MANDATORY)
#define SCSI_INQUIRY                        0x12        // Inquiry (MANDATORY)
#define SCSI_MODE_SEN6                      0x1A        // Mode Sense 6-byte (Device Specific)
#define SCSI_PREVENT_ALLOW_MEDIUM_REMOVAL   0x1E        // Prevent/Allow Medium Removal (O)
#define SCSI_READ_TOC                       0x43        // Read TOC (O)
#define SCSI_MODE_SEN10                     0x5A        // Mode Sense 10-byte (Device Specific)
#define SCSI_READ_MMC                       0xBE
#define SCSI_SET_SPEED                      0xBB

/*
    SCSI mode page codes for use with SCSI_MODE_SEN6 and SCSI_MODE_SEN10 commands
*/
#define SCSI_MODEPAGE_ERROR_RECOVERY        0x01        // Read/write error recovery page
#define SCSI_MODEPAGE_CDPARAMETERS          0x0D        // CD parameters page
#define SCSI_MODEPAGE_CDAUDIO_CONTROL       0x0E        // CD audio control page
#define SCSI_MODEPAGE_POWER_CONDITION       0x1A        // Power condition page
#define SCSI_MODEPAGE_TIMEOUT_AND_PROTECT   0x1D        // Timeout and protect page
#define SCSI_MODEPAGE_CAPS                  0x2A        // CD capabilities and mechanism status page
#define SCSI_MODEPAGE_ALL                   0x3F        // All mode pages

#define SCSI_READ_TOC_FORMAT_CDTEXT         5

#define SENSE_LEN                           14          // Default sense buffer length
#define SRB_DIR_SCSI                        0x00        // Direction determined by SCSI
#define SRB_POSTING                         0x01        // Enable ASPI posting
#define SRB_ENABLE_RESIDUAL_COUNT           0x04        // Enable residual byte count reporting
#define SRB_DIR_IN                          0x08        // Transfer from SCSI target to host
#define SRB_DIR_OUT                         0x10        // Transfer from host to SCSI target
#define SRB_EVENT_NOTIFY                    0x40        // Enable ASPI event notification

/*
    ASPI Commands
*/
#define SC_HA_INQUIRY                       0x00        // Host adapter inquiry
#define SC_GET_DEV_TYPE                     0x01        // Get device type
#define SC_EXEC_SCSI_CMD                    0x02        // Execute SCSI command
#define SC_ABORT_SRB                        0x03        // Abort an SRB
#define SC_RESET_DEV                        0x04        // SCSI bus device reset
#define SC_SET_HA_PARMS                     0x05        // Set HA parameters
#define SC_GET_DISK_INFO                    0x06        // Get Disk information
#define SC_RESCAN_SCSI_BUS                  0x07        // Rebuild SCSI device map
#define SC_GETSET_TIMEOUTS                  0x08        // Get/Set target timeouts

/*
    SRB Status
*/
#define SS_PENDING                          0x00        // SRB being processed
#define SS_COMP                             0x01        // SRB completed without error
#define SS_ABORTED                          0x02        // SRB aborted
#define SS_ABORT_FAIL                       0x03        // Unable to abort SRB
#define SS_ERR                              0x04        // SRB completed with error
#define SS_INVALID_CMD                      0x80        // Invalid ASPI command
#define SS_INVALID_HA                       0x81        // Invalid host adapter number
#define SS_NO_DEVICE                        0x82        // SCSI device not installed
#define SS_INVALID_SRB                      0xE0        // Invalid parameter set in SRB
#define SS_OLD_MANAGER                      0xE1        // ASPI manager doesn't support Windows
#define SS_BUFFER_ALIGN                     0xE1        // Buffer not aligned (replaces OLD_MANAGER in Win32)
#define SS_ILLEGAL_MODE                     0xE2        // Unsupported Windows mode
#define SS_NO_ASPI                          0xE3        // No ASPI managers resident
#define SS_FAILED_INIT                      0xE4        // ASPI for windows failed init
#define SS_ASPI_IS_BUSY                     0xE5        // No resources available to execute cmd
#define SS_BUFFER_TO_BIG                    0xE6        // Buffer size to big to handle!
#define SS_MISMATCHED_COMPONENTS            0xE7        // The DLLs/EXEs of ASPI don't version check
#define SS_NO_ADAPTERS                      0xE8        // No host adapters to manage
#define SS_INSUFFICIENT_RESOURCES           0xE9        // Couldn't allocate resources needed to init
#define SS_ASPI_IS_SHUTDOWN                 0xEA        // Call came to ASPI after PROCESS_DETACH
#define SS_BAD_INSTALL                      0xEB        // The DLL or other components are installed wrong


//=============================================================================================
//  Typedefs
//=============================================================================================
#ifdef FMOD_SUPPORT_PRAGMAPACK
    #pragma pack(1)
#endif

struct SRB
{
    BYTE        SRB_Cmd;
    BYTE        SRB_Status;
    BYTE        SRB_HaId;
    BYTE        SRB_Flags;
    DWORD       SRB_Hdr_Rsvd;
};

typedef struct                                  // Offset
{                                               // HX/DEC
    BYTE        SRB_Cmd;                        // 00/000 ASPI command code = SC_HA_INQUIRY
    BYTE        SRB_Status;                     // 01/001 ASPI command status byte
    BYTE        SRB_HaId;                       // 02/002 ASPI host adapter number
    BYTE        SRB_Flags;                      // 03/003 ASPI request flags
    DWORD       SRB_Hdr_Rsvd;                   // 04/004 Reserved, MUST = 0
    BYTE        HA_Count;                       // 08/008 Number of host adapters present
    BYTE        HA_SCSI_ID;                     // 09/009 SCSI ID of host adapter
    BYTE        HA_ManagerId[16];               // 0A/010 String describing the manager
    BYTE        HA_Identifier[16];              // 1A/026 String describing the host adapter
    BYTE        HA_Unique[16];                  // 2A/042 Host Adapter Unique parameters
    WORD        HA_Rsvd1;                       // 3A/058 Reserved, MUST = 0
}
SRB_HAInquiry, *PSRB_HAInquiry, FAR *LPSRB_HAInquiry;

typedef struct                                  // Offset
{                                               // HX/DEC
    BYTE        SRB_Cmd;                        // 00/000 ASPI command code = SC_GET_DEV_TYPE
    BYTE        SRB_Status;                     // 01/001 ASPI command status byte
    BYTE        SRB_HaId;                       // 02/002 ASPI host adapter number
    BYTE        SRB_Flags;                      // 03/003 Reserved, MUST = 0
    DWORD       SRB_Hdr_Rsvd;                   // 04/004 Reserved, MUST = 0
    BYTE        SRB_Target;                     // 08/008 Target's SCSI ID
    BYTE        SRB_Lun;                        // 09/009 Target's LUN number
    BYTE        SRB_DeviceType;                 // 0A/010 Target's peripheral device type
    BYTE        SRB_Rsvd1;                      // 0B/011 Reserved, MUST = 0
}
SRB_GDEVBlock, *PSRB_GDEVBlock, FAR *LPSRB_GDEVBlock;

typedef struct                                  // Offset
{                                               // HX/DEC
    BYTE        SRB_Cmd;                        // 00/000 ASPI command code = SC_EXEC_SCSI_CMD
    BYTE        SRB_Status;                     // 01/001 ASPI command status byte
    BYTE        SRB_HaId;                       // 02/002 ASPI host adapter number
    BYTE        SRB_Flags;                      // 03/003 ASPI request flags
    DWORD       SRB_Hdr_Rsvd;                   // 04/004 Reserved
    BYTE        SRB_Target;                     // 08/008 Target's SCSI ID
    BYTE        SRB_Lun;                        // 09/009 Target's LUN number
    WORD        SRB_Rsvd1;                      // 0A/010 Reserved for Alignment
    DWORD       SRB_BufLen;                     // 0C/012 Data Allocation Length
    BYTE        FAR *SRB_BufPointer;            // 10/016 Data Buffer Pointer
    BYTE        SRB_SenseLen;                   // 14/020 Sense Allocation Length
    BYTE        SRB_CDBLen;                     // 15/021 CDB Length
    BYTE        SRB_HaStat;                     // 16/022 Host Adapter Status
    BYTE        SRB_TargStat;                   // 17/023 Target Status
    VOID        FAR *SRB_PostProc;              // 18/024 Post routine
    BYTE        SRB_Rsvd2[20];                  // 1C/028 Reserved, MUST = 0
    BYTE        CDBByte[16];                    // 30/048 SCSI CDB
    BYTE        SenseArea[SENSE_LEN+2];         // 50/064 Request Sense buffer
}
SRB_ExecSCSICmd, *PSRB_ExecSCSICmd, FAR *LPSRB_ExecSCSICmd;

typedef struct                                  // Offset
{                                               // HX/DEC
    BYTE        SRB_Cmd;                        // 00/000 ASPI command code = SC_ABORT_SRB
    BYTE        SRB_Status;                     // 01/001 ASPI command status byte
    BYTE        SRB_HaId;                       // 02/002 ASPI host adapter number
    BYTE        SRB_Flags;                      // 03/003 Reserved
    DWORD       SRB_Hdr_Rsvd;                   // 04/004 Reserved
    VOID        FAR *SRB_ToAbort;               // 08/008 Pointer to SRB to abort
}
SRB_Abort, *PSRB_Abort, FAR *LPSRB_Abort;

#ifdef FMOD_SUPPORT_PRAGMAPACK
    #ifdef 	CODEWARRIOR
    #pragma pack(0)
    #else
    #pragma pack()
    #endif
#endif

typedef struct
{
    unsigned char         ha;
    unsigned char         tgt;
    unsigned char         lun;
    unsigned char         letter;
    bool                  used;
    HANDLE                device;
    FMOD_CDDA_InquiryData inq_data;

} FMOD_CDDA_NTSCSIDRIVE;

typedef struct
{
    USHORT Length;
    UCHAR  ScsiStatus;
    UCHAR  PathId;
    UCHAR  TargetId;
    UCHAR  Lun;
    UCHAR  CdbLength;
    UCHAR  SenseInfoLength;
    UCHAR  DataIn;
    ULONG  DataTransferLength;
    ULONG  TimeOutValue;
    ULONG  DataBufferOffset;
    ULONG  SenseInfoOffset;
    UCHAR  Cdb[16];
} SCSI_PASS_THROUGH;

typedef struct
{
    USHORT Length;
    UCHAR  ScsiStatus;
    UCHAR  PathId;
    UCHAR  TargetId;
    UCHAR  Lun;
    UCHAR  CdbLength;
    UCHAR  SenseInfoLength;
    UCHAR  DataIn;
    ULONG  DataTransferLength;
    ULONG  TimeOutValue;
    PVOID  DataBuffer;
    ULONG  SenseInfoOffset;
    UCHAR  Cdb[16];
} SCSI_PASS_THROUGH_DIRECT;

typedef struct
{
    SCSI_PASS_THROUGH_DIRECT spt;
    ULONG Filler;
    UCHAR ucSenseBuf[32];
} SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER;

typedef struct
{
    ULONG Length;
    UCHAR PortNumber;
    UCHAR PathId;
    UCHAR TargetId;
    UCHAR Lun;
} SCSI_ADDRESS;

typedef void (*POSTPROCFUNC)();

typedef struct
{
    unsigned char session;

    unsigned char pre_emphasis           : 1;
    unsigned char digital_copy_permitted : 1;
    unsigned char data_track             : 1;
    unsigned char four_channel           : 1;
    unsigned char adr                    : 4;

    unsigned char tno;
    unsigned char point;
    unsigned char min;
    unsigned char sec;
    unsigned char frame;
    unsigned char zero;
    unsigned char pmin;
    unsigned char psec;
    unsigned char pframe;

} FMOD_CDDA_RawTocEntry;

typedef struct
{
    unsigned char         datalen_hi;
    unsigned char         datalen_lo;
    unsigned char         first_session;
    unsigned char         last_session;
    FMOD_CDDA_RawTocEntry toc_entry[1];

} FMOD_CDDA_RawTocDesc;


static char FMOD_CDDA_StrUnknown[] = "Unknown";

FMOD_CDDA_StringList FMOD_CDDA_Aspi_Strings[] =
{
    { SS_PENDING,                "SRB being processed" },
    { SS_COMP,                   "SRB completed without error" },
    { SS_ABORTED,                "SRB aborted" },
    { SS_ABORT_FAIL,             "Unable to abort SRB" },
    { SS_ERR,                    "SRB completed with error" },
    { SS_INVALID_CMD,            "Invalid ASPI command" },
    { SS_INVALID_HA,             "Invalid host adapter number" },
    { SS_NO_DEVICE,              "SCSI device not installed" },
    { SS_INVALID_SRB,            "Invalid parameter set in SRB" },
    { SS_OLD_MANAGER,            "ASPI manager doesn't support Windows" },
    { SS_BUFFER_ALIGN,           "Buffer not aligned" },
    { SS_ILLEGAL_MODE,           "Unsupported Windows mode" },
    { SS_NO_ASPI,                "No ASPI managers resident" },
    { SS_FAILED_INIT,            "ASPI for windows failed init" },
    { SS_ASPI_IS_BUSY,           "No resources available to execute cmd" },
    { SS_BUFFER_TO_BIG,          "Buffer size too big to handle!" },
    { SS_MISMATCHED_COMPONENTS,  "The DLLs/EXEs of ASPI don't version check" },
    { SS_NO_ADAPTERS,            "No host adapters to manage" },
    { SS_INSUFFICIENT_RESOURCES, "Couldn't allocate resources needed to init" },
    { SS_ASPI_IS_SHUTDOWN,       "Call came to ASPI after PROCESS_DETACH" },
    { SS_BAD_INSTALL,            "The DLL or other components are installed wrong" },
    { static_cast<unsigned int>(-1), (char *)-1 }
};


//=============================================================================================
//  Function declarations
//=============================================================================================
FMOD_RESULT FMOD_CDDA_Close(FMOD_CDDA_DEVICE *cdrom);
bool        FMOD_CDDA_InquiryCommand(unsigned char adapter_id, unsigned char target_id, unsigned char lun_id);
bool        FMOD_CDDA_PreventMediaRemoval(FMOD_CDDA_DEVICE *cdrom, bool prevent);
int         FMOD_CDDA_IssueScsiCmd(FMOD_CDDA_DEVICE *cdrom, int flags, unsigned char *cdb_data, int cdb_len, char *buf, int buflen);

FMOD_RESULT FMOD_CDDA_NTSCSI_Init();
bool        FMOD_CDDA_NTSCSI_Shutdown();
DWORD       FMOD_CDDA_NTSCSI_GetASPI32SupportInfo();
DWORD       FMOD_CDDA_NTSCSI_SendASPI32Command(SRB *srb);
int         FMOD_CDDA_NTSCSI_GetDeviceIndex(unsigned char ha, unsigned char tgt, unsigned char lun);


//=============================================================================================
//  Variables
//=============================================================================================
static bool                      FMOD_CDDA_Initialised          = false;
static FUNC_GETASPI32SUPPORTINFO FMOD_CDDA_GetASPI32SupportInfo = 0;
static FUNC_SENDASPI32COMMAND    FMOD_CDDA_SendASPI32Command    = 0;
static HINSTANCE                 FMOD_CDDA_DllHandle            = 0;
static int                       FMOD_CDDA_NumDevices           = 0;
static unsigned int              FMOD_CDDA_Aspi_Timeout         = 10000;
static FMOD_CDDA_DEVICE         *FMOD_CDDA_DeviceList[FMOD_CDDA_MAX_DEVICES];
static bool                      FMOD_CDDA_NTSCSI_Initialised   = false;
static FMOD_CDDA_NTSCSIDRIVE    *FMOD_CDDA_NTSCSI_Drive         = 0;
static int                       FMOD_CDDA_NTSCSI_NumAdapters;
static int                       FMOD_CDDA_NTSCSI_AdapterLookup[FMOD_CDDA_MAX_DEVICES];
static int                       FMOD_CDDA_FakeLun              = 0;


#ifdef FMOD_OS_CDDA_DEBUG
void FMOD_OS_CDDA_DebugInit()
{
    FILE *fp = fopen(FMOD_OS_CDDA_LOG_FILENAME, "wt");
    if (fp)
    {
        OSVERSIONINFO ver;

        fprintf(fp, "FMOD Version:\t%d.%d.%d\n", (((unsigned int)FMOD_VERSION) >> 16) & 0xffff, (((unsigned int)FMOD_VERSION) >> 8) & 0xff, FMOD_VERSION & 0xff);
        ver.dwOSVersionInfoSize = sizeof(ver);
        fprintf(fp, "OS Version:\t");
        if (GetVersionEx(&ver))
        {
            signed char is95 = ((ver.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) && (ver.dwMinorVersion == 0));
            fprintf(fp, "%d.%d  Build %d  %s\n", ver.dwMajorVersion, 
                                                 ver.dwMinorVersion, 
                                                 is95 ? (ver.dwBuildNumber & 0xffff) : ver.dwBuildNumber, 
                                                 ver.szCSDVersion);
        }
        fclose(fp);
    }
}
void FMOD_OS_CDDA_DebugPrint(char *format, ...)
{
    FILE *fp = 0;
    va_list arglist;
    va_start(arglist, format);
    for (;;)
    {
        fp = fopen(FMOD_OS_CDDA_LOG_FILENAME, "at");
        if (fp)
        {
            break;
        }
        else
        {
            Sleep(10);
        }
    }
    vfprintf(fp, format, arglist);
    fclose(fp);
    va_end(arglist);
}
void FMOD_OS_CDDA_AddProfileData(int elapsedtime, unsigned int bytesread)
{
    if (elapsedtime > 0)
    {
        int bytespersecond = (int)(bytesread / ((float)elapsedtime / 1000.0f));
        FMOD_OS_CDDA_DEBUGPRINT("    %d kb/s\n", bytespersecond / 1024);
    }
}
#endif


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
char *FMOD_CDDA_GetString(unsigned int value, FMOD_CDDA_StringList *stringlist)
{
    int i;

    for (i=0;stringlist[i].string != (char *)-1;i++)
    {
        if (stringlist[i].value == value)
        {
            return stringlist[i].string;
        }
    }

    return FMOD_CDDA_StrUnknown;
}


/*
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
// VERIFY: 5f1259a3 manual
// OFFSET: 0x00918A70
FMOD_RESULT FMOD_OS_CDDA_Init(bool force_aspi)
{
    unsigned int status, error;
    int          num_ha, adapter_id, lun_id, target_id, i;
    bool         ntscsi_ok = false;

    if (FMOD_CDDA_Initialised)
    {
        return FMOD_OK;
    }

    FMOD_OS_CDDA_DEBUGINIT;

    for (i=0;i < FMOD_CDDA_MAX_DEVICES;i++)
    {
        FMOD_CDDA_DeviceList[i] = 0;
    }
    FMOD_CDDA_NumDevices = 0;

    /*
        Try using NTSCSI first
    */
    if (!force_aspi)
    {
        if (FMOD_CDDA_NTSCSI_Init() == FMOD_OK)
        {
            FMOD_CDDA_GetASPI32SupportInfo = FMOD_CDDA_NTSCSI_GetASPI32SupportInfo;
            FMOD_CDDA_SendASPI32Command    = FMOD_CDDA_NTSCSI_SendASPI32Command;
            FMOD_OS_CDDA_DEBUGPRINT("Using NTSCSI\n");
            ntscsi_ok = true;
        }
    }

    if (!ntscsi_ok)
    {
        /*
            Then try using ASPI
        */
        char dllname[2048];

        for (i=0;i < FMOD_CDDA_MAX_DEVICES;i++)
        {
            FMOD_CDDA_DeviceList[i] = 0;
        }
        FMOD_CDDA_NumDevices = 0;

        FMOD_strcpy(dllname, FMOD_CDDA_ASPIDLLNAME);
        if (!(FMOD_CDDA_DllHandle = LoadLibrary(dllname)))
        {
            GetSystemDirectory(dllname, 2047);
            FMOD_strcat(dllname, "\\");
            FMOD_strcat(dllname, FMOD_CDDA_ASPIDLLNAME);

            if (!(FMOD_CDDA_DllHandle = LoadLibrary(dllname)))
            {
                GetWindowsDirectory(dllname, 2047);
                FMOD_strcat(dllname, "\\");
                FMOD_strcat(dllname, FMOD_CDDA_ASPIDLLNAME);
                FMOD_CDDA_DllHandle = LoadLibrary(dllname);
            }
        }

        if (!FMOD_CDDA_DllHandle)
        {
            FMOD_OS_CDDA_Shutdown();
            return FMOD_ERR_CDDA_DRIVERS;
        }

        FMOD_CDDA_GetASPI32SupportInfo = (FUNC_GETASPI32SUPPORTINFO)GetProcAddress(FMOD_CDDA_DllHandle, FMOD_CDDA_GETASPI32SUPPORTINFO);
        FMOD_CDDA_SendASPI32Command    = (FUNC_SENDASPI32COMMAND)GetProcAddress(FMOD_CDDA_DllHandle, FMOD_CDDA_SENDASPI32COMMAND);

        if (!FMOD_CDDA_GetASPI32SupportInfo || !FMOD_CDDA_SendASPI32Command)
        {
            FMOD_OS_CDDA_DEBUGPRINT("FMOD_OS_CDDA_Init: GetProcAddress failed!\n");
            FMOD_OS_CDDA_Shutdown();

            return FMOD_ERR_CDDA_DRIVERS;
        }

        FMOD_OS_CDDA_DEBUGPRINT("Using ASPI\n");
    }

    status = FMOD_CDDA_GetASPI32SupportInfo();
    error  = HIBYTE(LOWORD(status));
    if (error == SS_COMP)
    {
        num_ha = LOBYTE(LOWORD(status));
    }
    else
    {
        FMOD_OS_CDDA_DEBUGPRINT("FMOD_OS_CDDA_Init: ASPI/NTSCSI didn't startup properly : %s\n", FMOD_CDDA_GetString(error, FMOD_CDDA_Aspi_Strings));
        FMOD_OS_CDDA_Shutdown();

        return FMOD_ERR_CDDA_INIT;
    }

    /*
        Get info on all available cdrom drives
    */
    for (adapter_id=0;adapter_id < 16;adapter_id++)
    {
        for (target_id=0;target_id < 12;target_id++)
        {
            for (lun_id=0;lun_id < 8;lun_id++)
            {
                SRB_GDEVBlock srb;
                unsigned int start_time;

                FMOD_memset(&srb, 0, sizeof(srb));

                srb.SRB_Cmd    = SC_GET_DEV_TYPE;
                srb.SRB_HaId   = adapter_id;
                srb.SRB_Target = target_id;
                srb.SRB_Lun    = lun_id;

                FMOD_CDDA_SendASPI32Command((SRB *)&srb);

                start_time = GetTickCount();
                while ((srb.SRB_Status == SS_PENDING) && ((GetTickCount() - start_time) < 10000))
                {
                    FMOD_OS_Time_Sleep(1);
                }

#ifdef FMOD_OS_CDDA_DEBUG
                switch (srb.SRB_DeviceType)
                {
                    case DTYPE_WORM :
                        FMOD_OS_CDDA_DEBUGPRINT("%02d:%02d:%02d = DTYPE_WORM", adapter_id, target_id, lun_id);
                        break;
                    case DTYPE_CDROM :
                        FMOD_OS_CDDA_DEBUGPRINT("%02d:%02d:%02d = DTYPE_CDROM", adapter_id, target_id, lun_id);
                        break;
                }
#endif

                if ((srb.SRB_DeviceType == DTYPE_CDROM) || (srb.SRB_DeviceType == DTYPE_WORM))
                {
                    FMOD_CDDA_InquiryCommand(adapter_id, target_id, lun_id);

#ifdef FMOD_OS_CDDA_DEBUG
                    if (USING_NTSCSI)
                    {
                        char letter = FMOD_CDDA_NTSCSI_GetDeviceIndex(adapter_id, target_id, lun_id);
                        if (letter)
                        {
                            FMOD_OS_CDDA_DEBUGPRINT(" = %c:\n", letter + 'A');
                        }
                        else
                        {
                            FMOD_OS_CDDA_DEBUGPRINT(" = ???:\n");
                        }
                    }
                    else
                    {
                        FMOD_OS_CDDA_DEBUGPRINT("\n");
                    }
#endif
                }
            }
        }
    }

    if (FMOD_CDDA_NumDevices < 1)
    {
        FMOD_OS_CDDA_DEBUGPRINT("FMOD_OS_CDDA_Init: no cdrom devices found\n");
        FMOD_OS_CDDA_Shutdown();

        return FMOD_ERR_CDDA_NODEVICES;
    }

    FMOD_CDDA_Initialised = true;
    FMOD_OS_CDDA_DEBUGPRINT("FMOD_OS_CDDA_Init: Ok\n");

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
// VERIFY: f9449a79 manual
// OFFSET: 0x00918460
FMOD_RESULT FMOD_OS_CDDA_Shutdown()
{
    int i;

    if (!FMOD_CDDA_Initialised)
    {
        return FMOD_OK;
    }

    for (i=0;i < FMOD_CDDA_MAX_DEVICES;i++)
    {
        if (FMOD_CDDA_DeviceList[i])
        {
            FMOD_CDDA_Close(FMOD_CDDA_DeviceList[i]);
            FMOD_Memory_Free(FMOD_CDDA_DeviceList[i]);
            FMOD_CDDA_DeviceList[i] = 0;
        }
    }

    if (FMOD_CDDA_DllHandle)
    {
        FreeLibrary(FMOD_CDDA_DllHandle);
        FMOD_CDDA_DllHandle = 0;
    }

    FMOD_CDDA_NTSCSI_Shutdown();

    FMOD_CDDA_GetASPI32SupportInfo = 0;
    FMOD_CDDA_SendASPI32Command    = 0;

    FMOD_CDDA_Initialised = false;

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
bool FMOD_OS_CDDA_IsDriveLetter(char *name)
{
    if ((name[1] == ':') && (name[2] == 0))
    {
        /*
            Drive letter style - "d:" (or wide char)
        */
        if (FMOD_isalpha(name[0]))
        {
            return true;
        }
    }

    return false;
}


// VERIFY: 41767b20 manual
// OFFSET: 0x00916F20
bool FMOD_OS_CDDA_IsDeviceName(char *name)
{
    if (!name)
    {
        return false;
    }

    if (FMOD_OS_CDDA_IsDriveLetter(name))
    {
        return true;
    }
    else if ((name[2] == ':') && (name[5] == ':') && (name[8] == 0))
    {
        /*
            SCSI address style - "00:01:00"
        */
        if (FMOD_isdigit(name[0]) && FMOD_isdigit(name[1]) &&
            FMOD_isdigit(name[3]) && FMOD_isdigit(name[4]) &&
            FMOD_isdigit(name[6]) && FMOD_isdigit(name[7]))
        {
            return true;
        }
    }

    return false;
}


/*
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_CDDA_GetNumDevices(int *num)
{
    FMOD_RESULT result;

    if (!FMOD_CDDA_Initialised)
    {
        result = FMOD_OS_CDDA_Init(false);
        if (result != FMOD_OK)
        {
            return result;
        }
    }

    if (!num)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    *num = FMOD_CDDA_NumDevices;

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_CDDA_GetDeviceName(int devicenum, char *name, int namelen, char *scsiaddr, int scsiaddrlen, char *devicename, int devicenamelen)
{
    FMOD_RESULT result;
    FMOD_CDDA_DEVICE *device;

    if (!FMOD_CDDA_Initialised)
    {
        result = FMOD_OS_CDDA_Init(false);
        if (result != FMOD_OK)
        {
            return result;
        }
    }

    device = FMOD_CDDA_DeviceList[devicenum];
    if (device)
    {
        if (name && namelen)
        {
            FMOD_strncpy(name, device->name, namelen);
        }

        if (scsiaddr && scsiaddrlen)
        {
            FMOD_strncpy(scsiaddr, device->scsiaddr, scsiaddrlen);
        }

        if (devicename && devicenamelen)
        {
            FMOD_strncpy(devicename, device->devicename, devicenamelen);
        }
    }
    else
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
// VERIFY: db47d99e manual
// OFFSET: 0x009184E0
FMOD_RESULT FMOD_OS_CDDA_GetDriveFromLetter(int letter, FMOD_CDDA_DEVICE **device)
{
    int i;
    FMOD_CDDA_DEVICE *d;

    letter -= ((letter > 'Z') ? 'a' : 'A');

    FMOD_OS_CDDA_DEBUGPRINT("FMOD_OS_CDDA_GetDriveFromLetter: %c:\n", letter + 'a');

    if (!device)
    {
        return FMOD_ERR_INVALID_PARAM;
    }
    *device = 0;

    for (i=0; i < FMOD_CDDA_NumDevices; i++)
    {
        d = FMOD_CDDA_DeviceList[i];
        if (d->letter == letter)
        {
            FMOD_OS_CDDA_DEBUGPRINT("FMOD_OS_CDDA_GetDriveFromLetter: Already mapped: %c: == %02d:%02d:%02d\n", d->letter + 'a', d->adapter_id, d->target_id, d->lun_id);
            *device = d;
            return FMOD_OK;
        }
    }

    /*
        If we're using ASPI we have to try to map the specified drive letter to a SCSI id to
        find what drive we're after
    */
    if (!USING_NTSCSI)
    {
        MCI_OPEN_PARMS   mciOpenParms;
        MCI_STATUS_PARMS mciStatusParms;
        MCIERROR         err;
        MCIDEVICEID      device_id;
        char             drive_letter[] = "c:";
        int              numtracks, count;
        int              mci_toc[99], aspi_toc[99];

        FMOD_OS_CDDA_DEBUGPRINT("FMOD_OS_CDDA_GetDriveFromLetter: Finding SCSI ID for %c:\n", letter + 'a');

        /*
            If there's only one drive found by ASPI then it's probably our man
        */
        if (FMOD_CDDA_NumDevices == 1)
        {
            FMOD_OS_CDDA_DEBUGPRINT("FMOD_OS_CDDA_GetDriveFromLetter: Only one drive\n");

            /*
                Make sure they specified a valid drive letter though
            */
            FMOD_memset(&mciOpenParms, 0, sizeof(MCI_OPEN_PARMS));
            mciOpenParms.lpstrDeviceType = (LPCSTR)MCI_DEVTYPE_CD_AUDIO;
            drive_letter[0] = letter + 'A';
            mciOpenParms.lpstrElementName = drive_letter;
            err = mciSendCommand(0, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_ELEMENT | MCI_OPEN_TYPE_ID, (DWORD_PTR)(LPVOID)&mciOpenParms);
            if (err)
            {
                FMOD_OS_CDDA_DEBUGPRINT("FMOD_OS_CDDA_GetDriveFromLetter: Open failed\n");
                return FMOD_ERR_CDDA_INVALID_DEVICE;
            }
            device_id = mciOpenParms.wDeviceID;
            mciSendCommand(device_id, MCI_CLOSE, 0, 0);

            d = FMOD_CDDA_DeviceList[0];
            d->letter = letter;
            *device = d;
            return FMOD_OK;
        }

        /*
            If there's only one drive left unmapped then it's probably our man
        */
        {
            int unmapped = 0;

            for (i=0;i < FMOD_CDDA_NumDevices;i++)
            {
                if (FMOD_CDDA_DeviceList[i]->letter == 0)
                {
                    unmapped++;
                }
            }

            if (unmapped == 1)
            {
                FMOD_OS_CDDA_DEBUGPRINT("FMOD_OS_CDDA_GetDriveFromLetter: Only one drive unmapped\n");

                for (i=0;i < FMOD_CDDA_NumDevices;i++)
                {
                    if (FMOD_CDDA_DeviceList[i]->letter == 0)
                    {
                        /*
                            Make sure they specified a valid drive letter though
                        */
                        FMOD_memset(&mciOpenParms, 0, sizeof(MCI_OPEN_PARMS));
                        mciOpenParms.lpstrDeviceType = (LPCSTR)MCI_DEVTYPE_CD_AUDIO;
                        drive_letter[0] = letter + 'A';
                        mciOpenParms.lpstrElementName = drive_letter;
                        err = mciSendCommand(0, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_ELEMENT | MCI_OPEN_TYPE_ID, (DWORD_PTR)(LPVOID)&mciOpenParms);
                        if (err)
                        {
                            FMOD_OS_CDDA_DEBUGPRINT("FMOD_OS_CDDA_GetDriveFromLetter: Open failed\n");
                            return FMOD_ERR_CDDA_INVALID_DEVICE;
                        }
                        device_id = mciOpenParms.wDeviceID;
                        mciSendCommand(device_id, MCI_CLOSE, 0, 0);

                        d = FMOD_CDDA_DeviceList[i];
                        d->letter = letter;
                        *device = d;
                        return FMOD_OK;
                    }
                }
            }
        }

        /*
            Read the TOC using MCI
        */
        FMOD_memset(mci_toc,  0, sizeof(int) * 99);
        FMOD_memset(aspi_toc, 0, sizeof(int) * 99);

        FMOD_memset(&mciOpenParms, 0, sizeof(MCI_OPEN_PARMS));
        mciOpenParms.lpstrDeviceType = (LPCSTR)MCI_DEVTYPE_CD_AUDIO;
        drive_letter[0] = letter + 'A';
        mciOpenParms.lpstrElementName = drive_letter;
        err = mciSendCommand(0, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_ELEMENT | MCI_OPEN_TYPE_ID, (DWORD_PTR)(LPVOID)&mciOpenParms);
        if (err)
        {
            FMOD_OS_CDDA_DEBUGPRINT("FMOD_OS_CDDA_GetDriveFromLetter: Open failed\n");
            return FMOD_ERR_CDDA_INVALID_DEVICE;
        }
        device_id = mciOpenParms.wDeviceID;

        FMOD_memset(&mciStatusParms, 0, sizeof(MCI_STATUS_PARMS));
        mciStatusParms.dwItem = MCI_STATUS_READY;
        err = mciSendCommand(device_id, MCI_STATUS, MCI_STATUS_ITEM | MCI_WAIT, (DWORD_PTR)(LPVOID)&mciStatusParms);
        if (err || (mciStatusParms.dwReturn == FALSE))
        {
            FMOD_OS_CDDA_DEBUGPRINT("FMOD_OS_CDDA_GetDriveFromLetter: Status ready failed\n");
            mciSendCommand(device_id, MCI_CLOSE, 0, 0);
            return FMOD_ERR_CDDA_INVALID_DEVICE;
        }

        FMOD_memset(&mciStatusParms, 0, sizeof(MCI_STATUS_PARMS));
        mciStatusParms.dwItem = MCI_STATUS_NUMBER_OF_TRACKS;
        err = mciSendCommand(device_id, MCI_STATUS, MCI_STATUS_ITEM | MCI_WAIT, (DWORD_PTR)(LPVOID)&mciStatusParms);
        if (err || (mciStatusParms.dwReturn == 0))
        {
            FMOD_OS_CDDA_DEBUGPRINT("FMOD_OS_CDDA_GetDriveFromLetter: Number of tracks failed\n");
            mciSendCommand(device_id, MCI_CLOSE, 0, 0);
            return FMOD_ERR_CDDA_INVALID_DEVICE;
        }
        numtracks = (int)mciStatusParms.dwReturn;

        for (count = 1; count <= numtracks; count++)
        {
            FMOD_memset(&mciStatusParms, 0, sizeof(MCI_STATUS_PARMS));
            mciStatusParms.dwItem  = MCI_STATUS_POSITION;
            mciStatusParms.dwTrack = count;
            err = mciSendCommand(device_id, MCI_STATUS, MCI_STATUS_ITEM | MCI_TRACK, (DWORD_PTR)(LPVOID)&mciStatusParms);
            if (err)
            {
                FMOD_OS_CDDA_DEBUGPRINT("FMOD_OS_CDDA_GetDriveFromLetter: Position failed\n");
                mciSendCommand(device_id, MCI_CLOSE, 0, 0);
                return FMOD_ERR_CDDA_INVALID_DEVICE;
            }
            mci_toc[count - 1] = (MCI_MSF_MINUTE(mciStatusParms.dwReturn) * 60 * 75) + 
                                 (MCI_MSF_SECOND(mciStatusParms.dwReturn) * 75) + 
                                  MCI_MSF_FRAME(mciStatusParms.dwReturn);
        }

        FMOD_memset(&mciStatusParms, 0, sizeof(MCI_STATUS_PARMS));
        mciStatusParms.dwItem  = MCI_STATUS_LENGTH;
        mciStatusParms.dwTrack = numtracks;
        err = mciSendCommand(device_id, MCI_STATUS, MCI_STATUS_ITEM | MCI_TRACK, (DWORD_PTR)(LPVOID)&mciStatusParms);
        if (err)
        {
            FMOD_OS_CDDA_DEBUGPRINT("FMOD_OS_CDDA_GetDriveFromLetter: Length failed\n");
            mciSendCommand(device_id, MCI_CLOSE, 0, 0);
            return FMOD_ERR_CDDA_INVALID_DEVICE;
        }
        mci_toc[numtracks] = mci_toc[numtracks - 1] + (MCI_MSF_MINUTE(mciStatusParms.dwReturn) * 60 * 75) + 
                                                      (MCI_MSF_SECOND(mciStatusParms.dwReturn) * 75) + 
                                                       MCI_MSF_FRAME(mciStatusParms.dwReturn) + 1;
        numtracks++;

	    mciSendCommand(device_id, MCI_CLOSE, 0, 0);

#ifdef FMOD_OS_CDDA_DEBUG
        FMOD_OS_CDDA_DEBUGPRINT("FMOD_OS_CDDA_GetDriveFromLetter: MCI\n");
        for (count = 0; count < numtracks; count++)
        {
            FMOD_OS_CDDA_DEBUGPRINT("FMOD_OS_CDDA_GetDriveFromLetter: %d. %d\n", count, mci_toc[count]);
        }
#endif

        /*
            Read the TOC of all unmapped drives using ASPI and compare it to the MCI TOC
        */
        for (i=0;i < FMOD_CDDA_NumDevices;i++)
        {
            d = FMOD_CDDA_DeviceList[i];
            if (d->letter == 0)
            {
                FMOD_CDDA_TOC toc;
                int aspi_numtracks = 0;

                FMOD_OS_CDDA_DEBUGPRINT("FMOD_OS_CDDA_GetDriveFromLetter: Trying %s\n", d->scsiaddr);

                if (FMOD_OS_CDDA_ReadTocRaw(d, &toc) != FMOD_OK)
                {
                    if (FMOD_OS_CDDA_ReadToc(d, &toc) != FMOD_OK)
                    {
                        FMOD_OS_CDDA_DEBUGPRINT("FMOD_OS_CDDA_GetDriveFromLetter: ReadToc/Raw failed\n");
                        continue;
                    }
                }

                for (count = 0; count < (int)(toc.num_tracks - 1); count++)
                {
                    aspi_toc[aspi_numtracks++] = toc.start_sector[count] + 150;
                }
                aspi_toc[aspi_numtracks++] = toc.start_sector[count];

#ifdef FMOD_OS_CDDA_DEBUG
                FMOD_OS_CDDA_DEBUGPRINT("FMOD_OS_CDDA_GetDriveFromLetter: ASPI\n");
                for (count = 0; count < aspi_numtracks; count++)
                {
                    FMOD_OS_CDDA_DEBUGPRINT("FMOD_OS_CDDA_GetDriveFromLetter: %d. %d\n", count, aspi_toc[count]);
                }
#endif

                if (aspi_numtracks != numtracks)
                {
                    FMOD_OS_CDDA_DEBUGPRINT("FMOD_OS_CDDA_GetDriveFromLetter: Different number of tracks\n");
                    continue;
                }

                {
                    bool goto_next_drive = false;

                    for (count = 0; count < numtracks; count++)
                    {
                        if (mci_toc[count] != aspi_toc[count])
                        {
                            FMOD_OS_CDDA_DEBUGPRINT("FMOD_OS_CDDA_GetDriveFromLetter: Different TOC\n");
                            goto_next_drive = true;
                            break;
                        }
                    }

                    if (goto_next_drive)
                    {
                        continue;
                    }
                }

                d->letter = letter;
                *device   = d;

                FMOD_OS_CDDA_DEBUGPRINT("FMOD_OS_CDDA_GetDriveFromLetter: %c: == %02d:%02d:%02d\n", d->letter + 'a', d->adapter_id, d->target_id, d->lun_id);

                return FMOD_OK;
            }
        }
    }

    FMOD_OS_CDDA_DEBUGPRINT("FMOD_OS_CDDA_GetDriveFromLetter: Failed finding %c:\n", letter + 'a');

    return FMOD_ERR_CDDA_INVALID_DEVICE;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: 85165690 manual
// OFFSET: 0x00918990
FMOD_RESULT FMOD_OS_CDDA_OpenDevice(char *name, FMOD_CDDA_DEVICE **device)
{
    int               count;
    FMOD_CDDA_DEVICE *d = 0;

    if (!device)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    for (count = 0; count < FMOD_CDDA_MAX_DEVICES; count++)
    {
        d = FMOD_CDDA_DeviceList[count];
        if (d)
        {
            if (d->name)
            {
                if (!FMOD_stricmp(name, d->name))
                {
                    break;
                }
            }

            if (d->scsiaddr)
            {
                if (!FMOD_strcmp(name, d->scsiaddr))
                {
                    break;
                }
            }

            if (d->devicename)
            {
                if (!FMOD_strcmp(name, d->devicename))
                {
                    break;
                }
            }
        }
    }
    if (count >= FMOD_CDDA_MAX_DEVICES)
    {
        d = 0;

        if (FMOD_OS_CDDA_IsDriveLetter(name))
        {
            FMOD_OS_CDDA_GetDriveFromLetter(name[0], &d);
        }
    }

    if (d)
    {
        *device = d;
        d->device_open++;
    }
    else
    {
        FMOD_OS_CDDA_DEBUGPRINT("FMOD_OS_CDDA_OpenDevice: Couldn't access device %s\n", name);
        *device = 0;
        return FMOD_ERR_INVALID_PARAM;
    }

    FMOD_OS_CDDA_DEBUGPRINT("FMOD_OS_CDDA_OpenDevice: Opened %s\n", name);

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: 0c45eecf manual
// OFFSET: 0x00916FC0
FMOD_RESULT FMOD_OS_CDDA_CloseDevice(FMOD_CDDA_DEVICE *device)
{
    if (!device)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (FMOD_CDDA_NTSCSI_Drive)
    {
        if (FMOD_CDDA_NTSCSI_Drive[device->letter].used && (FMOD_CDDA_NTSCSI_Drive[device->letter].device != INVALID_HANDLE_VALUE))
        {
            CloseHandle(FMOD_CDDA_NTSCSI_Drive[device->letter].device);
            FMOD_CDDA_NTSCSI_Drive[device->letter].device = INVALID_HANDLE_VALUE;
        }
    }

    device->device_open--;

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: ab1f8a85 auto
// OFFSET: 0x00917AA0
FMOD_RESULT FMOD_OS_CDDA_ReadSectors(FMOD_CDDA_DEVICE *device, char *buf, unsigned int start_sector, unsigned int num_sectors)
{
    unsigned char cmd[12];

    if (!device)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    FMOD_memset(buf, 0, num_sectors * SIZEOF_CDDA_SECTOR);
    FMOD_memset(cmd, 0, sizeof(cmd));

    /*
        Could support other read methods here i.e. MMC2/3/4, proprietory ones etc.
    */
    cmd[0]   = SCSI_READ_MMC;
    cmd[1]   = device->lun_id << 5;
    cmd[2]   = (unsigned char)(start_sector >> 24);
    cmd[3]   = (unsigned char)((start_sector >> 16) & 0xff);
    cmd[4]   = (unsigned char)((start_sector >> 8) & 0xff);
    cmd[5]   = (unsigned char)(start_sector & 0xff);
    cmd[7]   = (unsigned char)((num_sectors >> 8) & 0xff);
    cmd[8]   = (unsigned char)(num_sectors & 0xff);
    cmd[9]   = 0xF8;        // Vendor specific byte

    if (FMOD_CDDA_IssueScsiCmd(device, SRB_DIR_IN, cmd, sizeof(cmd), buf, num_sectors * SIZEOF_CDDA_SECTOR) != SS_COMP)
    {
        return FMOD_ERR_CDDA_READ;
    }

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: 6f147c4f auto
// OFFSET: 0x00917020
static unsigned int FMOD_OS_CDDA_hmsf_to_lba(unsigned char hours, unsigned char minutes, unsigned char seconds, unsigned char frames)
{
    return (hours * 1053696) + (minutes * 60 * 75) + (seconds * 75) + frames;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: 33020b77 auto
// OFFSET: 0x00917050
void FMOD_CDDA_AddUserTocEntry(FMOD_CDDA_DEVICE *device, unsigned int sector)
{
    device->usertoc.min[device->usertoc.numtracks]   = sector / (60 * 75);
    device->usertoc.sec[device->usertoc.numtracks]   = ((sector % (60 * 75)) / 75);
    device->usertoc.frame[device->usertoc.numtracks] = sector % 75;
    device->usertoc.numtracks++;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: 26cc7346 manual
// OFFSET: 0x00917B50
FMOD_RESULT FMOD_OS_CDDA_ReadTocRaw(FMOD_CDDA_DEVICE *device, FMOD_CDDA_TOC *toc)
{
    char          buf[SIZEOF_CDROM_SECTOR];
    int           num_tracks, datasize, first_track, last_track, session, i;
    unsigned int  leadout_start, leadout_start_orig, first_session_leadout, max_leadout, last_start;
    unsigned char cmd[10];
    FMOD_CDDA_RawTocDesc  *t = (FMOD_CDDA_RawTocDesc *)buf;
    FMOD_CDDA_RawTocEntry *toc_entry;

    if (!device || !toc)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    FMOD_memset(buf, 0, sizeof(buf));
    FMOD_memset(toc, 0, sizeof(FMOD_CDDA_TOC));
    if (toc == &device->toc)
    {
        FMOD_memset(&device->usertoc, 0, sizeof(FMOD_CDTOC));
    }

    cmd[0] = SCSI_READ_TOC;                 // Operation code
    cmd[1] = 0;                             // TIME | Reserved
    cmd[2] = 2;                             // Format | Reserved
    cmd[3] = 0;                             // Reserved
    cmd[4] = 0;                             // Reserved
    cmd[5] = 0;                             // Reserved
    cmd[6] = 1;                             // Track/session number
    cmd[7] = SIZEOF_CDROM_SECTOR >> 8;      // Allocation length msb
    cmd[8] = SIZEOF_CDROM_SECTOR & 0xff;    // Allocation length lsb
    cmd[9] = 0;                             // Control

    if (FMOD_CDDA_IssueScsiCmd(device, SRB_DIR_IN, cmd, sizeof(cmd), buf, SIZEOF_CDROM_SECTOR) != SS_COMP)
    {
        FLOG((FMOD_DEBUG_USER_ANDREW, __FILE__, __LINE__, "FMOD_OS_CDDA_ReadTocRaw", "SCSI cmd failed\n"));
        return FMOD_ERR_CDDA_NODISC;
    }

    datasize = (t->datalen_hi << 8) | (t->datalen_lo - 2);
    if ((datasize < (4 * sizeof(FMOD_CDDA_RawTocEntry))) || ((datasize % sizeof(FMOD_CDDA_RawTocEntry)) != 0))
    {
        FLOG((FMOD_DEBUG_USER_ANDREW, __FILE__, __LINE__, "FMOD_OS_CDDA_ReadTocRaw", "Wrong amount of data returned\n"));
        return FMOD_ERR_CDDA_READ;
    }

    num_tracks = datasize / sizeof(FMOD_CDDA_RawTocEntry);
    first_track = last_track = session = 0;
    leadout_start = leadout_start_orig = first_session_leadout = max_leadout = last_start = 0;

    if (num_tracks > 0)
    {
        toc->num_tracks = 0;

        for (i=0;i < num_tracks;i++)
        {
            toc_entry = &t->toc_entry[i];

            if (toc_entry->tno != 0)
            {
                FLOG((FMOD_DEBUG_USER_ANDREW, __FILE__, __LINE__, "FMOD_OS_CDDA_ReadTocRaw", "entry %d  toc_entry->tno = %d\n", i, toc_entry->tno));
            }

            switch (toc_entry->point)
            {
                // First track number
                case 0xA0 :
                    if ((session + 1) == toc_entry->session)
                    {
                        session = toc_entry->session;
                    }
                    else
                    {
                        FLOG((FMOD_DEBUG_USER_ANDREW, __FILE__, __LINE__, "FMOD_OS_CDDA_ReadTocRaw", "entry %d  session = %d  should = %d\n", i, toc_entry->session, session + 1));
                    }

                    if (toc_entry->adr != 1)
                    {
                        FLOG((FMOD_DEBUG_USER_ANDREW, __FILE__, __LINE__, "FMOD_OS_CDDA_ReadTocRaw", "entry %d  adr = %d  should = 1\n", i, toc_entry->adr));
                    }

                    if ((first_track < toc_entry->pmin) && (last_track < toc_entry->pmin))
                    {
                        first_track = toc_entry->pmin;
                    }
                    else
                    {
                        FLOG((FMOD_DEBUG_USER_ANDREW, __FILE__, __LINE__, "FMOD_OS_CDDA_ReadTocRaw", "entry %d  first track >= %d or %d\n", i, toc_entry->pmin, first_track, last_track));
                    }
                    break;

                // Last track number
                case 0xA1 :
                    if (session != toc_entry->session)
                    {
                        FLOG((FMOD_DEBUG_USER_ANDREW, __FILE__, __LINE__, "FMOD_OS_CDDA_ReadTocRaw", "entry %d  session = %d  should = %d\n", i, toc_entry->session, session));
                    }

                    if (toc_entry->adr != 1)
                    {
                        FLOG((FMOD_DEBUG_USER_ANDREW, __FILE__, __LINE__, "FMOD_OS_CDDA_ReadTocRaw", "entry %d  adr = %d  should = 1\n", i, toc_entry->adr));
                    }

                    if ((first_track <= toc_entry->pmin) && (last_track < toc_entry->pmin))
                    {
                        last_track = toc_entry->pmin;
                    }
                    else
                    {
                        FLOG((FMOD_DEBUG_USER_ANDREW, __FILE__, __LINE__, "FMOD_OS_CDDA_ReadTocRaw", "entry %d  last track >= %d or %d\n", i, toc_entry->pmin, first_track, last_track));
                    }
                    break;

                // Start location of leadout area
                case 0xA2 :
                    if (session != toc_entry->session)
                    {
                        FLOG((FMOD_DEBUG_USER_ANDREW, __FILE__, __LINE__, "FMOD_OS_CDDA_ReadTocRaw", "entry %d  session = %d  should = %d\n", i, toc_entry->session, session));
                    }

                    if (toc_entry->adr != 1)
                    {
                        FLOG((FMOD_DEBUG_USER_ANDREW, __FILE__, __LINE__, "FMOD_OS_CDDA_ReadTocRaw", "entry %d  adr = %d  should = 1\n", i, toc_entry->adr));
                    }

                    {
                        unsigned int tmp = FMOD_OS_CDDA_hmsf_to_lba(toc_entry->zero, toc_entry->pmin, toc_entry->psec, toc_entry->pframe);

                        if (first_session_leadout == 0)
                        {
                            first_session_leadout = tmp - 150;
                        }

                        if (tmp > leadout_start)
                        {
                            leadout_start_orig = tmp;
                            leadout_start = tmp;
                        }
                        else
                        {
                            FLOG((FMOD_DEBUG_USER_ANDREW, __FILE__, __LINE__, "FMOD_OS_CDDA_ReadTocRaw", "entry %d  leadout start = %d <= %d\n", i, tmp, leadout_start));
                        }
                    }
                    break;

                // Start time of next possible program area. Used to identify multi-session discs
                case 0xB0 :
                    if (session != toc_entry->session)
                    {
                        FLOG((FMOD_DEBUG_USER_ANDREW, __FILE__, __LINE__, "FMOD_OS_CDDA_ReadTocRaw", "entry %d  session = %d  should = %d\n", i, toc_entry->session, session));
                    }

                    if (toc_entry->adr != 5)
                    {
                        FLOG((FMOD_DEBUG_USER_ANDREW, __FILE__, __LINE__, "FMOD_OS_CDDA_ReadTocRaw", "entry %d  adr = %d  should = 5\n", i, toc_entry->adr));
                    }

                    if (FMOD_OS_CDDA_hmsf_to_lba(0, toc_entry->min, toc_entry->sec, toc_entry->frame) < (leadout_start + 6750))
                    {
                        FLOG((FMOD_DEBUG_USER_ANDREW, __FILE__, __LINE__, "FMOD_OS_CDDA_ReadTocRaw", "entry %d  next program area = %u < leadout_start + 6750 = %u\n", i, FMOD_OS_CDDA_hmsf_to_lba(0, toc_entry->min, toc_entry->sec, toc_entry->frame), leadout_start + 6750));
                    }

                    if ((max_leadout != 0) && (FMOD_OS_CDDA_hmsf_to_lba(toc_entry->zero, toc_entry->pmin, toc_entry->psec, toc_entry->pframe) != max_leadout))
                    {
                        FLOG((FMOD_DEBUG_USER_ANDREW, __FILE__, __LINE__, "FMOD_OS_CDDA_ReadTocRaw", "entry %d  max leadout = %u != last max leadout = %u\n", i, FMOD_OS_CDDA_hmsf_to_lba(toc_entry->zero, toc_entry->pmin, toc_entry->psec, toc_entry->pframe), max_leadout));
                    }

                    if (max_leadout == 0)
                    {
                        max_leadout = FMOD_OS_CDDA_hmsf_to_lba(toc_entry->zero, toc_entry->pmin, toc_entry->psec, toc_entry->pframe);
                    }
                    break;

                // Not interested in these
                case 0xB1 :
                case 0xB2 :
                case 0xB3 :
                case 0xB4 :
                case 0xB5 :
                case 0xB6 :
                case 0xC0 :
                case 0xC1 :
                    break;

                // Actual track info
                default :
                    if (session != toc_entry->session)
                    {
                        FLOG((FMOD_DEBUG_USER_ANDREW, __FILE__, __LINE__, "FMOD_OS_CDDA_ReadTocRaw", "entry %d  session = %d  should = %d\n", i, toc_entry->session, session));
                    }

                    if ((toc_entry->point < first_track) || (toc_entry->point > last_track))
                    {
                        FLOG((FMOD_DEBUG_USER_ANDREW, __FILE__, __LINE__, "FMOD_OS_CDDA_ReadTocRaw", "entry %d  track number %d should be between %d and %d\n", i, toc_entry->point, first_track, last_track));
                    }
                    else
                    {
                        unsigned int track_start = FMOD_OS_CDDA_hmsf_to_lba(toc_entry->zero, toc_entry->pmin, toc_entry->psec, toc_entry->pframe);

                        if (leadout_start < track_start)
                        {
                            leadout_start = track_start + 1;
                        }

                        if ((track_start < last_start) || (track_start >= leadout_start))
                        {
                            FLOG((FMOD_DEBUG_USER_ANDREW, __FILE__, __LINE__, "FMOD_OS_CDDA_ReadTocRaw", "entry %d  track start = %u < last_start (%u) or > leadout_start (%u)\n", i, track_start, last_start, leadout_start));
                        }
                        else
                        {
                            last_start = track_start;

                            *(unsigned char *)(&toc->flags[toc->num_tracks]) = 0;
                            toc->track_number[toc->num_tracks] = toc_entry->point;
                            toc->start_sector[toc->num_tracks] = FMOD_OS_CDDA_hmsf_to_lba(toc_entry->zero, toc_entry->pmin, toc_entry->psec, toc_entry->pframe);
                            toc->start_sector[toc->num_tracks] -= 150;

                            if ((toc_entry->point != first_track) && (toc->num_sectors[toc->num_tracks - 1] == 0))
                            {
                                toc->num_sectors[toc->num_tracks - 1] = toc->start_sector[toc->num_tracks] - toc->start_sector[toc->num_tracks - 1];
                            }

                            if (toc_entry->point == last_track)
                            {
                                /*
                                    06/09/05 AJS - 150 because last audio track was always 2 seconds out
                                */
                                toc->num_sectors[toc->num_tracks] = leadout_start - toc->start_sector[toc->num_tracks] - 150;
                            }

                            if (toc == &device->toc)
                            {
                                FMOD_CDDA_AddUserTocEntry(device, toc->start_sector[toc->num_tracks] + (75 * 2));
                            }

                            if (!toc_entry->data_track || (toc != &device->toc))
                            {
                                toc->num_tracks++;
                            }
                        }
                    }

                    break;
            }
        }

        *(unsigned char *)(&toc->flags[toc->num_tracks]) = 0;
        toc->track_number[toc->num_tracks] = 0xAA;
        toc->start_sector[toc->num_tracks] = leadout_start_orig;
        toc->num_tracks++;

        if (toc == &device->toc)
        {
            FMOD_CDDA_AddUserTocEntry(device, leadout_start_orig);
        }
    }

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: 4e81b315 manual
// OFFSET: 0x00917EF0
FMOD_RESULT FMOD_OS_CDDA_ReadToc(FMOD_CDDA_DEVICE *device, FMOD_CDDA_TOC *toc)
{
    int           num_tracks, i;
    unsigned char cmd[10];
    unsigned char buf[SIZEOF_CDROM_SECTOR];

    if (!device || !toc)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    FMOD_memset(buf, 0, sizeof(buf));
    FMOD_memset(toc, 0, sizeof(FMOD_CDDA_TOC));
    if (toc == &device->toc)
    {
        FMOD_memset(&device->usertoc, 0, sizeof(FMOD_CDTOC));
    }

    cmd[0] = SCSI_READ_TOC;                 // Operation code
    cmd[1] = 0;                             // Logical unit number
    cmd[2] = 0;                             // Reserved
    cmd[3] = 0;                             // Reserved
    cmd[4] = 0;                             // Reserved
    cmd[5] = 0;                             // Reserved
    cmd[6] = 1;                             // Starting track
    cmd[7] = SIZEOF_CDROM_SECTOR >> 8;      // Allocation length msb
    cmd[8] = SIZEOF_CDROM_SECTOR & 0xff;    // Allocation length lsb
    cmd[9] = 0;                             // Control

    if (FMOD_CDDA_IssueScsiCmd(device, SRB_DIR_IN, cmd, sizeof(cmd), (char *)buf, SIZEOF_CDROM_SECTOR) != SS_COMP)
    {
        return FMOD_ERR_CDDA_NODISC;
    }

    num_tracks = (((unsigned char)buf[0] << 8) | ((unsigned char)buf[1]) - 2) / 8;
    if (num_tracks > 0)
    {
        unsigned char *toc_stream = buf + 4;
        unsigned char *p ;

        toc->num_tracks = 0;

        for (i=0;i < num_tracks;i++)
        {
            p = toc_stream + (i * 8);
            *(unsigned char *)(&toc->flags[toc->num_tracks]) = p[1];
            toc->track_number[toc->num_tracks] = p[2];
            FMOD_memcpy(&toc->start_sector[toc->num_tracks], &p[4], sizeof(unsigned int));
            toc->start_sector[toc->num_tracks] = FMOD_SWAPENDIAN_DWORD(toc->start_sector[toc->num_tracks]);

            if (toc->num_tracks && (toc->num_sectors[toc->num_tracks - 1] == 0))
            {
                /*
                    Note: We can never get the correct length of the last audio track in a session if there are 
                          more sessions following. The "start_sector[thistrack] - start_sector[lasttrack]" doesn't
                          take into account the big gap between sessions.
                */

                toc->num_sectors[toc->num_tracks - 1] = toc->start_sector[toc->num_tracks] - toc->start_sector[toc->num_tracks - 1];

                if (toc->flags[toc->num_tracks].data_track)
                {
//                    toc->num_sectors[toc->num_tracks - 1] -= 6750;
                    /*
                        This'll adjust for a session leadout and new session info - it's not correct but it works
                    */
                    toc->num_sectors[toc->num_tracks - 1] -= 11250;
                }
            }

            /*
                Only add user TOC entries if we're reading into the cdrom's TOC
            */
            if (toc == &device->toc)
            {
                FMOD_CDDA_AddUserTocEntry(device, toc->start_sector[toc->num_tracks] + (75 * 2));
            }
            else
            {
                /*
                    If this is the leadout track and we're not reading to the user TOC then adjust the pos
                    (this is only for the ASPI drive mapping stuff, so it lines up with MCI)
                */
                if (toc->track_number[toc->num_tracks] == 0xAA)
                {
                    toc->start_sector[toc->num_tracks] += 150;
                }
            }

            if (!toc->flags[toc->num_tracks].data_track || (toc->track_number[toc->num_tracks] == 0xAA) || (toc != &device->toc))
            {
                toc->num_tracks++;
            }
        }
    }

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: 105365d0 auto
// OFFSET: 0x009170C0
bool FMOD_CDDA_ScsiAbort(SRB_ExecSCSICmd *srb_to_abort, unsigned char adapter_id)
{
    SRB_Abort srb;
    DWORD status = 0;

    FMOD_memset(&srb, 0, sizeof(srb));
    srb.SRB_Cmd     = SC_ABORT_SRB;
    srb.SRB_HaId    = adapter_id;
    srb.SRB_Flags   = 0;
    srb.SRB_ToAbort = (SRB *)&srb_to_abort;

    status = FMOD_CDDA_SendASPI32Command((SRB *)&srb);

    if (srb.SRB_Status != SS_COMP)
    {
        return false;
    }

    return true;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: 2759896f manual
// OFFSET: 0x00917100
int FMOD_CDDA_IssueScsiCmd_Internal(FMOD_CDDA_DEVICE *cdrom, int flags, unsigned char *cdb_data, int cdb_len, char *buf, int buflen, unsigned char adapter_id, unsigned char target_id, unsigned char lun_id)
{
    SRB_ExecSCSICmd srb;
    DWORD aspi_status;
    DWORD event_status = 0;
    HANDLE event = 0;

    if ((event = CreateEvent(0, TRUE, FALSE, 0)) == 0)
    {
        return SS_ABORTED;
    }

    FMOD_memset(&srb, 0, sizeof(srb));
    srb.SRB_Cmd        = SC_EXEC_SCSI_CMD;
    srb.SRB_HaId       = adapter_id;
    srb.SRB_Flags      = flags | SRB_EVENT_NOTIFY;
    srb.SRB_Target     = target_id;
    srb.SRB_Lun        = lun_id;
    srb.SRB_SenseLen   = SENSE_LEN;
    srb.SRB_CDBLen     = cdb_len;
    srb.SRB_BufLen     = buflen;
    srb.SRB_BufPointer = (unsigned char *)buf;
    srb.SRB_PostProc   = (VOID FAR*)event;
    if (cdb_data != 0)
    {
        FMOD_memcpy(&srb.CDBByte, cdb_data, cdb_len);
    }

    ResetEvent(event);

#ifdef FMOD_OS_CDDA_DEBUG
    {
        unsigned int start_time;
        FMOD_OS_Time_GetMs(&start_time);
#endif

    /*
        We do this so it works from codec_cdda.dll
    */
    if (cdrom)
    {
        aspi_status = cdrom->SendASPI32Command((SRB *)&srb);
    }
    else
    {
        aspi_status = FMOD_CDDA_SendASPI32Command((SRB *)&srb);
    }

    /*
        Wait until we're signalled
    */
    if (aspi_status == SS_PENDING)
    {
        event_status = WaitForSingleObject(event, FMOD_CDDA_Aspi_Timeout);
        if (event_status == WAIT_TIMEOUT)
        {
            ResetEvent(event);
        }

        if (srb.SRB_Status == SS_PENDING)
        {
            /*
                Timed out
            */
            FMOD_CDDA_ScsiAbort(&srb, adapter_id);
            CloseHandle(event);
            return SS_ABORTED;
        }
    }

#ifdef FMOD_OS_CDDA_DEBUG
        {
            unsigned int elapsed;
            FMOD_OS_Time_GetMs(&elapsed);
            elapsed -= start_time;
            FMOD_OS_CDDA_DEBUGPRINT("%d\n", elapsed);
        }
    }
#endif

    if (cdrom)
    {
        cdrom->sense_key.SK   = srb.SenseArea[2] & 0xf;
        cdrom->sense_key.ASC  = srb.SenseArea[12];
        cdrom->sense_key.ASCQ = srb.SenseArea[13];
    }

    /*
        Check ASPI command status
    */
    if (srb.SRB_Status != SS_COMP)
    {
        FLOG((FMOD_DEBUG_USER_ANDREW, __FILE__, __LINE__, "FMOD_CDDA_IssueScsiCmd_Internal", "failed : status = %02x  SK = %02x  ASC = %02x  ASCQ = %02x\n",
                            srb.SRB_Status, cdrom->sense_key.SK, cdrom->sense_key.ASC, cdrom->sense_key.ASCQ));
    }

    CloseHandle(event);

    return srb.SRB_Status;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
int FMOD_CDDA_IssueScsiCmd(FMOD_CDDA_DEVICE *cdrom, int flags, unsigned char *cdb_data, int cdb_len, char *buf, int buflen)
{
    if (!cdrom)
    {
        return SS_ABORTED;
    }

    return FMOD_CDDA_IssueScsiCmd_Internal(cdrom, flags, cdb_data, cdb_len, buf, buflen, cdrom->adapter_id, cdrom->target_id, cdrom->lun_id);
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: 810c6383 manual
// OFFSET: 0x009180F0
FMOD_RESULT FMOD_CDDA_Close(FMOD_CDDA_DEVICE *cdrom)
{
    if (!cdrom)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (cdrom->device_open)
    {
        FMOD_CDDA_PreventMediaRemoval(cdrom, false);
    }

    SAFE_FREE(cdrom->caps);
    SAFE_FREE(cdrom->name);
    SAFE_FREE(cdrom->scsiaddr);
    SAFE_FREE(cdrom->devicename);

    cdrom->device_open = 0;

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: 2066887f manual
// OFFSET: 0x009181F0
bool FMOD_CDDA_InquiryCommand(unsigned char adapter_id, unsigned char target_id, unsigned char lun_id)
{
    int                   letter;
    unsigned char         cmd[6];
    FMOD_CDDA_InquiryData inq_data;

    FMOD_memset(&inq_data, 0, sizeof(inq_data));

    cmd[0] = SCSI_INQUIRY;          // Operation code
    cmd[1] = lun_id << 5;           // Logical unit number
    cmd[2] = 0;                     // Page code
    cmd[3] = 0;                     // Reserved
    cmd[4] = sizeof(inq_data);      // Allocation length
    cmd[5] = 0;                     // Control

    if (FMOD_CDDA_IssueScsiCmd_Internal(0, SRB_DIR_IN, cmd, sizeof(cmd), (char *)&inq_data, sizeof(inq_data), adapter_id, target_id, lun_id) != SS_COMP)
    {
        FMOD_OS_CDDA_DEBUGPRINT("FMOD_CDDA_InquiryCommand: SCSI cmd failed\n");
        return false;
    }

    if (USING_NTSCSI)
    {
        letter = FMOD_CDDA_NTSCSI_GetDeviceIndex(adapter_id, target_id, lun_id);
        if (!letter)
        {
            FMOD_OS_CDDA_DEBUGPRINT("FMOD_CDDA_InquiryCommand: Invalid drive letter\n");
            FLOG((FMOD_DEBUG_USER_ANDREW, __FILE__, __LINE__, "FMOD_CDDA_InquiryCommand", "Invalid drive letter\n"));
            return false;
        }
    }
    else
    {
        /*
            Defer drive letter mapping until the user tries to access a drive
        */
        letter = 0;
    }

    if (FMOD_CDDA_NumDevices < FMOD_CDDA_MAX_DEVICES)
    {
        FMOD_CDDA_DEVICE *cdrom;
        char tmp[256];

        cdrom = FMOD_CDDA_DeviceList[FMOD_CDDA_NumDevices] = (FMOD_CDDA_DEVICE *)FMOD_Memory_Calloc(sizeof(FMOD_CDDA_DEVICE));
        if (!FMOD_CDDA_DeviceList[FMOD_CDDA_NumDevices])
        {
            return false;
        }

        cdrom->letter         = (USING_NTSCSI) ? FMOD_CDDA_NTSCSI_GetDeviceIndex(adapter_id, target_id, lun_id) : 0;
	    cdrom->target_id      = target_id;
	    cdrom->adapter_id     = adapter_id;
	    cdrom->lun_id         = lun_id;
        cdrom->device_open    = 0;
	    cdrom->speed          = 32;
	    cdrom->toc.num_tracks = 0;
        cdrom->caps           = 0;

        FMOD_memset(&cdrom->sense_key, 0, sizeof(cdrom->sense_key));
	    FMOD_memset(&cdrom->toc, 0, sizeof(cdrom->toc));
	    FMOD_memset(&cdrom->usertoc, 0, sizeof(cdrom->usertoc));
        FMOD_memcpy(&cdrom->inq_data, &inq_data, sizeof(inq_data));

        if (cdrom->letter)
        {
            sprintf(tmp, "%c:", cdrom->letter + 'A');
            cdrom->name = FMOD_strdup(tmp);
            if (!cdrom->name)
            {
                return false;
            }
        }

        sprintf(tmp, "%02x:%02x:%02x", cdrom->target_id, cdrom->adapter_id, cdrom->lun_id);
        cdrom->scsiaddr = FMOD_strdup(tmp);
        if (!cdrom->scsiaddr)
        {
            return false;
        }

        tmp[0] = 0;
        FMOD_strncat(tmp, cdrom->inq_data.vendor_id, 8);
        FMOD_strcat(tmp, " ");
        FMOD_strncat(tmp, cdrom->inq_data.product_id, 16);
        FMOD_strcat(tmp, " ");
        FMOD_strncat(tmp, cdrom->inq_data.product_revision, 4);
        cdrom->devicename = FMOD_strdup(tmp);
        if (!cdrom->devicename)
        {
            return false;
        }

        cdrom->SendASPI32Command = FMOD_CDDA_SendASPI32Command;
        FMOD_CDDA_NumDevices++;
        return true;
    }
    else
    {
        FMOD_OS_CDDA_DEBUGPRINT("FMOD_CDDA_InquiryCommand: too many cdrom devices!\n");
        return false;
    }
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: be3f3d0c manual
// OFFSET: 0x00917220
bool FMOD_OS_CDDA_TestUnitReady(FMOD_CDDA_DEVICE *device)
{
    unsigned char cmd[6];

    if (!device)
    {
        return false;
    }

    cmd[0] = SCSI_TST_U_RDY;                 // Operation code
    cmd[1] = device->lun_id << 5;            // Logical unit number
    cmd[2] = 0;                              // Reserved
    cmd[3] = 0;                              // Reserved
    cmd[4] = 0;                              // Reserved
    cmd[5] = 0;                              // Control

    if (FMOD_CDDA_IssueScsiCmd(device, SRB_DIR_IN, cmd, sizeof(cmd), 0, 0) != SS_COMP)
    {
        return false;
    }

    return true;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
bool FMOD_CDDA_PreventMediaRemoval(FMOD_CDDA_DEVICE *cdrom, bool prevent)
{
    unsigned char cmd[6];

    if (!cdrom)
    {
        return false;
    }

    cmd[0] = SCSI_PREVENT_ALLOW_MEDIUM_REMOVAL;     // Operation code
    cmd[1] = cdrom->lun_id << 5;                    // Logical unit number
    cmd[2] = 0;                                     // Reserved
    cmd[3] = 0;                                     // Reserved
    cmd[4] = prevent ? 1 : 0;                       // Prevent
    cmd[5] = 0;                                     // Control

    if (FMOD_CDDA_IssueScsiCmd(cdrom, SRB_DIR_OUT, cmd, sizeof(cmd), 0, 0) != SS_COMP)
    {
        return false;
    }

    return true;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: 88b06765 manual
// OFFSET: 0x00917280
FMOD_RESULT FMOD_OS_CDDA_SetSpeed(FMOD_CDDA_DEVICE *cdrom, int speed)
{
    int cmd_size = 10;
    unsigned char speed_hi, speed_lo;
    unsigned char cmd[12];

    if (!cdrom)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (speed == -1)
    {
        speed = cdrom->speed;
        if (speed <= 0)
        {
            return FMOD_ERR_INVALID_PARAM;
        }
    }

    FMOD_memset(cmd, 0, sizeof(cmd));

    speed = (1764 * speed) / 10;
    speed_hi = (unsigned char)((speed >> 8) & 0xFF);
    speed_lo = (unsigned char)(speed & 0xFF);

    cmd[0] = SCSI_SET_SPEED;        // Operation Code
    cmd[1] = cdrom->lun_id << 5;    // Lun
    cmd[2] = speed_hi;              // High word of speed
    cmd[3] = speed_lo;              // Low word of speed
    cmd[4] = 0xFF;                  // High word of speed
    cmd[5] = 0xFF;                  // Low word of speed
    cmd_size = 12;

    if (FMOD_CDDA_IssueScsiCmd(cdrom, SRB_DIR_OUT, cmd, cmd_size, 0, 0) != SS_COMP)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: 88ba97c5 manual
// OFFSET: 0x00917320
int FMOD_CDDA_NTSCSI_GetNumAdapters()
{
    int buf[256], i, num_adapters = 0;

    FMOD_memset(buf, 0, sizeof(buf));
    buf[0] = 1;

    for (i=0;i < 26;i++)
    {
        if (FMOD_CDDA_NTSCSI_Drive[i].used)
        {
            buf[FMOD_CDDA_NTSCSI_Drive[i].ha] = 1;
        }
    }

    for (i=0;i <= 255;i++)
    {
        if (buf[i])
        {
            FMOD_CDDA_NTSCSI_AdapterLookup[num_adapters] = i;
            num_adapters++;
        }
    }

    return num_adapters;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: 2aaead6e manual
// OFFSET: 0x009173B0
HANDLE FMOD_CDDA_NTSCSI_GetFileHandle(int i)
{
    char            buf[12];
    DWORD           dwFlags;
    HANDLE          handle = 0;

    /*
        NT 4.0 wants just the GENERIC_READ flag, and Win2K wants both GENERIC_READ and GENERIC_WRITE. Try both...
    */
    /*
        If Win2K or greater, add GENERIC_WRITE
    */
    dwFlags = GENERIC_READ;

    if (IsWindowsVersionOrGreater(5, 0, 0))
    {
        dwFlags |= GENERIC_WRITE;
    }

    sprintf(buf, "\\\\.\\%c:", (char)('A' + i));
    handle = CreateFile(buf, dwFlags, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (handle == INVALID_HANDLE_VALUE)
    {
        /*
            Didn't work - try again with the GENERIC_WRITE bit flipped
        */
        dwFlags ^= GENERIC_WRITE;
        handle = CreateFile(buf, dwFlags, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    }

    if (handle == INVALID_HANDLE_VALUE)
    {
        FMOD_OS_CDDA_DEBUGPRINT("FMOD_CDDA_NTSCSI_GetFileHandle: i == %d failed\n", i);
    }

    return handle;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: 6d42a080 manual
// OFFSET: 0x00917460
bool FMOD_CDDA_NTSCSI_GetDriveInformation(int i, FMOD_CDDA_NTSCSIDRIVE *drive)
{
    SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER spt_dwb;
    SCSI_ADDRESS  scsi_addr;
    HANDLE        handle;
    BOOL          status;
    DWORD         length, returned;
    bool          result = true;
    unsigned char inq_data[192];

    if (!drive)
    {
        return false;
    }

    drive->used = false;

    handle = FMOD_CDDA_NTSCSI_GetFileHandle(i);
    if (handle == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    FMOD_memset(inq_data, 0, 192);
    FMOD_memset(&spt_dwb, 0, sizeof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER));

    spt_dwb.spt.Length             = sizeof(SCSI_PASS_THROUGH);
    spt_dwb.spt.CdbLength          = 6;
    spt_dwb.spt.SenseInfoLength    = 24;
    spt_dwb.spt.DataIn             = SCSI_IOCTL_DATA_IN;
    spt_dwb.spt.DataTransferLength = 192;
    spt_dwb.spt.TimeOutValue       = 2;
    spt_dwb.spt.DataBuffer         = inq_data;
    spt_dwb.spt.SenseInfoOffset    = offsetof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, ucSenseBuf);
    spt_dwb.spt.Cdb[0]             = SCSI_INQUIRY;
    spt_dwb.spt.Cdb[4]             = 192;

    length = sizeof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER);

    status = DeviceIoControl(handle, IOCTL_SCSI_PASS_THROUGH_DIRECT, &spt_dwb, length, &spt_dwb, length, &returned, 0);
    if (!status)
    {
        FMOD_OS_CDDA_DEBUGPRINT("FMOD_CDDA_NTSCSI_GetDriveInformation: SCSI_INQUIRY failed\n");
        CloseHandle(handle);
        return false;
    }

    FMOD_memcpy((unsigned char *)&drive->inq_data, inq_data, 36);

    /*
        Get the address (path/tgt/lun) of the drive via IOCTL_SCSI_GET_ADDRESS
    */
    FMOD_memset(&scsi_addr, 0, sizeof(SCSI_ADDRESS));
    scsi_addr.Length = sizeof(SCSI_ADDRESS);
    if (DeviceIoControl(handle, IOCTL_SCSI_GET_ADDRESS, 0, 0, &scsi_addr, sizeof(SCSI_ADDRESS), &returned, 0))
    {
        drive->used   = true;
        drive->ha     = scsi_addr.PortNumber;
        drive->tgt    = scsi_addr.TargetId;
        drive->lun    = scsi_addr.Lun;
        drive->letter = i;
        FMOD_OS_CDDA_DEBUGPRINT("  portnumber = %02d\n  pathid     = %02d\n  targetid   = %02d\n  lun        = %02d\n", scsi_addr.PortNumber, scsi_addr.PathId, scsi_addr.TargetId, scsi_addr.Lun);
        FMOD_OS_CDDA_DEBUGPRINT("  %c: = %02d:%02d:%02d\n", drive->letter + 'A', drive->ha, drive->tgt, drive->lun);
    }
    else
    {
        /*
            Some USB/Firewire etc. devices don't support IOCTL_SCSI_GET_ADDRESS but we can just fake a
            SCSI address without hurting anyone.
        */
/*
        result = false;
        FMOD_OS_CDDA_DEBUGPRINT("FMOD_CDDA_NTSCSI_GetDriveInformation: IOCTL_SCSI_GET_ADDRESS failed on %c:\n", i + 'a');
*/
        drive->used   = true;
        drive->ha     = 10;
        drive->tgt    = 10;
        drive->lun    = FMOD_CDDA_FakeLun++;
        drive->letter = i;
        FMOD_OS_CDDA_DEBUGPRINT("  portnumber = %02d\n  pathid     = %02d\n  targetid   = %02d\n  lun        = %02d\n", drive->ha, scsi_addr.PathId, drive->tgt, drive->lun);
        FMOD_OS_CDDA_DEBUGPRINT("  %c: = %02d:%02d:%02d\n", drive->letter + 'A', drive->ha, drive->tgt, drive->lun);
    }

    drive->device = INVALID_HANDLE_VALUE;
    CloseHandle(handle);

    return result;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: 28d5b0f1 auto
// OFFSET: 0x009175C0
FMOD_RESULT FMOD_CDDA_NTSCSI_Init()
{
    char buf[4];
    int  i, num_drives = 0;

    if (FMOD_CDDA_NTSCSI_Initialised)
    {
        return FMOD_OK;
    }

    FMOD_CDDA_NTSCSI_Drive = (FMOD_CDDA_NTSCSIDRIVE *)FMOD_Memory_Calloc(sizeof(FMOD_CDDA_NTSCSIDRIVE) * 26);
    FMOD_CDDA_NTSCSI_NumAdapters = 0;
    FMOD_CDDA_FakeLun = 0;

    for (i=0;i < 26;i++)
    {
        FMOD_CDDA_NTSCSI_Drive[i].device = INVALID_HANDLE_VALUE;
    }

    FMOD_OS_CDDA_DEBUGPRINT("Finding devices:\n");
    for (i=2;i < 26;i++)
    {
        UINT type;

        sprintf(buf, "%c:\\", (char)('A' + i));
        type = GetDriveType(buf);

#ifdef FMOD_OS_CDDA_DEBUG
        switch (type)
        {
            case DRIVE_UNKNOWN :
                FMOD_OS_CDDA_DEBUGPRINT("%s = DRIVE_UNKNOWN\n", buf);
                break;
            case DRIVE_NO_ROOT_DIR :
                break;
            case DRIVE_REMOVABLE :
                FMOD_OS_CDDA_DEBUGPRINT("%s = DRIVE_REMOVABLE\n", buf);
                break;
            case DRIVE_FIXED :
                FMOD_OS_CDDA_DEBUGPRINT("%s = DRIVE_FIXED\n", buf);
                break;
            case DRIVE_REMOTE :
                FMOD_OS_CDDA_DEBUGPRINT("%s = DRIVE_REMOTE\n", buf);
                break;
            case DRIVE_CDROM :
                FMOD_OS_CDDA_DEBUGPRINT("%s = DRIVE_CDROM\n", buf);
                break;
            case DRIVE_RAMDISK :
                FMOD_OS_CDDA_DEBUGPRINT("%s = DRIVE_RAMDISK\n", buf);
                break;
            default :
                FMOD_OS_CDDA_DEBUGPRINT("%s = ???\n", buf);
                break;
        }
#endif

        if (type == DRIVE_CDROM)
        {
            FMOD_CDDA_NTSCSI_GetDriveInformation(i, &FMOD_CDDA_NTSCSI_Drive[i]);
            if (FMOD_CDDA_NTSCSI_Drive[i].used)
            {
                num_drives++;
            }
        }
    }

    FMOD_CDDA_NTSCSI_NumAdapters = FMOD_CDDA_NTSCSI_GetNumAdapters();
    FMOD_CDDA_NTSCSI_Initialised = true;

    return num_drives ? FMOD_OK : FMOD_ERR_CDDA_NODEVICES;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: 2b091936 auto
// OFFSET: 0x009176B0
bool FMOD_CDDA_NTSCSI_Shutdown()
{
    int i;

    if (!FMOD_CDDA_NTSCSI_Initialised)
    {
        return false;
    }

    for (i=2;i < 26;i++)
    {
        if (FMOD_CDDA_NTSCSI_Drive[i].used)
        {
            CloseHandle(FMOD_CDDA_NTSCSI_Drive[i].device);
        }
    }

    SAFE_FREE(FMOD_CDDA_NTSCSI_Drive);

    FMOD_CDDA_NTSCSI_Initialised = false;

    return true;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: 91485c5a auto
// OFFSET: 0x00917730
int FMOD_CDDA_NTSCSI_GetDeviceIndex(unsigned char ha, unsigned char tgt, unsigned char lun)
{
    int i;

    for (i=2;i < 26;i++)
    {
        if (FMOD_CDDA_NTSCSI_Drive[i].used)
        {
            if ((FMOD_CDDA_NTSCSI_Drive[i].ha == ha)   && 
                (FMOD_CDDA_NTSCSI_Drive[i].tgt == tgt) && 
                (FMOD_CDDA_NTSCSI_Drive[i].lun == lun))
            {
                return i;
            }
        }
    }

    return 0;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: 5ccc7713 auto
// OFFSET: 0x00917770
DWORD FMOD_CDDA_NTSCSI_HAInquiry(SRB_HAInquiry *srb)
{
    srb->HA_Count = FMOD_CDDA_NTSCSI_NumAdapters;

    if (srb->SRB_HaId >= FMOD_CDDA_NTSCSI_NumAdapters)
    {
        srb->SRB_Status = SS_INVALID_HA;
        return SS_INVALID_HA;
    }

    srb->HA_SCSI_ID = 7;
    FMOD_memcpy(srb->HA_ManagerId,  "FMODASPI v1.0   ", 16);
    FMOD_memcpy(srb->HA_Identifier, "SCSI Adapter    ", 16);
    srb->HA_Identifier[13] = (char)('0' + srb->SRB_HaId);
    FMOD_memset(srb->HA_Unique, 0, 16);
    srb->HA_Unique[3] = 8;

    *((DWORD *)&srb->HA_Unique[4]) = 64 * 1024;

    srb->SRB_Status = SS_COMP;

    return SS_COMP;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: fef4538d auto
// OFFSET: 0x00917810
DWORD FMOD_CDDA_NTSCSI_GetDeviceType(SRB_GDEVBlock *srb)
{
    srb->SRB_Status = SS_NO_DEVICE;

    if (FMOD_CDDA_NTSCSI_GetDeviceIndex(srb->SRB_HaId, srb->SRB_Target, srb->SRB_Lun))
    {
        srb->SRB_Status = SS_COMP;
    }

    if (srb->SRB_Status == SS_COMP)
    {
        srb->SRB_DeviceType = DTYPE_CDROM;
    }
    else
    {
        srb->SRB_DeviceType = DTYPE_UNKNOWN;
    }

    return srb->SRB_Status;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: ad952d6e auto
// OFFSET: 0x00917860
DWORD FMOD_CDDA_NTSCSI_ExecScsiCmd(SRB_ExecSCSICmd *srb, bool retrying)
{
    SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER spt_dwb;
    BOOL status;
    int index;
    ULONG length, returned = 0;

    index = FMOD_CDDA_NTSCSI_GetDeviceIndex(srb->SRB_HaId, srb->SRB_Target, srb->SRB_Lun);
    if (index == 0)
    {
        srb->SRB_Status = SS_ERR;
        return SS_ERR;
    }

    if (srb->CDBByte[0] == SCSI_INQUIRY)
    {
        srb->SRB_Status = SS_COMP;
        FMOD_memcpy(srb->SRB_BufPointer, (char *)&FMOD_CDDA_NTSCSI_Drive[index].inq_data, 36);
        return SS_COMP;
    }

    if (FMOD_CDDA_NTSCSI_Drive[index].device == INVALID_HANDLE_VALUE)
    {
        FMOD_CDDA_NTSCSI_Drive[index].device = FMOD_CDDA_NTSCSI_GetFileHandle(FMOD_CDDA_NTSCSI_Drive[index].letter);
    }

    FMOD_memset(&spt_dwb, 0, sizeof(spt_dwb));

    spt_dwb.spt.Length     = sizeof(SCSI_PASS_THROUGH);
    if (srb->SRB_Flags & SRB_DIR_IN)
    {
        spt_dwb.spt.DataIn = SCSI_IOCTL_DATA_IN;
    }
    else if (srb->SRB_Flags & SRB_DIR_OUT)
    {
        spt_dwb.spt.DataIn = SCSI_IOCTL_DATA_OUT;
    }
    else
    {
        spt_dwb.spt.DataIn = SCSI_IOCTL_DATA_UNSPECIFIED;
    }
    spt_dwb.spt.DataTransferLength = srb->SRB_BufLen;
    spt_dwb.spt.TimeOutValue       = 15;
    spt_dwb.spt.DataBuffer         = srb->SRB_BufPointer;
	spt_dwb.spt.SenseInfoLength    = srb->SRB_SenseLen;
    spt_dwb.spt.SenseInfoOffset    = offsetof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, ucSenseBuf);
    spt_dwb.spt.CdbLength          = srb->SRB_CDBLen;
    FMOD_memcpy(spt_dwb.spt.Cdb, srb->CDBByte, srb->SRB_CDBLen);

    length = sizeof(spt_dwb);

    status = DeviceIoControl(FMOD_CDDA_NTSCSI_Drive[index].device, IOCTL_SCSI_PASS_THROUGH_DIRECT,
                                &spt_dwb, length, &spt_dwb, length, &returned, 0);

    FMOD_memcpy(srb->SenseArea, spt_dwb.ucSenseBuf, srb->SRB_SenseLen);

    if (status)
    {
        srb->SRB_Status = SS_COMP;
        if (spt_dwb.spt.ScsiStatus)
        {
            srb->SRB_Status = SS_ERR;
        }
    }
    else
    {
        DWORD rc;

        srb->SRB_Status   = SS_ERR;
        srb->SRB_TargStat = 0x0004;

        rc = GetLastError();

        if (rc == ERROR_SEM_TIMEOUT)
        {
            /*
                DeviceIoControl timed out? Or maybe the device did?
            */
            FMOD_OS_CDDA_DEBUGPRINT("FMOD_CDDA_NTSCSI_ExecScsiCmd: ERROR_SEM_TIMEOUT\n");
        }
        else if (rc == ERROR_IO_DEVICE)
        {
            FMOD_OS_CDDA_DEBUGPRINT("FMOD_CDDA_NTSCSI_ExecScsiCmd: ERROR_IO_DEVICE\n");
        }
        else
        {
            if (!retrying && ((rc == ERROR_MEDIA_CHANGED) || (rc == ERROR_INVALID_HANDLE)))
            {
                if (rc != ERROR_INVALID_HANDLE)
                {
                    CloseHandle(FMOD_CDDA_NTSCSI_Drive[index].device);
                }

                FMOD_CDDA_NTSCSI_GetDriveInformation(index, &FMOD_CDDA_NTSCSI_Drive[index]);
                return FMOD_CDDA_NTSCSI_ExecScsiCmd(srb, true);
            }
        }
    }

    return srb->SRB_Status;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: 7ffb92eb auto
// OFFSET: 0x00917A20
DWORD FMOD_CDDA_NTSCSI_GetASPI32SupportInfo()
{
    DWORD rc;

    if (!FMOD_CDDA_NTSCSI_NumAdapters)
    {
        rc = (DWORD)(MAKEWORD(0, SS_NO_ADAPTERS));
    }
    else
    {
        rc = (DWORD)(MAKEWORD(FMOD_CDDA_NTSCSI_NumAdapters, SS_COMP));
    }

    return rc;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: 84c70a56 manual
// OFFSET: 0x00917A40
DWORD FMOD_CDDA_NTSCSI_SendASPI32Command(SRB *srb)
{
    if (!srb)
    {
        return SS_ERR;
    }

    if (srb->SRB_HaId < FMOD_CDDA_NTSCSI_NumAdapters)
    {
        srb->SRB_HaId = FMOD_CDDA_NTSCSI_AdapterLookup[srb->SRB_HaId];
    }

    switch(srb->SRB_Cmd)
    {
        case SC_HA_INQUIRY :
            return FMOD_CDDA_NTSCSI_HAInquiry((SRB_HAInquiry *)srb);

        case SC_GET_DEV_TYPE :
            return FMOD_CDDA_NTSCSI_GetDeviceType((SRB_GDEVBlock *)srb);

        case SC_EXEC_SCSI_CMD :
            return FMOD_CDDA_NTSCSI_ExecScsiCmd((SRB_ExecSCSICmd *)srb, false);

        case SC_RESET_DEV :
        default :
            srb->SRB_Status = SS_ERR;
            return SS_ERR;
    }

    return SS_ERR;
}


}
