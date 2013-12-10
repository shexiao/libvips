/*
 * C program cmyk2srgbjpeg (CMYK image conversion to sRGB JPEG)
 *
 * Version 2.4
 *
 * USAGE
 *
 *   cmyk2srgbjpeg INPUT_IMAGE OUTPUT_IMAGE_NAME_BEFORE_EXTENSION
 *
 * where
 *
 *   INPUT_IMAGE_FILE is an image
 *
 *   OUTPUT_IMAGE_NAME_BEFORE_EXTENSION is the desired name (possibly
 *     including path) of the converted image if it is created, to
 *     which a configurable extension identifying a JPEG image will be
 *     appended (.jpg, as delivered).
 *
 * WHAT IT DOES
 *
 * cmyk2srgbjpeg first attempts to detect whether its file argument is
 * a CMYK image.
 *
 * If it does not identify the file as being CMYK, cmyk2srgbjpeg does
 * not do anything.
 *
 * If it determines INPUT_IMAGE_FILE to be CMYK, it converts it to
 * sRGB JPEG using the embedded colour profile if possible,
 * substituting a default ICC otherwise.
 *
 * The result of the conversion is saved into
 * OUTPUT_IMAGE_NAME_BEFORE_EXTENSION.jpg (if .jpg is the configured
 * extension), provided it points elsewhere than INPUT_IMAGE_FILE.
 *
 * CUSTOMISATION
 *
 * Numerous compile time options are set with #defines. See below.
 *
 * NON-LIBRARY REQUIREMENTS:
 *
 * The program expects to find a "backstop" CMYK ICC profile at the
 * location specified by CMYK2SRGBJPEG_BACKUP_CMYK_ICC.  It also
 * expects to find an ICM profile specifying sRGB at the location
 * specified by CMYK2SRGBJPEG_SRGB_ICM. Full paths may be given.
 *
 * CREDITS
 *
 * The development of cmyk2srgbjpeg was partially funded by Booking.com.
 *
 * TODO
 *
 * We need a better way to give the paths of the ICC profiles, switch to doing
 * it vipsthumbnail-style.
 *
 * HISTORY
 *
 * 10/12/13
 * 	- pasted into vips source tree
 * 	- reworked for vips8 API
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /*HAVE_CONFIG_H*/
#include <vips/intl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#include <vips/vips.h>

/*
 * Copyright 2011 Nicolas Robidoux, Adam Turcotte and John Cupitt. All
 * rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NICOLAS ROBIDOUX, ADAM TURCOTTE AND
 * JOHN CUPITT ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NICOLAS ROBIDOUX AND ADAM TURCOTTE OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * COMPILING
 *
 * Provided you have installed libvips and its dependencies (this can
 * be done by installing nip2 through your package manager, for
 * example) and pkg-config is configured correctly (package managers
 * generally do this for you), something like
 *
 *   gcc -Wall -o cmyk2srgbjpeg cmyk2srgbjpeg.c \
 *      `pkg-config --libs --cflags vips`
 *
 * should work. This gcc call assumes that pkg-config can find
 * vips. To find out the version(s) of the VIPS library that
 * pkg-config sees:
 *
 *   pkg-config --list-all | grep vips
 *
 * If you prefer to leave the code alone, you may use the gcc -D
 * method of setting macro values instead of changing the values
 * below.
 *
 * Because cmyk2srgbjpeg is a driver and as such does very little,
 * there is little benefit to compiling it with "higher" optimisation
 * flags (like "-march=native -O2", appropriate if you are compiling
 * on the machine the code is to run on). If you want speed, compile
 * the libvips library itself (and its main dependencies) with
 * optimisation flags matched to your hardware. Better yet, install
 * lcms2-dev so libvips can link to it (unlike its predecessor,
 * lcms2-dev can convert in parallel).
 *
 * It is easy to add the capability to set VIPS runtime options to
 * this program. Depending on the target use, this may make a have a
 * big impact on speed and/or memory usage. As configured,
 * cmyk2srgbjpeg program will use little RAM at the expense of more
 * temporary disk space.  Contact nicolas dot robidoux at gmail dot
 * com for details.
 *
 * Note that the program may emit error messages, for example
 * 
 *   vips warning: im_icc_import: intent 1 (RELATIVE) not ...
 *   ... supported by profile; falling back to default intent ...
 *   ... (usually PERCEPTUAL)
 *
 * Such messages do not directly affect the return codes.
 */


