/*
 * jsimd_x86_64.c
 *
 * Copyright 2009 Pierre Ossman <ossman@cendio.se> for Cendio AB
 * Copyright 2009-2011, 2014 D. R. Commander
 * Copyright 2015, Intel Corporation
 *
 * Based on the x86 SIMD extension for IJG JPEG library,
 * Copyright (C) 1999-2006, MIYASAKA Masaru.
 * For conditions of distribution and use, see copyright notice in jsimdext.inc
 *
 * This file contains the interface between the "normal" portions
 * of the library and the SIMD implementations when running on a
 * 64-bit x86 architecture.
 */

#define JPEG_INTERNALS
#include "../jinclude.h"
#include "../jpeglib.h"
#include "../jsimd.h"
#include "../jdct.h"
#include "../jsimddct.h"
#include "jsimd.h"

/*
 * In the PIC cases, we have no guarantee that constants will keep
 * their alignment. This macro allows us to verify it at runtime.
 */
#define IS_ALIGNED(ptr, order) (((size_t)ptr & ((1 << order) - 1)) == 0)

#define IS_ALIGNED_SSE(ptr) (IS_ALIGNED(ptr, 4)) /* 16 byte alignment */

static unsigned int simd_support = ~0;

/*
 * Check what SIMD accelerations are supported.
 *
 * FIXME: This code is racy under a multi-threaded environment.
 */
LOCAL(void)
init_simd (void)
{
	char *env = NULL;

	if (simd_support != ~0U)
		return;

	simd_support = jpeg_simd_cpu_support();

	/* Force different settings through environment variables */
	env = getenv("JSIMD_FORCEMMX");
	if ((env != NULL) && (strcmp(env, "1") == 0))
		simd_support &= JSIMD_MMX;
	env = getenv("JSIMD_FORCE3DNOW");
	if ((env != NULL) && (strcmp(env, "1") == 0))
		simd_support &= JSIMD_3DNOW|JSIMD_MMX;
	env = getenv("JSIMD_FORCESSE");
	if ((env != NULL) && (strcmp(env, "1") == 0))
		simd_support &= JSIMD_SSE|JSIMD_MMX;
	env = getenv("JSIMD_FORCESSE2");
	if ((env != NULL) && (strcmp(env, "1") == 0))
		simd_support &= JSIMD_SSE2;
	env = getenv("JSIMD_FORCEAVX2");
	if ((env != NULL) && (strcmp(env, "1") == 0))
		simd_support &= JSIMD_AVX2;
	env = getenv("JSIMD_FORCENONE");
	if ((env != NULL) && (strcmp(env, "1") == 0))
		simd_support = 0;

  if(simd_support & JSIMD_AVX2)
		putenv("JSIMD_LENGTH_256=1");
  else
		putenv("JSIMD_LENGTH_256=0");
}

