# VulkanGraphicsEngine
Working on making this into a graphics demo engine. I took the code I wrote for the Vulkan tutorial, and attempted to create some useful abstractions around it, in order to refamiliarize myself with Vulkan, after not having worked with it for several years. My goal is to eventually use this to build some interesting demos and learn some advanced techniques.

# Build
Builds with Visual Studio 2019.

Requires Vulkan SDK (tested with 1.2.162.1) to be installed from: https://vulkan.lunarg.com/sdk/home. The installer will set an environment variable VULKAN_SDK, which the Visual Studio Project uses for an include path.

Also requires the VulkanMemoryAllocator to be in the dependencies directory (installed via zip or github).