////////////////////////////////////////////////////////////////////////////////

/* BEGINNING OF USER CONFIGURABLE #DEFINES */

////////////////////////////////////////////////////////////////////////////////

/*
 * BACKSTOP CMYK ICC COLOUR PROFILE
 *
 * Path+name of a "Backstop" CMYK compatible ICC profile. It is used
 * when an image detected to be CMYK is not found to have an embedded
 * profile. You may specify a full path. Many nip2 installs put a
 * usable one, namely HP5000_UVDuraImageGlossMaxQ.icc, in
 * /usr/local/share/nip2/data/.
 *
 * (Put in double quotes: It is a string.)
 */

#ifndef CMYK2SRGBJPEG_BACKSTOP_CMYK_ICC
#define CMYK2SRGBJPEG_BACKSTOP_CMYK_ICC \
	"/home/john/vips/share/nip2/data/HP5000_UVDuraImageGlossMaxQ.icc"
#endif

////////////////////////////////////////////////////////////////////////////////

/*
 * SRGB ICM COLOUR PROFILE
 *
 * Path+name of sRGB.icm.  Many nip2 installs put a copy in
 * /usr/local/share/nip2/data/. Many ImageMagick installs put a copy
 * in /usr/local/etc/ImageMagick/.
 *
 * (Put in double quotes: It is a string.)
 */

#ifndef CMYK2SRGBJPEG_SRGB_ICM
#define CMYK2SRGBJPEG_SRGB_ICM \
	"/home/john/vips/share/nip2/data/sRGB.icm"
#endif

////////////////////////////////////////////////////////////////////////////////

/*
 * RETURN CODES
 *
 * Modify to suit. They do not need to be different from each
 * other. For example, you could set all but
 * CMYK2SRGBJPEG_FATAL_ERROR to 0. Integers between 0 and 255 are
 * generally considered safer (although -1 for fatal errors is
 * somewhat traditional).
 *
 * The one return code identifier which is not self-explanatory is
 * CMYK2SRGBJPEG_CMYK_WITH_UNUSABLE_ICC. This return code is issued
 * when the program does not succeed in using an embedded ICC, but
 * successfully converts the image with the backstop ICC.
 *
 * Look into things when CMYK2SRGBJPEG_FATAL_ERROR,
 * CMYK2SRGBJPEG_CMYK_NO_ICC or CMYK2SRGBJPEG_CMYK_WITH_UNUSABLE_ICC
 * are returned.
 */

/*
 * Nothing was done (not detected as CMYK).
 */
#ifndef CMYK2SRGBJPEG_PROBABLY_NOT_CMYK
#define CMYK2SRGBJPEG_PROBABLY_NOT_CMYK      0
#endif

/*
 * The program failed.
 */
#ifndef CMYK2SRGBJPEG_FATAL_ERROR
#define CMYK2SRGBJPEG_FATAL_ERROR            1
#endif

/*
 * The program could not import with the embedded ICC profile, so the
 * backstop profile was substituted.
 */
#ifndef CMYK2SRGBJPEG_CMYK_WITH_UNUSABLE_ICC
#define CMYK2SRGBJPEG_CMYK_WITH_UNUSABLE_ICC 2
#endif

/*
 * No embedded ICC profile was detected, so the backstop profile was
 * substituted.
 */
#ifndef CMYK2SRGBJPEG_CMYK_NO_ICC
#define CMYK2SRGBJPEG_CMYK_NO_ICC            3
#endif

/*
 * Import was performed with the embedded ICC profile.
 */
#ifndef CMYK2SRGBJPEG_CMYK_WITH_USABLE_ICC
#define CMYK2SRGBJPEG_CMYK_WITH_USABLE_ICC   4
#endif

////////////////////////////////////////////////////////////////////////////////

/*
 * JPEG QUALITY SETTING
 *
 * Set to an integer value between 0 and 100. 99 is basically as good
 * as it gets. 95 should be good enough for natural images only shown
 * at a fraction of their original size.
 */
#ifndef CMYK2SRGBJPEG_JPEG_QUALITY
#define CMYK2SRGBJPEG_JPEG_QUALITY 99
#endif

////////////////////////////////////////////////////////////////////////////////

