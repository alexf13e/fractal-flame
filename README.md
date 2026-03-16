# Fractal Flame Iterated Function System
This program is for generating simple fractal flames, such as the examples below or in the [examples folder](https://github.com/alexf13e/fractal-flame/tree/main/examples)

<img width="1920" height="1080" alt="flame_3_12_14_18" src="https://github.com/user-attachments/assets/d2a91b60-2e28-4287-b770-e0e0b1c560c1" />
<img width="1920" height="1080" alt="flame_5_13_13" src="https://github.com/user-attachments/assets/b289945f-5467-4b38-a3ae-c8ba5a7125ce" />
<img width="1920" height="1080" alt="flame_5_2_2" src="https://github.com/user-attachments/assets/61f9010c-9ca7-40cf-a2f1-996cd46fb1a0" />

The program intends to make it as simple as possible to generate, preview and save images to a file. On startup, a set of 3 random variations, colours and weights are selected. These can be tweaked and added to, with the results being gradually rendered to the screen as a preview.
The fractal can then be re-rendered at the desired resolution and sample count, and the result saved to a file.

## Usage
The keys `WASD` can be used to pan the view around, and `QE` are used to zoom the view. Useful information may be shown in the terminal window, especially when saving and loading files. It may be hidden behind the main window when starting the program.
The image below shows an example set of variations after starting the program, and the meanings of the settings are as follows:
### Settings
* Samples per frame - how many sample points will be calculated every frame of the preview. Higher values make the fractal appear faster, but reduce the interactive frame rate
* Initial iterations - how many iterations should be applied to the sample point before it is rendered. This reduces noise from the random start point of the sample
* Iterations - how many iterations should be applied after the initial ones. The position of the sample point will be rendered after each of these iterations
* Gamma - the pixel value will be set to `pow(pixel, 1/gamma)` in a post processing step
* Darkness - the pixel value will be multiplied by `1/darkness` in a post processing step before gamma. "Darkness" is chosen as opposed to brightness, as the slider is nicer to control this way
* Clear every frame - prevents samples from accumulating by resetting the preview buffer every frame. Sometimes useful
* Clear image - resets the preview, clearing all accumulated samples
* Reset camera - resets the position of the camera to the centre of the world
* Pause - pauses the accumulation of samples. The camera cannot be moved while paused, as moving the view requires re-rendering the fractal
* Save flame config - saves the current set of variation settings to a file
* Load flame config - loads a set of variation settings from a file
* Previous flame - allows going back to the previous flame after randomising or loading from file (in case of accidentally going past a random flame you liked)

### Variations
This is this list of variations currently being applied to the sample points.
* Randomise [value] - randomises this value for each variation in the list. Useful for searching for nice shapes and colour schemes
* Variation - the numbers refer to the list found at the end of this document: https://flam3.com/flame_draves.pdf
* Colour - the colour associated with the variation
* Weight - affects the probability of this variation being chosen by a sample point. Variations with equal weight have equal probability of being chosen. The chance of a variation being chosen is its weight divided by the sum of all weights
* Remove - remove this variation from the list
* Add variation - adds a variation with default settings
<img width="469" height="1069" alt="image" src="https://github.com/user-attachments/assets/8af1c24d-ebff-4e1b-96db-f0c72f891a28" />



### Render
* Render resolution - the size in pixels of the output file. This does not affect the preview, which matches the resolution of the window. NOTE: the brightness of a pixel is proportional to the amount of times a sample point is rendered to it. Therefore, higher resolutions have a lower chance of each pixel being rendered to, and are often darker. Compensate for this with the darkness slider or more samples.
* Number of samples - the total number of samples which will be calculated for the rendered image
* Match current preview sample num - forces the number of samples in the rendered image to match how many samples have been calculated so far in the preview. Untick this to set the number of samples manually.
* Transparent background - renders the output with transparency. Otherwise a black background is set.
* Render - click to select a location to save the image, and then it will be rendered
<img width="405" height="235" alt="image" src="https://github.com/user-attachments/assets/eda24eeb-2c34-48c4-a61a-b9dc8299e3fb" />


## Build Dependencies
* GLFW - https://www.glfw.org/
* glad - https://glad.dav1d.de/
* ImGui - https://github.com/ocornut/imgui
* Native File Dialog Extended - https://github.com/btzy/nativefiledialog-extended

Thanks also to
* stb for saving images - https://github.com/nothings/stb/tree/master
* ProjectPhysX for method of writing kernel code - https://github.com/ProjectPhysX/OpenCL-Wrapper

## To do
* Some sort of denoising
* Figure out colour disparity between preview and render
