//
// Author: Wolfgang Spraul
//
// This is free and unencumbered software released into the public domain.
// For details see the UNLICENSE file at the root of the source tree.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <sys/stat.h>
#define MACRO_STR(arg)	#arg

#define HASHARRAY_NUM_INDICES (256*256)

// Strings are distributed among 1024 bins. Each bin
// is one continuous stream of zero-terminated strings
// prefixed with a 2*16-bit header. The allocation
// increment for each bin is 32k.
struct hashed_strarray
{
	uint32_t bin_offsets[HASHARRAY_NUM_INDICES]; // min offset is 4, 0 means no entry
	uint16_t index_to_bin[HASHARRAY_NUM_INDICES];
	char* bin_strings[1024];
	int bin_len[1024]; // points behind the last zero-termination
};

//
// columns
//  'L' = X+L logic block
//  'M' = X+M logic block
//  'B' = block ram
//  'D' = dsp (macc)
//  'R' = registers and center IO/logic column
//
//  'n' = noio - can follow L or M to designate a logic
//        column without IO at top or bottom
//  'g' = gclk - can follow LlMmBD to designate exactly one
//        place on the left and right side of the chip where
//        the global clock is separated into left and right
//        half (on each side of the chip, for a total of 4
//        vertical clock separations).
//
// wiring on the left and right side is described with 16
// characters for each row, order is top-down
//   'W' = wired
//   'U' = unwired
//

#define XC6SLX9_ROWS	4
#define XC6SLX9_COLUMNS	"M L Bg M L D M R M Ln M L Bg M L"
#define XC6SLX9_LEFT_WIRING \
	/* row 3 */ "UWUWUWUW" "WWWWUUUU" \
	/* row 2 */ "UUUUUUUU" "WWWWWWUU" \
	/* row 1 */ "WWWUUWUU" "WUUWUUWU" \
	/* row 0 */ "UWUUWUUW" "UUWWWWUU"
#define XC6SLX9_RIGHT_WIRING \
	/* row 3 */ "UUWWUWUW" "WWWWUUUU" \
	/* row 2 */ "UUUUUUUU" "WWWWWWUU" \
	/* row 1 */ "WWWUUWUU" "WUUWUUWU" \
	/* row 0 */ "UWUUWUUW" "UUWWWWUU"

struct fpga_model
{
	int cfg_rows;
	char cfg_columns[512];
	char cfg_left_wiring[1024], cfg_right_wiring[1024];

	int tile_x_range, tile_y_range;
	int center_x;
	int center_y;
	// Left and right gclk separators will be located on
	// the device column (+1 or +2) of the logic or dsp/macc
	// column as indicated in the chip's cfg_columns with a 'g'.
	int left_gclk_sep_x, right_gclk_sep_x;

	struct fpga_tile* tiles;
	struct hashed_strarray str;

	// tmp_str will be allocated to hold max(tile_x_range, tile_y_range)
	// pointers, useful for string seeding when running wires.
	const char** tmp_str;
};