GLOBAL(int)
jsimd_can_rgb_ycc (void)
{
  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if ((RGB_PIXELSIZE != 3) && (RGB_PIXELSIZE != 4))
    return 0;

  init_simd();
  if (simd_support & JSIMD_SSE2) 
	  return 1;

  if (simd_support & JSIMD_AVX2) 
	  return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_rgb_gray (void)
{
  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if ((RGB_PIXELSIZE != 3) && (RGB_PIXELSIZE != 4))
    return 0;

  init_simd();
  if (simd_support & JSIMD_SSE2) 
    return 1;
  if ((simd_support & JSIMD_AVX2))
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_ycc_rgb (void)
{
  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if ((RGB_PIXELSIZE != 3) && (RGB_PIXELSIZE != 4))
    return 0;

  init_simd();
  if (simd_support & JSIMD_SSE2) 
    return 1;
  if ((simd_support & JSIMD_AVX2))
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_ycc_rgb565 (void)
{
  return 0;
}

GLOBAL(void)
jsimd_rgb_ycc_convert (j_compress_ptr cinfo,
                       JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
                       JDIMENSION output_row, int num_rows)
{
  void (*sse2fct)(JDIMENSION, JSAMPARRAY, JSAMPIMAGE, JDIMENSION, int);
  void (*avx2fct)(JDIMENSION, JSAMPARRAY, JSAMPIMAGE, JDIMENSION, int);

  switch(cinfo->in_color_space) {
    case JCS_EXT_RGB:
      sse2fct=jsimd_extrgb_ycc_convert_sse2;
      avx2fct=jsimd_extrgb_ycc_convert_avx2;
      break;
    case JCS_EXT_RGBX:
    case JCS_EXT_RGBA:
      sse2fct=jsimd_extrgbx_ycc_convert_sse2;
      avx2fct=jsimd_extrgbx_ycc_convert_avx2;
      break;
    case JCS_EXT_BGR:
      sse2fct=jsimd_extbgr_ycc_convert_sse2;
      avx2fct=jsimd_extbgr_ycc_convert_avx2;
      break;
    case JCS_EXT_BGRX:
    case JCS_EXT_BGRA:
      sse2fct=jsimd_extbgrx_ycc_convert_sse2;
      avx2fct=jsimd_extbgrx_ycc_convert_avx2;
      break;
    case JCS_EXT_XBGR:
    case JCS_EXT_ABGR:
      sse2fct=jsimd_extxbgr_ycc_convert_sse2;
      avx2fct=jsimd_extxbgr_ycc_convert_avx2;
      break;
    case JCS_EXT_XRGB:
    case JCS_EXT_ARGB:
      sse2fct=jsimd_extxrgb_ycc_convert_sse2;
      avx2fct=jsimd_extxrgb_ycc_convert_avx2;
      break;
    default:
      sse2fct=jsimd_rgb_ycc_convert_sse2;
      avx2fct=jsimd_rgb_ycc_convert_avx2;
      break;
  }

  if (simd_support & JSIMD_AVX2)
    avx2fct(cinfo->image_width, input_buf, output_buf, output_row, num_rows);
  else if (simd_support & JSIMD_SSE2)
    sse2fct(cinfo->image_width, input_buf, output_buf, output_row, num_rows);
}

GLOBAL(void)
jsimd_rgb_gray_convert (j_compress_ptr cinfo,
                        JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
                        JDIMENSION output_row, int num_rows)
{
  void (*sse2fct)(JDIMENSION, JSAMPARRAY, JSAMPIMAGE, JDIMENSION, int);
  void (*avx2fct)(JDIMENSION, JSAMPARRAY, JSAMPIMAGE, JDIMENSION, int);

  switch(cinfo->in_color_space) {
    case JCS_EXT_RGB:
      sse2fct=jsimd_extrgb_gray_convert_sse2;
      avx2fct=jsimd_extrgb_gray_convert_avx2;
      break;
    case JCS_EXT_RGBX:
    case JCS_EXT_RGBA:
      sse2fct=jsimd_extrgbx_gray_convert_sse2;
      avx2fct=jsimd_extrgbx_gray_convert_avx2;
      break;
    case JCS_EXT_BGR:
      sse2fct=jsimd_extbgr_gray_convert_sse2;
      avx2fct=jsimd_extbgr_gray_convert_avx2;
      break;
    case JCS_EXT_BGRX:
    case JCS_EXT_BGRA:
      sse2fct=jsimd_extbgrx_gray_convert_sse2;
      avx2fct=jsimd_extbgrx_gray_convert_avx2;
      break;
    case JCS_EXT_XBGR:
    case JCS_EXT_ABGR:
      sse2fct=jsimd_extxbgr_gray_convert_sse2;
      avx2fct=jsimd_extxbgr_gray_convert_avx2;
      break;
    case JCS_EXT_XRGB:
    case JCS_EXT_ARGB:
      sse2fct=jsimd_extxrgb_gray_convert_sse2;
      avx2fct=jsimd_extxrgb_gray_convert_avx2;
      break;
    default:
      sse2fct=jsimd_rgb_gray_convert_sse2;
      avx2fct=jsimd_rgb_gray_convert_avx2;
      break;
  }

  if (simd_support & JSIMD_AVX2)
  	avx2fct(cinfo->image_width, input_buf, output_buf, output_row, num_rows);
  else if (simd_support & JSIMD_SSE2)
    sse2fct(cinfo->image_width, input_buf, output_buf, output_row, num_rows);
}

GLOBAL(void)
jsimd_ycc_rgb_convert (j_decompress_ptr cinfo,
                       JSAMPIMAGE input_buf, JDIMENSION input_row,
                       JSAMPARRAY output_buf, int num_rows)
{
  void (*sse2fct)(JDIMENSION, JSAMPIMAGE, JDIMENSION, JSAMPARRAY, int);
  void (*avx2fct)(JDIMENSION, JSAMPIMAGE, JDIMENSION, JSAMPARRAY, int);

  switch(cinfo->out_color_space) {
    case JCS_EXT_RGB:
      sse2fct=jsimd_ycc_extrgb_convert_sse2;
      avx2fct=jsimd_ycc_extrgb_convert_avx2;
      break;
    case JCS_EXT_RGBX:
    case JCS_EXT_RGBA:
      sse2fct=jsimd_ycc_extrgbx_convert_sse2;
      avx2fct=jsimd_ycc_extrgbx_convert_avx2;
      break;
    case JCS_EXT_BGR:
      sse2fct=jsimd_ycc_extbgr_convert_sse2;
      avx2fct=jsimd_ycc_extbgr_convert_avx2;
      break;
    case JCS_EXT_BGRX:
    case JCS_EXT_BGRA:
      sse2fct=jsimd_ycc_extbgrx_convert_sse2;
      avx2fct=jsimd_ycc_extbgrx_convert_avx2;
      break;
    case JCS_EXT_XBGR:
    case JCS_EXT_ABGR:
      sse2fct=jsimd_ycc_extxbgr_convert_sse2;
      avx2fct=jsimd_ycc_extxbgr_convert_avx2;
      break;
    case JCS_EXT_XRGB:
    case JCS_EXT_ARGB:
      sse2fct=jsimd_ycc_extxrgb_convert_sse2;
      avx2fct=jsimd_ycc_extxrgb_convert_avx2;
      break;
    default:
      sse2fct=jsimd_ycc_rgb_convert_sse2;
      avx2fct=jsimd_ycc_rgb_convert_avx2;
      break;
  }

  if (simd_support & JSIMD_AVX2)
	  avx2fct(cinfo->output_width, input_buf, input_row, output_buf, num_rows);
  else if (simd_support & JSIMD_SSE2)
	  sse2fct(cinfo->output_width, input_buf, input_row, output_buf, num_rows);
}

GLOBAL(void)
jsimd_ycc_rgb565_convert (j_decompress_ptr cinfo,
                          JSAMPIMAGE input_buf, JDIMENSION input_row,
                          JSAMPARRAY output_buf, int num_rows)
{
}

GLOBAL(int)
jsimd_can_h2v2_downsample (void)
{
  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;

  init_simd();
   if (simd_support & JSIMD_SSE2)
    return 1;
  if (simd_support & JSIMD_AVX2)
	  return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_h2v1_downsample (void)
{
  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;

  init_simd();

  if (simd_support & JSIMD_SSE2)
    return 1;

  if (simd_support & JSIMD_AVX2)
    return 1;

  return 0;

}

GLOBAL(void)
jsimd_h2v2_downsample (j_compress_ptr cinfo, jpeg_component_info * compptr,
                       JSAMPARRAY input_data, JSAMPARRAY output_data)
{
  	if (simd_support & JSIMD_AVX2) 
		jsimd_h2v2_downsample_avx2(cinfo->image_width, cinfo->max_v_samp_factor,
				compptr->v_samp_factor, compptr->width_in_blocks,
				input_data, output_data);
	else if ( simd_support & JSIMD_SSE2)
		jsimd_h2v2_downsample_sse2(cinfo->image_width, cinfo->max_v_samp_factor,
				compptr->v_samp_factor, compptr->width_in_blocks,
				input_data, output_data);
}

GLOBAL(void)
jsimd_h2v1_downsample (j_compress_ptr cinfo, jpeg_component_info * compptr,
                       JSAMPARRAY input_data, JSAMPARRAY output_data)
{
	if (simd_support & JSIMD_AVX2) 
		jsimd_h2v1_downsample_avx2(cinfo->image_width, cinfo->max_v_samp_factor,
				compptr->v_samp_factor, compptr->width_in_blocks,
				input_data, output_data);
	else if ( simd_support & JSIMD_SSE2)
		jsimd_h2v1_downsample_sse2(cinfo->image_width, cinfo->max_v_samp_factor,
				compptr->v_samp_factor, compptr->width_in_blocks,
				input_data, output_data);
}

GLOBAL(int)
jsimd_can_h2v2_upsample (void)
{
  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;

	init_simd();

  if (simd_support & JSIMD_SSE2)
    return 1;
  if (simd_support & JSIMD_AVX2)
	  return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_h2v1_upsample (void)
{
  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;

  init_simd();
  if (simd_support & JSIMD_SSE2)
    return 1;
  if (simd_support & JSIMD_AVX2)
	  return 1;

  return 0;

}

GLOBAL(void)
jsimd_h2v2_upsample (j_decompress_ptr cinfo,
                     jpeg_component_info * compptr,
                     JSAMPARRAY input_data,
                     JSAMPARRAY * output_data_ptr)
{
	if (simd_support & JSIMD_AVX2)
		jsimd_h2v2_upsample_avx2(cinfo->max_v_samp_factor, cinfo->output_width,
				input_data, output_data_ptr);
	else if (simd_support & JSIMD_SSE2)
		jsimd_h2v2_upsample_sse2(cinfo->max_v_samp_factor, cinfo->output_width,
				input_data, output_data_ptr);
}

GLOBAL(void)
jsimd_h2v1_upsample (j_decompress_ptr cinfo,
                     jpeg_component_info * compptr,
                     JSAMPARRAY input_data,
                     JSAMPARRAY * output_data_ptr)
{
	if (simd_support & JSIMD_AVX2)
		jsimd_h2v1_upsample_avx2(cinfo->max_v_samp_factor, cinfo->output_width,
				input_data, output_data_ptr);
	else if (simd_support & JSIMD_SSE2)
		jsimd_h2v1_upsample_sse2(cinfo->max_v_samp_factor, cinfo->output_width,
				input_data, output_data_ptr);
}

GLOBAL(int)
jsimd_can_h2v2_fancy_upsample (void)
{
  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;

  init_simd();
  if (simd_support & JSIMD_SSE2)
    return 1;
  if (simd_support & JSIMD_AVX2)
	  return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_h2v1_fancy_upsample (void)
{
  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;

  init_simd();
  if (simd_support & JSIMD_SSE2)
    return 1;
  if (simd_support & JSIMD_AVX2)
	  return 1;

  return 0;
}

GLOBAL(void)
jsimd_h2v2_fancy_upsample (j_decompress_ptr cinfo,
                           jpeg_component_info * compptr,
                           JSAMPARRAY input_data,
                           JSAMPARRAY * output_data_ptr)
{
	if(simd_support & JSIMD_AVX2)
		jsimd_h2v2_fancy_upsample_avx2(cinfo->max_v_samp_factor,
				compptr->downsampled_width, input_data,
				output_data_ptr);

	else if(simd_support & JSIMD_SSE2)
		jsimd_h2v2_fancy_upsample_sse2(cinfo->max_v_samp_factor,
				compptr->downsampled_width, input_data,
				output_data_ptr);

}

GLOBAL(void)
jsimd_h2v1_fancy_upsample (j_decompress_ptr cinfo,
                           jpeg_component_info * compptr,
                           JSAMPARRAY input_data,
                           JSAMPARRAY * output_data_ptr)
{
	if(simd_support & JSIMD_AVX2)
		jsimd_h2v1_fancy_upsample_avx2(cinfo->max_v_samp_factor,
				compptr->downsampled_width, input_data,
				output_data_ptr);

	else if(simd_support & JSIMD_SSE2)
		jsimd_h2v1_fancy_upsample_sse2(cinfo->max_v_samp_factor,
				compptr->downsampled_width, input_data,
				output_data_ptr);
}

GLOBAL(int)
jsimd_can_h2v2_merged_upsample (void)
{
  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;

  init_simd();

  if (simd_support & JSIMD_SSE2)
    return 1;

  if (simd_support & JSIMD_AVX2)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_h2v1_merged_upsample (void)
{
  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;

  init_simd();
  if (simd_support&JSIMD_SSE2)
    return 1;

  if (simd_support & JSIMD_AVX2)
    return 1;

  return 0;
}

GLOBAL(void)
jsimd_h2v2_merged_upsample (j_decompress_ptr cinfo,
                            JSAMPIMAGE input_buf,
                            JDIMENSION in_row_group_ctr,
                            JSAMPARRAY output_buf)
{
  void (*sse2fct)(JDIMENSION, JSAMPIMAGE, JDIMENSION, JSAMPARRAY);
  void (*avx2fct)(JDIMENSION, JSAMPIMAGE, JDIMENSION, JSAMPARRAY);

  switch(cinfo->out_color_space) {
    case JCS_EXT_RGB:
      sse2fct=jsimd_h2v2_extrgb_merged_upsample_sse2;
      avx2fct=jsimd_h2v2_extrgb_merged_upsample_avx2;
      break;
    case JCS_EXT_RGBX:
    case JCS_EXT_RGBA:
      sse2fct=jsimd_h2v2_extrgbx_merged_upsample_sse2;
      avx2fct=jsimd_h2v2_extrgbx_merged_upsample_avx2;
      break;
    case JCS_EXT_BGR:
      sse2fct=jsimd_h2v2_extbgr_merged_upsample_sse2;
      avx2fct=jsimd_h2v2_extbgr_merged_upsample_avx2;
      break;
    case JCS_EXT_BGRX:
    case JCS_EXT_BGRA:
      sse2fct=jsimd_h2v2_extbgrx_merged_upsample_sse2;
      avx2fct=jsimd_h2v2_extbgrx_merged_upsample_avx2;
      break;
    case JCS_EXT_XBGR:
    case JCS_EXT_ABGR:
      sse2fct=jsimd_h2v2_extxbgr_merged_upsample_sse2;
      avx2fct=jsimd_h2v2_extxbgr_merged_upsample_avx2;
      break;
    case JCS_EXT_XRGB:
    case JCS_EXT_ARGB:
      sse2fct=jsimd_h2v2_extxrgb_merged_upsample_sse2;
      avx2fct=jsimd_h2v2_extxrgb_merged_upsample_avx2;
      break;
    default:
      sse2fct=jsimd_h2v2_merged_upsample_sse2;
      avx2fct=jsimd_h2v2_merged_upsample_avx2;
      break;
  }

  if (simd_support & JSIMD_AVX2)
	  avx2fct(cinfo->output_width, input_buf, in_row_group_ctr, output_buf);
  else if (simd_support & JSIMD_SSE2)
	  sse2fct(cinfo->output_width, input_buf, in_row_group_ctr, output_buf);
}

GLOBAL(void)
jsimd_h2v1_merged_upsample (j_decompress_ptr cinfo,
                            JSAMPIMAGE input_buf,
                            JDIMENSION in_row_group_ctr,
                            JSAMPARRAY output_buf)
{
  void (*sse2fct)(JDIMENSION, JSAMPIMAGE, JDIMENSION, JSAMPARRAY);
  void (*avx2fct)(JDIMENSION, JSAMPIMAGE, JDIMENSION, JSAMPARRAY);

  switch(cinfo->out_color_space) {
    case JCS_EXT_RGB:
      sse2fct=jsimd_h2v1_extrgb_merged_upsample_sse2;
      avx2fct=jsimd_h2v1_extrgb_merged_upsample_avx2;
      break;
    case JCS_EXT_RGBX:
    case JCS_EXT_RGBA:
      sse2fct=jsimd_h2v1_extrgbx_merged_upsample_sse2;
      avx2fct=jsimd_h2v1_extrgbx_merged_upsample_avx2;
      break;
    case JCS_EXT_BGR:
      sse2fct=jsimd_h2v1_extbgr_merged_upsample_sse2;
      avx2fct=jsimd_h2v1_extbgr_merged_upsample_avx2;
      break;
    case JCS_EXT_BGRX:
    case JCS_EXT_BGRA:
      sse2fct=jsimd_h2v1_extbgrx_merged_upsample_sse2;
      avx2fct=jsimd_h2v1_extbgrx_merged_upsample_avx2;
      break;
    case JCS_EXT_XBGR:
    case JCS_EXT_ABGR:
      sse2fct=jsimd_h2v1_extxbgr_merged_upsample_sse2;
      avx2fct=jsimd_h2v1_extxbgr_merged_upsample_avx2;
      break;
    case JCS_EXT_XRGB:
    case JCS_EXT_ARGB:
      sse2fct=jsimd_h2v1_extxrgb_merged_upsample_sse2;
      avx2fct=jsimd_h2v1_extxrgb_merged_upsample_avx2;
      break;
    default:
      sse2fct=jsimd_h2v1_merged_upsample_sse2;
      avx2fct=jsimd_h2v1_merged_upsample_avx2;
      break;
  }
  if (simd_support & JSIMD_AVX2)
	  avx2fct(cinfo->output_width, input_buf, in_row_group_ctr, output_buf);
  else if (simd_support & JSIMD_SSE2)
	  sse2fct(cinfo->output_width, input_buf, in_row_group_ctr, output_buf);
}

GLOBAL(int)
jsimd_can_convsamp (void)
{
  /* The code is optimised for these values only */
  if (DCTSIZE != 8)
    return 0;
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if (sizeof(DCTELEM) != 2)
    return 0;

  init_simd();
  if (simd_support & JSIMD_SSE2)
    return 1;
  if (simd_support & JSIMD_AVX2)
    return 1;


  return 0;
}

GLOBAL(int)
jsimd_can_convsamp_float (void)
{
  /* The code is optimised for these values only */
  if (DCTSIZE != 8)
    return 0;
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if (sizeof(FAST_FLOAT) != 4)
    return 0;

  init_simd();
  if (simd_support & JSIMD_SSE2) 
    return 1;
  if ((simd_support & JSIMD_AVX2))
    return 1;

  return 0;

}

GLOBAL(void)
jsimd_fdct_merged_ifast
(JSAMPARRAY sample_data, JDIMENSION start_col,JCOEFPTR coef_block, DCTELEM * divisors)
{
	DCTELEM * divisors_avx = divisors + 4*DCTSIZE2;
	jsimd_fdct_merged_ifast_avx2(sample_data,start_col,coef_block,divisors_avx);
}

GLOBAL(void)
jsimd_fdct_merged_islow
(JSAMPARRAY sample_data, JDIMENSION start_col,JCOEFPTR coef_block, DCTELEM* divisors)
{
	DCTELEM * divisors_avx = divisors + 4*DCTSIZE2;
	jsimd_fdct_merged_islow_avx2(sample_data,start_col,coef_block,divisors_avx);
}

GLOBAL(void)
jsimd_convsamp (JSAMPARRAY sample_data, JDIMENSION start_col,
                DCTELEM * workspace)
{
  jsimd_convsamp_sse2(sample_data, start_col, workspace);
}

GLOBAL(void)
jsimd_convsamp_float (JSAMPARRAY sample_data, JDIMENSION start_col,
                      FAST_FLOAT * workspace)
{
	if (simd_support & JSIMD_AVX2)
		jsimd_convsamp_float_avx2(sample_data, start_col, workspace);
	else if (simd_support & JSIMD_SSE2)
		jsimd_convsamp_float_sse2(sample_data, start_col, workspace);
}

GLOBAL(int)
jsimd_can_fdct_islow (void)
{
  /* The code is optimised for these values only */
  if (DCTSIZE != 8)
    return 0;
  if (sizeof(DCTELEM) != 2)
    return 0;

  init_simd();
  if (simd_support & JSIMD_SSE2)
    return 1;
  if (simd_support & JSIMD_AVX2)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_fdct_ifast (void)
{
  /* The code is optimised for these values only */
  if (DCTSIZE != 8)
    return 0;
  if (sizeof(DCTELEM) != 2)
    return 0;

  init_simd();
  if (simd_support & JSIMD_AVX2) 
	  return 1;

  if (simd_support & JSIMD_SSE2)
	  return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_fdct_float (void)
{
  /* The code is optimised for these values only */
  if (DCTSIZE != 8)
    return 0;
  if (sizeof(FAST_FLOAT) != 4)
    return 0;

  init_simd();
  if (simd_support & JSIMD_AVX2) 
	  return 1;

  if (simd_support & JSIMD_SSE2)
	  return 1;

  return 0;
}

GLOBAL(void)
jsimd_fdct_islow (DCTELEM * data)
{
  jsimd_fdct_islow_sse2(data);
}

GLOBAL(void)
jsimd_fdct_ifast (DCTELEM * data)
{
  jsimd_fdct_ifast_sse2(data);
}

GLOBAL(void)
jsimd_fdct_float (FAST_FLOAT * data)
{
  if ((simd_support & JSIMD_AVX2))
    jsimd_fdct_float_sse(data);
  else
    jsimd_fdct_float_sse(data);
}

GLOBAL(void)
jsimd_fdct_merged_float
(JSAMPARRAY sample_data, JDIMENSION start_col,JCOEFPTR coef_block, FAST_FLOAT * divisors)
{
  jsimd_fdct_merged_float_avx2(sample_data,start_col,coef_block,divisors);
}

GLOBAL(int)
jsimd_can_quantize (void)
{
  /* The code is optimised for these values only */
  if (DCTSIZE != 8)
    return 0;
  if (sizeof(JCOEF) != 2)
    return 0;
  if (sizeof(DCTELEM) != 2)
    return 0;

 init_simd();
  if (simd_support & JSIMD_AVX2) 
	  return 1;
  if (simd_support & JSIMD_SSE2)
	  return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_quantize_float (void)
{
  /* The code is optimised for these values only */
  if (DCTSIZE != 8)
    return 0;
  if (sizeof(JCOEF) != 2)
    return 0;
  if (sizeof(FAST_FLOAT) != 4)
    return 0;

  init_simd();
  if (simd_support & JSIMD_SSE2)
    return 1;
  if (simd_support & JSIMD_AVX2)
    return 1;
 
  return 0;
}

GLOBAL(void)
jsimd_quantize (JCOEFPTR coef_block, DCTELEM * divisors,
                DCTELEM * workspace)
{
  jsimd_quantize_sse2(coef_block, divisors, workspace);
}

GLOBAL(void)
jsimd_quantize_float (JCOEFPTR coef_block, FAST_FLOAT * divisors,
                      FAST_FLOAT * workspace)
{
	if (simd_support & JSIMD_AVX2)
		jsimd_quantize_float_avx2(coef_block, divisors, workspace);
	else if (simd_support & JSIMD_SSE2)
		jsimd_quantize_float_sse2(coef_block, divisors, workspace);
}

GLOBAL(int)
jsimd_can_idct_2x2 (void)
{
  /* The code is optimised for these values only */
  if (DCTSIZE != 8)
    return 0;
  if (sizeof(JCOEF) != 2)
    return 0;
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if (sizeof(ISLOW_MULT_TYPE) != 2)
    return 0;

  init_simd();
  if (simd_support & JSIMD_SSE2)
    return 1;
  if (simd_support & JSIMD_AVX2)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_idct_4x4 (void)
{
  /* The code is optimised for these values only */
  if (DCTSIZE != 8)
    return 0;
  if (sizeof(JCOEF) != 2)
    return 0;
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if (sizeof(ISLOW_MULT_TYPE) != 2)
    return 0;

  init_simd();
  if (simd_support & JSIMD_SSE2)
    return 1;
  if (simd_support & JSIMD_AVX2)
    return 1;

  return 0;
}

GLOBAL(void)
jsimd_idct_2x2 (j_decompress_ptr cinfo, jpeg_component_info * compptr,
                JCOEFPTR coef_block, JSAMPARRAY output_buf,
                JDIMENSION output_col)
{
  jsimd_idct_2x2_sse2(compptr->dct_table, coef_block, output_buf, output_col);
}

GLOBAL(void)
jsimd_idct_4x4 (j_decompress_ptr cinfo, jpeg_component_info * compptr,
                JCOEFPTR coef_block, JSAMPARRAY output_buf,
                JDIMENSION output_col)
{
  jsimd_idct_4x4_sse2(compptr->dct_table, coef_block, output_buf, output_col);
}

GLOBAL(int)
jsimd_can_idct_islow (void)
{
  /* The code is optimised for these values only */
  if (DCTSIZE != 8)
    return 0;
  if (sizeof(JCOEF) != 2)
    return 0;
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if (sizeof(ISLOW_MULT_TYPE) != 2)
    return 0;

  init_simd();
  if (simd_support & JSIMD_SSE2)
    return 1;
  if (simd_support & JSIMD_AVX2)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_idct_ifast (void)
{
  /* The code is optimised for these values only */
  if (DCTSIZE != 8)
    return 0;
  if (sizeof(JCOEF) != 2)
    return 0;
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if (sizeof(IFAST_MULT_TYPE) != 2)
    return 0;
  if (IFAST_SCALE_BITS != 2)
    return 0;

  init_simd();
  if (simd_support & JSIMD_SSE2)
	  return 1;
  if (simd_support & JSIMD_AVX2)
	  return 1;

    return 0;
}

GLOBAL(int)
jsimd_can_idct_float (void)
{
  if (DCTSIZE != 8)
    return 0;
  if (sizeof(JCOEF) != 2)
    return 0;
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if (sizeof(FAST_FLOAT) != 4)
    return 0;
  if (sizeof(FLOAT_MULT_TYPE) != 4)
    return 0;

  init_simd();
  if (simd_support & JSIMD_AVX2) 
	  return 1;
  if (simd_support & JSIMD_SSE2)
	  return 1;

  return 0;
}

GLOBAL(void)
jsimd_idct_islow (j_decompress_ptr cinfo, jpeg_component_info * compptr,
                  JCOEFPTR coef_block, JSAMPARRAY output_buf,
                  JDIMENSION output_col)
{
  jsimd_idct_islow_sse2(compptr->dct_table, coef_block, output_buf,
                        output_col);
}

GLOBAL(void)
jsimd_idct_ifast (j_decompress_ptr cinfo, jpeg_component_info * compptr,
                  JCOEFPTR coef_block, JSAMPARRAY output_buf,
                  JDIMENSION output_col)
{
  jsimd_idct_ifast_sse2(compptr->dct_table, coef_block, output_buf,
                        output_col);
}

GLOBAL(void)
jsimd_simd256_idct_islow (j_decompress_ptr cinfo, jpeg_component_info * compptr,
                  JCOEFPTR coef_block, JSAMPARRAY output_buf,
                  JDIMENSION output_col)
{
	/*
   * bit 0 of output_col isnot used in standard IDCT processing, 
	 * I use it for indicating single block IDCT transform
   * */
#define IDCT_IFAST_MAGIC_BIT 1
	if((output_col & IDCT_IFAST_MAGIC_BIT) == 0)
		jsimd_idct_islow_avx2(compptr->dct_table, coef_block, output_buf,
				output_col);
	else
		jsimd_idct_islow_sse2(compptr->dct_table, coef_block, output_buf,
				output_col&(JDIMENSION)(-2));
}

GLOBAL(void)
jsimd_simd256_idct_ifast (j_decompress_ptr cinfo, jpeg_component_info * compptr,
                  JCOEFPTR coef_block, JSAMPARRAY output_buf,
                  JDIMENSION output_col)
{
	/*
   * bit 0 of output_col isnot used in standard IDCT processing, 
	 * I use it for indicating single block IDCT transform
   * */
#define IDCT_IFAST_MAGIC_BIT 1
  if((output_col & IDCT_IFAST_MAGIC_BIT) == 0)
    jsimd_idct_ifast_avx2(compptr->dct_table, coef_block, output_buf,
        output_col);
  else
    jsimd_idct_ifast_sse2(compptr->dct_table, coef_block, output_buf,
        output_col&(JDIMENSION)(-2));
}

  GLOBAL(void)
jsimd_idct_float (j_decompress_ptr cinfo, jpeg_component_info * compptr,
                  JCOEFPTR coef_block, JSAMPARRAY output_buf,
                  JDIMENSION output_col)
{
	if(simd_support & JSIMD_AVX2)
		jsimd_idct_float_avx2(compptr->dct_table, coef_block, output_buf,
				output_col);
	else if (simd_support & JSIMD_SSE2)
		jsimd_idct_float_sse2(compptr->dct_table, coef_block, output_buf,
				output_col);
}

