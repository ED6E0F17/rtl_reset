/*
 * rtl-sdr, turns your Realtek RTL2832 based DVB dongle into a SDR receiver
 * rtl_reset - based on rtl_test, test and benchmark tool
 *
 * Copyright (C) 2012-2014 by Steve Markgraf <steve@steve-m.de>
 * Copyright (C) 2012-2014 by Kyle Keen <keenerd@gmail.com>
 * Copyright (C) 2014 by Michael Tatarinov <kukabu@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifdef __APPLE__
#include <sys/time.h>
#else
#include <time.h>
#endif

#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>
#include "getopt/getopt.h"
#endif

#include <libusb-1.0/libusb.h>
#include "knowndevices.h"

void usage( void ) {
	fprintf( stderr,
			 "rtl_reset, a tool for resetting RTL2832 based DVB-T receivers\n"
			 "Usage:\n"
			 "\t[-d device_index (default: 0)]\n" );
	exit( 1 );
}

static rtlsdr_dongle_t *find_known_device( uint16_t vid, uint16_t pid ) {
	unsigned int i;
	rtlsdr_dongle_t *device = NULL;

	for ( i = 0; i < sizeof( known_devices ) / sizeof( rtlsdr_dongle_t ); i++ ) {
		if ( known_devices[i].vid == vid && known_devices[i].pid == pid ) {
			device = &known_devices[i];
			break;
		}
	}

	return device;
}

uint32_t rtlsdr_get_device_count( void ) {
	int i;
	libusb_context *ctx;
	libusb_device **list;
	uint32_t device_count = 0;
	struct libusb_device_descriptor dd;
	ssize_t cnt;

	libusb_init( &ctx );

	cnt = libusb_get_device_list( ctx, &list );

	for ( i = 0; i < cnt; i++ ) {
		libusb_get_device_descriptor( list[i], &dd );
		if ( find_known_device( dd.idVendor, dd.idProduct ) ) {
			device_count++;
		}
	}

	libusb_free_device_list( list, 1 );
	libusb_exit( ctx );

	return device_count;
}

const char *rtlsdr_get_device_name( uint32_t index ) {
	int i;
	libusb_context *ctx;
	libusb_device **list;
	struct libusb_device_descriptor dd;
	rtlsdr_dongle_t *device = NULL;
	uint32_t device_count = 0;
	ssize_t cnt;

	libusb_init( &ctx );

	cnt = libusb_get_device_list( ctx, &list );

	for ( i = 0; i < cnt; i++ ) {
		libusb_get_device_descriptor( list[i], &dd );

		device = find_known_device( dd.idVendor, dd.idProduct );

		if ( device ) {
			device_count++;

			if ( index == device_count - 1 ) {
				break;
			}
		}
	}

	libusb_free_device_list( list, 1 );

	libusb_exit( ctx );

	if ( device ) {
		return device->name;
	} else {
		return "";
	}
}


int verbose_device_search( char *s ) {
	int i, device_count, device, offset;
	char *s2;
	char vendor[256], product[256], serial[256];
	device_count = rtlsdr_get_device_count();
	if ( !device_count ) {
		fprintf( stderr, "No supported devices found.\n" );
		return -1;
	}
	fprintf( stderr, "Found %d device(s):\n", device_count );
	for ( i = 0; i < device_count; i++ ) {
		rtlsdr_get_device_usb_strings( (uint32_t)i, vendor, product, serial );
		fprintf( stderr, "  %d:  %s, %s, SN: %s\n", i, vendor, product, serial );
	}
	fprintf( stderr, "\n" );
	/* does string look like raw id number */
	device = (int)strtol( s, &s2, 0 );
	if ( s2[0] == '\0' && device >= 0 && device < device_count ) {
		fprintf( stderr, "Using device %d: %s\n",
				 device, rtlsdr_get_device_name( (uint32_t)device ) );
		return device;
	}
	/* does string exact match a serial */
	for ( i = 0; i < device_count; i++ ) {
		rtlsdr_get_device_usb_strings( (uint32_t)i, vendor, product, serial );
		if ( strcmp( s, serial ) != 0 ) {
			continue;
		}
		device = i;
		fprintf( stderr, "Using device %d: %s\n",
				 device, rtlsdr_get_device_name( (uint32_t)device ) );
		return device;
	}
	/* does string prefix match a serial */
	for ( i = 0; i < device_count; i++ ) {
		rtlsdr_get_device_usb_strings( (uint32_t)i, vendor, product, serial );
		if ( strncmp( s, serial, strlen( s ) ) != 0 ) {
			continue;
		}
		device = i;
		fprintf( stderr, "Using device %d: %s\n",
				 device, rtlsdr_get_device_name( (uint32_t)device ) );
		return device;
	}
	/* does string suffix match a serial */
	for ( i = 0; i < device_count; i++ ) {
		rtlsdr_get_device_usb_strings( (uint32_t)i, vendor, product, serial );
		offset = strlen( serial ) - strlen( s );
		if ( offset < 0 ) {
			continue;
		}
		if ( strncmp( s, serial + offset, strlen( s ) ) != 0 ) {
			continue;
		}
		device = i;
		fprintf( stderr, "Resetting device %d: %s\n",
				 device, rtlsdr_get_device_name( (uint32_t)device ) );
		return device;
	}
	fprintf( stderr, "No matching devices found.\n" );
	return -1;
}