enum fpga_tile_type
{
	NA = 0,
	ROUTING, ROUTING_BRK, ROUTING_VIA,
	HCLK_ROUTING_XM, HCLK_ROUTING_XL, HCLK_LOGIC_XM, HCLK_LOGIC_XL,
	LOGIC_XM, LOGIC_XL,
	REGH_ROUTING_XM, REGH_ROUTING_XL, REGH_LOGIC_XM, REGH_LOGIC_XL,
	BRAM_ROUTING, BRAM_ROUTING_BRK,
	BRAM,
	BRAM_ROUTING_TERM_T, BRAM_ROUTING_TERM_B, BRAM_ROUTING_VIA_TERM_T, BRAM_ROUTING_VIA_TERM_B,
	BRAM_TERM_LT, BRAM_TERM_RT, BRAM_TERM_LB, BRAM_TERM_RB,
	HCLK_BRAM_ROUTING, HCLK_BRAM_ROUTING_VIA, HCLK_BRAM,
	REGH_BRAM_ROUTING, REGH_BRAM_ROUTING_VIA, REGH_BRAM_L, REGH_BRAM_R,
	MACC,
	HCLK_MACC_ROUTING, HCLK_MACC_ROUTING_VIA, HCLK_MACC,
	REGH_MACC_ROUTING, REGH_MACC_ROUTING_VIA, REGH_MACC_L,
	PLL_T, DCM_T, PLL_B, DCM_B, REG_T,
	REG_TERM_T, REG_TERM_B, REG_B,
	REGV_TERM_T, REGV_TERM_B,
	HCLK_REGV,
	REGV, REGV_BRK, REGV_T, REGV_B, REGV_MIDBUF_T, REGV_HCLKBUF_T, REGV_HCLKBUF_B, REGV_MIDBUF_B,
	REGC_ROUTING, REGC_LOGIC, REGC_CMT,
	CENTER, // unique center tile in the middle of the chip
	IO_T, IO_B, IO_TERM_T, IO_TERM_B, IO_ROUTING, IO_LOGIC_TERM_T, IO_LOGIC_TERM_B,
	IO_OUTER_T, IO_INNER_T, IO_OUTER_B, IO_INNER_B,
	IO_BUFPLL_TERM_T, IO_LOGIC_REG_TERM_T, IO_BUFPLL_TERM_B, IO_LOGIC_REG_TERM_B,
	LOGIC_ROUTING_TERM_B, LOGIC_NOIO_TERM_B,
	MACC_ROUTING_TERM_T, MACC_ROUTING_TERM_B, MACC_VIA_TERM_T,
	MACC_TERM_TL, MACC_TERM_TR, MACC_TERM_BL, MACC_TERM_BR,
	ROUTING_VIA_REGC, ROUTING_VIA_IO, ROUTING_VIA_IO_DCM, ROUTING_VIA_CARRY,
	CORNER_TERM_L, CORNER_TERM_R,
	IO_TERM_L_UPPER_TOP, IO_TERM_L_UPPER_BOT, IO_TERM_L_LOWER_TOP, IO_TERM_L_LOWER_BOT,
	IO_TERM_R_UPPER_TOP, IO_TERM_R_UPPER_BOT, IO_TERM_R_LOWER_TOP, IO_TERM_R_LOWER_BOT,
	IO_TERM_L, IO_TERM_R,
	HCLK_TERM_L, HCLK_TERM_R,
	REGH_IO_TERM_L, REGH_IO_TERM_R,
	REG_L, REG_R,
	IO_PCI_L, IO_PCI_R, IO_RDY_L, IO_RDY_R,
	IO_L, IO_R,
	IO_PCI_CONN_L, IO_PCI_CONN_R,
	CORNER_TERM_T, CORNER_TERM_B,
	ROUTING_IO_L,
	HCLK_ROUTING_IO_L, HCLK_ROUTING_IO_R, REGH_ROUTING_IO_L, REGH_ROUTING_IO_R,
	ROUTING_IO_L_BRK, ROUTING_GCLK,
	REGH_IO_L, REGH_IO_R, REGH_MCB, HCLK_MCB,
	ROUTING_IO_VIA_L, ROUTING_IO_VIA_R, ROUTING_IO_PCI_CE_L, ROUTING_IO_PCI_CE_R,
	CORNER_TL, CORNER_BL,
	CORNER_TR_UPPER, CORNER_TR_LOWER, CORNER_BR_UPPER, CORNER_BR_LOWER,
	HCLK_IO_TOP_UP_L, HCLK_IO_TOP_UP_R,
	HCLK_IO_TOP_SPLIT_L, HCLK_IO_TOP_SPLIT_R,
	HCLK_IO_TOP_DN_L, HCLK_IO_TOP_DN_R,
	HCLK_IO_BOT_UP_L, HCLK_IO_BOT_UP_R,
	HCLK_IO_BOT_SPLIT_L, HCLK_IO_BOT_SPLIT_R,
	HCLK_IO_BOT_DN_L, HCLK_IO_BOT_DN_R,
};

// Some constant to make the core more readable
#define LEFT_IO_ROUTING 2
#define LEFT_IO_DEVS	3
#define TOP_IO_TILES	2

// tile flags

#define TF_FABRIC_ROUTING_COL		0x00000001 // only set in y==0, not for left and right IO routing or center
#define TF_FABRIC_LOGIC_COL		0x00000002 // only set in y==0
#define TF_FABRIC_BRAM_MACC_ROUTING_COL	0x00000004 // only set in y==0
#define TF_FABRIC_BRAM_COL		0x00000008 // only set in y==0
#define TF_FABRIC_MACC_COL		0x00000010 // only set in y==0
#define TF_BRAM_DEV			0x00000020
#define TF_MACC_DEV			0x00000040
#define TF_LOGIC_XL_DEV			0x00000080
#define TF_LOGIC_XM_DEV			0x00000100
#define TF_IOLOGIC_DELAY_DEV		0x00000200
#define TF_DCM_DEV			0x00000400
#define TF_PLL_DEV			0x00000800

