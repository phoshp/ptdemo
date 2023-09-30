project('ptdemo', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++20'])

inc = include_directories('include')

sources = [ 'src/main.cpp', 'src/GameInstance.cpp', 'src/graphics/RenderEngine.cpp', 'src/graphics/vulkan/VulkanRenderer.cpp', 'src/graphics/vulkan/VkBootstrap.cpp']
deps = [
  dependency('vulkan'),
  dependency('glm'),
  dependency('shaderc'),
  dependency('sdl2')
]

executable('ptdemo',
           sources,
           include_directories: inc,
           dependencies: deps,
           install : true)
