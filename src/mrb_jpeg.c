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

#include <jpeglib.h>

#define HASH_TRUE(mrb, hash, value) (mrb_obj_equal(mrb, mrb_hash_get(mrb, hash, mrb_symbol_value(mrb_intern(mrb, value))), mrb_true_value()))

static void
jpeg_error_func(j_common_ptr jpeg_ptr)
{
  jpeg_abort(jpeg_ptr);

  struct jpeg_error_mgr *err = (struct jpeg_error_mgr *)jpeg_ptr->err;
  char buf[JMSG_LENGTH_MAX];

  err->format_message(jpeg_ptr, buf);

  mrb_state *mrb = (mrb_state *)jpeg_ptr->client_data;
  mrb_raise(mrb, E_RUNTIME_ERROR, buf);
}

static mrb_value
mrb_jpeg_read(mrb_state *mrb, mrb_value self)
{
  mrb_value arg_config_hash = mrb_nil_value();
  mrb_value arg_filename = mrb_nil_value();

  int argc = mrb_get_args(mrb, "S|H", &arg_filename, &arg_config_hash);
  if (mrb_nil_p(arg_filename) || mrb_type(arg_filename) != MRB_TT_STRING) 
  {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "Invalid argument");
  }
  if(argc > 1 && mrb_type(arg_config_hash) != MRB_TT_HASH)
  {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "Invalid argument");
  }
  
  struct jpeg_decompress_struct dinfo;
  dinfo.client_data = mrb;

  struct jpeg_error_mgr jpeg_error;
  dinfo.err = jpeg_std_error(&jpeg_error);
  dinfo.err->error_exit = jpeg_error_func;

  FILE *fp = fopen(RSTRING_PTR(arg_filename), "r");
  if(fp == NULL)
  {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Could not open input file.");
  }
  jpeg_create_decompress(&dinfo);
  jpeg_stdio_src(&dinfo, fp);

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

  long scanline_size = dinfo.image_width * dinfo.output_components;
  void *jpeg_data = malloc(scanline_size * dinfo.image_height);

  int row = 0;
  for(row = 0; row < dinfo.image_height; row++)
  {
    JSAMPROW jpeg_row = (JSAMPROW)(jpeg_data + (scanline_size * row));
    jpeg_read_scanlines(&dinfo, &jpeg_row, 1);
  }

  struct RClass* module_jpeg = mrb_class_get(mrb, "JPEG");
  struct RClass* class_jpeg_image = mrb_class_ptr(mrb_const_get(mrb, mrb_obj_value(module_jpeg), mrb_intern(mrb, "JPEGImage")));
  mrb_value ivar = mrb_class_new_instance(mrb, 0, NULL, class_jpeg_image);
  mrb_iv_set(mrb, ivar, mrb_intern(mrb, "data"), mrb_str_new(mrb, jpeg_data, scanline_size * dinfo.image_height));
  mrb_iv_set(mrb, ivar, mrb_intern(mrb, "width"), mrb_fixnum_value(dinfo.image_width));
  mrb_iv_set(mrb, ivar, mrb_intern(mrb, "height"), mrb_fixnum_value(dinfo.image_height));

  jpeg_finish_decompress(&dinfo);
  jpeg_destroy_decompress(&dinfo);

  return ivar;
}

static mrb_value
mrb_jpeg_width_get(mrb_state *mrb, mrb_value self)
{
  return mrb_iv_get(mrb, self, mrb_intern(mrb, "width"));
}

static mrb_value
mrb_jpeg_height_get(mrb_state *mrb, mrb_value self)
{
  return mrb_iv_get(mrb, self, mrb_intern(mrb, "height"));
}

static mrb_value
mrb_jpeg_data_get(mrb_state *mrb, mrb_value self)
{
  return mrb_iv_get(mrb, self, mrb_intern(mrb, "data"));
}

void
mrb_mruby_jpeg_gem_init(mrb_state* mrb) 
{
  struct RClass *module_jpeg = mrb_define_module(mrb, "JPEG");
  mrb_define_class_method(mrb, module_jpeg, "read", mrb_jpeg_read, ARGS_REQ(1));

  struct RClass *class_jpeg_image = mrb_define_class_under(mrb, module_jpeg, "JPEGImage", mrb->object_class);
  mrb_define_method(mrb, class_jpeg_image, "data", mrb_jpeg_data_get, ARGS_NONE());
  mrb_define_method(mrb, class_jpeg_image, "width", mrb_jpeg_width_get, ARGS_NONE());
  mrb_define_method(mrb, class_jpeg_image, "height", mrb_jpeg_height_get, ARGS_NONE());
}

void
mrb_mruby_jpeg_gem_final(mrb_state* mrb) 
{
}
