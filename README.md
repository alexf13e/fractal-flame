# Fractal Flame Iterated Function System
This program is for generating simple fractal flames on the GPU using OpenCL. It can only do still images, and has a resolution limit dependent only on your GPU's VRAM.

The program can be downloaded from the releases page: https://github.com/alexf13e/fractal-flame/releases.

See here for example images with config files: https://alexf13e.neocities.org/fractals/.

See here for an explanation of how the images are produced: https://flam3.com/flame_draves.pdf.

Some more powerful programs by other people include:
* Apophysis: https://sourceforge.net/projects/apophysis7x/
* Chaotica: https://chaoticafractals.com/
* Fractorium: http://fractorium.com/
* Iterator.it: https://iterator.it/

<img width="1808" height="1238" alt="Screenshot_20260406_201115" src="https://github.com/user-attachments/assets/7c32bd92-72cc-4d82-b0bd-4005e8749a8b" />


## Installation
Download either the Windows or Linux zip file from the releases page: https://github.com/alexf13e/fractal-flame/releases. Extract the folder in the zip file, then run the executable. You will probably get a smartscreen popup on Windows and I'm not important enough to stop that from appearing. Click run anyway if you trust me, or by all means build the program yourself.

There is a Visual Studio solution provided for building on Windows, and some bash scripts for building on Linux with g++. Of course, you can build the program with whatever tools you wish; hopefully the included files give enough indication of what libraries are required (also see build dependencies at the end of the readme).

To uninstall, delete the folder the zip was extracted to. There is no proper install/uninstall process or additional files used by the program.

## Usage
Note: the program can create flickering images when moving the camera or changing settings with sliders very slowly.

On startup, 3 random fractals are created. These can be edited and added to, with the results being gradually rendered to the screen. The "randomise" buttons at the top right are a good starting point for getting different shapes without worrying about every individual setting.

The "colour processing" section on the left can help if the image is looking to dark or bright. See the section below for more information on how each setting works.

Once a satisfactory image has been created, it can then be rendered to a png image file, optionally at a different resolution and sample count to the preview. The raw pixel data can also be saved if you wish to manually process it - see the render section below for more details on the file format.

Configuration files to reproduce a given image can be saved and loaded with the buttons in the top right. Some examples are available here: https://alexf13e.neocities.org/fractals/.

For Linux:
* The program may not use the same GPU for OpenGL and OpenCL if you have more than one (e.g. in a laptop). If you have an Nvidia GPU, running `nvrun.sh` should make OpenGL and OpenCL use the same GPU. I do not have an AMD GPU available to test if a similar step is required for them.
* If you are using Wayland, I have been unable to get OpenGL and OpenCL to cooperate there, so it is currently unsupported (sorry).

### Controls
The keyboard or mouse can be used to move, rotate and scale the fractal. Rotating and scaling is always relative to the centre of the screen. Note that keyboard controls move the **camera**, e.g. pressing the `A` key will move the camera to the left, and therefore the fractal moves to the right.

Most boxes containing numbers can be clicked and dragged to change them, or hold `ctrl` and click once to type in a number. Holding `shift` while clicking and dragging will change the value 10x as fast, while holding `alt` will change it 10x slower. Colour inputs can have the small coloured box clicked to show a colour picker.

Note that typing a value directly with `ctrl + click`  allows for some inputs to have their restrictions bypassed. This may be useful, but use at your own risk.

### Settings
* Pause/Resume - pauses/resumes the accumulation of samples. The camera can be moved while paused, but will only move the view of the currently rendered pixels. This can be useful for reframing the image without it constantly having to re-render. Changing most other options will unpause as it is assumed changing properties of the fractal is done when the current properties are not longer wanted.
* Clear image - resets the preview, clearing all accumulated samples.
  * Changing pretty much anything besides the colour processing forces a clear as well.
  * While this may be annoying sometimes, the intent is for the same settings to always lead to the same image - changing settings part way through a render could lead to results that cannot be easily reproduced.
* Clear every frame - prevents samples from accumulating by resetting the preview buffer every frame.
* Faster plotting - improves performance when lots of samples try to render to the same pixel at the same time, but consequently loses the colour from some points (disables atomic addition).
* Sample threads - how many GPU threads run every frame to produce sample points, this should default to an optimal value.
  * Lower values may increase frame rate slightly but not enough to compensate for the reduced samples and produce the image faster.
  * Higher values will make the image appear "more" each frame, but will usually take longer to process each frame.
* Initial iterations - how many iterations should be applied to the sample point before it is rendered. This reduces noise from the random start point of the sample.
* Drawing iterations - how many iterations should be applied after the initial ones, with the sample position drawn after each iteration.. 
  * Effectively just increases the number of samples per frame when initial iterations is sufficiently high (e.g. 20+).
* Max preview frames - how many frames should be rendered before stopping, set to 0 for infinite.
  * This is useful to find where the image has enough samples for good visibility without being overexposed (and to prevent the GPU from running at full speed forever).

