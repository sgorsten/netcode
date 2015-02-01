#include "Distribution.h"

#include <iostream>
#include <random>
#include <GLFW/glfw3.h>

enum { NUM_BALLS = 50 };
struct Ball { int px, py, vx, vy; };

class Server
{
	Ball balls[NUM_BALLS];
	IntegerDistribution dist;
public:
	Server()
	{
		std::mt19937 engine;
		for (auto & ball : balls)
		{
			ball.px = std::uniform_int_distribution<int>(500, 12300)(engine);
			ball.py = std::uniform_int_distribution<int>(500, 6700)(engine);
			ball.vx = std::uniform_int_distribution<int>(-1000, +1000)(engine);
			ball.vy = std::uniform_int_distribution<int>(-1000, +1000)(engine);
		}
	}

	void Update(double timestep)
	{
		for (auto & ball : balls)
		{
			ball.px += ball.vx * timestep;
			ball.py += ball.vy * timestep;
			if (ball.px < 200 && ball.vx < 0) ball.vx = -ball.vx;
			if (ball.px > 12600 && ball.vx > 0) ball.vx = -ball.vx;
			if (ball.py < 200 && ball.vy < 0) ball.vy = -ball.vy;
			if (ball.py > 7000 && ball.vy > 0) ball.vy = -ball.vy;
		}
	}

	void Encode(arith::Encoder & encoder)
	{
		for (auto & ball : balls)
		{
			dist.EncodeAndTally(encoder, ball.px);
			dist.EncodeAndTally(encoder, ball.py);
		}
	}
};

class Client
{
	Ball balls[NUM_BALLS];
	IntegerDistribution dist;
public:
	void SetBalls(const Ball * b) { memcpy(balls, b, sizeof(balls)); }

	void Draw() const
	{
		glClear(GL_COLOR_BUFFER_BIT);
		glPushMatrix();
		glOrtho(0, 12800, 7200, 0, -1, +1);
		for (auto & ball : balls)
		{
			glBegin(GL_TRIANGLE_FAN);
			for (int i = 0; i < 12; ++i)
			{
				float a = i*6.28f / 12;
				glVertex2f(ball.px + cos(a) * 100, ball.py + sin(a) * 100);
			}
			glEnd();
		}
		glPopMatrix();
	}

	void Decode(arith::Decoder & decoder)
	{
		for (auto & ball : balls)
		{
			ball.px = dist.DecodeAndTally(decoder);
			ball.py = dist.DecodeAndTally(decoder);
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
		std::cout << "Compressed state from " << (sizeof(Ball) * NUM_BALLS) << " B to " << buffer.size() << " B." << std::endl;

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