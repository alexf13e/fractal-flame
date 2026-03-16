
#ifndef CL_MANAGER_H
#define CL_MANAGER_H

#include <iostream>
#include <unordered_map>
#include <string>
#include <initializer_list>
#include <vector>

#define CL_HPP_ENABLE_EXCEPTIONS
#define CL_HPP_TARGET_OPENCL_VERSION 120
#define CL_HPP_MINIMUM_OPENCL_VERSION 120

#include "glad/glad.h"
#include "GLFW/glfw3.h"

#if defined(__linux__)
#include <CL/opencl.hpp>
#elif defined(_WIN32)
#include "CL/cl.hpp"
#endif


#define WORKGROUP_SIZE 64
#define CL_MANAGER_ENABLE_TIMING false

namespace CLManager
{
    struct CLGLBuffer
    {
        cl::BufferGL clBuffer;
        uint32_t glBuffer;
    };

    struct KernelTimeData
    {
        cl::Event timingEvent;
        float totalMilliseconds;
        uint32_t timesRun;

        KernelTimeData()
        {
            timingEvent = cl::Event();
            totalMilliseconds = 0;
            timesRun = 0;
        }
    };

    extern cl::Device device;
    extern cl::Context context;
    extern cl::CommandQueue queue;
    extern cl::Program program;
    extern cl::NDRange rangeLocal;

    extern std::unordered_map<std::string, cl::Buffer> buffers;
    extern std::unordered_map<std::string, CLGLBuffer> glBuffers;
    extern std::unordered_map<std::string, uint64_t> bufferMemUsage;

    extern std::unordered_map<std::string, cl::Kernel> kernels;
    extern std::unordered_map<std::string, cl::NDRange> kernelRanges;
#if CL_MANAGER_ENABLE_TIMING
    extern std::unordered_map<std::string, std::vector<KernelTimeData>> kernelTimings;
    extern std::vector<std::string> kernelPrintOrder;
    extern uint32_t timingHistorySize;
#endif

    const std::string getErrorString(int error);
    bool createDevice(GLFWwindow* window);
    bool loadSources(const std::string& kernelSource);
    bool init(GLFWwindow* window, const std::string& kernelSource);
    void setKernelRange(const std::string& kernelName, uint32_t range);
    void setKernelParamBuffer(const std::string& kernelName, uint32_t argStartNum, std::initializer_list<std::string> bufferNames);
    void setKernelParamGLBuffer(const std::string& kernelName, uint32_t argStartNum, std::initializer_list<std::string> bufferNames);
    void setKernelParamLocal(const std::string& kernelName, uint32_t argStartNum, uint32_t numBytes);
    void createKernel(const std::string& kernelName, uint32_t range=0);
    void runKernel(const std::string& kernelName);
    float getTotalBufferMemUsageMB();
    bool deleteBuffer(const std::string& bufferName);
    void newTimingFrame();
    void updateKernelTimings(const std::string& kernelName);

    template<class T>
    void readBuffer(const std::string& bufferName, const uint32_t numElements, const T* dest, const uint32_t offset=0)
    {
        int error = queue.enqueueReadBuffer(buffers[bufferName], true, offset * sizeof(T), numElements * sizeof(T),
            (void*)dest);
        if (error != CL_SUCCESS)
        {
            std::cout << "error reading buffer " << bufferName << ": " << getErrorString(error) << std::endl;
            std::cout << "attempted to read " << std::to_string(numElements * sizeof(T)) << " bytes at byte offset "
                << std::to_string(offset * sizeof(T)) << " from buffer holding " << bufferMemUsage[bufferName]
                << " bytes" << std::endl;
        }

        error = queue.finish();
        if (error != CL_SUCCESS)
        {
            std::cout << "error finishing queue reading buffer " << bufferName << ": " << getErrorString(error)
                << std::endl;
        }
    }

    template<class T>
    void writeBuffer(const std::string& bufferName, const uint32_t numElements, const T* data, const uint32_t offset=0)
    {
        int error = queue.enqueueWriteBuffer(buffers[bufferName], true, offset * sizeof(T), numElements * sizeof(T),
            (void*)data);
        if (error != CL_SUCCESS)
        {
            std::cout << "error writing buffer " << bufferName << ": " << getErrorString(error) << std::endl;
        }

        error = queue.finish();
        if (error != CL_SUCCESS)
        {
            std::cout << "error finishing queue writing buffer " << bufferName << ": " << getErrorString(error)
                << std::endl;
        }
    }

    template<class T>
    void fillBuffer(const std::string& bufferName, const uint32_t numElements, const T& value)
    {
        int error = queue.enqueueFillBuffer(buffers[bufferName], value, 0, numElements * sizeof(T));
        if (error != CL_SUCCESS)
        {
            std::cout << "error filling buffer " << bufferName << " of size " << numElements << " with value "
                << value << ": " << getErrorString(error) << std::endl;
        }

        error = queue.finish();
        if (error != CL_SUCCESS)
        {
            std::cout << "error finishing queue filling buffer " << bufferName << " of size " << numElements
                << " with value " << value << ": " << getErrorString(error) << std::endl;
        }
    }

