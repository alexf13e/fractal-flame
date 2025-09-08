
#ifndef FILE_DIALOG_H
#define FILE_DIALOG_H

#include <string>

#include <GLFW/glfw3.h>
#include <nfd_glfw3.h>

namespace FileDialog
{
    extern nfdwindowhandle_t parentWindow;

    void init(GLFWwindow* mainWindow);
    void destroy();
    std::string openDialog(const std::string& filterName, const std::string& filterFileTypes);
    std::string saveDialog(const std::string& defaultFileName);
}



#endif // !FILE_DIALOG_H
