* First do:
```
git submodule update --init --recursive
```

Also requires Vulkan SDK, but uses VULKAN_SDK environment variable that is configured by the LUNARG installer. Currently tested against:
* VulkanSDK/1.2.* - download installer from https://vulkan.lunarg.com/sdk/home.

Also, requires GLFW pre-compiled binaries, unpack them into a directory named: glfw
* glfw - version 3.3 or higher, https://www.glfw.org/download

