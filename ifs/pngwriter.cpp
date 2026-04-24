
#include "pngwriter.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>

#include <png.h>


namespace PNGWriter
{
    namespace
    {
        png_struct* pngData;
        png_info* pngInfo;
        FILE* fp;
        std::string outputFilePath;
        uint32_t imageWidth;
        bool active = false;
        bool dataWritten = false;

        void onPNGError()
        {
            std::cout << "PNGWriter encountered error" << std::endl;
            png_destroy_write_struct(&pngData, &pngInfo);
            if (fp != NULL) fclose(fp);
        }
    }

    bool getActive()
    {
        return active;
    }

    bool begin(const std::string& filePath, const uint32_t fullImageWidth, const uint32_t fullImageHeight)
    {
        if (active)
        {
            std::cout << "tried to begin writing multiple PNGs at once; only one is supported" << std::endl;
            return false;
        }

        active = true;
        dataWritten = false;

        // if (setjmp(png_jmpbuf(pngData)))
        // {
        //     onPNGError();
        //     return false;
        // }

        pngData = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        pngInfo = png_create_info_struct(pngData);

        fp = fopen(filePath.c_str(), "wb");
        outputFilePath = filePath;
        png_init_io(pngData, fp);

        png_set_user_limits(pngData, INT32_MAX, INT32_MAX);
        png_set_IHDR(pngData, pngInfo, fullImageWidth, fullImageHeight, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
        
        imageWidth = fullImageWidth;

        png_write_info(pngData, pngInfo);

        return true;
    }

    bool writeData(std::vector<uint8_t>& data, const uint32_t numRows)
    {
        if (!active)
        {
            std::cout << "tried to write PNG data when no file was active" << std::endl;
            return false;
        }

        if (data.size() % imageWidth != 0 || data.size() / 4 / numRows != imageWidth)
        {
            std::cout << "tried to write PNG data which was not a full number of rows" << std::endl;
            return false;
        }

        // if (setjmp(png_jmpbuf(pngData)))
        // {
        //     onPNGError();
        //     return false;
        // }

        std::vector<png_byte*> rowPointers(numRows);
        for (uint32_t r = 0; r < numRows; r++)
        {
            rowPointers[r] = &data[r * imageWidth * 4];
        }

        png_write_rows(pngData, rowPointers.data(), numRows);

        dataWritten = true;

        return true;
    }

    bool end()
    {
        if (!active)
        {
            std::cout << "tried to finish writing PNG when no file was active" << std::endl;
            return false;
        }

        // if (setjmp(png_jmpbuf(pngData)))
        // {
        //     onPNGError();
        //     return false;
        // }

        if (dataWritten)
        {
            png_write_end(pngData, pngInfo);
            png_destroy_write_struct(&pngData, &pngInfo);
            fclose(fp);
        }
        else
        {
            png_destroy_write_struct(&pngData, &pngInfo);
            fclose(fp);
            std::remove(outputFilePath.c_str());
        }
        
        active = false;

        return true;
    }
}