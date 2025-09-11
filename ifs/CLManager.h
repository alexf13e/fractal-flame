
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

#if defined(__linux__)
#include <CL/opencl.hpp>
#ifdef CL_MANAGER_GL
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_GLX
#endif
#elif defined(_WIN32)
#include "CL/cl.hpp"
#ifdef CL_MANAGER_GL
#define GLFW_EXPOSE_NATIVE_WGL
#endif
#endif

#ifdef CL_MANAGER_GL
#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"
#endif

#define WORKGROUP_SIZE 64
#define CL_MANAGER_ENABLE_TIMING false


namespace CLManager
{
#ifdef CL_MANAGER_GL
    struct CLGLBuffer
    {
        cl::BufferGL clBuffer;
        uint32_t glBuffer;
    };
#endif

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

    const std::string getErrorString(int error);

    bool createDevice();
    bool loadSources(const std::string& kernelSource);
    bool init(const std::string kernelSource);

    void createKernel(const std::string& kernelName, uint32_t range=0);
    void setKernelRange(const std::string& kernelName, uint32_t range);
    void setKernelParamBuffer(const std::string& kernelName, uint32_t argStartNum, std::initializer_list<std::string> bufferNames);
    void setKernelParamLocal(const std::string& kernelName, uint32_t argStartNum, uint32_t numBytes);
    template<class T> void setKernelParamValue(const std::string& kernelName, uint32_t argStartNum, const T& value);
    void runKernel(const std::string& kernelName);
    
    template<class T> bool createBuffer(const std::string& bufferName, const uint32_t numElements, const T* data = nullptr);
    template<class T> void readBuffer(const std::string& bufferName, const uint32_t numElements, const T* dest, const uint32_t offset=0);
    template<class T> void writeBuffer(const std::string& bufferName, const uint32_t numElements, const T* data, const uint32_t offset=0);
    template<class T> void fillBuffer(const std::string& bufferName, const uint32_t numElements, const T& value);
    template<class T> void copyBuffer(const std::string& src, const std::string& dst, const uint32_t srcOffset, const uint32_t dstOffset, const uint32_t numElements);
    bool deleteBuffer(const std::string& bufferName);
    float getTotalBufferMemUsageMB();
    
    void newTimingFrame();
    void updateKernelTimings(const std::string& kernelName);


#ifdef CL_MANAGER_IMPL

    cl::Device device;
    cl::Context context;
    cl::CommandQueue queue;
    cl::Program program;
    cl::NDRange rangeLocal;

    std::unordered_map<std::string, cl::Buffer> buffers;
    std::unordered_map<std::string, uint64_t> bufferMemUsage;

    std::unordered_map<std::string, cl::Kernel> kernels;
    std::unordered_map<std::string, cl::NDRange> kernelRanges;
#if CL_MANAGER_ENABLE_TIMING
    std::unordered_map<std::string, std::vector<KernelTimeData>> kernelTimings;
    std::vector<std::string> kernelPrintOrder;
    uint32_t timingHistorySize;
#endif