#define Y_INNER_TOP		0x0001
#define Y_INNER_BOTTOM		0x0002
#define Y_CHIP_HORIZ_REGS	0x0004
#define Y_ROW_HORIZ_AXSYMM	0x0008
#define Y_BOTTOM_OF_ROW		0x0010

// multiple checks are combined with OR logic
int is_aty(int check, struct fpga_model* model, int y);

#define X_OUTER_LEFT			0x00000001
#define X_INNER_LEFT			0x00000002
#define X_INNER_RIGHT			0x00000004
#define X_OUTER_RIGHT			0x00000008
#define X_ROUTING_COL			0x00000010 // includes routing col in left and right IO and center
#define X_ROUTING_TO_BRAM_COL		0x00000020
#define X_ROUTING_TO_MACC_COL		0x00000040
#define X_LOGIC_COL			0x00000080 // includes the center logic col
#define X_FABRIC_ROUTING_COL		0x00000100
#define X_FABRIC_LOGIC_COL		0x00000200
#define X_FABRIC_BRAM_MACC_ROUTING_COL	0x00000400
#define X_FABRIC_BRAM_COL		0x00000800
#define X_FABRIC_MACC_COL		0x00001000
#define X_CENTER_ROUTING_COL		0x00002000
#define X_CENTER_LOGIC_COL		0x00004000
#define X_CENTER_CMTPLL_COL		0x00008000
#define X_CENTER_REGS_COL		0x00010000
#define X_LEFT_IO_ROUTING_COL		0x00020000
#define X_LEFT_IO_DEVS_COL		0x00040000
#define X_RIGHT_IO_ROUTING_COL		0x00080000
#define X_RIGHT_IO_DEVS_COL		0x00100000
#define X_LEFT_SIDE			0x00200000 // true for anything left of the center (not including center)

// multiple checks are combined with OR logic
int is_atx(int check, struct fpga_model* model, int x);

// True for all tiles that are in the regular 0..15 row tiles of a routing col
#define YX_ROUTING_TILE		0x0001

int is_atyx(int check, struct fpga_model* model, int y, int x);

#define SWITCH_BIDIRECTIONAL		0x40000000

struct fpga_tile
{
	enum fpga_tile_type type;
	int flags;

	// expect up to 64 devices per tile
	int num_devices;
	struct fpga_device* devices;

	// expect up to 5k connection point names per tile
	// 2*16 bit per entry
	//   - index into conn_point_dests (not multiplied by 3) (16bit)
	//   - hashed string array index (16 bit)
	int num_conn_point_names; // conn_point_names is 2*num_conn_point_names 16-bit words
	uint16_t* conn_point_names; // num_conn_point_names*2 16-bit-words: 16(conn)-16(str)

	// expect up to 28k connection point destinations to other tiles per tile
	// 3*16 bit per destination:
	//   - x coordinate of other tile (16bit)
	//   - y coordinate of other tile (16bit)
	//   - hashed string array index for conn_point_names name in other tile (16bit)
	int num_conn_point_dests; // conn_point_dests array is 3*num_conn_point_dests 16-bit words
	uint16_t* conn_point_dests; // num_conn_point_dests*3 16-bit words: 16(x)-16(y)-16(conn_name)

	// expect up to 4k switches per tile
	// 32bit: 31    off: no connection   on: connected
	//        30    off: unidirectional  on: bidirectional
	//        29:15 from, index into conn_point_names
	//        14:0  to, index into conn_point_names
	int num_switches;
	uint32_t* switches;
};

int fpga_build_model(struct fpga_model* model,
	int fpga_rows, const char* columns,
	const char* left_wiring, const char* right_wiring);
void fpga_free_model(struct fpga_model* model);

const char* fpga_tiletype_str(enum fpga_tile_type type);

uint32_t hash_djb2(const unsigned char* str);

const char* strarray_lookup(struct hashed_strarray* array, uint16_t idx);
// The found or created index will never be 0, so the caller
// can use 0 as a special value to indicate 'no string'.
int strarray_find_or_add(struct hashed_strarray* array, const char* str,
	uint16_t* idx);
int strarray_used_slots(struct hashed_strarray* array);
void strarray_init(struct hashed_strarray* array);
void strarray_free(struct hashed_strarray* array);
