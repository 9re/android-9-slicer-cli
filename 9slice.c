#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <png.h>
#include <errno.h>

#define NINE_SLICE_ASSERT(cnd, ...)             \
  do                                            \
    {                                           \
     if (!(cnd))                                \
       {                                        \
         fprintf(stderr, __VA_ARGS__);          \
         exit(1);                               \
       }                                        \
    } while(0);

struct nine_slice_args
{
  int argc;
  char **argv;
  const char *program;
  const char *input;
  const char *output;
  png_uint_32 inset_top;
  png_uint_32 inset_left;
  png_uint_32 inset_bottom;
  png_uint_32 inset_right;
  png_uint_32 padding_top;
  png_uint_32 padding_left;
  png_uint_32 padding_bottom;
  png_uint_32 padding_right;
  int dpi;
};

struct nine_slice_png
{
  FILE *file_ptr;
  png_structp png_ptr;
  png_infop info_ptr;
  png_uint_32 width;
  png_uint_32 height;
  png_bytepp row_pointers;
};

void parse_numbers(int argc, char **argv, int *index, const char *param_name, int count, png_uint_32 **result)
{
  for (int j = 1; j <= count; ++j)
    {
      NINE_SLICE_ASSERT(*index + j < argc && argv[*index + j][0] != '-',
                        "too few argument for %s requires %d, but given %d!\n", param_name, count, j - 1);
      **(result++) = atoi(argv[*index + j]);
    }

  *index += count;
}

void scale_numbers(int count, float scale, png_uint_32 **numbers)
{
  for (int i = 0; i < count; ++i)
    {
      **(numbers + i) *= scale;
    }
}

void parse_args(int argc, char **argv, struct nine_slice_args *args)
{
  static const struct nine_slice_args args_zero = {0};

  *args = args_zero;
  args->argc = argc;
  args->argv = argv;
  args->program = *argv;
  args->output = "output.png";
  args->dpi = 160;

  png_uint_32 *insets[4] = {
    &args->inset_top,
    &args->inset_left,
    &args->inset_bottom,
    &args->inset_right
  };
  png_uint_32 *paddings[4] = {
    &args->padding_top,
    &args->padding_left,
    &args->padding_bottom,
    &args->padding_right
  };
  
  for (int i = 1; i < argc; i++)
    {
      if (argv[i][0] == '-')
        {
          switch (argv[i][1])
            {
            case 'o' :
              {
                NINE_SLICE_ASSERT(i + 1 < argc && argv[i + 1][0] != '-',
                                  "too few argument for output requires 1, but given 0!\n");
                args->output = argv[++i];
                break;
              }
            case 'i' :
              {
                parse_numbers(argc, argv, &i, "inset", 4, insets);
                break;
              }
            case 'p' :
              {
                parse_numbers(argc, argv, &i, "inset", 4, paddings);
                break;
              }
            case 'd' :
              {
                NINE_SLICE_ASSERT(i + 1 < argc && argv[i + 1][0] != '-',
                                  "too few argument for dpi requires 1, but given 0!\n");
                args->dpi = atoi(argv[++i]);
                break;
              }
            default :
              {
                NINE_SLICE_ASSERT(0, "unkown option %s!\n", argv[i]);
              }
            }
        }
      else
        {
          // naiive TODO
          args->input = argv[i];
        }
    }

  NINE_SLICE_ASSERT(args->input, "input not given!\n");

  // scale insets & paddings
  scale_numbers(4, (float)args->dpi / 160.0f, insets);
  scale_numbers(4, (float)args->dpi / 160.0f, paddings);
}

struct nine_slice_png read_png(const char *filename)
{
  struct nine_slice_png png = {0};
  
  FILE *fp = fopen(filename, "rb");
  NINE_SLICE_ASSERT(fp, "failed to open file '%s'!\n", filename);

  const unsigned char header[8];
  fread((void *)header, 1, 8, fp);
  bool is_png = !png_sig_cmp(header, 0, 8);
  NINE_SLICE_ASSERT(is_png, "the given file '%s' is not a png file!\n", filename);

  png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
  NINE_SLICE_ASSERT(png_ptr, "unable png_create_read_struct!\n");
  
  png_infop info_ptr  = png_create_info_struct(png_ptr);
  if (!info_ptr)
    {
      png_destroy_read_struct(&png_ptr, 0, 0);
      exit(1);
    }
  
  if (setjmp(png_jmpbuf(png_ptr)))
    {
      fprintf(stderr, "error occurred while reading!\n");
      png_destroy_read_struct(&png_ptr, &info_ptr, 0);
      if (fp)
        {
          fclose(fp);
        }
      exit(1);
    }

