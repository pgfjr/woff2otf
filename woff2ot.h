#pragma once

/*
    Woff2OT: WOFF to OpenType (PostScript/TrueType) converter
    Copyright (c) 2020 by Peter Frane Jr.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.


    The author may be contacted via the e-mail address pfranejr@hotmail.com
*/

#include <stdexcept>
#include <string>
#include <cstdio>
#include <vector>
#include <stdint.h>
#include <stdlib.h>
#include <zlib.h>


using namespace std;

#pragma comment(lib, "zdll.lib")

#define COPYRIGHT_NOTICE "Woff2OT v. 1.0\nCopyright (c) 2000 P.D. Frane Jr.\n"

#ifdef _MSC_VER
#define bswap16(x) _byteswap_ushort(x)
#define bswap32(x) _byteswap_ulong(x)
#else
#define bswap16(x) __builtin_bswap16(x)
#define bswap32(x) __builtin_bswap32(x)
#endif

const uint32_t OPENTYPE_TRUETYPE = 0x00010000;
const uint32_t OPENTYPE_TRUETYPE_MAC = 0x74727565;
const uint32_t OPENTYPE_CFF = 0x4F54544F;

typedef uint32_t offset32;
typedef unsigned char byte_t;

struct WOFF_header
{
    uint32_t m_signature{ 0 };	//0x774F4646 'wOFF'
    uint32_t m_flavor{ 0 };	//	The "sfnt version" of the input font.
    uint32_t m_length{ 0 };	//	Total size of the WOFF file.
    uint16_t m_num_tables{ 0 };	//	Number of entries in directory of font tables.
    uint16_t m_reserved{ 0 };	//	Reserved; set to zero.
    uint32_t m_total_sfnt_size{ 0 };	//	Total size needed for the uncompressed font data, including the sfnt header, directory, and font tables(including padding).
    uint16_t m_major_version{ 1 };	//	Major version of the WOFF file.
    uint16_t m_minor_version{ 0 };	//	Minor version of the WOFF file.
    uint32_t m_meta_offset{ 0 };	//	Offset to metadata block, from beginning of WOFF file.
    uint32_t m_meta_length{ 0 };	//	Length of compressed metadata block.
    uint32_t m_meta_orig_length{ 0 };	//	Uncompressed size of metadata block.
    uint32_t m_priv_offset{ 0 };	//	Offset to private data block, from beginning of WOFF file.
    uint32_t m_priv_length{ 0 };	//	Length of private data block.
};

struct table_directory_entry
{
    uint32_t	tag{ 0 };
    uint32_t	offset{ 0 };
    uint32_t	comp_length{ 0 };//	Length of the compressed data, excluding padding.
    uint32_t	orig_length{ 0 };//	Length of the uncompressed table, excluding padding.
    uint32_t	orig_checksum{ 0 };

};

struct offset_table
{
    uint32_t sfntVersion;
    uint16_t numTables;
    uint16_t searchRange;
    uint16_t entrySelector;
    uint16_t rangeShift;
};

struct table_record
{
    uint32_t table_tag{ 0 };
    uint32_t checksum{ 0 };
    offset32 offset{ 0 };
    uint32_t length{ 0 };
};

