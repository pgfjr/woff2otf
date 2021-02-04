#include "woff2ot.h"

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        puts(COPYRIGHT_NOTICE);

        puts("Usage 1: woff2ot input_font_filename.woff output_font_filename.[otf|ttf]\n");
        puts("Usage 2: woff2ot -type input_font_filename.woff\n");
        puts("         Use the latter to determine whether the actual font type is ");
        puts("         an OpenType PostScript (.OTF) or an OpenType TrueType (.TTF) font");
        return 1;
    }
    else if ('-' == argv[1][0])
    {
        if (strcmp(&argv[1][1], "type") == 0)
        {
            Woff2OT ot;
            uint32_t type;

            type = ot.write_font_type(argv[2]);

            // 0 == unknown type

            return (0 == type) ? 1 : 0;
        }
        else
        {
            printf("Unknown switch: %s\n", argv[1]);

            return 1;
        }
    }
    else
    {
        Woff2OT ot;

        if (ot.convert(argv[1], argv[2]))
        {
            puts("Success");

            return 0;
        }
        else
        {
            puts(ot.error().c_str());

            return 1;
        }
    }
}

