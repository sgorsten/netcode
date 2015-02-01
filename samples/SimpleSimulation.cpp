#include <iostream>
#include <GLFW/glfw3.h>

int main(int argc, char * argv []) try
{
	if (glfwInit() != GL_TRUE) throw std::runtime_error("glfwInit() failed.");
	auto win = glfwCreateWindow(1280, 720, "Simple Simulation", nullptr, nullptr);
	if (!win) throw std::runtime_error("glfwCreateWindow(...) failed.");
	glfwMakeContextCurrent(win);

	while (!glfwWindowShouldClose(win))
	{
		glfwPollEvents();
		glClear(GL_COLOR_BUFFER_BIT);
		glfwSwapBuffers(win);
	}

	glfwDestroyWindow(win);
	glfwTerminate();
	return EXIT_SUCCESS;
}
catch (const std::exception & e)
{
	std::cerr << "Caught exception: " << e.what() << std::endl;
	glfwTerminate();
	return EXIT_FAILURE;
}