    const std::string getErrorString(int error)
    {
        switch (error) {
            // run-time and JIT compiler errors
        case 0: return "CL_SUCCESS";
        case -1: return "CL_DEVICE_NOT_FOUND";
        case -2: return "CL_DEVICE_NOT_AVAILABLE";
        case -3: return "CL_COMPILER_NOT_AVAILABLE";
        case -4: return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
        case -5: return "CL_OUT_OF_RESOURCES";
        case -6: return "CL_OUT_OF_HOST_MEMORY";
        case -7: return "CL_PROFILING_INFO_NOT_AVAILABLE";
        case -8: return "CL_MEM_COPY_OVERLAP";
        case -9: return "CL_IMAGE_FORMAT_MISMATCH";
        case -10: return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
        case -11: return "CL_BUILD_PROGRAM_FAILURE";
        case -12: return "CL_MAP_FAILURE";
        case -13: return "CL_MISALIGNED_SUB_BUFFER_OFFSET";
        case -14: return "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";
        case -15: return "CL_COMPILE_PROGRAM_FAILURE";
        case -16: return "CL_LINKER_NOT_AVAILABLE";
        case -17: return "CL_LINK_PROGRAM_FAILURE";
        case -18: return "CL_DEVICE_PARTITION_FAILED";
        case -19: return "CL_KERNEL_ARG_INFO_NOT_AVAILABLE";

            // compile-time errors
        case -30: return "CL_INVALID_VALUE";
        case -31: return "CL_INVALID_DEVICE_TYPE";
        case -32: return "CL_INVALID_PLATFORM";
        case -33: return "CL_INVALID_DEVICE";
        case -34: return "CL_INVALID_CONTEXT";
        case -35: return "CL_INVALID_QUEUE_PROPERTIES";
        case -36: return "CL_INVALID_COMMAND_QUEUE";
        case -37: return "CL_INVALID_HOST_PTR";
        case -38: return "CL_INVALID_MEM_OBJECT";
        case -39: return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
        case -40: return "CL_INVALID_IMAGE_SIZE";
        case -41: return "CL_INVALID_SAMPLER";
        case -42: return "CL_INVALID_BINARY";
        case -43: return "CL_INVALID_BUILD_OPTIONS";
        case -44: return "CL_INVALID_PROGRAM";
        case -45: return "CL_INVALID_PROGRAM_EXECUTABLE";
        case -46: return "CL_INVALID_KERNEL_NAME";
        case -47: return "CL_INVALID_KERNEL_DEFINITION";
        case -48: return "CL_INVALID_KERNEL";
        case -49: return "CL_INVALID_ARG_INDEX";
        case -50: return "CL_INVALID_ARG_VALUE";
        case -51: return "CL_INVALID_ARG_SIZE";
        case -52: return "CL_INVALID_KERNEL_ARGS";
        case -53: return "CL_INVALID_WORK_DIMENSION";
        case -54: return "CL_INVALID_WORK_GROUP_SIZE";
        case -55: return "CL_INVALID_WORK_ITEM_SIZE";
        case -56: return "CL_INVALID_GLOBAL_OFFSET";
        case -57: return "CL_INVALID_EVENT_WAIT_LIST";
        case -58: return "CL_INVALID_EVENT";
        case -59: return "CL_INVALID_OPERATION";
        case -60: return "CL_INVALID_GL_OBJECT";
        case -61: return "CL_INVALID_BUFFER_SIZE";
        case -62: return "CL_INVALID_MIP_LEVEL";
        case -63: return "CL_INVALID_GLOBAL_WORK_SIZE";
        case -64: return "CL_INVALID_PROPERTY";
        case -65: return "CL_INVALID_IMAGE_DESCRIPTOR";
        case -66: return "CL_INVALID_COMPILER_OPTIONS";
        case -67: return "CL_INVALID_LINKER_OPTIONS";
        case -68: return "CL_INVALID_DEVICE_PARTITION_COUNT";

            // extension errors
        case -1000: return "CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR";
        case -1001: return "CL_PLATFORM_NOT_FOUND_KHR";
        case -1002: return "CL_INVALID_D3D10_DEVICE_KHR";
        case -1003: return "CL_INVALID_D3D10_RESOURCE_KHR";
        case -1004: return "CL_D3D10_RESOURCE_ALREADY_ACQUIRED_KHR";
        case -1005: return "CL_D3D10_RESOURCE_NOT_ACQUIRED_KHR";
        default: return "Unknown OpenCL error: " + std::to_string(error);
        }
    }

