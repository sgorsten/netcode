#include "View.h"

#include <cstdint>
#include <vector>
#include <iostream>
#include <random>
#include <GLFW/glfw3.h>

enum { NUM_OBJECTS = 50 };
struct PhysicsObject { float px, py, vx, vy; VObject vobj; };

class Server
{
	VServer server;
	PhysicsObject objects[NUM_OBJECTS];
public:
	Server()
	{
		VClass cl = vCreateClass(2);
		server = vCreateServer(&cl, 1);

		std::mt19937 engine;
		for (auto & object : objects)
		{
			object.px = std::uniform_real_distribution<float>(50, 1230)(engine);
			object.py = std::uniform_real_distribution<float>(50, 670)(engine);
			object.vx = std::uniform_real_distribution<float>(-100, +100)(engine);
			object.vy = std::uniform_real_distribution<float>(-100, +100)(engine);
			object.vobj = vCreateObject(server, cl);
		}
	}

	void Update(float timestep)
	{
		for (auto & object : objects)
		{
			object.px += object.vx * timestep;
			object.py += object.vy * timestep;
			if (object.px < 20 && object.vx < 0) object.vx = -object.vx;
			if (object.px > 1260 && object.vx > 0) object.vx = -object.vx;
			if (object.py < 20 && object.vy < 0) object.vy = -object.vy;
			if (object.py > 700 && object.vy > 0) object.vy = -object.vy;
		}
	}

	std::vector<uint8_t> PublishUpdate()
	{
		for (auto & object : objects)
		{
			vSetObjectInt(object.vobj, 0, object.px * 10);
			vSetObjectInt(object.vobj, 1, object.py * 10);
		}

		uint8_t buffer[2048];
		int used = vPublishUpdate(server, buffer, sizeof(buffer));
		if (used > sizeof(buffer)) throw std::runtime_error("Buffer not large enough.");
		return {buffer, buffer + used};
	}
};

class Client
{
	VClass objectClass;
	VClient client;
public:
	Client()
	{
		objectClass = vCreateClass(2);
		client = vCreateClient(&objectClass, 1);
	}

	void Draw() const
	{
		glClear(GL_COLOR_BUFFER_BIT);
		glPushMatrix();
		glOrtho(0, 1280, 720, 0, -1, +1);
		for (int i = 0, n = vGetViewCount(client); i < n; ++i)
		{
			auto view = vGetView(client, i);
			if (vGetViewClass(view) == objectClass)
			{
				float x = vGetViewInt(view, 0)*0.1f;
				float y = vGetViewInt(view, 1)*0.1f;
				glBegin(GL_TRIANGLE_FAN);
				for (int i = 0; i < 12; ++i)
				{
					float a = i*6.28f / 12;
					glVertex2f(x + cos(a) * 10, y + sin(a) * 10);
				}
				glEnd();
			}
		}
		glPopMatrix();
	}

	void ConsumeUpdate(const std::vector<uint8_t> & buffer)
	{
		vConsumeUpdate(client, buffer.data(), buffer.size());
	}
};

int main(int argc, char * argv []) try
{
	Server server;
	Client client;

	if (glfwInit() != GL_TRUE) throw std::runtime_error("glfwInit() failed.");
	auto win = glfwCreateWindow(1280, 720, "Simple Simulation", nullptr, nullptr);
	if (!win) throw std::runtime_error("glfwCreateWindow(...) failed.");
	glfwMakeContextCurrent(win);

	double t0 = glfwGetTime();
	while (!glfwWindowShouldClose(win))
	{
		glfwPollEvents();

		const double t1 = glfwGetTime(), timestep = t1 - t0;
		if (timestep < 1.0 / 60) continue;
		t0 = t1;
		server.Update(static_cast<float>(timestep));

		auto buffer = server.PublishUpdate();
		std::cout << "Compressed state from " << (sizeof(int)*2*NUM_OBJECTS) << " B to " << buffer.size() << " B." << std::endl;

		client.ConsumeUpdate(buffer);
		client.Draw();	
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