/*
 * Copyright (c) 2013 Carson McDonald
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy 
 * of this software and associated documentation files (the "Software"), to deal 
 * in the Software without restriction, including without limitation the rights 
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
 * of the Software, and to permit persons to whom the Software is furnished to do 
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all 
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS 
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR 
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN 
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION 
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <mruby.h>
#include <mruby/data.h>
#include <mruby/class.h>
#include <mruby/variable.h>
#include <mruby/string.h>
#include <mruby/hash.h>

#include <stdio.h>
#include <stdlib.h>

#include <jpeglib.h>

#define HASH_TRUE(mrb, hash, value) (mrb_obj_equal(mrb, mrb_hash_get(mrb, hash, mrb_symbol_value(mrb_intern_lit(mrb, value))), mrb_true_value()))

enum decompress_type {FILE_SRC, MEMORY_SRC};

const static JOCTET EOI_BUFFER[1] = { JPEG_EOI };

static void init_source(j_decompress_ptr dinfo) { }

static boolean fill_input_buffer(j_decompress_ptr dinfo)
{
  struct jpeg_source_mgr *source_mgr = (struct jpeg_source_mgr *)dinfo->src;
  source_mgr->next_input_byte = EOI_BUFFER;
  source_mgr->bytes_in_buffer = 1;
  return TRUE;
}

static void skip_input_data(j_decompress_ptr dinfo, long num_bytes)
{
  struct jpeg_source_mgr *source_mgr = (struct jpeg_source_mgr *)dinfo->src;

  if (source_mgr->bytes_in_buffer < num_bytes) 
  {
    source_mgr->next_input_byte = EOI_BUFFER;
    source_mgr->bytes_in_buffer = 1;
  } 
  else 
  {
    source_mgr->next_input_byte += num_bytes;
    source_mgr->bytes_in_buffer -= num_bytes;
  }
}

static void term_source(j_decompress_ptr dinfo) { }

static void
jpeg_error_func(j_common_ptr jpeg_ptr)
{
  struct jpeg_error_mgr *err;
  char buf[JMSG_LENGTH_MAX];
  mrb_state *mrb;

  jpeg_abort(jpeg_ptr);

  err = (struct jpeg_error_mgr *)jpeg_ptr->err;

  err->format_message(jpeg_ptr, buf);

  mrb = (mrb_state *)jpeg_ptr->client_data;
  mrb_raise(mrb, E_RUNTIME_ERROR, buf);
}

static void
jpeg_memory_src(j_decompress_ptr dinfo, const JOCTET *data, size_t data_size)
{
  struct jpeg_source_mgr *source_mgr = (struct jpeg_source_mgr *)(*dinfo->mem->alloc_small)((j_common_ptr)dinfo, JPOOL_PERMANENT, sizeof(struct jpeg_source_mgr));

  source_mgr->init_source = init_source;
  source_mgr->fill_input_buffer = fill_input_buffer;
  source_mgr->skip_input_data = skip_input_data;
  source_mgr->resync_to_restart = jpeg_resync_to_restart;
  source_mgr->term_source = term_source;

  source_mgr->next_input_byte = (const JOCTET *)data;
  source_mgr->bytes_in_buffer = data_size;

  dinfo->src = source_mgr;
}

static mrb_value
mrb_jpeg_decompress_common(mrb_state *mrb, mrb_value self, enum decompress_type dtype)
{
  struct jpeg_decompress_struct dinfo;
  struct jpeg_error_mgr jpeg_error;
  long scanline_size;
  void *jpeg_data;
  int row = 0;
  struct RClass* module_jpeg;
  struct RClass* class_jpeg_image;
  mrb_value ivar;

  mrb_value arg_config_hash = mrb_nil_value();
  mrb_value arg_data = mrb_nil_value();

  int argc = mrb_get_args(mrb, "S|H", &arg_data, &arg_config_hash);
  if (mrb_nil_p(arg_data) || mrb_type(arg_data) != MRB_TT_STRING) 
  {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "Invalid argument");
  }
  if(argc > 1 && mrb_type(arg_config_hash) != MRB_TT_HASH)
  {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "Invalid argument");
  }
  
  dinfo.client_data = mrb;

  dinfo.err = jpeg_std_error(&jpeg_error);
  dinfo.err->error_exit = jpeg_error_func;

  jpeg_create_decompress(&dinfo);

  if(dtype == FILE_SRC)
  {
    FILE *fp = fopen(RSTRING_PTR(arg_data), "r");
    if(fp == NULL)
    {
      jpeg_destroy_decompress(&dinfo);
      mrb_raise(mrb, E_RUNTIME_ERROR, "Could not open input file.");
    }
    jpeg_stdio_src(&dinfo, fp);
  }
  else
  {
    jpeg_memory_src(&dinfo, (const JOCTET *)RSTRING_PTR(arg_data), RSTRING_LEN(arg_data));
  }

  jpeg_read_header(&dinfo, 1); 

  if(!mrb_obj_equal(mrb, arg_config_hash, mrb_nil_value()) && HASH_TRUE(mrb, arg_config_hash, "force_grayscale"))
  {
    dinfo.output_components = 1;
  }
  else
  {
    if (dinfo.out_color_space == JCS_GRAYSCALE) 
    {
      dinfo.output_components = 1;
    }
    else 
    {
      dinfo.output_components = 3;
      dinfo.out_color_space = JCS_RGB;
    }
  }

  jpeg_start_decompress(&dinfo);

  scanline_size = dinfo.image_width * dinfo.output_components;
  jpeg_data = malloc(scanline_size * dinfo.image_height);

  for(row = 0; row < dinfo.image_height; row++)
  {
    JSAMPROW jpeg_row = (JSAMPROW)(jpeg_data + (scanline_size * row));
    jpeg_read_scanlines(&dinfo, &jpeg_row, 1);
  }

  module_jpeg = mrb_class_get(mrb, "JPEG");
  class_jpeg_image = mrb_class_ptr(mrb_const_get(mrb, mrb_obj_value(module_jpeg), mrb_intern_lit(mrb, "JPEGImage")));
  ivar = mrb_class_new_instance(mrb, 0, NULL, class_jpeg_image);
  mrb_iv_set(mrb, ivar, mrb_intern_lit(mrb, "data"), mrb_str_new(mrb, jpeg_data, scanline_size * dinfo.image_height));
  mrb_iv_set(mrb, ivar, mrb_intern_lit(mrb, "width"), mrb_fixnum_value(dinfo.image_width));
  mrb_iv_set(mrb, ivar, mrb_intern_lit(mrb, "height"), mrb_fixnum_value(dinfo.image_height));

  jpeg_finish_decompress(&dinfo);
  jpeg_destroy_decompress(&dinfo);

  return ivar;
}

static mrb_value
mrb_jpeg_decompress_data(mrb_state *mrb, mrb_value self)
{
  return mrb_jpeg_decompress_common(mrb, self, MEMORY_SRC);
}

static mrb_value
mrb_jpeg_decompress_file(mrb_state *mrb, mrb_value self)
{
  return mrb_jpeg_decompress_common(mrb, self, FILE_SRC);
}

static mrb_value
mrb_jpeg_width_get(mrb_state *mrb, mrb_value self)
{
  return mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "width"));
}

static mrb_value
mrb_jpeg_height_get(mrb_state *mrb, mrb_value self)
{
  return mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "height"));
}

static mrb_value
mrb_jpeg_data_get(mrb_state *mrb, mrb_value self)
{
  return mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "data"));
}

void
mrb_mruby_jpeg_gem_init(mrb_state* mrb) 
{
  struct RClass *class_jpeg_image;
  struct RClass *module_jpeg;

  module_jpeg = mrb_define_module(mrb, "JPEG");
  mrb_define_class_method(mrb, module_jpeg, "decompress_file", mrb_jpeg_decompress_file, ARGS_REQ(1));
  mrb_define_class_method(mrb, module_jpeg, "decompress_data", mrb_jpeg_decompress_data, ARGS_REQ(1));

  class_jpeg_image = mrb_define_class_under(mrb, module_jpeg, "JPEGImage", mrb->object_class);
  mrb_define_method(mrb, class_jpeg_image, "data", mrb_jpeg_data_get, ARGS_NONE());
  mrb_define_method(mrb, class_jpeg_image, "width", mrb_jpeg_width_get, ARGS_NONE());
  mrb_define_method(mrb, class_jpeg_image, "height", mrb_jpeg_height_get, ARGS_NONE());
}

void
mrb_mruby_jpeg_gem_final(mrb_state* mrb) 
{
}
