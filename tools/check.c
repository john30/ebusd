/*
 * Copyright (C) Roland Jax 2012-2013 <roland.jax@liwest.at>
 *
 * This file is part of ebusd.
 *
 * ebusd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebusd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebusd. If not, see http://www.gnu.org/licenses/.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <string.h>

#include "ebus-decode.h"

int main(void) {

	int i, j, end, ret;

	int bcd, d1b, data[SERIAL_BUFSIZE + 1];
	float d1c, d2b, d2c;

	char byte;
	
	unsigned char hex, tmp, crc, crc_calc[2] ;

	tmp = 0;
	end = 0;
	do {
		i = 0;
		printf("Input: ");
		while ((byte = fgetc(stdin)) != EOF) {
			
			if (byte == '\n') {
				break;
			}
				
			if (byte == 'q') {
				end = 1;
				break;
			}
			
			if (i < (int) sizeof(data)) {
				ret = eb_htoi(&byte);
				if (ret != -1) {
					data[i] = ret;
					i++;
				}
			} else {
				break;
			}
		}		
		
		if (!end) {
		
			for (j = 0; j < i; j += 2) {
				
				bcd = 0;
				d1b = 0;
				ret = 0;
				d1c = 0.0;
				d2b = 0.0;
				d2c = 0.0;

				hex = (unsigned char) (data[j]*16 + data[j+1]);
				
				ret = eb_bcd_to_int(hex, &bcd);
				ret = eb_d1b_to_int(hex, &d1b);
				
				ret = eb_d1c_to_float(hex, &d1c);
				printf("hex %02x ->\tbcd: %3d\td1b: %4d"
					"\td1c: %5.1f", hex, bcd, d1b, d1c);

				
				if (j == 2) {
					ret = eb_d2b_to_float(tmp, hex, &d2b);
					ret = eb_d2c_to_float(tmp, hex, &d2c);
					crc_calc[0] = tmp;
					crc_calc[1] = hex;
					crc = eb_calc_crc(crc_calc,2);
					printf("\td2b: %10.5f\td2c: %12.6f"
						"\tcrc: %02x\n", d2b, d2c, crc);
				}
				else {
					tmp = hex;
					printf("\n");
				}
			}
		}

	} while (end == 0);
	
	return 0;
}
