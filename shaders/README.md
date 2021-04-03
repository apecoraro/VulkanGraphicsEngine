* Naming Scheme

For each descriptor in the shader program:
[DescriptorType]_d[BindingIndex]_

Descriptor Types:
* ub - uniform buffer
* s - sampler

For each input:
[InputType0]_[InputType1]...[InputTypeN]_in

Input Types:
* xy - 2D vertices
* xyz - 3D vertices
* rgb - 3 component color
* rgba - 4 component color
* uv - 2D texture coordinates.

xyz_rgb_uv_in - xyz position, rgb color, uv texture coords.

For each output:
[OutputType0]_[OutputType1]...[OutputTypeN]_out

Output types are same as input types.

Example:

ub_d0_xyz_rgb_uv_in_rgb_uv_out.vert - vertex shader with a single uniform buffer at binding 0, 3D position input at attribute binding 0, 3 component color at attribute binding 1, 2D texture coordinates at attribute binding 2, outputs a 3 component color, and 2D texture coordinate.

