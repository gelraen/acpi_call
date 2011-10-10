#include <contrib/dev/acpica/include/acpi.h>
#include "acpi_call_io.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/param.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define	MAX_ACPI_PATH	1024 // XXX
#define MAX_ACPI_ARGS	7

char dev_path[MAXPATHLEN] = "/dev/acpi";
char method_path[MAX_ACPI_PATH] = "";
size_t result_buf_size = 1024;
char output_format = 'b';

int verbose;

ACPI_OBJECT args[MAX_ACPI_ARGS];
struct acpi_call_descr params;

void parse_opts(int, char *[]);
void show_help(FILE*);
int parse_buffer(ACPI_OBJECT*, char*);
void print_params(struct acpi_call_descr*);
void print_acpi_object(ACPI_OBJECT*);
void print_acpi_buffer(ACPI_BUFFER*, char);

int main(int argc, char * argv[])
{
	int fd;

	bzero(&params, sizeof(params));
	params.path = method_path;
	params.args.Count = 0;
	params.args.Pointer = args;

	verbose = 0;
	
	parse_opts(argc, argv);

	params.result.Length = result_buf_size;
	params.result.Pointer = malloc(result_buf_size);

	if (params.result.Pointer == NULL)
	{
		perror("malloc");
		return 1;
	}

	if (method_path[0] == 0)
	{
		fprintf(stderr, "Please specify path to method with -p flag\n");
		return 1;
	}

	if (verbose)
		print_params(&params);

	fd = open(dev_path, O_RDWR);
	if (fd < 0)
	{
		perror("open");
		return 1;
	}
	if (ioctl(fd, ACPIIO_CALL, &params) == -1)
	{
		perror("ioctl");
		return 1;
	}

	if (verbose)
		printf("Status: %d\nResult: ", params.retval);
	print_acpi_buffer(&params.result, output_format);
	printf("\n");

	return params.retval;
}

void parse_opts(int argc, char * argv[])
{
	char c;
	int i;

	while ((c = getopt(argc, argv, "hvd:p:i:s:b:o:")) != -1)
	{
		switch(c)
		{
		case 'h':
			show_help(stdout);
			exit(0);
			break;
		case 'v':
			verbose = 1;
			break;
		case 'd':
			strlcpy(dev_path, optarg, MAXPATHLEN);
			break;
		case 'p':
			strlcpy(method_path, optarg, MAX_ACPI_PATH);
			break;
		case 'i':
		case 's':
		case 'b':
			i = params.args.Count;
			if (i >= MAX_ACPI_ARGS)
			{
				fprintf(stderr, "Maximum number of arguments exceeded\n");
				exit(1);
			}
			switch (optopt)
			{
			case 'i':
				args[i].Type = ACPI_TYPE_INTEGER;
				args[i].Integer.Value = strtol(optarg, NULL, 10);
				break;
			case 's':
				args[i].Type = ACPI_TYPE_STRING;
				args[i].String.Length = strlen(optarg);
				args[i].String.Pointer = optarg;
				break;
			case 'b':
				if (parse_buffer(&args[i], optarg))
				{
					fprintf(stderr, "Unable to parse hexstring to buffer: %s\n", optarg);
					exit(1);
				}
				break;
			}
			params.args.Count++;
			break;
		case 'o':
			output_format = optarg[0];
			switch (optarg[0])
			{
			case 'i':
			case 's':
			case 'b':
				break;
			default:
				fprintf(stderr, "Incorrect output format: %c", optarg[0]);
				show_help(stderr);
				exit(1);
			}
		default:
			show_help(stderr);
			exit(1);
		}
	}
}

void show_help(FILE* f)
{
	fprintf(f, "Options:\n");
	fprintf(f, "  -h              - print this help\n");
	fprintf(f, "  -v              - be verbose\n");
	fprintf(f, "  -d filename     - specify path to ACPI control pseudo-device. Default: /dev/acpi\n");
	fprintf(f, "  -p path         - full path to ACPI method\n");
	fprintf(f, "  -i number       - add integer argument\n");
	fprintf(f, "  -s string       - add string argument\n");
	fprintf(f, "  -b hexstring    - add buffer argument\n");
	fprintf(f, "  -o i|s|b        - print result as integer|string|hexstring\n");
}

int parse_buffer(ACPI_OBJECT *dst, char *src)
{
	char tmp[3] = {0};
	size_t len = strlen(src)/2, i;

	dst->Type = ACPI_TYPE_BUFFER;
	dst->Buffer.Length = len;
	if ((dst->Buffer.Pointer = (UINT8*)malloc(len)) == NULL)
	{
		fprintf(stderr, "parse_buffer: Failed to allocate %d bytes\n", len);
		exit(1);
	}

	for(i = 0; i < len; i++)
	{
		tmp[0] = src[i*2];
		tmp[1] = src[i*2+1];
		dst->Buffer.Pointer[i] = strtol(tmp, NULL, 16);
	}

	return 0;
}

void print_params(struct acpi_call_descr* p)
{
	int i, j;

	printf("Path: %s\n", p->path);
	printf("Number of arguments: %d\n", p->args.Count);
	for(i = 0; i < p->args.Count; i++)
	{
		switch (p->args.Pointer[i].Type)
		{
		case ACPI_TYPE_INTEGER:
			printf("Argument %d type: Integer\n", i+1);
			break;
		case ACPI_TYPE_STRING:
			printf("Argument %d type: String\n", i+1);
			break;
		case ACPI_TYPE_BUFFER:
			printf("Argument %d type: Buffer\n", i+1);
			break;
		}
		printf("Argument %d value: ", i+1);
		print_acpi_object(&(p->args.Pointer[i]));
		printf("\n");
	}
}

void print_acpi_object(ACPI_OBJECT* obj)
{
	int i;

	switch (obj->Type)
	{
	case ACPI_TYPE_INTEGER:
		printf("%llu", obj->Integer.Value);
		break;
	case ACPI_TYPE_STRING:
		printf("%s", obj->String.Pointer);
		break;
	case ACPI_TYPE_BUFFER:
		for(i = 0; i < obj->Buffer.Length; i++)
		{
			printf("%X", obj->Buffer.Pointer[i]);
		}
		break;
	default:
		printf("Unknown object type '%d'", obj->Type);
	}	
}

void print_acpi_buffer(ACPI_BUFFER* buf, char format)
{
	int i;

	switch (format)
	{
	case 'i':
		printf("%llu", *((ACPI_INTEGER*)(buf->Pointer)));
		break;
	case 's':
		printf("%s", buf->Pointer);
		break;
	case 'b':
		for(i = 0; i < buf->Length; i++)
		{
			printf("%X", ((UINT8*)(buf->Pointer))[i]);
		}
		break;
	}
}
