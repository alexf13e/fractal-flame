
#ifndef PNG_WRITER_H
#define PNG_WRITER_H

#include <cstdint>
#include <string>
#include <vector>


namespace PNGWriter
{
    namespace
    {
        void onPNGError();
    }

    void setRowBatchHeight(uint32_t rbh);
    bool getActive();
    bool begin(const std::string& filePath, const uint32_t fullImageWidth, const uint32_t fullImageHeight);
    bool writeData(std::vector<uint8_t>& data, const uint32_t numRows);
    bool end();
}



#endif //PNG_WRITER_H