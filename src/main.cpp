#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <windows.h>

//#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;
typedef uint16_t u16;

typedef float f32;
typedef double f64;

#define GIGABYTE(a) (u64) a * 1000*1000*1000

int main(void)
{
	u8 * memory_pool = (u8*) malloc(GIGABYTE(1));
    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit())
        return -1;

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(1280, 720, "CPP Voxels!!", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
		printf("unable to create the window");
        return -1;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}