  png_init_io(png_ptr, fp);
  png_set_sig_bytes(png_ptr, 8);
  png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_EXPAND | PNG_TRANSFORM_GRAY_TO_RGB, 0);

  png.file_ptr = fp;
  png.png_ptr = png_ptr;
  png.info_ptr = info_ptr;
  png.width = png_get_image_width(png_ptr, info_ptr);
  png.height = png_get_image_height(png_ptr, info_ptr);  
  png.row_pointers = png_get_rows(png_ptr, info_ptr);

  return png;
}

void destroy_png_input(struct nine_slice_png *png)
{
  png_destroy_read_struct(&png->png_ptr, &png->info_ptr, 0);
  if (png->file_ptr)
    {
      fclose(png->file_ptr);
    }
}

struct nine_slice_png obtain_write_png(const char *filename, png_uint_32 width, png_uint_32 height)
{
  struct nine_slice_png png = {0};

  FILE *fp = fopen(filename, "wb");
  NINE_SLICE_ASSERT(fp, "couldn't open file %s for writing!\n", filename);

  png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
  NINE_SLICE_ASSERT(png_ptr, "png_create_write_struct failed!\n");

  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr)
    {
      png_destroy_write_struct(&png_ptr, 0);
      exit(1);
    }

  
  if (setjmp(png_jmpbuf(png_ptr)))
    {
      png_destroy_write_struct(&png_ptr, &info_ptr);
      fclose(fp);
      exit(1);
    }

  png_init_io(png_ptr, fp);

  png_bytepp row_pointers = png_malloc(png_ptr, height * (sizeof (png_bytep)));
  for (int i = 0; i < height; i++)
    {
      row_pointers[i] = 0;
    }

  for (int i = 0; i < height; i++)
    {
      row_pointers[i] = png_malloc(png_ptr, width * 4);
    }

  png_set_rows(png_ptr, info_ptr, row_pointers);
  png_set_IHDR(png_ptr, info_ptr, width, height,
               8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  
  png.file_ptr = fp;
  png.png_ptr = png_ptr;
  png.info_ptr = info_ptr;
  png.row_pointers = row_pointers;
  png.width = width;
  png.height = height;

  return png;
}

void destroy_png_output(struct nine_slice_png *png)
{
  for (int i = 0; i < png->height; ++i)
    {
      png_free(png->png_ptr, png->row_pointers[i]);
    }
  png_free(png->png_ptr, png->row_pointers);
  png_destroy_write_struct(&png->png_ptr, &png->info_ptr);
  if (png->file_ptr)
    {
      fclose(png->file_ptr);
    }
}

int main(int argc, char **argv)
{
  static png_byte black_point[4] = {0, 0, 0, 0xff};
  struct nine_slice_args args;

  parse_args(argc, argv, &args);

  struct nine_slice_png input = read_png(args.input);
  struct nine_slice_png output = obtain_write_png(args.output, input.width + 2, input.height + 2);

  // insets and paddings are non-negative hence passing '-' is treated as options!
  if (args.inset_top >= input.height ||
      args.inset_left >= input.width ||
      args.inset_bottom >= input.height ||
      args.inset_right >= input.width)
    {
      fprintf(stderr, "inset exceeds the range!\n");
      goto cleanup;
    }

  if (args.padding_top >= input.height ||
      args.padding_left >= input.width ||
      args.padding_bottom >= input.height ||
      args.padding_right >= input.width)
    {
      fprintf(stderr, "padding exceeds the range!\n");
      goto cleanup;
      }

  // copy original
  for (int i = 0; i < input.height; ++i)
    {
      memcpy(output.row_pointers[i + 1] + 4, input.row_pointers[i], input.width * 4);
    }

  for (png_uint_32 x = args.inset_left + 1; x < output.width - args.inset_right - 1; ++x)
    {
      memcpy(output.row_pointers[0] + x * 4, black_point, 4);
    }

  for (png_uint_32 y = args.inset_top + 1; y < output.height - args.inset_bottom - 1; ++y)
    {
      memcpy(output.row_pointers[y], black_point, 4);
    }

  for (png_uint_32 x = args.padding_left + 1; x < output.width - args.padding_right - 1; ++x)
    {
      memcpy(output.row_pointers[output.height - 1] + x * 4, black_point, 4);
    }
  
  for (png_uint_32 y = args.padding_top + 1; y < output.height - args.padding_bottom - 1; ++y)
    {
      memcpy(output.row_pointers[y] + (output.width - 1) * 4, black_point, 4);
    }

  png_write_png(output.png_ptr, output.info_ptr, 0, 0);

 cleanup:  
  destroy_png_input(&input);
  destroy_png_output(&output);
  
  return 0;
}