    bool createDevice()
    {
        std::vector<cl::Platform> all_platforms;
        cl::Platform::get(&all_platforms);
        if (all_platforms.size() <= 0)
        {
            std::cout << "no platforms found, exiting" << std::endl;
            return false;
        }

        int error;
        bool contextSuccess;
        for (cl::Platform platform : all_platforms)
        {
            std::vector<cl::Device> platform_devices;
            platform.getDevices(CL_DEVICE_TYPE_ALL, &platform_devices);
            for (cl::Device d : platform_devices)
            {
                context = cl::Context(d, 0, 0, 0, &error);
                //std::cout << "  trying device " << d.getInfo<CL_DEVICE_NAME>() << ": " << getErrorString(error) << std::endl;

                if (error == CL_SUCCESS)
                {
                    contextSuccess = true;
                    device = d;
                    break;
                }
            }

            if (contextSuccess) break;
        }

        if (!contextSuccess)
        {
            std::cout << "failed to create CL context" << std::endl;
            return false;
        }

        std::cout << "Using device: " << device.getInfo<CL_DEVICE_NAME>() << std::endl;

        queue = cl::CommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &error);
        if (error != CL_SUCCESS)
        {
            std::cout << "error creating command queue: " << getErrorString(error) << std::endl;
            return false;
        }

