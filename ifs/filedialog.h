
#ifndef FILE_DIALOG_H
#define FILE_DIALOG_H

#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#include <nfd_glfw3.h>

namespace FileDialog
{
    extern nfdwindowhandle_t parentWindow;

    void init(GLFWwindow* mainWindow);
    void destroy();
    std::string openDialog(const std::vector<nfdu8filteritem_t>& filters);
    std::string saveDialog(const std::string& defaultFileName, const std::vector<nfdu8filteritem_t>& filters);
}



#endif // !FILE_DIALOG_H