class Woff2OT
{
    FILE* m_input_file{ nullptr };
    FILE* m_output_file{ nullptr };
    string m_error;
    void clear()
    {
        if (m_input_file)
        {
            fclose(m_input_file);
        }
        if (m_output_file)
        {
            fclose(m_output_file);
        }

        m_input_file = m_output_file = nullptr;
    }
    void load_input_file(const char* woff_file)
    {
        m_input_file = fopen(woff_file, "rb");

        if (!m_input_file)
        {
            throw runtime_error("Unable to load the WOFF font file");
        }
    }
    void create_output_file(const char* ot_file)
    {
        m_output_file = fopen(ot_file, "wb");

        if (!m_output_file)
        {
            throw runtime_error("Unable to create the OpenType file");
        }
    }
    uint32_t compute_table_sizes(table_directory_entry* tbl_directory_entry_data, uint32_t num_tables)
    {
        uint32_t total_size = 0;

        for (uint16_t i = 0; i < num_tables; ++i)
        {
            uint32_t size = bswap32(tbl_directory_entry_data[i].orig_length);

            total_size += (size + 3) & 0xFFFFFFFC;
        }

        return total_size;
    }
    void write_temp_table_record(uint16_t num_tables)
    {
        table_record record;

        fwrite(&record, 1, num_tables * sizeof(record), m_output_file);
    }
    uint32_t get_max_buffer_size(table_directory_entry* tbl_directory_entry_data, uint16_t num_tables)
    {
        uint32_t size = 0;

        for (uint16_t i = 0; i < num_tables; ++i)
        {
            uint32_t length = bswap32(tbl_directory_entry_data[i].orig_length);
            uint32_t offset = bswap32(tbl_directory_entry_data[i].offset);
            uint32_t comp_length = bswap32(tbl_directory_entry_data[i].comp_length);

            tbl_directory_entry_data[i].orig_length = length;
            tbl_directory_entry_data[i].offset = offset;
            tbl_directory_entry_data[i].comp_length = comp_length;

            size = length > size ? length : size;
        }

        return size;
    }
    void pad_table(byte_t* buffer, uint32_t length, uint32_t padded_length)
    {
        for (uint32_t i = length; i < padded_length; ++i)
        {
            buffer[i] = 0;
        }
    }
    static int compare_offset(const void* a, const void* b)
    {
        table_directory_entry* i = (table_directory_entry*)a;
        table_directory_entry* j = (table_directory_entry*)b;

        return i->offset - j->offset;
    }
    /*
    void dump_tables(table_directory_entry* tbl_directory_entry_data, uint32_t num_tables)
    {
        for (uint16_t i = 0; i < num_tables; ++i)
        {
            printf("%.*s %u\n", 4, (char*)&tbl_directory_entry_data[i].tag, tbl_directory_entry_data[i].offset);
        }
    }
    */
    void write_tables(table_directory_entry* tbl_directory_entry_data, uint32_t num_tables)
    {
        uint32_t buffer_size = get_max_buffer_size(tbl_directory_entry_data, num_tables) + 4;
        vector<Bytef> buf1(buffer_size);
        vector<Bytef> buf2(buffer_size);
        byte_t* original_data = buf1.data();
        byte_t* compressed_data = buf2.data();

        qsort(tbl_directory_entry_data, num_tables, sizeof(*tbl_directory_entry_data), compare_offset);

        for (uint16_t i = 0; i < num_tables; ++i)
        {
            table_directory_entry& entry = tbl_directory_entry_data[i];
            uint32_t offset = entry.offset;
            uLongf orig_length = entry.orig_length;
            uLongf compressed_length = entry.comp_length;
            int ret;

            fseek(m_input_file, offset, SEEK_SET);

            entry.offset = bswap32(ftell(m_output_file));

            if (compressed_length < orig_length)
            {
                uLongf dest_len = buffer_size;
                uLongf new_dest_len;

                fread(compressed_data, 1, compressed_length, m_input_file);

                ret = uncompress(original_data, &dest_len, compressed_data, compressed_length);

                if (ret != Z_OK)
                {
                    throw runtime_error("Error decompressing the table");
                }
                else if (dest_len != orig_length)
                {
                    throw runtime_error("Decompressed length does not match the original length");
                }

                new_dest_len = (dest_len + 3) & 0xFFFFFFFC;

                pad_table(original_data, dest_len, new_dest_len);

                fwrite(original_data, 1, new_dest_len, m_output_file);

                entry.orig_length = bswap32(dest_len);
            }
            else
            {
                uLongf new_length = (orig_length + 3) & 0xFFFFFFFC;

                fread(original_data, 1, orig_length, m_input_file);

                pad_table(original_data, orig_length, new_length);

                fwrite(original_data, 1, new_length, m_output_file);

                entry.orig_length = bswap32(orig_length);
            }
        }
    }
    static int compare_tag(const void* a, const void* b)
    {
        table_directory_entry* i = (table_directory_entry*)a;
        table_directory_entry* j = (table_directory_entry*)b;

        return strncmp(((char*)&i->tag), ((char*)&j->tag), 4);
    }
    void rewrite_table_record(long offset, table_directory_entry* tbl_directory_entry_data, uint32_t num_tables)
    {
        table_record record;

        qsort(tbl_directory_entry_data, num_tables, sizeof(*tbl_directory_entry_data), compare_tag);

        fseek(m_output_file, offset, SEEK_SET);

        for (uint16_t i = 0; i < num_tables; ++i)
        {
            table_directory_entry& entry = tbl_directory_entry_data[i];

            record.checksum = entry.orig_checksum;
            record.length = entry.orig_length;
            record.offset = entry.offset;
            record.table_tag = entry.tag;

            fwrite(&record, 1, sizeof(record), m_output_file);
        }
    }
    uint16_t max_power_of_2(uint16_t num_tables)
    {
        for (int i = num_tables; i >= 1; --i)
        {
            if ((i & (i - 1)) == 0)
                return i;
        }
        return 0;
    }
    void write_ot_header(uint32_t flavor, uint16_t num_tables)
    {
        uint16_t mp2 = max_power_of_2(num_tables);
        offset_table header;

        header.sfntVersion = flavor;
        header.numTables = bswap16(num_tables);
        header.searchRange = bswap16(mp2 * 16);
        header.entrySelector = bswap16((uint16_t)log2(mp2));
        header.rangeShift = bswap16(num_tables * 16 - (mp2 * 16));

        fwrite(&header, 1, sizeof(header), m_output_file);
    }
    void parse_input_file()
    {
        WOFF_header hdr;

        if (fread(&hdr, 1, sizeof(hdr), m_input_file) != sizeof(hdr))
        {
            throw runtime_error("Error reading the input file");
        }
        else
        {
            uint16_t num_tables = bswap16(hdr.m_num_tables);
            long offset;

            if (num_tables > 0)
            {
                vector<table_directory_entry> tbl_directory_entry(num_tables);
                table_directory_entry* tbl_directory_entry_data = tbl_directory_entry.data();
                uint32_t total_sfnt_size = bswap32(hdr.m_total_sfnt_size);
                uint32_t size_to_read = num_tables * sizeof(*tbl_directory_entry_data);
                uint32_t total_table_size;
                uint32_t computed_sfnt_size;

                // write the OT header

                write_ot_header(hdr.m_flavor, num_tables);

                offset = ftell(m_output_file);

                write_temp_table_record(num_tables);

                if (fread(tbl_directory_entry_data, 1, size_to_read, m_input_file) != size_to_read)
                {
                    throw runtime_error("Error reading the table directory entries");
                }

                total_table_size = compute_table_sizes(tbl_directory_entry_data, num_tables);

                computed_sfnt_size = sizeof(offset_table) + (num_tables * sizeof(table_record)) + total_table_size;

                if (total_sfnt_size != computed_sfnt_size)
                {
                    throw runtime_error("Invalid 'totalSfntSize' size");
                }

                write_tables(tbl_directory_entry_data, num_tables);

                rewrite_table_record(offset, tbl_directory_entry_data, num_tables);
            }
            else
            {
                throw runtime_error("No tables found");
            }
        }
    }

public:
    Woff2OT() : m_error()
    {}
    ~Woff2OT()
    {}
    string error() const
    {
        return m_error;
    }
    uint32_t get_font_type(const char* woff_file)
    {
        uint32_t result = 0;

        try
        {
            WOFF_header hdr;

            load_input_file(woff_file);

            if (fread(&hdr, 1, sizeof(hdr), m_input_file) != sizeof(hdr))
            {
                throw runtime_error("Error reading the input file");
            }

            result = bswap32(hdr.m_flavor);
        }
        catch (const exception& ex)
        {
            m_error = ex.what();
        }

        clear();

        return result;
    }
    uint32_t write_font_type(const char* woff_file)
    {
        uint32_t type;

        type = get_font_type(woff_file);

        switch (type)
        {
        case OPENTYPE_TRUETYPE:
            puts("Font type is OpenType TrueType (.TTF)");
            break;
        case OPENTYPE_TRUETYPE_MAC:
            puts("Font type is OpenType TrueType for Mac (.TTF)");
            break;
        case OPENTYPE_CFF:
            puts("Font type is OpenType PostScript (.OTF)");
            break;
        default:
            puts("Font type is unknown or font is not a WOFF font");
            break;
        }

        return type;
    }
    bool convert(const char* woff_file, const char* ot_file)
    {
        bool result = true;
        try
        {
            load_input_file(woff_file);
            create_output_file(ot_file);
            parse_input_file();
        }
        catch (const exception& ex)
        {
            m_error = ex.what();

            result = false;
        }

        clear();

        return result;
    }  
};