    template<class T>
    bool createBuffer(const std::string& bufferName, const uint32_t numElements, const T* data=nullptr)
    {
        //if buffer already exists, will be automatically deleted when existing cl::Buffer goes out of scope
        int error = 0;
        buffers[bufferName] = cl::Buffer(context, CL_MEM_READ_WRITE, numElements * sizeof(T), nullptr, &error);

        if (error != CL_SUCCESS)
        {
            std::cout << "error creating buffer: " << bufferName << " with " << numElements << " elements: "
                << getErrorString(error) << std::endl;
            return false;
        }

        bufferMemUsage[bufferName] = numElements * sizeof(T);

        if (data != nullptr)
        {
            writeBuffer(bufferName, numElements, data);
        }
        else
        {
            fillBuffer<T>(bufferName, numElements, (T)0);
        }

        return true;
    }

    template<class T>
    void copyBuffer(const std::string& src, const std::string& dst, const uint32_t srcOffset, const uint32_t dstOffset,
        const uint32_t numElements)
    {
        int error = queue.enqueueCopyBuffer(buffers[src], buffers[dst], srcOffset * sizeof(T), dstOffset * sizeof(T),
            numElements * sizeof(T));
        if (error != CL_SUCCESS)
        {
            std::cout << "error enqueueing copying buffer " << src << "[" << srcOffset << ":" << numElements << "] to "
                << dst << "[" << dstOffset << ":" << numElements << "]: " << getErrorString(error)
                << std::endl;
        }

        error = queue.finish();
        if (error != CL_SUCCESS)
        {
            std::cout << "error finishing queue copying buffer " << src << "[" << srcOffset << ":" << numElements <<
                "] to " << dst << "[" << dstOffset << ":" << numElements << "]: " << getErrorString(error)
                << std::endl;
        }
    }

    template<class T>
    void readGLBuffer(const std::string& bufferName, const uint32_t numElements, const T* dest, const uint32_t offset = 0)
    {
        glBindBuffer(GL_ARRAY_BUFFER, glBuffers[bufferName].glBuffer);
        glGetBufferSubData(GL_ARRAY_BUFFER, offset, numElements * sizeof(T), (void*)dest);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    template<class T>
    bool createGLBuffer(const std::string& bufferName, const GLenum target, const uint32_t vao,
        const uint32_t numElements, T* data = nullptr)
    {
        glBindVertexArray(vao);

        if (glBuffers.find(bufferName) == glBuffers.end())
        {
            //buffer doesn't exist so create
            glBuffers[bufferName] = CLGLBuffer();
            glGenBuffers(1, &(glBuffers[bufferName].glBuffer));
        }

        glBindBuffer(target, glBuffers[bufferName].glBuffer);
        glBufferData(target, numElements * sizeof(T), data, GL_STATIC_DRAW);
        glBindVertexArray(0);

        int error = 0;
        glBuffers[bufferName].clBuffer = cl::BufferGL(context, CL_MEM_READ_WRITE, glBuffers[bufferName].glBuffer,
            &error);

        if (error != CL_SUCCESS)
        {
            std::cout << "error creating GL buffer " << bufferName << ": " << getErrorString(error) << std::endl;
            return false;
        }

        bufferMemUsage[bufferName] = numElements * sizeof(T);

        return true;
    }

    template<class T>
    bool createGLBufferNoVAO(const std::string& bufferName, const GLenum target, const uint32_t numElements,
        T* data = nullptr)
    {
        if (glBuffers.find(bufferName) == glBuffers.end())
        {
            //buffer doesn't exist so create
            glBuffers[bufferName] = CLGLBuffer();
            glGenBuffers(1, &(glBuffers[bufferName].glBuffer));
        }

        glBindBuffer(target, glBuffers[bufferName].glBuffer);
        glBufferData(target, numElements * sizeof(T), data, GL_STATIC_DRAW);

        int error = 0;
        glBuffers[bufferName].clBuffer = cl::BufferGL(context, CL_MEM_READ_WRITE, glBuffers[bufferName].glBuffer,
            &error);

        if (error != CL_SUCCESS)
        {
            std::cout << "error creating GL buffer " << bufferName << ": " << getErrorString(error) << std::endl;
            return false;
        }

        bufferMemUsage[bufferName] = numElements * sizeof(T);

        return true;
    }

    template<class T>
    void copyGLBuffer(const std::string& src, const std::string& dst, const uint32_t srcOffset, const uint32_t dstOffset,
        const uint32_t numElements)
    {
        glBindBuffer(GL_COPY_READ_BUFFER, glBuffers[src].glBuffer);
        glBindBuffer(GL_COPY_WRITE_BUFFER, glBuffers[dst].glBuffer);
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, srcOffset, dstOffset, numElements * sizeof(T));
        int err = glGetError();
        if (err != GL_NO_ERROR)
        {
            std::cout << "error copying GL buffer: " << err << std::endl;
        }
    }

    template<class T>
    void setKernelParamValue(const std::string& kernelName, uint32_t argStartNum, const T& value)
    {
        int error = kernels[kernelName].setArg(argStartNum, sizeof(T), (void*)&value);
        if (error != CL_SUCCESS)
        {
            std::cout << "error code " << getErrorString(error) << " setting kernel value parameter at position "
                << argStartNum << " in kernel " << kernelName << std::endl;
        }
    }
}

#endif
