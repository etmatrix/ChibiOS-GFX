/*
    ChibiOS/GFX - Copyright (C) 2012
                 Joel Bodenmann aka Tectu <joel@unormal.org>

    This file is part of ChibiOS/GFX.

    ChibiOS/GFX is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    ChibiOS/GFX is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/**
 * @file    drivers/gdisp/ILI9325/gdisp_lld_board_example.h
 * @brief   GDISP Graphic Driver subsystem board interface for the ILI9325 display.
 *
 * @addtogroup GDISP
 * @{
 */

#ifndef GDISP_LLD_BOARD_H
#define GDISP_LLD_BOARD_H

static __inline void lld_gdisp_init_board(void) {
	#error "ILI9325: You must implement the init_board routine for your board"
}

static __inline void lld_gdisp_reset_pin(bool_t state) {
	#error "ILI9325: You must implement setpin_reset routine for your board"
}

static __inline void lld_gdisp_write_index(uint16_t data) {
	#error "ILI9325: You must implement write_index routine for your board"
}

static __inline void lld_gdisp_write_data(uint16_t data) {
	#error "ILI9325: You must implement write_data routine for your board"
}

static __inline uint16_t lld_gdisp_read_data(void) {
	#error "ILI9325: You must implement read_data routine for your board"
}

/* if not available, just ignore the argument and return */
static __inline uint16_t lld_gdisp_backlight(uint8_t percentage) {
	#error "ILI9325: You must implement set_backlight routine for your board"
}

#endif /* GDISP_LLD_BOARD_H */
/** @} */

