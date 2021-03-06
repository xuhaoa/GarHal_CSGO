#pragma warning( disable : 4100 4047 4024 4022 4201 4311 4057 4213 4189 4081 4189 4706 4214 4459 4273)

#include <ntdef.h>
#include "data.h"
#include "messages.h"
#include "communication.h"
#include "memory.h"


NTSTATUS CreateCall(PDEVICE_OBJECT DeviceObject, PIRP irp)
{
	irp->IoStatus.Status = STATUS_SUCCESS;
	irp->IoStatus.Information = 0;

	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS CloseCall(PDEVICE_OBJECT DeviceObject, PIRP irp)
{
	irp->IoStatus.Status = STATUS_SUCCESS;
	irp->IoStatus.Information = 0;

	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

// IOCTL Call Handler function

NTSTATUS IoControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	NTSTATUS Status;
	ULONG BytesIO = 0;

	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);

	// Code received from user space
	ULONG ControlCode = stack->Parameters.DeviceIoControl.IoControlCode;
	if (ControlCode == IO_READ_REQUEST)
	{
		// Get the input buffer & format it to our struct
		PKERNEL_READ_REQUEST ReadInput = (PKERNEL_READ_REQUEST)Irp->AssociatedIrp.SystemBuffer;
		PKERNEL_READ_REQUEST ReadOutput = (PKERNEL_READ_REQUEST)Irp->AssociatedIrp.SystemBuffer;

		PEPROCESS Process;

		// Get our process
		if (NT_SUCCESS(PsLookupProcessByProcessId(ReadInput->ProcessId, &Process))) {

			//read from ReadInput->Address and write it to pBuff so we can use the data in our controller
			KeReadVirtualMemory(Process, ReadInput->Address, ReadInput->pBuff, ReadInput->Size);
		}
			
		//DbgPrintEx(0, 0, "Read Params:  %lu, %#010x \n", ReadInput->ProcessId, ReadInput->Address);
		//DbgPrintEx(0, 0, "Value: %lu \n", ReadOutput->Response);

		Status = STATUS_SUCCESS;
		BytesIO = sizeof(KERNEL_READ_REQUEST);
	}
	else if (ControlCode == IO_WRITE_REQUEST)
	{
		// Get the input buffer & format it to our struct
		PKERNEL_WRITE_REQUEST WriteInput = (PKERNEL_WRITE_REQUEST)Irp->AssociatedIrp.SystemBuffer;

		PEPROCESS Process;
		// Get our process
		if (NT_SUCCESS(PsLookupProcessByProcessId(WriteInput->ProcessId, &Process))) {
			// copy the value of pBuff to WriteInput->Address
			KeWriteVirtualMemory(Process, WriteInput->pBuff, WriteInput->Address, WriteInput->Size);
		}
			
		//DbgPrintEx(0, 0, "Write Params:  %lu, %#010x \n", WriteInput->Value, WriteInput->Address);

		Status = STATUS_SUCCESS;
		BytesIO = sizeof(KERNEL_WRITE_REQUEST);
	}
	else if (ControlCode == IO_GET_ID_REQUEST)
	{
		PULONG OutPut = (PULONG)Irp->AssociatedIrp.SystemBuffer;
		*OutPut = csgoId;

		DebugMessageNormal("A UserMode Application requested the ProcessID: %#010x", csgoId);
		Status = STATUS_SUCCESS;
		BytesIO = sizeof(*OutPut);
	}
	else if (ControlCode == IO_GET_MODULE_REQUEST)
	{
		PULONG OutPut = (PULONG)Irp->AssociatedIrp.SystemBuffer;
		*OutPut = ClientAddress;

		DebugMessageNormal("A UserMode Application requested the ClientAddress: %#010x", ClientAddress);
		Status = STATUS_SUCCESS;
		BytesIO = sizeof(*OutPut);
	}
	else if (ControlCode == IO_GET_ENGINE_MODULE_REQUEST)
	{
		PULONG OutPut = (PULONG)Irp->AssociatedIrp.SystemBuffer;
		*OutPut = EngineAddress;

		DebugMessageNormal("A UserMode Application requested the EngineAddress: %#010x", EngineAddress);
		Status = STATUS_SUCCESS;
		BytesIO = sizeof(*OutPut);
	}
	else if (ControlCode == IO_GET_MODULE_REQUEST_LENGTH)
	{
		PULONG OutPut = (PULONG)Irp->AssociatedIrp.SystemBuffer;
		*OutPut = ClientSize;

		DebugMessageNormal("A UserMode Application requested the size of Client: %#010x", ClientSize);
		Status = STATUS_SUCCESS;
		BytesIO = sizeof(*OutPut);
	}
	else if (ControlCode == IO_GET_ENGINE_MODULE_REQUEST_LENGTH)
	{
		PULONG OutPut = (PULONG)Irp->AssociatedIrp.SystemBuffer;
		*OutPut = EngineSize;

		DebugMessageNormal("A UserMode Application requested the size of Engine: %#010x", EngineSize);
		Status = STATUS_SUCCESS;
		BytesIO = sizeof(*OutPut);
	}
	else
	{
		// if the code is unknown
		Status = STATUS_INVALID_PARAMETER;
		BytesIO = 0;
	}

	// Complete the request
	Irp->IoStatus.Status = Status;
	Irp->IoStatus.Information = BytesIO;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return Status;
}