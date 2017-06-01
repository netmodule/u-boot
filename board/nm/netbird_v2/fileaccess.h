/**@file	/home/eichenberger/projects/nbhw16/u-boot/board/nm/netbird_v2/fileaccess.h
 * @author	eichenberger
 * @version	704
 * @date
 * 	Created:	Tue 06 Jun 2017 02:02:33 PM CEST \n
 * 	Last Update:	Tue 06 Jun 2017 02:02:33 PM CEST
 */
#ifndef FILEACCESS_H
#define FILEACCESS_H

void fs_set_console(void);
int read_file(const char* filename, char *buf, int size);

#endif // FILEACCESS_H
