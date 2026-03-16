
#include "filedialog.h"


namespace FileDialog
{
    nfdwindowhandle_t parentWindow;

    void init(GLFWwindow* mainWindow)
    {
        NFD_Init();
        NFD_GetNativeWindowFromGLFWWindow(mainWindow, &parentWindow);
    }

    void destroy()
    {
        NFD_Quit();
    }

    std::string openDialog(const std::string& filterName, const std::string& filterFileTypes)
    {
        nfdu8char_t* outPath;
        nfdu8filteritem_t filters[1] = { { filterName.c_str(), filterFileTypes.c_str()}};
        
        nfdopendialogu8args_t args = { 0 };
        args.filterList = filters;
        args.filterCount = 1;
        args.parentWindow = parentWindow;

        nfdresult_t result = NFD_OpenDialogU8_With(&outPath, &args);
        if (result == NFD_OKAY)
        {
            std::string result = std::string(outPath);
            NFD_FreePathU8(outPath);
            return result;
        }
        else if (result == NFD_CANCEL)
        {
            return "";
        }
    }

    std::string saveDialog(const std::string& defaultFileName)
    {
        nfdu8char_t* outPath;
        nfdu8filteritem_t filters[1] = { { "PNG Image", "png"}};

        nfdsavedialogu8args_t args = { 0 };
        args.defaultName = defaultFileName.c_str();
        args.filterCount = 1;
        args.filterList = filters;
        args.parentWindow = parentWindow;

        nfdresult_t result = NFD_SaveDialogU8_With(&outPath, &args);
        if (result == NFD_OKAY)
        {
            std::string result = std::string(outPath);
            NFD_FreePathU8(outPath);
            return result;
        }
        else if (result == NFD_CANCEL)
        {
            return "";
        }
    }
}
