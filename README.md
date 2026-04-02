# Fractal Flame Iterated Function System
This program is for generating simple fractal flames on the GPU using OpenCL. See here example images with config files: https://alexf13e.neocities.org/fractals/.

The program can be downloaded from the releases page: https://github.com/alexf13e/fractal-flame/releases.

See here for an explanation of how the images are produced: https://flam3.com/flame_draves.pdf.

<img width="1730" height="1276" alt="Screenshot_20260326_234609" src="https://github.com/user-attachments/assets/6072f7d0-b6a2-4ac9-8e52-fcadd0c65667" />


## Installation
Download either the Windows or Linux zip file from the releases page: https://github.com/alexf13e/fractal-flame/releases. Extract the contents of the zip file, then run the executable. You will probably get a smartscreen popup on Windows and I'm not important enough to stop that from appearing. Click run anyway if you trust me, or by all means build the program yourself. To uninstall, delete the folder the zip was extracted to.

For Linux:
* The program may not use the same GPU for OpenGL and OpenCL if you have more than one (e.g. in a laptop). If you have an Nvidia GPU, running `nvrun.sh` should make OpenGL and OpenCL use the same GPU. I do not have an AMD GPU available to test if a similar step is required for them.
* If you are using Wayland, I have been unable to get OpenGL and OpenCL to cooperate there, so it is currently unsupported (sorry).

## Usage
Note: the program can create flickering images when moving the camera or changing settings with sliders very slowly.

On startup, 3 random fractals are created. These can be edited and added to, with the results being gradually rendered to the screen as a preview. The "randomise" buttons at the top right are a good starting point for getting different shapes without worrying about every individual setting.

The "colour processing" section on the left can help if the image is looking to dark or bright. See the section below for more information on how each setting works.

Once a satisfactory image has been created, it can then be rendered to a png image file, optionally at a different resolution and sample count to the preview. The raw pixel data can also be saved if you wish to manually process it - see the render section below for more details on the file format.

Configuration files to reproduce a given image can be saved and loaded with the buttons in the top right. Some example ones are available here: https://alexf13e.neocities.org/fractals/.

### Controls
The keyboard or mouse can be used to move, rotate and scale the fractal. Rotating and scaling is always relative to the centre of the screen. Note that keyboard controls move the **camera**, i.e. pressing the `A` key will move the camera to the left, and therefore the fractal moves to the right.

Most boxes containing numbers can be clicked and dragged to change them, or hold `ctrl` and click once to type in a number. Holding `shift` while clicking and dragging will change the value 10x as fast, while holding `alt` will change it 10x slower. Colour inputs can have the small coloured box clicked to show a colour picker.

Note that typing a value directly with `ctrl + click`  allows for some inputs to have their restrictions bypassed. This may be useful, but use at your own risk.

### Settings
* Pause/Resume - pauses/resumes the accumulation of samples. The camera cannot be moved while paused, as moving the view requires re-rendering the fractal.
* Clear image - resets the preview, clearing all accumulated samples.
* Clear every frame - prevents samples from accumulating by resetting the preview buffer every frame.
* Faster plotting - improves performance when lots of points try to render to the same pixel at the same time but consequently losing the colour from some points (disables atomic addition).
* Sample threads - how many GPU threads run every frame to produce sample points, this should default to an optimal value. Lower values may increase frame rate slightly but not enough to compensate for the reduced samples and produce the image faster. Higher values will make the image appear "more" each frame, but will usually take longer to process each frame.
* Initial iterations - how many iterations should be applied to the sample point before it is rendered. This reduces noise from the random start point of the sample.
* Drawing iterations - how many iterations should be applied after the initial ones. The position of the sample point will be rendered after each of these iterations, and effectively just increases the number of samples per frame when initial iterations is sufficiently high (e.g. 20+).
* Max preview frames - how many frames should be rendered before stopping, set to 0 for infinite. This is useful to find where the image has enough samples for good visibility without being overexposed (and prevent the GPU from running at full speed forever).

