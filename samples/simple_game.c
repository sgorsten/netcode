#include <netcode.h>
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <stdio.h>

void error(const char * message)
{
    fprintf(stderr, message);
    exit(EXIT_FAILURE);
}

int main(int argc, char * argv[])
{
    if (glfwInit() != GL_TRUE) error("glfwInit() failed.");
	GLFWwindow * win = glfwCreateWindow(1280, 720, "Simple Simulation", NULL, NULL);
	if (!win) error("glfwCreateWindow(...) failed.");
	glfwMakeContextCurrent(win);

	double t0 = glfwGetTime();
	while (!glfwWindowShouldClose(win))
	{
		glfwPollEvents();

		const double t1 = glfwGetTime(), timestep = t1 - t0;
		if (timestep < 1.0 / 60) continue;
		t0 = t1;
        
		glClear(GL_COLOR_BUFFER_BIT);
		glPushMatrix();
		glOrtho(0, 1280, 720, 0, -1, +1);
        glPopMatrix();
		glfwSwapBuffers(win);
	}

	glfwDestroyWindow(win);
	glfwTerminate();
    return EXIT_SUCCESS;
}