### Camera
* Reset camera - resets the position of the camera to the centre of the world.
* Postion/zoom/angle - changes the respective values for the camera. The world moves/rotates in the opposite direction to the camera.
* Guidelines - draw an overlay with lines to help positioning the fractal within the frame. These lines are not drawn on the image when saving.

### Colour processing
The the three options here don't necessarily follow their traditional effects on the pixel values. I have used them in a way that I find made images which I preferred the look of.
* Brightness - the pixel values are always multiplied by this, and higher `intensity` values are also affected by `brightness` values.
* Intensity - similar to contrast, where larger values allow the colours to get a lot brighter and darker.
  * When set to 1, there is (in my opinion) a nicer range of colours and contrast, however images can easily become overexposed and too bright.
  * When set to 0, the colours can look quite flat and boring but will converge more nicely.
* Gamma - pixel values are raised to the power of `1/gamma`

There is no dedicated saturation slider, however the image appears more saturated with lower gamma (increase brightness to compensate as the image will become darker), and vice versa for reduced saturation.

The full colour processing algorithm is provided below to hopefully make it clearer what these options do. Prior to this step, a pixel's value will have the colour of a variation added to its `xyz` values, and 1 added to its `z` value.
```
//pix.w is the number of samples which have landed on this pixel
if (pix.w <= 0) draw background and return

//scale colour magnitude logarithmically as more samples land on the same pixel
pix *= log10(pix.w) / pix.w
pix.xyz *= brightness

//temporary colours to be blended with intensity variable
flat = pow(pix.xyz, 1.0f / gamma)
intense = flat * pix.w * brightness

pix.xyz = intensity * intense + (1.0f - intensity) * flat
pix.w = 1.0f

result = clamp(pix, 0.0f, 1.0f)
```

### Render
* Match preview - the resolution and number of samples will match the preview to produce the same image.
* Render resolution - the size in pixels of the output file.
  * This does not affect the preview, which uses the resolution of the window.
  * NOTE: the brightness of a pixel is proportional to the amount of times a sample point is rendered to it. Higher resolutions have a lower chance of each pixel being rendered to, so are often darker. Compensate for this with the darkness slider or more samples.
* Number of frames - how many frames-worth of samples to render.
  * Should be set higher when rendering higher resolution images as more samples are needed to cover all the pixels.
* Save as image - click to select a location to save the image, and then it will be rendered.
* Save unprocessed data - save the pixel data without applying colour processing or converting to an image format.
  * The output file format begins with 8 bytes for the image dimensions - 4 byte `unsigned integers` for the width and height. The rest of the file consists of the pixel data, where each pixel component is stored in `RGBA` order, each as a 4 byte `float`.
  * The purpose of this is to be able to load the values into your own custom processing solution (e.g. python script) to have more control over the final colours.

### Info
Sometimes useful information will be displayed here. There may also be information in the terminal window, which is likely behind the main window.

### Variations
* Save flame config - saves the current set of variation settings to a file.
* Load flame config - loads a set of variation settings from a file.
* Previous flame - allows going back to the previous flame after loading from file or randomising (in case of accidentally going past a random flame you liked).
* Randomise [value] - randomises this value for each variation in the list. Useful for searching for nice shapes and colour schemes and forming a gambling addiction.
* Add variation - adds a variation with default settings.
* Variation - this is the main characteristic of the shape which will be formed, and refers to the list found here: https://flam3.com/flame_draves.pdf#page=16.
* Colour - the colour associated with the variation. May look different in the image due to colour processing settings.
* Weight - affects the probability of this variation being chosen by a sample point. The chance of a variation being chosen is its weight divided by the sum of all weights. E.g. two variations with a weight of 1 will each have a 1/2 chance to be chosen, weights of 0.5 and 1 have 1/3 and 2/3 respectively.
* Rotation, translation and scale - transforms points after applying the variation function (i.e. post-transform) every time that variation is used.
  * Rotation and scale use the translation position as the centre.
  * Pre-transforms are currently not implemented (for pretty much no reason other than UI design/simplification).
* Remove - remove this variation from the list.

There is a limit of 64 variations at once. This is fairly arbitrary (a maximum of some sort is required, and I honestly just haven't bothered to write code to query GPU information to figure out the real maximum), however above more than 5 or so I find it quite difficult to make anything coherent. The maximum is set quite far beyond that for those who are more capable than me. There is very little (if any) performance impact from the amount of variations being used.

## Build Dependencies
* glad - https://glad.dav1d.de/
* GLFW - https://www.glfw.org/
* glm - https://github.com/g-truc/glm
* ImGui - https://github.com/ocornut/imgui
* Native File Dialog Extended - https://github.com/btzy/nativefiledialog-extended
* OpenCL - https://github.com/KhronosGroup/OpenCL-SDK
* stb image - https://github.com/nothings/stb/tree/master

On Linux, chances are your package manager has some of these, e.g. `glfw` `glm` `nvidia-opencl` `mesa-opencl`
