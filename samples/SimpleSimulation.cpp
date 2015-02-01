#include "Distribution.h"

#include <iostream>
#include <random>
#include <GLFW/glfw3.h>

enum { NUM_OBJECTS = 50 };
struct Object { int px, py, vx, vy; };
struct Frame { Object objects[NUM_OBJECTS]; };

class Server
{
	Frame current, last;
	IntegerDistribution dist;
public:
	Server()
	{
		std::mt19937 engine;
		for (auto & object : current.objects)
		{
			object.px = std::uniform_int_distribution<int>(500, 12300)(engine);
			object.py = std::uniform_int_distribution<int>(500, 6700)(engine);
			object.vx = std::uniform_int_distribution<int>(-1000, +1000)(engine);
			object.vy = std::uniform_int_distribution<int>(-1000, +1000)(engine);
		}
		for (auto & object : last.objects)
		{
			object = { 0, 0, 0, 0 };
		}
	}

	void Update(double timestep)
	{
		for (auto & object : current.objects)
		{
			object.px += object.vx * timestep;
			object.py += object.vy * timestep;
			if (object.px < 200 && object.vx < 0) object.vx = -object.vx;
			if (object.px > 12600 && object.vx > 0) object.vx = -object.vx;
			if (object.py < 200 && object.vy < 0) object.vy = -object.vy;
			if (object.py > 7000 && object.vy > 0) object.vy = -object.vy;
		}
	}

	void Encode(arith::Encoder & encoder)
	{
		for (int i = 0; i < NUM_OBJECTS; ++i)
		{
			dist.EncodeAndTally(encoder, current.objects[i].px - last.objects[i].px);
			dist.EncodeAndTally(encoder, current.objects[i].py - last.objects[i].py);
		}
		last = current;
	}
};

class Client
{
	Frame current, last;
	IntegerDistribution dist;
public:
	Client()
	{
		memset(&current, 0, sizeof(current));
		memset(&last, 0, sizeof(last));
	}

	void Draw() const
	{
		glClear(GL_COLOR_BUFFER_BIT);
		glPushMatrix();
		glOrtho(0, 12800, 7200, 0, -1, +1);
		for (auto & object : current.objects)
		{
			glBegin(GL_TRIANGLE_FAN);
			for (int i = 0; i < 12; ++i)
			{
				float a = i*6.28f / 12;
				glVertex2f(object.px + cos(a) * 100, object.py + sin(a) * 100);
			}
			glEnd();
		}
		glPopMatrix();
	}

	void Decode(arith::Decoder & decoder)
	{
		last = current;
		for (int i = 0; i < NUM_OBJECTS; ++i)
		{
			current.objects[i].px = last.objects[i].px + dist.DecodeAndTally(decoder);
			current.objects[i].py = last.objects[i].py + dist.DecodeAndTally(decoder);
		}
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
		server.Update(timestep);

		std::vector<uint8_t> buffer;
		arith::Encoder encoder(buffer);
		server.Encode(encoder);
		encoder.Finish();
		std::cout << "Compressed state from " << (sizeof(Object) * NUM_OBJECTS) << " B to " << buffer.size() << " B." << std::endl;

		client.Decode(arith::Decoder(buffer));
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