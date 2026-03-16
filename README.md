# Fractal Flame Iterated Function System
This program is for generating simple fractal flames, such as the examples below

<img width="1920" height="1080" alt="flame_3_12_14_18" src="https://github.com/user-attachments/assets/d2a91b60-2e28-4287-b770-e0e0b1c560c1" />
<img width="1920" height="1080" alt="flame_5_13_13" src="https://github.com/user-attachments/assets/b289945f-5467-4b38-a3ae-c8ba5a7125ce" />
<img width="1920" height="1080" alt="flame_5_2_2" src="https://github.com/user-attachments/assets/61f9010c-9ca7-40cf-a2f1-996cd46fb1a0" />

The program intends to make it as simple as possible to generate, preview and save images to a file. On startup, a set of 3 random variations, colours and weights are selected. These can be tweaked and added to, with the results being gradually rendered to the screen as a preview.
The fractal can then be re-rendered at the desired resolution and sample count, and the result saved to a file.

## Usage
The keys `WASD` can be used to pan the view around, and `QE` are used to zoom the view. Useful information may be shown in the cmd window, especially when saving images.
The image below shows an example set of variations after starting the program, and the meaning of the settings are as follows:
### Settings
* Samples per frame - how many sample points will be calculated every frame of the preview. Higher values make the fractal appear faster, but reduce the interactive frame rate
* Initial iterations - how many iterations should be applied to the sample point before it is rendered. This reduces noise from the random start point of the sample
* Iterations - how many iterations should be applied after the initial ones. The position of the sample point will be rendered after each of these iterations
* Gamma - the pixel value will be set to `pow(pixel, 1/gamma)` in a post processing step
* Darkness - the pixel value will be multiplied by `1/darkness` in a post processing step before gamma. "Darkness" is chosen as opposed to brightness, as the slider is nicer to control this way
* Clear every frame - prevents samples from accumulating by resetting the preview buffer every frame. Sometimes useful
* Pause - pauses the accumulation of samples. The camera cannot be moved while paused, as moving the view requires re-rendering the fractal
* Clear image - resets the preview, clearing all accumulated samples

### Variations
This is this list of variations currently being applied to the sample points.
* Variation - the numbers refer to the list found at the end of this document: https://flam3.com/flame_draves.pdf
* LCh - the colour associated with the variation, in the LCh colour space (https://bottosson.github.io/posts/oklab/#the-oklab-color-space)
* Weight - affects the probability of this variation being chosen by a sample point. Variations with equal weight have equal probability of being chosen
* Remove - remove this variation from the list
* Add variation - adds a variation with default settings
* Randomise [value] - randomises this value for each variation in the list. Useful for searching for nice shapes and colour schemes
<img width="539" height="1073" alt="image" src="https://github.com/user-attachments/assets/3826bc68-3af0-4d15-a50b-ad2b81255fdd" />

### Render
* Render resolution - the size in pixels of the output file. This does not affect the preview, which matches the resolution of the window. NOTE: the brightness of a pixel is proportional to the amount of times a sample point is rendered to it. Therefore, higher resolutions have a lower chance of each pixel being rendered to, and are often darker. Compensate for this with the darkness slider.
* Number of samples - the total number of samples which will be calculated for the rendered image
* Match current preview sample num - forces the number of samples in the rendered image to match how many samples have been calculated so far in the preview. Untick this to set the number of samples manually.
* Transparent background - renders the output with transparency. Otherwise a black background is set.
* Render - click to select a location to save the image, and then it will be rendered
<img width="498" height="238" alt="image" src="https://github.com/user-attachments/assets/168dea0e-0e62-4915-a7f5-1fcbf8992e9b" />

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
