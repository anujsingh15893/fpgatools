//
// Author: Wolfgang Spraul
//
// This is free and unencumbered software released into the public domain.
// For details see the UNLICENSE file at the root of the source tree.
//

//
// Design principles of a floorplan file
//
// What needs to be in the file:
// - all devices, configuration for each device
//   probably multiple lines that are adding config strings
// - wires maybe separately, and/or as named connection points
//   in tiles?
// - connection pairs that can be enabled/disabled
// - global flags and configuration registers
// - the static data should be optional (unused conn pairs,
//   unused devices, wires)
//
// - each line should be in the global namespace, line order
//   should not matter
// - file should be easily parsable with bison
// - lines should typically not exceed 80 characters
//

void printf_version(void);
int printf_tiles(struct fpga_model* model);
int printf_devices(struct fpga_model* model);
int printf_ports(struct fpga_model* model);
int printf_conns(struct fpga_model* model);
int printf_switches(struct fpga_model* model);