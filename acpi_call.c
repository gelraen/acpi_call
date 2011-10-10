#include <sys/types.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpiio.h>
#include "acpi_call_io.h"

static int
acpi_call_ioctl(u_long cmd, caddr_t addr, void *arg)
{
	struct acpi_call_descr *params;
	ACPI_BUFFER	result;

	result.Length = ACPI_ALLOCATE_BUFFER;
	result.Pointer = NULL;

	if (cmd == ACPIIO_CALL) {
		params = (struct acpi_call_descr*)addr;
		params->retval = AcpiEvaluateObject(NULL, params->path, &params->args, &result);
		if (ACPI_SUCCESS(params->retval))
		{
			if (result.Pointer != NULL)
			{
				if (params->result.Pointer != NULL)
				{	
					params->result.Length = min(params->result.Length, result.Length);
					copyout(result.Pointer, params->result.Pointer,
							params->result.Length);
					params->reslen = result.Length;
				}
				AcpiOsFree(result.Pointer);
			}
		}
	}

	return (0);
}

static int
acpi_call_loader(struct module *m, int what, void *arg)
{
	int err = 0;
        
   	switch (what) {
	case MOD_LOAD:
		err = acpi_register_ioctl(ACPIIO_CALL, acpi_call_ioctl, NULL);
		break;
	case MOD_UNLOAD:
		acpi_deregister_ioctl(ACPIIO_CALL, acpi_call_ioctl);
		break;
	default:
		err = EOPNOTSUPP;
		break;
	}
	return(err);
}

static moduledata_t acpi_call_mod = {
	"acpi_call",
	acpi_call_loader,
	NULL
};  

DECLARE_MODULE(acpi_call, acpi_call_mod, SI_SUB_KLD, SI_ORDER_ANY);