int libusb_get_usb_strings( libusb_device_handle *devh, char *manufact, char *product,
							char *serial ) {
	struct libusb_device_descriptor dd;
	libusb_device *device = NULL;
	const int buf_max = 256;
	int r = 0;

	device = libusb_get_device( devh );
	r = libusb_get_device_descriptor( device, &dd );
	if ( r < 0 ) {
		return -1;
	}

	if ( manufact ) {
		memset( manufact, 0, buf_max );
		libusb_get_string_descriptor_ascii( devh, dd.iManufacturer, (unsigned char *)manufact, buf_max );
	}

	if ( product ) {
		memset( product, 0, buf_max );
		libusb_get_string_descriptor_ascii( devh, dd.iProduct, (unsigned char *)product, buf_max );
	}

	if ( serial ) {
		memset( serial, 0, buf_max );
		libusb_get_string_descriptor_ascii( devh, dd.iSerialNumber, (unsigned char *)serial, buf_max );
	}

	return 0;
}

int rtlsdr_get_device_usb_strings( uint32_t index, char *manufact, char *product, char *serial ) {
	int r = -2;
	int i;
	libusb_context *ctx;
	libusb_device **list;
	struct libusb_device_descriptor dd;
	rtlsdr_dongle_t *device = NULL;
	struct libusb_device_handle *devh;
	uint32_t device_count = 0;
	ssize_t cnt;

	libusb_init( &ctx );
	cnt = libusb_get_device_list( ctx, &list );

	for ( i = 0; i < cnt; i++ ) {
		libusb_get_device_descriptor( list[i], &dd );

		device = find_known_device( dd.idVendor, dd.idProduct );
		if ( device ) {
			device_count++;

			if ( index == device_count - 1 ) {
				r = libusb_open( list[i], &devh );
				if ( !r ) {
					r = libusb_get_usb_strings( devh, manufact, product, serial );
					libusb_close( devh );
				}
				break;
			}
		}
	}

	libusb_free_device_list( list, 1 );
	libusb_exit( ctx );

	return r;
}

int rtl_device_reset( uint32_t index ) {
	int r = -2;
	int i;
	libusb_context *ctx;
	libusb_device **list;
	struct libusb_device_descriptor dd;
	rtlsdr_dongle_t *device = NULL;
	struct libusb_device_handle *devh;
	uint32_t device_count = 0;
	ssize_t cnt;

	libusb_init( &ctx );
	cnt = libusb_get_device_list( ctx, &list );

	for ( i = 0; i < cnt; i++ ) {
		libusb_get_device_descriptor( list[i], &dd );

		device = find_known_device( dd.idVendor, dd.idProduct );
		if ( device ) {
			device_count++;

			if ( index == device_count - 1 ) {
				r = libusb_open( list[i], &devh );
				if ( !r ) {
					r = libusb_reset_device( devh );
					libusb_close( devh );
				}
				break;
			}
		}
	}

	libusb_free_device_list( list, 1 );
	libusb_exit( ctx );

	return r;
}

int main( int argc, char **argv ) {
	int opt = -1;
	int devGiven = 0;
	int devIndex = 0;
	while ( ( opt = getopt( argc, argv, "d:h" ) ) != -1 ) {
		switch ( opt ) {
		case 'd':
			devIndex = verbose_device_search( optarg );
			devGiven = 1;
			break;
		case 'h':
		default:
			usage();
			break;
		}
	}

	if ( !devGiven ) {
		devIndex = verbose_device_search( "0" );
	}

	if ( devIndex < 0 ) {
		return -ENODEV;
	}

	return rtl_device_reset( devIndex );
}
