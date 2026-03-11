# Fractal Flame Iterated Function System
This program is for generating simple fractal flames, such as the examples below or in the examples folder: https://github.com/alexf13e/fractal-flame/tree/main/examples.

The program can be downloaded from the releases page: https://github.com/alexf13e/fractal-flame/releases.

See here for an explanation of how the images are produced: https://flam3.com/flame_draves.pdf.


| <img width="3840" height="2160" alt="10000000_13_3" src="https://github.com/user-attachments/assets/549a5a72-1c5e-4f12-9005-1046ec3a4c2d" /> | <img width="3840" height="2160" alt="18480000_16_4_1" src="https://github.com/user-attachments/assets/25e7e38b-6f99-4fb5-a783-e3656819ed7a" /> |
|-|-|
| <img width="3840" height="2160" alt="50000000_7_13" src="https://github.com/user-attachments/assets/2c93cb42-e010-40b7-8377-360f1daf654e" /> | <img width="1920" height="1080" alt="6580000_16_18_27_7" src="https://github.com/user-attachments/assets/4863607d-fa2b-49a5-b6e5-6d0709341b05" /> |
| <img width="1920" height="1080" alt="5000000_6_9" src="https://github.com/user-attachments/assets/187c1580-8a65-4377-8ca2-5807531bf008" /> | <img width="3840" height="2160" alt="10000000_9_3_7" src="https://github.com/user-attachments/assets/019aa88b-ea27-4496-b0eb-268ad219c601" /> |
| <img width="1920" height="1080" alt="10000000_18_6_1_1" src="https://github.com/user-attachments/assets/232d68b7-18b5-41a3-a916-459793540ab9" /> | <img width="1920" height="1080" alt="4000000_13_14" src="https://github.com/user-attachments/assets/e86f9404-639c-41e3-a984-2c9dbccb9791" /> |

## Usage
On startup, 3 random fractals are created. These can be tweaked and added to, with the results being gradually rendered to the screen as a preview.
The fractal can then be rendered to a file at the desired resolution and sample count.

The keys `WASD` can be used to pan the view around, and `QE` are used to zoom the view. Useful information may be shown in the terminal window, especially when saving and loading files. It may be hidden behind the main window when starting the program.
The image below shows an example set of variations after starting the program, and the meanings of the settings are as follows:

### Settings
* Samples per frame - how many sample points will be calculated every frame of the preview. Higher values make the fractal appear faster, but reduce the interactive frame rate.
* Max preview samples - how many samples to render in the preview before stopping, can be set to 0 for infinite.
* Initial iterations - how many iterations should be applied to the sample point before it is rendered. This reduces noise from the random start point of the sample.
* Iterations - how many iterations should be applied after the initial ones. The position of the sample point will be rendered after each of these iterations, and effectively just increases the number of samples per frame when `initial iterations` is sufficiently high (e.g. 20+).
* Gamma - the pixel value will be set to `pow(pixel, 1/gamma)` in a post processing step.
* Darkness - the pixel value will be multiplied by `1/darkness` in a post processing step before gamma. "Darkness" is chosen as opposed to brightness, as the slider is nicer to control this way.
* Faster plotting - improves performance when lots of points try to render to the same pixel at the same time but consequently losing the colour from some points (disables atomic addition).
* Clear every frame - prevents samples from accumulating by resetting the preview buffer every frame.
* Clear image - resets the preview, clearing all accumulated samples.
* Reset camera - resets the position of the camera to the centre of the world.
* Pause/Resume - pauses/resumes the accumulation of samples. The camera cannot be moved while paused, as moving the view requires re-rendering the fractal.
* Save flame config - saves the current set of variation settings to a file.
* Load flame config - loads a set of variation settings from a file.
* Previous flame - allows going back to the previous flame after randomising or loading from file (in case of accidentally going past a random flame you liked).

### Render
* Render resolution - the size in pixels of the output file. This does not affect the preview, which uses the resolution of the window. NOTE: the brightness of a pixel is proportional to the amount of times a sample point is rendered to it. Therefore, higher resolutions have a lower chance of each pixel being rendered to, and are often darker. Compensate for this with the darkness slider or more samples.
* Match preview size - forces the output resolution to match the window resolution. Untick to set resolution manually.
* Number of samples - the total number of samples which will be calculated for the rendered image.
* Match current preview sample num - forces the number of samples in the rendered image to match how many samples have been calculated so far in the preview. Untick this to set the number of samples manually.
* Save as image - click to select a location to save the image, and then it will be rendered. The terminal window will print out the stage of rendering and a reminder of where the image was saved.

<img width="439" height="680" alt="left" src="https://github.com/user-attachments/assets/82311180-72b3-4fdf-a2ab-47356eb3e66e" />

### Variations
This is this list of variations currently being applied to the sample points.
* Randomise [value] - randomises this value for each variation in the list. Useful for searching for nice shapes and colour schemes and forming a gambling addiction.
* Add variation - adds a variation with default settings.
* Variation - this is the main characteristic of the shape which will be formed, and refers to the list found here: https://flam3.com/flame_draves.pdf#page=16.
* Colour - the colour associated with the variation.
* Weight - affects the probability of this variation being chosen by a sample point. The chance of a variation being chosen is its weight divided by the sum of all weights. E.g. two variations with a weight of 1 will each have a 1/2 chance to be chosen, weights of 0.5 and 1 have 1/3 and 2/3 respectively.
* Rotation, translation and scale - transforms points every time that variation is used. Rotation and scale use the translation position as the centre.
* Remove - remove this variation from the list.

<img width="410" height="441" alt="right" src="https://github.com/user-attachments/assets/2b471d6a-4e5b-4f6c-a2a2-110941e55d5c" />

## Build Dependencies
* GLFW - https://www.glfw.org/
* glad - https://glad.dav1d.de/
* ImGui - https://github.com/ocornut/imgui
* Native File Dialog Extended - https://github.com/btzy/nativefiledialog-extended
* stb image - https://github.com/nothings/stb/tree/master
