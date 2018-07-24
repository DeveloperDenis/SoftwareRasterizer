# Sample Output Images

![Sample Image 1](/output/monkey_wireframe.gif)
![Sample Image 2](/output/monkey_flat.gif)
![Sample Image 3](/output/monkey_phong.gif)

# Features

A basic software 3D rasterizer written in C++ without any external libraries that runs on both Windows and Linux.
You can swap between three different rendering modes, wire frame, flat shading, and phong shading by clicking the left mouse button.

To create the software rasterizer many different aspects of graphics programming were implemented, such as:
- Bresenham's line drawing algorithm
- Triangle rasterization using barycentric coordinates (earlier versions were made that used a simple scan line fill algorithm or cross products to rasterize the triangle).
- Rotation, scale, and translation matrices
- View matrix calculation (using the Look-At approach)
- Perspective projection matrix calculation
- Z-Buffers to make sure only the closest triangles are present in the final image
- Various shading approaches (notably, flat shading and Phong shading, though earlier versions did use Gouraud shading)