/*
 * JPEG EXTENSION USED WHEN SAVING TO DISK
 *
 * Must be recognised as a JPEG extension by VIPS. (A period is
 * inserted before it, of course.)
 *
 * (Put in double quotes.)
 */
#ifndef CMYK2SRGBJPEG_JPEG_EXTENSION
#define CMYK2SRGBJPEG_JPEG_EXTENSION "jpg"
#endif

////////////////////////////////////////////////////////////////////////////////

/*
 * COLOUR CONVERSION INTENT
 *
 * Use IM_INTENT_RELATIVE_COLORIMETRIC unless you know what you are
 * doing or you visually check the results.
 */
#ifndef CMYK2SRGBJPEG_CONVERSION_INTENT
#define CMYK2SRGBJPEG_CONVERSION_INTENT IM_INTENT_RELATIVE_COLORIMETRIC
#endif

////////////////////////////////////////////////////////////////////////////////

/* END OF USER CONFIGURABLE #DEFINES */

////////////////////////////////////////////////////////////////////////////////

int
main( int argc, char **argv )
{
	GOptionContext *context;
	GError *error = NULL;
	VipsImage *global;
	VipsImage **t;

	if( vips_init( argv[0] ) )
	        vips_error_exit( "unable to start VIPS" );
	textdomain( GETTEXT_PACKAGE );
	setlocale( LC_ALL, "" );

        context = g_option_context_new( _( "INPUT_IMAGE OUTPUT_IMAGE_NAME_BEFORE_EXTENSION - convert CMYK to sRGB" ) );

	g_option_context_add_group( context, vips_get_option_group() );
	if( !g_option_context_parse( context, &argc, &argv, &error ) ) {
		if( error ) {
			fprintf( stderr, "%s\n", error->message );
			g_error_free( error );
		}

		vips_error_exit( "try \"%s --help\"", g_get_prgname() );
	}
	g_option_context_free( context );

	global = vips_image_new();
	t = (VipsImage **) vips_object_local_array( VIPS_OBJECT( global ), 5 );

	if( argc != 3 ||
		!(t[0] = vips_image_new_mode( argv[1], "rs" )) ) {
		g_object_unref( global );
		return CMYK2SRGBJPEG_FATAL_ERROR;
	}

	/*
	 * Read the type and only do something if the input image is CMYK
	 * (type 15).
	 */
	if( vips_image_get_interpretation( t[0] ) == 
		VIPS_INTERPRETATION_CMYK ) {
		char output_filename[VIPS_PATH_MAX];

		/*
		 * Is OUTPUT_IMAGE_NAME_BEFORE_EXTENSION so long that we
		 * can't safely add a period, an extension and JPEG quality
		 * specifier (longest is "[Q=100]")?
		 */
		if( strlen( argv[2] ) >
			VIPS_PATH_MAX - 
				(strlen( CMYK2SRGBJPEG_JPEG_EXTENSION ) + 8) ) {
			g_object_unref( global );
			return CMYK2SRGBJPEG_FATAL_ERROR;
		}

		/*
		 * Assemble the output filename from the second argument,
		 * adding a period, the extension, and the JPEG quality
		 * specifier.
		 */
		vips_snprintf( output_filename, VIPS_PATH_MAX, "%s.%s[Q=%d]",
			argv[2],
			CMYK2SRGBJPEG_JPEG_EXTENSION,
			CMYK2SRGBJPEG_JPEG_QUALITY );

		if( vips_icc_transform( t[0], &t[1], CMYK2SRGBJPEG_SRGB_ICM,
			"input_profile", CMYK2SRGBJPEG_BACKSTOP_CMYK_ICC,
			"intent", CMYK2SRGBJPEG_CONVERSION_INTENT,
			"embedded", TRUE, 
			NULL ) ) {
			g_object_unref( global );
			return CMYK2SRGBJPEG_FATAL_ERROR;
		}

		if( vips_image_write_to_file( t[1], output_filename ) ) {
			g_object_unref( global );
			return CMYK2SRGBJPEG_FATAL_ERROR;
		}

		g_object_unref( global );
		return CMYK2SRGBJPEG_CMYK_WITH_USABLE_ICC;
	}
	else {
		/*
		 * Not detected as a CMYK image. Do nothing.
		 */
		g_object_unref( global );
		return CMYK2SRGBJPEG_PROBABLY_NOT_CMYK;
	}
}
