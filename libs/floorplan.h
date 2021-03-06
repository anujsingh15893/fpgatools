//
// Author: Wolfgang Spraul
//
// This is free and unencumbered software released into the public domain.
// For details see the UNLICENSE file at the root of the source tree.
//

int read_floorplan(struct fpga_model *model, FILE *f);
#define FP_DEFAULT	0x0000
#define FP_NO_HEADER	0x0001
int write_floorplan(FILE *f, struct fpga_model *model, int flags);

void printf_version(FILE *f);
int printf_tiles(FILE *f, struct fpga_model *model);
int printf_devices(FILE *f, struct fpga_model *model, int config_only);
int printf_ports(FILE *f, struct fpga_model *model);
int printf_conns(FILE *f, struct fpga_model *model);
int printf_switches(FILE *f, struct fpga_model *model);
int printf_nets(FILE *f, struct fpga_model *model);

int printf_IOB(FILE* f, struct fpga_model* model,
	int y, int x, int type_idx, int config_only);
int printf_LOGIC(FILE* f, struct fpga_model* model,
	int y, int x, int type_idx, int config_only);
int printf_BUFGMUX(FILE* f, struct fpga_model* model,
	int y, int x, int type_idx, int config_only);
int printf_BUFIO(FILE* f, struct fpga_model* model,
	int y, int x, int type_idx, int config_only);
int printf_BSCAN(FILE* f, struct fpga_model* model,
	int y, int x, int type_idx, int config_only);