        return true;
    }

    bool loadSources(const std::string& kernelSource)
    {
        cl::Program::Sources sources;
        sources.push_back({ kernelSource.c_str(), kernelSource.length() });
        program = cl::Program(context, sources);

        std::vector<cl::Device> d{ device };

        int error = program.build(d, "-cl-finite-math-only -cl-no-signed-zeros -cl-mad-enable -w");
        if (error != CL_SUCCESS)
        {
            std::cout << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device) << std::endl;
            std::cout << "failed to compile kernel code: " << getErrorString(error) << std::endl;
            return false;
        }

        return true;
    }

    bool init(const std::string kernelSource)
    {
        if (!createDevice()) return false;
        if (!loadSources(kernelSource)) return false;

        rangeLocal = cl::NDRange(WORKGROUP_SIZE);
    }

    void createKernel(const std::string& kernelName, uint32_t range)
    {
        kernels[kernelName] = cl::Kernel(program, kernelName.c_str());
        setKernelRange(kernelName, range);

#if CL_MANAGER_ENABLE_TIMING
        kernelTimings[kernelName] = std::vector<KernelTimeData>();
        kernelPrintOrder.push_back(kernelName);
#endif
    }

    void setKernelRange(const std::string& kernelName, uint32_t range)
    {
        kernelRanges[kernelName] = cl::NDRange(((range + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE) * WORKGROUP_SIZE);
    }

    void setKernelParamBuffer(const std::string& kernelName, uint32_t argStartNum, std::initializer_list<std::string> bufferNames)
    {
        for (const std::string& bufferName : bufferNames)
        {
            int error = kernels[kernelName].setArg(argStartNum, buffers[bufferName]);
            if (error != CL_SUCCESS)
            {
                std::cout << "error code " << getErrorString(error) << " setting kernel buffer parameter " <<
                    bufferName << " at position " << argStartNum << " in kernel " << kernelName << std::endl;
            }

            argStartNum++;
        }
    }

    void setKernelParamLocal(const std::string& kernelName, uint32_t argStartNum, uint32_t numBytes)
    {
        int error = kernels[kernelName].setArg(argStartNum, numBytes, NULL);
        if (error != CL_SUCCESS)
        {
            std::cout << "error code " << getErrorString(error) << " setting kernel local parameter at position " <<
                argStartNum << " in kernel " << kernelName << std::endl;
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

    void runKernel(const std::string& kernelName)
    {
#if CL_MANAGER_ENABLE_TIMING
        cl::Event* timingEvent = nullptr;
        if (kernelTimings.find(kernelName) != kernelTimings.end() && timingHistorySize > 0)
        {
            timingEvent = &kernelTimings[kernelName].back().timingEvent;
            kernelTimings[kernelName].back().timesRun++;
        }
        int error = queue.enqueueNDRangeKernel(kernels[kernelName], cl::NullRange, kernelRanges[kernelName], rangeLocal, nullptr, timingEvent);
#else
        int error = queue.enqueueNDRangeKernel(kernels[kernelName], cl::NullRange, kernelRanges[kernelName], rangeLocal);
#endif

        if (error != CL_SUCCESS)
        {
            std::cout << "error enqueueing kernel " << kernelName << ": " << getErrorString(error) << std::endl;
        }

        error = queue.finish();
        if (error != CL_SUCCESS)
        {
            std::cout << "error finishing queue running kernel " << kernelName << ": " << getErrorString(error) << std::endl;
        }

#if CL_MANAGER_ENABLE_TIMING
        updateKernelTimings(kernelName);
#endif
    }

    template<class T>
    bool createBuffer(const std::string& bufferName, const uint32_t numElements, const T* data)
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
    void readBuffer(const std::string& bufferName, const uint32_t numElements, const T* dest, const uint32_t offset)
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
    void writeBuffer(const std::string& bufferName, const uint32_t numElements, const T* data, const uint32_t offset)
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

    bool deleteBuffer(const std::string& bufferName)
    {
        if (buffers.find(bufferName) == buffers.end())
        {
            return false;
        }

        buffers.erase(bufferName);
        bufferMemUsage.erase(bufferName);
        return true;
    }

    float getTotalBufferMemUsageMB()
    {
        float totalMB = 0;
        for (auto& element : bufferMemUsage)
        {
            totalMB += element.second / (float)(1 << 20);
        }

        return totalMB;
    }

    void newTimingFrame()
    {
#if CL_MANAGER_ENABLE_TIMING
        for (auto& kt : kernelTimings)
        {
            if (timingHistorySize <= 0)
            {
                kt.second.clear();
            }
            else
            {
                if (kt.second.size() >= timingHistorySize) kt.second.erase(kt.second.begin(), kt.second.begin() + (kt.second.size() - timingHistorySize + 1));
                kt.second.push_back(KernelTimeData());
            }
        }
#endif
    }

    void updateKernelTimings(const std::string& kernelName)
    {
#if CL_MANAGER_ENABLE_TIMING
        if (timingHistorySize == 0) return;

        if (kernelTimings.find(kernelName) == kernelTimings.end())
        {
            std::cout << "tried to update unknown timer \"" << kernelName << "\"" << std::endl;
            return;
        }

        cl_ulong time_start;
        cl_ulong time_end;

        int error = kernelTimings[kernelName].back().timingEvent.getProfilingInfo(CL_PROFILING_COMMAND_START, &time_start);
        if (error)
        {
            std::cout << "error getting start time from event " << kernelName << ": " << std::to_string(error) << std::endl;
        }

        error = kernelTimings[kernelName].back().timingEvent.getProfilingInfo(CL_PROFILING_COMMAND_END, &time_end);
        if (error)
        {
            std::cout << "error getting end time from event " << kernelName << ": " << std::to_string(error) << std::endl;
        }

        double nanoSeconds = time_end - time_start;
        kernelTimings[kernelName].back().totalMilliseconds += nanoSeconds / 1e6;
#endif
    }

#endif //CL_MANAGER_IMPL

#ifdef CL_MANAGER_GL
    bool createDeviceWithGLContext(GLFWwindow* window);
    bool initWithGLContext(GLFWwindow* window, const std::string& kernelSource);
    void setKernelParamGLBuffer(const std::string& kernelName, uint32_t argStartNum, std::initializer_list<std::string> bufferNames);
    template<class T> bool createGLBuffer(const std::string& bufferName, const GLenum target, const uint32_t vao, const uint32_t numElements, T* data = nullptr);
    template<class T> bool createGLBufferNoVAO(const std::string& bufferName, const GLenum target, const uint32_t numElements, T* data = nullptr);
    template<class T> void readGLBuffer(const std::string& bufferName, const uint32_t numElements, const T* dest, const uint32_t offset = 0);
    template<class T> void copyGLBuffer(const std::string& src, const std::string& dst, const uint32_t srcOffset, const uint32_t dstOffset, const uint32_t numElements);

#ifdef CL_MANAGER_IMPL
    std::unordered_map<std::string, CLGLBuffer> glBuffers;

    bool createDeviceWithGLContext(GLFWwindow* window)
    {
        std::vector<cl::Platform> all_platforms;
        cl::Platform::get(&all_platforms);
        if (all_platforms.size() <= 0)
        {
            std::cout << "no platforms found, exiting" << std::endl;
            return false;
        }

        int error;
        bool contextSuccess = false;
        for (cl::Platform platform : all_platforms)
        {
            //std::cout << "trying platform " << platform.getInfo<CL_PLATFORM_NAME>() << std::endl;

#ifdef __linux__
            cl_context_properties contextProps[] = {
                CL_GL_CONTEXT_KHR, (cl_context_properties)glfwGetGLXContext(window),
                CL_GLX_DISPLAY_KHR, (cl_context_properties)glfwGetX11Display(),
                CL_CONTEXT_PLATFORM, (cl_context_properties)platform(),
                0
            };
#endif
#ifdef _WIN32
            cl_context_properties contextProps[] = {
                CL_GL_CONTEXT_KHR, (cl_context_properties)glfwGetWGLContext(window),
                CL_WGL_HDC_KHR, (cl_context_properties)wglGetCurrentDC(),
                CL_CONTEXT_PLATFORM, (cl_context_properties)platform(),
                0
            };
#endif

            std::vector<cl::Device> platform_devices;
            platform.getDevices(CL_DEVICE_TYPE_ALL, &platform_devices);
            for (cl::Device d : platform_devices)
            {
                context = cl::Context(d, contextProps, nullptr, nullptr, &error);
                //std::cout << "  trying device " << d.getInfo<CL_DEVICE_NAME>() << ": " << getErrorString(error) << std::endl;

                if (error == CL_SUCCESS)
                {
                    contextSuccess = true;
                    device = d;
                    break;
                }
            }

            if (contextSuccess) break;
        }

        if (!contextSuccess)
        {
            std::cout << "failed to create shared CL and GL context" << std::endl;
            return false;
        }

        std::cout << "Using device: " << device.getInfo<CL_DEVICE_NAME>() << std::endl;

        queue = cl::CommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &error);
        if (error != CL_SUCCESS)
        {
            std::cout << "error creating command queue: " << getErrorString(error) << std::endl;
            return false;
        }

        return true;
    }

    bool initWithGLContext(GLFWwindow* window, const std::string& kernelSource)
    {
        if (!createDeviceWithGLContext(window)) return false;
        if (!loadSources(kernelSource)) return false;

        rangeLocal = cl::NDRange(WORKGROUP_SIZE);

#if CL_MANAGER_ENABLE_TIMING
        timingHistorySize = 300;
#endif
        return true;
    }

    void setKernelParamGLBuffer(const std::string& kernelName, uint32_t argStartNum, std::initializer_list<std::string> bufferNames)
    {
        for (const std::string& bufferName : bufferNames)
        {
            int error = kernels[kernelName].setArg(argStartNum, glBuffers[bufferName].clBuffer);
            if (error != CL_SUCCESS)
            {
                std::cout << "error code " << getErrorString(error) << " setting kernel GL buffer parameter " <<
                    bufferName << " at position " << argStartNum << " in kernel " << kernelName << std::endl;
            }

            argStartNum++;
        }
    }

    template<class T>
    bool createGLBuffer(const std::string& bufferName, const GLenum target, const uint32_t vao,
        const uint32_t numElements, T* data)
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
        T* data)
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
    void readGLBuffer(const std::string& bufferName, const uint32_t numElements, const T* dest, const uint32_t offset)
    {
        glBindBuffer(GL_ARRAY_BUFFER, glBuffers[bufferName].glBuffer);
        glGetBufferSubData(GL_ARRAY_BUFFER, offset, numElements * sizeof(T), (void*)dest);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
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

#endif //CL_MANAGER_IMPL
#endif //CL_MANAGER_GL
}

#endif //CL_MANAGER_H