### Camera
* Reset camera - resets the position of the camera to the centre of the world.
* Postion/zoom/angle - changes the respective values for the camera.
* Guidelines - draw an overlay with lines to help positioning the fractal within the frame. These lines are not drawn on the image when saving.

### Colour processing
The the three options here don't necessarily follow their traditional effects on the pixel values. I have used them in a way that I find made images which I preferred the look of. Intensity is similar to contrast, where larger values allow the colours to get a lot brighter and darker, however this means that images could easily become overexposed and too bright. With intensity set to 0, the colours can look quite flat and boring but will converge more nicely. There is no dedicated saturation slider, however the image appears more saturated with lower gamma (increase brightness to compensate as the image will become darker), and vice versa for reduced saturation.
The full colour processing algorithm is:
```
//pix.w is the number of samples which have landed on this pixel
if (pix.w <= 0) draw background and return

pix *= log10(pix.w) / pix.w //pix.w becomes log10(pix.w)
pix.xyz *= brightness

//temporary colours to be blended with intensity variable
flat = pow(pix.xyz, 1.0f / gamma)
intense = flat * pix.w * brightness

pix.xyz = intensity * intense + (1.0f - intensity) * flat
pix.w = 1.0f

final colour = clamp(pix, 0.0f, 1.0f)
```

### Render
* Render resolution - the size in pixels of the output file. This does not affect the preview, which uses the resolution of the window. NOTE: the brightness of a pixel is proportional to the amount of times a sample point is rendered to it. Higher resolutions have a lower chance of each pixel being rendered to, and are often darker. Compensate for this with the darkness slider or more samples.
* Match preview size - forces the output resolution to match the window resolution. Untick to set resolution manually.
* Number of frames - how many frames-worth of samples to render. Should be set higher when rendering higher resolution images as more samples are needed to cover all the pixels.
* Match current preview sample num - forces the number of samples in the rendered image to match how many samples have been calculated so far in the preview. Untick this to set the number of samples manually.
* Save as image - click to select a location to save the image, and then it will be rendered.
* Save unprocessed data - save the pixel data without applying colour processing or converting to an image format. The output file format begins with 8 bytes for the image dimensions - 4 byte `unsigned integers` for the width and height. The rest of the file consists of the pixel data, where each pixel component is stored in `RGBA` order, each as a 4 byte `float`. The purpose of this is to be able to load the values into your own custom processing solution (e.g. python script) to have more control over the processing.

### Info
Sometimes useful information will be displayed here. There may also be information in the terminal window, which is likely behind the main window.

### Variations
* Save flame config - saves the current set of variation settings to a file.
* Load flame config - loads a set of variation settings from a file.
* Previous flame - allows going back to the previous flame after loading from file or randomising (in case of accidentally going past a random flame you liked).
* Randomise [value] - randomises this value for each variation in the list. Useful for searching for nice shapes and colour schemes and forming a gambling addiction.
* Add variation - adds a variation with default settings.
* Variation - this is the main characteristic of the shape which will be formed, and refers to the list found here: https://flam3.com/flame_draves.pdf#page=16.
* Colour - the colour associated with the variation.
* Weight - affects the probability of this variation being chosen by a sample point. The chance of a variation being chosen is its weight divided by the sum of all weights. E.g. two variations with a weight of 1 will each have a 1/2 chance to be chosen, weights of 0.5 and 1 have 1/3 and 2/3 respectively.
* Rotation, translation and scale - transforms points (before applying the variation function) every time that variation is used. Rotation and scale use the translation position as the centre.
* Remove - remove this variation from the list.

## Build Dependencies
* glad - https://glad.dav1d.de/
* GLFW - https://www.glfw.org/
* glm - https://github.com/g-truc/glm
* ImGui - https://github.com/ocornut/imgui
* Native File Dialog Extended - https://github.com/btzy/nativefiledialog-extended
* OpenCL - https://github.com/KhronosGroup/OpenCL-SDK
* stb image - https://github.com/nothings/stb/tree/master

On Linux, chances are your package manager has some of these, e.g. `glfw` `glfw-devel` `glm` `nvidia-opencl` `mesa-opencl`
